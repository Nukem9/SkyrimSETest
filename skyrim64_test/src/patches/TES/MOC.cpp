#include <DirectXMath.h>
#include "../rendering/common.h"
#include "../../common.h"
#include "BSShader/BSShaderUtil.h"
#include "BSShader/BSShaderProperty.h"
#include "BSBatchRenderer.h"
#include "MOC.h"

#include "NiMain/BSMultiBoundNode.h"

#include "NiMain/NiNode.h"
#include "NiMain/NiCamera.h"
#include <smmintrin.h>
using namespace DirectX;

#include "MOC_ThreadedMerger.h"
#include <meshoptimizer/src/meshoptimizer.h>

const int MOC_WIDTH = 1280;
const int MOC_HEIGHT = 720;

#if SKYRIM64_USE_TRACY

extern ID3D11Texture2D *g_OcclusionTexture;
extern ID3D11ShaderResourceView *g_OcclusionTextureSRV;

namespace MOC
{
	AutoPtr(NiNode *, WorldScenegraph, 0x2F4CE30);

	MOC_ThreadedMerger *ThreadedMOC;

	struct IndexPair
	{
		uint32_t *Data;
		uint32_t Count;
	};

	std::unordered_map<void *, float *> m_VertMap;
	std::unordered_map<void *, IndexPair> m_IndexMap;

	uint32_t *ConvertIndices(const void *Input, uint32_t Count, uint32_t MaxVertexCount)
	{
		const uint16_t *in = (uint16_t *)Input;
		uint32_t *out = new uint32_t[Count];

		for (uint32_t i = 0; i < Count; i++)
			out[i] = in[i];

		return out;
	}

	float *ConvertVerts(const void *Input, uint32_t Count, uint32_t ByteStride)
	{
		float *in = (float *)Input;
		float *out = new float[Count * 4];
		float *base = out;

		for (uint32_t i = 0; i < Count; i++)
		{
			//
			// X -> X
			// Y -> Y
			// Z -> 1.0f
			// W -> Z
			// Remaining data discarded
			//
			out[0] = in[0];
			out[1] = in[1];
			out[2] = 1.0f;
			out[3] = in[2];

			in = (float *)((uintptr_t)in + ByteStride);
			out += 4;
		}

		return base;
	}

	SRWLOCK vertLock = SRWLOCK_INIT;
	void GetCachedVerticesAndIndices(BSGeometry *Geometry, IndexPair *Indices, float **Vertices)
	{
		void *dataPtr;

		const void *indexData;
		uint32_t indexCount;

		const void *vertexData;
		uint32_t vertexCount;
		uint32_t vertexStride;

		AcquireSRWLockExclusive(&vertLock);

		if (Geometry->QType() == GEOMETRY_TYPE_TRISHAPE)
		{
			auto triShape = static_cast<BSTriShape *>(Geometry);
			auto rendererData = reinterpret_cast<BSGraphics::TriShape *>(triShape->QRendererData());
			dataPtr = rendererData;

			auto itrI = m_IndexMap.find(rendererData);
			auto itrV = m_VertMap.find(rendererData);

			if (itrI != m_IndexMap.end())
				*Indices = itrI->second;
			else
				Indices->Data = nullptr;

			if (itrV != m_VertMap.end())
				*Vertices = itrV->second;
			else
				*Vertices = nullptr;

			indexData = rendererData->m_RawIndexData;
			indexCount = triShape->m_TriangleCount * 3;

			vertexData = rendererData->m_RawVertexData;
			vertexCount = triShape->m_VertexCount;
			vertexStride = BSGeometry::CalculateVertexSize(rendererData->m_VertexDesc);
		}
		else if (Geometry->QType() == GEOMETRY_TYPE_DYNAMIC_TRISHAPE)
		{
			AssertMsg(false, "BSGraphics::DynamicTriShape -> indices are always a nullptr");
		}
		else
		{
			Assert(false);
		}

		// If one wasn't found, it needs conversion
		if (!Indices->Data)
		{
			IndexPair p;
			p.Data = ConvertIndices(indexData, indexCount, vertexCount);
			p.Count = indexCount;

			if (indexCount > 300)
				p.Count = meshopt_simplify(p.Data, p.Data, indexCount, (const float *)vertexData, vertexCount, vertexStride, (size_t)(indexCount * .50f), 1e-3f);// Target 33% of original triangles

			m_IndexMap.insert_or_assign(dataPtr, p);
			*Indices = p;
		}

		if (!*Vertices)
		{
			*Vertices = ConvertVerts(vertexData, vertexCount, vertexStride);
			m_VertMap.insert_or_assign(dataPtr, *Vertices);
		}

		ReleaseSRWLockExclusive(&vertLock);
	}

	void RemoveCachedVerticesAndIndices(void *RendererData)
	{
		uint32_t *indices = nullptr;
		float *verts = nullptr;

		AcquireSRWLockExclusive(&vertLock);

		if (auto itr = m_IndexMap.find(RendererData); itr != m_IndexMap.end())
		{
			indices = itr->second.Data;
			m_IndexMap.erase(itr);
		}

		if (auto itr = m_VertMap.find(RendererData); itr != m_VertMap.end())
		{
			verts = itr->second;
			m_VertMap.erase(itr);
		}

		ReleaseSRWLockExclusive(&vertLock);

		// Both buffers might be used outside the lock - I don't care right now (cell_unload() + moc_render() => BOOM)
		if (indices)
			delete[] indices;

		if (verts)
			delete[] verts;
	}

	void UpdateDepthViewTexture()
	{
		ZoneScopedN("MOC UpdateDepthView");
		ThreadedMOC->UpdateDepthViewTexture(BSGraphics::Renderer::QInstance()->Data.pContext, g_OcclusionTexture);
	}

	void ForceFlush()
	{
		ProfileTimer("MOC WaitForRender");
		ThreadedMOC->Flush();
	}

	XMMATRIX MyView;
	XMMATRIX MyProj;
	XMMATRIX MyViewProj;
	NiPoint3 MyPosAdjust;

	bool mocInit = false;

	void Init()
	{
		ThreadedMOC = new MOC_ThreadedMerger(MOC_WIDTH, MOC_HEIGHT, 4, true);

		ThreadedMOC->SetTraverseSceneCallback(TraverseSceneGraphCallback);
		ThreadedMOC->SetRenderGeometryCallback(RenderGeometryCallback);

		mocInit = true;
	}

	BSMultiBoundAABB *GetAABBNode(const NiAVObject *Object)
	{
		if (BSMultiBoundNode *multiBoundNode = Object->IsMultiBoundNode())
		{
			if (multiBoundNode->spMultiBound && multiBoundNode->spMultiBound->spShape)
			{
				auto shape = multiBoundNode->spMultiBound->spShape;

				if (shape->IsExactKindOf(NiRTTI::ms_BSMultiBoundAABB))
					return static_cast<BSMultiBoundAABB *>(shape);
			}
		}

		return nullptr;
	}

	bool TestSphere(NiAVObject *Object);
	bool TestAABB(BSMultiBoundAABB *Object);

	bool TestObject(NiAVObject *Object)
	{
		if (!mocInit || !ui::opt::EnableOcclusionTesting)
			return true;

		if (Object->QAppCulled())
			return true;

		ProfileCounterInc("MOC CullObjectCount");
		ProfileTimer("MOC CullTest");

		BSMultiBoundAABB *aabb = GetAABBNode(Object);
		bool visible = false;

		if (aabb)
			visible = TestAABB(aabb);
		else
			visible = TestSphere(Object);

		if (visible)
		{
			ProfileCounterInc("MOC CullObjectPassed");
		}

		return visible;
	}

	bool TestSphere(NiAVObject *Object)
	{
		if (Object->m_kWorldBound.m_fRadius <= 5.0f)
			return true;

		//if (Object->IsTriShape() && Object->IsGeometry()->QType() != GEOMETRY_TYPE_TRISHAPE)
		//	return true;

		float sphereRadius = Object->m_kWorldBound.m_fRadius;

		XMMATRIX view = MyView;
		XMMATRIX projection = MyProj;

		// w as 1.0f
		XMVECTOR bounds = _mm_sub_ps(_mm_setr_ps(
			Object->m_kWorldBound.m_kCenter.x,
			Object->m_kWorldBound.m_kCenter.y,
			Object->m_kWorldBound.m_kCenter.z,
			1.0f),
			MyPosAdjust.AsXmm());

		// Never cull sphere if player is inside it
		if (XMVector3Length(bounds).m128_f32[0] <= sphereRadius)
			return true;

		// ------ Early depth rejection test

		XMVECTOR v = XMVectorSubtract(_mm_setzero_ps(), bounds);
		XMVECTOR closestPoint = XMVectorAdd(bounds, XMVectorScale(XMVector3Normalize(v), sphereRadius));
		closestPoint = XMVector4Transform(XMVectorSetW(closestPoint, 1.0f), MyViewProj);// Project to clip space

		float closestSpherePointW = closestPoint.m128_f32[3];

		if (closestSpherePointW < 0.000001f)
			return true;

		// ------

		XMVECTOR viewEye = { view.r[0].m128_f32[3], view.r[1].m128_f32[3], view.r[2].m128_f32[3], 0.0f };
		viewEye = XMVectorNegate(viewEye);

		XMVECTOR viewEyeSphereDirection = XMVectorSubtract(viewEye, bounds);
		float cameraSphereDistance = XMVector3Length(viewEyeSphereDirection).m128_f32[0];// distance()

		XMVECTOR viewUp = { view.r[0].m128_f32[1], view.r[1].m128_f32[1], view.r[2].m128_f32[1], 0.0f };
		XMVECTOR viewRight = XMVector3Normalize(XMVector3Cross(viewEyeSphereDirection, viewUp));

		// Help handle perspective distortion.
		// http://article.gmane.org/gmane.games.devel.algorithms/21697/
		float fRadius = cameraSphereDistance * tan(asin(sphereRadius / cameraSphereDistance));

		// Compute the offsets for the points around the sphere
		XMVECTOR vUpRadius = XMVectorScale(viewUp, fRadius);
		XMVECTOR vRightRadius = XMVectorScale(viewRight, fRadius);

		// Generate the 4 corners of the sphere in world space
		XMVECTOR vCorner0WS = XMVectorSubtract(XMVectorAdd(bounds, vUpRadius), vRightRadius);		// Top-Left
		XMVECTOR vCorner1WS = XMVectorAdd(XMVectorAdd(bounds, vUpRadius), vRightRadius);			// Top-Right
		XMVECTOR vCorner2WS = XMVectorSubtract(XMVectorSubtract(bounds, vUpRadius), vRightRadius);  // Bottom-Left
		XMVECTOR vCorner3WS = XMVectorAdd(XMVectorSubtract(bounds, vUpRadius), vRightRadius);		// Bottom-Right

		// Project the 4 corners of the sphere into clip space, then convert to normalized device coordinates
		XMVECTOR vCorner0CS = XMVector4Transform(vCorner0WS, MyViewProj);
		XMVECTOR vCorner1CS = XMVector4Transform(vCorner1WS, MyViewProj);
		XMVECTOR vCorner2CS = XMVector4Transform(vCorner2WS, MyViewProj);
		XMVECTOR vCorner3CS = XMVector4Transform(vCorner3WS, MyViewProj);

		XMVECTOR vCorner0NDC = XMVectorDivide(vCorner0CS, XMVectorSplatW(vCorner0CS));
		XMVECTOR vCorner1NDC = XMVectorDivide(vCorner1CS, XMVectorSplatW(vCorner1CS));
		XMVECTOR vCorner2NDC = XMVectorDivide(vCorner2CS, XMVectorSplatW(vCorner2CS));
		XMVECTOR vCorner3NDC = XMVectorDivide(vCorner3CS, XMVectorSplatW(vCorner3CS));

		// Bounding rect mins and maxs
		XMVECTOR xyMins = _mm_min_ps(vCorner0NDC, _mm_min_ps(vCorner1NDC, _mm_min_ps(vCorner2NDC, vCorner3NDC)));// zw discarded
		XMVECTOR xyMaxs = _mm_max_ps(vCorner0NDC, _mm_max_ps(vCorner1NDC, _mm_max_ps(vCorner2NDC, vCorner3NDC)));// zw discarded

		auto r = ThreadedMOC->GetMOC()->TestRect(xyMins.m128_f32[0], xyMins.m128_f32[1], xyMaxs.m128_f32[0], xyMaxs.m128_f32[1], closestSpherePointW);

		if (r != MaskedOcclusionCulling::VISIBLE)
		{
#define CONVERT_X(x) ((x) + 1.0) * 2560 * 0.5 + 0
#define CONVERT_Y(y) (1.0 - (y)) * 1440 * 0.5 + 0
			/*
			static SRWLOCK lock = SRWLOCK_INIT;

			AcquireSRWLockExclusive(&lock);

			ImGui::GetWindowDrawList()->AddRect(ImVec2(CONVERT_X(xyMins.m128_f32[0]), CONVERT_Y(xyMins.m128_f32[1])), ImVec2(CONVERT_X(xyMaxs.m128_f32[0]), CONVERT_Y(xyMaxs.m128_f32[1])), IM_COL32(255, 0, 0, 255));

			ReleaseSRWLockExclusive(&lock);
			*/
			return false;
		}

		return true;
	}

	bool TestAABB(BSMultiBoundAABB *Object)
	{
		const static int AABB_VERTICES = 8;
		const static uint32_t sBBxInd[AABB_VERTICES] = { 1, 0, 0, 1, 1, 1, 0, 0 };
		const static uint32_t sBByInd[AABB_VERTICES] = { 1, 1, 1, 1, 0, 0, 0, 0 };
		const static uint32_t sBBzInd[AABB_VERTICES] = { 1, 1, 0, 0, 0, 1, 1, 0 };

		// w ends up being garbage, but it doesn't matter - we ignore it anyway.
		__m128 vCenter = _mm_sub_ps(Object->m_kCenter.AsXmm(), MyPosAdjust.AsXmm());
		__m128 vHalf = Object->m_kHalfExtents.AsXmm();

		__m128 vMin = _mm_sub_ps(vCenter, vHalf);
		__m128 vMax = _mm_add_ps(vCenter, vHalf);

		// transforms
		__m128 xRow[2], yRow[2], zRow[2];
		xRow[0] = _mm_mul_ps(_mm_shuffle_ps(vMin, vMin, 0x00), MyViewProj.r[0]);
		xRow[1] = _mm_mul_ps(_mm_shuffle_ps(vMax, vMax, 0x00), MyViewProj.r[0]);
		yRow[0] = _mm_mul_ps(_mm_shuffle_ps(vMin, vMin, 0x55), MyViewProj.r[1]);
		yRow[1] = _mm_mul_ps(_mm_shuffle_ps(vMax, vMax, 0x55), MyViewProj.r[1]);
		zRow[0] = _mm_mul_ps(_mm_shuffle_ps(vMin, vMin, 0xaa), MyViewProj.r[2]);
		zRow[1] = _mm_mul_ps(_mm_shuffle_ps(vMax, vMax, 0xaa), MyViewProj.r[2]);

		// Find the minimum of each component
		__m128 minVert = _mm_add_ps(MyViewProj.r[3], _mm_add_ps(_mm_add_ps(_mm_min_ps(xRow[0], xRow[1]), _mm_min_ps(yRow[0], yRow[1])), _mm_min_ps(zRow[0], zRow[1])));
		float minW = minVert.m128_f32[3];

		if (minW < 0.00000001f)
			return true;

		__m128 screenMin = _mm_set1_ps(FLT_MAX);
		__m128 screenMax = _mm_set1_ps(-FLT_MAX);
		__m128 baseVert = MyViewProj.r[3];

		for (uint32_t i = 0; i < AABB_VERTICES; i++)
		{
			// Transform the vertex
			__m128 vert = baseVert;
			vert += xRow[sBBxInd[i]];
			vert += yRow[sBByInd[i]];
			vert += zRow[sBBzInd[i]];

			// We have inverted z; z is in front of near plane iff z <= w.
			__m128 vertZ = _mm_shuffle_ps(vert, vert, 0xaa); // vert.zzzz
			__m128 vertW = _mm_shuffle_ps(vert, vert, 0xff); // vert.wwww

			// project
			__m128 xformedPos = _mm_div_ps(vert, vertW);

			// update bounds
			screenMin = _mm_min_ps(screenMin, xformedPos);
			screenMax = _mm_max_ps(screenMax, xformedPos);
		}

		MaskedOcclusionCulling::CullingResult r = ThreadedMOC->GetMOC()->TestRect(screenMin.m128_f32[0], screenMin.m128_f32[1], screenMax.m128_f32[0], screenMax.m128_f32[1], minW);

		if (r != MaskedOcclusionCulling::VISIBLE)
		{
#define CONVERT_X(x) ((x) + 1.0) * 2560 * 0.5 + 0
#define CONVERT_Y(y) (1.0 - (y)) * 1440 * 0.5 + 0
			/*
			static SRWLOCK lock = SRWLOCK_INIT;

			AcquireSRWLockExclusive(&lock);

			ImGui::GetWindowDrawList()->AddRect(ImVec2(CONVERT_X(screenMin.m128_f32[0]), CONVERT_Y(screenMin.m128_f32[1])), ImVec2(CONVERT_X(screenMax.m128_f32[0]), CONVERT_Y(screenMax.m128_f32[1])), IM_COL32(255, 0, 0, 255));

			ReleaseSRWLockExclusive(&lock);
			*/
			return false;
		}

		return true;
	}

	struct GeometryDistEntry
	{
		BSGeometry *Geometry;
		float DistanceSquared;
	};

	std::vector<GeometryDistEntry> GeoList;

	void RegisterGeometry(BSGeometry *Geometry)
	{
		// Strictly check for BSLightingShaderProperty
		BSShaderProperty *shaderProperty = Geometry->QShaderProperty();

		if (!shaderProperty)
			return;

		if (!shaderProperty->IsExactKindOf(NiRTTI::ms_BSLightingShaderProperty))
			return;

		// TriShape or dynamic TriShape only
		if (Geometry->QType() == GEOMETRY_TYPE_DYNAMIC_TRISHAPE)
		{
			auto dynTriShape = static_cast<BSDynamicTriShape *>(Geometry);
			auto rendererData = reinterpret_cast<BSGraphics::DynamicTriShape *>(dynTriShape->QRendererData());

			if (rendererData)
				Assert(rendererData->m_RawVertexData != nullptr);
		}
		else if (Geometry->QType() == GEOMETRY_TYPE_TRISHAPE)
		{
			auto triShape = static_cast<BSTriShape *>(Geometry);
			auto rendererData = reinterpret_cast<BSGraphics::TriShape *>(triShape->QRendererData());

			if (rendererData && rendererData->m_RawIndexData && triShape->m_TriangleCount > 1)
			{
				GeometryDistEntry entry;
				entry.Geometry = Geometry;
				entry.DistanceSquared = XMVector3LengthSq(_mm_sub_ps(Geometry->m_kWorldBound.m_kCenter.AsXmm(), MyPosAdjust.AsXmm())).m128_f32[0];

				GeoList.push_back(entry);
			}
		}
	}

	void RenderGeometryCallback(MaskedOcclusionCulling *MOC, void *UserData)
	{
		ProfileTimer("MOC RenderGeometry");
		ZoneScopedN("MOC RenderGeometry");

		BSGeometry *geometry = (BSGeometry *)UserData;

		// If double sided geometry, avoid culling back faces
		MaskedOcclusionCulling::BackfaceWinding winding = MaskedOcclusionCulling::BACKFACE_CW;

		if (geometry->QShaderProperty()->GetFlag(BSShaderProperty::BSSP_FLAG_TWO_SIDED))
			winding = MaskedOcclusionCulling::BACKFACE_NONE;

		// Grab LOD-ified mesh out
		IndexPair indexRawData;
		float *vertexRawData;
		GetCachedVerticesAndIndices(geometry, &indexRawData, &vertexRawData);

		XMMATRIX worldProj = BSShaderUtil::GetXMFromNiPosAdjust(geometry->GetWorldTransform(), MyPosAdjust);
		XMMATRIX worldViewProj = XMMatrixMultiply(worldProj, MyViewProj);

		MOC->RenderTriangles(
			vertexRawData,
			indexRawData.Data,
			indexRawData.Count / 3,
			(float *)&worldViewProj,
			winding,
			MaskedOcclusionCulling::CLIP_PLANE_SIDES);

		ProfileCounterInc("MOC ObjectsRendered");
		ProfileCounterAdd("MOC TrianglesRendered", indexRawData.Count / 3);
	}

	bool CullObject(const NiAVObject *Object, fplanes& Frustum)
	{
		if (!Object)
			return true;

		//
		// Flags
		//
		if (Object->QAppCulled() && !Object->QAlwaysDraw())
			return true;

//		if (!Object->IsVisualObjectI() && !Object->QAlwaysDraw())
//			return true;

		//
		// Names (TODO: remove this awful hack)
		//
		const char *name = Object->GetName()->c_str();

		if (name && name[0] == 'L' && name[1] == '2' && name[2] == '_')
			return true;

		//
		// Frustum tests
		//
		if (auto aabbNode = GetAABBNode(Object))
		{
			// Not all objects have valid boundaries (certain global cells)
			if (aabbNode->m_kHalfExtents.z > 1.0f)
			{
				__m128 center = _mm_sub_ps(aabbNode->m_kCenter.AsXmm(), MyPosAdjust.AsXmm());
				__m128 halfExtents = aabbNode->m_kHalfExtents.AsXmm();

				if (!Frustum.AABBInFrustum(center, halfExtents))
					return true;
			}
		}
		else if (Object->m_kWorldBound.m_fRadius > 10.0f)
		{
			__m128 center = _mm_sub_ps(_mm_setr_ps(
				Object->m_kWorldBound.m_kCenter.x,
				Object->m_kWorldBound.m_kCenter.y,
				Object->m_kWorldBound.m_kCenter.z,
				Object->m_kWorldBound.m_fRadius),
				MyPosAdjust.AsXmm());

			if (!Frustum.SphereInFrustum(center))
				return true;
		}

		return false;
	}

	void RenderRecursive(fplanes& f, const NiAVObject *Object, bool FirstLevel)
	{
		if (CullObject(Object, f))
			return;

		bool validBounds = Object->m_kWorldBound.m_fRadius > 1.0f;

		if (FirstLevel)
		{
			if (validBounds && Object->m_kWorldBound.m_fRadius < ui::opt::OccluderFirstLevelMinSize)
				return;
		}

		const NiNode *node = Object->IsNode();
		BSGeometry *geometry = Object->IsGeometry();

		if (node)
		{
			// Don't care about leaf anim nodes (trees, bushes, plants [alpha])
			if (!node->IsExactKindOf(NiRTTI::ms_BSLeafAnimNode))
			{
				// Enumerate children, but don't render this node specifically
				for (uint32_t i = 0; i < node->GetArrayCount(); i++)
					RenderRecursive(f, node->GetAt(i), false);
			}
		}
		else if (geometry)
		{
			if (geometry->m_kWorldBound.m_fRadius > 100.0f)
			{
				float d1 = geometry->GetWorldTranslate().x - MyPosAdjust.x;
				float d2 = geometry->GetWorldTranslate().y - MyPosAdjust.y;

				// Distance2DSqaured
				if (((d1 * d1) + (d2 * d2)) < (ui::opt::OccluderMaxDistance * ui::opt::OccluderMaxDistance))
					RegisterGeometry(geometry);
			}
		}
	}

	void SendTraverseCommand(NiCamera *Camera)
	{
		if (!mocInit || !ui::opt::EnableOccluderRendering)
			return;

		ThreadedMOC->NotifyPreWork();
		ThreadedMOC->SubmitSceneRender(Camera);
		ThreadedMOC->ClearPreWorkNotify();
	}

	void TraverseSceneGraph(NiCamera *Camera)
	{
		ProfileTimer("MOC TraverseSceneGraph");
		ZoneScopedN("MOC TraverseSceneGraph");

		//
		// Scene graph hierarchy:
		//
		// "WorldRoot Node"            SceneGraph
		// -- "CameraRoot Node"        NiNode
		// ---- "WorldRoot Camera"     NiCamera
		// -- "shadow scene node"      ShadowSceneNode
		// ---- "Sky"                  BSMultiBoundNode
		// ---- "Weather"              NiNode
		// ---- "LODRoot"              BSClearZNode
		// ---- "ObjectLODRoot"        NiNode
		// ------ "(null)"             BSTempNodeManager
		// ------ "(null)"             NiNode
		// ------ "Cell X"             BSMultiBoundNode
		// ------ "Cell X"             BSMultiBoundNode
		// ------ "Cell X"             BSMultiBoundNode
		// ------ "Cell X"             BSMultiBoundNode
		//
		// Cell node hierarchy:
		//
		// "Cell "Wilderness" (0, 0)" BSMultiBoundNode
		// -- "ActorNode"             NiNode
		// -- "MarkerNode"            NiNode
		// -- "LandNode"              NiNode <- TARGET
		// -- "StaticNode"            NiNode <- TARGET
		// -- "DynamicNode"           NiNode
		// -- "OcclusionPlane Node"   NiNode
		// -- "Portal Node"           NiNode
		// -- "MultiBoundNode Node"   NiNode
		// -- "Collision Node"        NiNode
		//
		if (!Camera)
			Camera = static_cast<NiCamera *>(WorldScenegraph->GetAt(0)->IsNode()->GetAt(0));

		GeoList.clear();

		ThreadedMOC->Clear();
		ThreadedMOC->NotifyPreWork();

		MyPosAdjust = Camera->GetWorldTranslate();
		Camera->CalculateViewProjection(MyView, MyProj, MyViewProj);

		float fov = atan(1.0f / MyProj.r[0].m128_f32[0]) * 2.0f * (180.0f / 3.14159265359f);
		float aspect = MyProj.r[1].m128_f32[1] / MyProj.r[0].m128_f32[0];

		float mynear = MyProj.r[2].m128_f32[3] / (MyProj.r[2].m128_f32[2] - 1.0f);
		float myfar = MyProj.r[2].m128_f32[3] / (MyProj.r[2].m128_f32[2] + 1.0f);

		fplanes p;
		p.InitializeFrustumAABB(myfar, mynear, aspect, fov, Camera->GetWorldTranslate().AsXmm(), Camera->GetWorldDirection().AsXmm(), Camera->GetWorldUpVector().AsXmm());

		p.CreateFromViewProjMatrix(MyViewProj);



		const NiNode *node = WorldScenegraph;	// SceneGraph
		node = node->GetAt(1)->IsNode();		// ShadowSceneNode
		node = node->GetAt(3)->IsNode();		// NiNode (ObjectLODRoot)

		// Skip the first 2 child nodes
		for (uint32_t i = 2; i < node->GetArrayCount(); i++)
		{
			if (!node->GetAt(i))
				continue;

			// Recursively render everything in the cell
			const NiNode *cellNode = node->GetAt(i)->IsNode();

			if (CullObject(cellNode, p))
				continue;

			const NiNode *landNode = cellNode->GetAt(2)->IsNode();
			const NiNode *staticNode = cellNode->GetAt(3)->IsNode();

			// Everything in these loops will be some kind of node
			if (!CullObject(landNode, p))
			{
				for (uint32_t i = 0; i < landNode->GetArrayCount(); i++)
					RenderRecursive(p, landNode->GetAt(i), true);
			}

			if (!CullObject(staticNode, p))
			{
				for (uint32_t i = 0; i < staticNode->GetArrayCount(); i++)
					RenderRecursive(p, staticNode->GetAt(i), true);
			}
		}

		// Sort front to back (approx)
		std::sort(GeoList.begin(), GeoList.end(),
		[](GeometryDistEntry& a, GeometryDistEntry& b) -> bool
		{
			return a.DistanceSquared < b.DistanceSquared;
		});

		for (GeometryDistEntry& entry : GeoList)
			ThreadedMOC->SubmitGeometry(entry.Geometry);

		ThreadedMOC->ClearPreWorkNotify();
	}

	void TraverseSceneGraphCallback(MaskedOcclusionCulling *MOC, void *UserData)
	{
		NiCamera *camera = (NiCamera *)UserData;
		TraverseSceneGraph(camera);
	}
}

#endif