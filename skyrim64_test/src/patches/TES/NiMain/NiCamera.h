#pragma once

#include <DirectXMath.h>
#include "NiAVObject.h"

struct NiFrustum
{
	float m_fLeft;
	float m_fRight;
	float m_fTop;
	float m_fBottom;
	float m_fNear;
	float m_fFar;
	bool m_bOrtho;
};
static_assert_offset(NiFrustum, m_fLeft, 0x0);
static_assert_offset(NiFrustum, m_fRight, 0x4);
static_assert_offset(NiFrustum, m_fTop, 0x8);
static_assert_offset(NiFrustum, m_fBottom, 0xC);
static_assert_offset(NiFrustum, m_fNear, 0x10);
static_assert_offset(NiFrustum, m_fFar, 0x14);
static_assert_offset(NiFrustum, m_bOrtho, 0x18);

class NiCamera : public NiAVObject
{
public:
	float m_aafWorldToCam[4][4];
	NiFrustum m_kViewFrustum;
	char _pad0[0x1C];

	inline const NiPoint3& NiCamera::GetWorldLocation() const
	{
		return m_kWorld.m_Translate;
	}

	inline NiPoint3 NiCamera::GetWorldDirection() const
	{
		return m_kWorld.m_Rotate.GetCol<0>();
	}

	inline NiPoint3 NiCamera::GetWorldUpVector() const
	{
		return m_kWorld.m_Rotate.GetCol<1>();
	}

	inline NiPoint3 NiCamera::GetWorldRightVector() const
	{
		return m_kWorld.m_Rotate.GetCol<2>();
	}

	void CalculateViewProjection(DirectX::XMMATRIX& ViewProj) const
	{
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;

		CalculateViewProjection(view, projection, ViewProj);
	}

	void CalculateViewProjection(DirectX::XMMATRIX& View, DirectX::XMMATRIX& Proj, DirectX::XMMATRIX& ViewProj) const
	{
		// Ported directly from game code
		NiPoint3 dir = GetWorldDirection();
		NiPoint3 up = GetWorldUpVector();
		NiPoint3 right = GetWorldRightVector();

		View.r[0] = { right.x, up.x, dir.x, 0.0f };
		View.r[1] = { right.y, up.y, dir.y, 0.0f };
		View.r[2] = { right.z, up.z, dir.z, 0.0f };
		View.r[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

		float rightLeftDiff = m_kViewFrustum.m_fRight - m_kViewFrustum.m_fLeft;
		float rightLeftSum = m_kViewFrustum.m_fRight + m_kViewFrustum.m_fLeft;
		float rightLeftRatio = -((1.0f / rightLeftDiff) * rightLeftSum);

		float topBottomDiff = m_kViewFrustum.m_fTop - m_kViewFrustum.m_fBottom;
		float topBottomSum = m_kViewFrustum.m_fTop + m_kViewFrustum.m_fBottom;
		float topBottomRatio = -((1.0f / topBottomDiff) * topBottomSum);

		float invNearFarDiff = 1.0f / (m_kViewFrustum.m_fFar - m_kViewFrustum.m_fNear);

		Proj.r[0] = _mm_setzero_ps();
		Proj.r[1] = _mm_setzero_ps();
		Proj.r[2] = _mm_setzero_ps();
		Proj.r[3] = _mm_setzero_ps();

		Proj.r[0].m128_f32[0] = (1.0f / rightLeftDiff) * 2.0f;
		Proj.r[1].m128_f32[1] = (1.0f / topBottomDiff) * 2.0f;

		if (!m_kViewFrustum.m_bOrtho)
		{
			Proj.r[2].m128_f32[0] = rightLeftRatio;
			Proj.r[2].m128_f32[1] = topBottomRatio;
			Proj.r[2].m128_f32[2] = invNearFarDiff * m_kViewFrustum.m_fFar;
			Proj.r[2].m128_f32[3] = 1.0f;

			float ratio = m_kViewFrustum.m_fNear * m_kViewFrustum.m_fFar;
			Proj.r[3].m128_f32[2] = -(ratio * invNearFarDiff);
		}
		else
		{
			Proj.r[2].m128_f32[2] = invNearFarDiff;

			Proj.r[3].m128_f32[0] = rightLeftRatio;
			Proj.r[3].m128_f32[1] = topBottomRatio;
			Proj.r[3].m128_f32[2] = -(invNearFarDiff * m_kViewFrustum.m_fNear);
			Proj.r[3].m128_f32[3] = 1.0f;
		}

		ViewProj = XMMatrixMultiply(View, Proj);
	}

	void GetViewerStrings(void(*Callback)(const char *, ...), bool Recursive) const
	{
		if (Recursive)
			__super::GetViewerStrings(Callback, Recursive);

		Callback("-- NiCamera --\n");
		Callback("World Dir = (%g, %g, %g)\n",
			GetWorldDirection().x,
			GetWorldDirection().y,
			GetWorldDirection().z);
		Callback("World Up = (%g, %g, %g)\n",
			GetWorldUpVector().x,
			GetWorldUpVector().y,
			GetWorldUpVector().z);
		Callback("World Right = (%g, %g, %g)\n",
			GetWorldRightVector().x,
			GetWorldRightVector().y,
			GetWorldRightVector().z);
	}
};
static_assert(sizeof(NiCamera) == 0x188);
static_assert_offset(NiCamera, m_aafWorldToCam, 0x110);
static_assert_offset(NiCamera, m_kViewFrustum, 0x150);