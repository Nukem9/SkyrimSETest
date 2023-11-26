#pragma once

#include <map>
#include <stdint.h>
#include <functional>
#include "../NiMain/common.h"
#include "../BSTScatterTable.h"
#include "../BSGraphics/BSGraphicsRenderer.h"
#include "BSShaderProperty.h"
#include "BSShaderMaterial.h"

class BSRenderPass;
class BSIStream;

#define BSSHADER_FORWARD_DEBUG 0

#define BSSHADER_FORWARD_CALL_ALWAYS(OptionIndex, Func, ...) \
{ \
	static uint32_t vtableIndex = XUtil::VtableIndexer::GetIndexOf(Func); \
	auto realFunc = Func; \
	*(uintptr_t *)&realFunc = *(uintptr_t*)(g_ModuleBase + OriginalVTableBase + (8 * vtableIndex)); \
	return (this->*realFunc)(__VA_ARGS__); \
}

#define BSSHADER_FORWARD_CALL(OptionIndex, Func, ...) \
if (g_ShaderToggles[m_Type][BSGraphics::CONSTANT_GROUP_LEVEL_##OptionIndex]) { BSSHADER_FORWARD_CALL_ALWAYS(OptionIndex, Func, __VA_ARGS__) }

#define DEFINE_SHADER_DESCRIPTOR(Type, Entries) static const ShaderDescriptor ShaderConfig##Type(#Type, { Entries })
#define CONFIG_ENTRY(a, b, c, d, e) { ShaderDescriptor::##a, ShaderDescriptor::##b, c, #d, #e },

class ShaderDescriptor
{
public:
	enum ShaderType
	{
		INVALID_SHADER_TYPE,
		PS,					// Pixel shader
		VS,					// Vertex shader
		CS,					// Compute shader
	};

	enum DeclType
	{
		INVALID_DECL_TYPE,
		PER_TEC,			// Constants
		PER_MAT,			// Constants
		PER_GEO,			// Constants
		SAMPLER,			// Samplers
		TEXTURE,			// Textures
	};

	struct Entry
	{
		ShaderType m_ShaderType;
		DeclType m_DeclType;
		int Index;
		const char *DataType;
		const char *Name;
	};

	const char *const Type;
	std::map<int, const Entry *> ByConstantIndexVS;
	std::map<int, const Entry *> ByConstantIndexPS;
	std::map<int, const Entry *> ByConstantIndexCS;
	std::map<int, const Entry *> BySamplerIndex;
	std::map<int, const Entry *> ByTextureIndex;

private:
	std::vector<Entry> m_Entries;

public:
	ShaderDescriptor(const char *FXType, std::initializer_list<Entry> InputData) : Type(FXType), m_Entries(InputData)
	{
		for (Entry& e : m_Entries)
		{
			Assert(e.m_ShaderType != INVALID_SHADER_TYPE);
			Assert(e.m_DeclType != INVALID_DECL_TYPE);

			if (e.m_DeclType == PER_TEC || e.m_DeclType == PER_MAT || e.m_DeclType == PER_GEO)
			{
				switch (e.m_ShaderType)
				{
				case VS: ByConstantIndexVS[e.Index] = &e; break;
				case PS: ByConstantIndexPS[e.Index] = &e; break;
				case CS: ByConstantIndexCS[e.Index] = &e; break;
				default: Assert(false); break;
				}
			}
			else if (e.m_DeclType == SAMPLER)
				BySamplerIndex[e.Index] = &e;
			else if (e.m_DeclType == TEXTURE)
				ByTextureIndex[e.Index] = &e;
		}

		// m_Entries should never be modified after this point
	}

	const std::vector<Entry>& AllEntries() const
	{
		return m_Entries;
	}
};

class __declspec(align(8)) NiBoneMatrixSetterI
{
public:
	virtual ~NiBoneMatrixSetterI()
	{
	}

	virtual void SetBoneMatrix(NiSkinInstance *SkinInstance, NiSkinPartition::Partition *Partition, const NiTransform *Transform) = 0;
};
static_assert(sizeof(NiBoneMatrixSetterI) == 0x8);

class __declspec(align(8)) BSReloadShaderI
{
public:
	virtual void ReloadShaders(BSIStream *Stream) = 0;
};

class __declspec(align(8)) BSShader : public NiRefObject, public NiBoneMatrixSetterI, public BSReloadShaderI
{
private:
	template<typename T>
	class TechniqueIDStorage
	{
	public:
		T m_Value;

		uint32_t GetKey()
		{
			return m_Value->m_TechniqueID;
		}
	};

	template<typename T, typename Storage = TechniqueIDStorage<T>>
	class TechniqueIDMap : public BSTScatterTable<
		uint32_t,
		T,
		Storage,
		BSTScatterTableDefaultHashPolicy<uint32_t>,
		BSTScatterTableHeapAllocator<BSTScatterTableEntry<uint32_t, T, Storage>>>
	{
	};

public:
	static bool g_ShaderToggles[16][3];
	static const ShaderDescriptor *ShaderMetadata[];

	BSShader(const char *LoaderType);
	virtual ~BSShader();

	virtual bool SetupTechnique(uint32_t Technique) = 0;
	virtual void RestoreTechnique(uint32_t Technique) = 0;
	virtual void SetupMaterial(BSShaderMaterial const *Material);
	virtual void RestoreMaterial(BSShaderMaterial const *Material);
	virtual void SetupGeometry(BSRenderPass *Pass, uint32_t Flags) = 0;
	virtual void RestoreGeometry(BSRenderPass *Pass, uint32_t RenderFlags) = 0;
	virtual void GetTechniqueName(uint32_t Technique, char *Buffer, uint32_t BufferSize);
	virtual void ReloadShaders(bool Unknown);

	virtual void ReloadShaders(BSIStream *Stream) override;
	virtual void SetBoneMatrix(NiSkinInstance *SkinInstance, NiSkinPartition::Partition *Partition, const NiTransform *Transform) override;

	// Both of these functions are virtual, but removed from SkyrimSE.exe itself
	void CreateVertexShader(
		uint32_t Technique,
		const char *SourceFile,
		const std::vector<std::pair<const char *, const char *>>& Defines,
		std::function<const char *(int Index)> GetConstant);

	void CreatePixelShader(
		uint32_t Technique,
		const char *SourceFile,
		const std::vector<std::pair<const char *, const char *>>& Defines,
		std::function<const char *(int Index)> GetSampler,
		std::function<const char *(int Index)> GetConstant);

	void CreateHullShader(uint32_t Technique, const char *SourceFile, const std::vector<std::pair<const char *, const char *>>& Defines);
	void CreateDomainShader(uint32_t Technique, const char *SourceFile, const std::vector<std::pair<const char *, const char *>>& Defines);

	void hk_Load(BSIStream *Stream);

	bool BeginTechnique(uint32_t VertexShaderID, uint32_t PixelShaderID, bool IgnorePixelShader);
	void EndTechnique();

	void SetupGeometryAlphaBlending(const NiAlphaProperty *AlphaProperty, BSShaderProperty *ShaderProperty, bool a4);
	void SetupAlphaTestRef(const NiAlphaProperty *AlphaProperty, BSShaderProperty *ShaderProperty);

	static std::vector<std::pair<const char *, const char *>> GetAnySourceDefines(uint32_t Type, uint32_t Technique);
	static std::string GetAnyTechniqueName(uint32_t Type, uint32_t Technique);
	static const char *GetVariableType(uint32_t Type, const char *Name);
	static ShaderDescriptor::DeclType GetVariableCategory(uint32_t Type, const char *Name);
	static const char *GetVSConstantName(uint32_t Type, uint32_t Index);
	static const char *GetPSConstantName(uint32_t Type, uint32_t Index);
	static const char *GetPSSamplerName(uint32_t Type, uint32_t Index, uint32_t TechniqueID);

	uint32_t m_Type;
	TechniqueIDMap<BSGraphics::VertexShader *> m_VertexShaderTable;
	TechniqueIDMap<BSGraphics::PixelShader *> m_PixelShaderTable;
	const char *m_LoaderType;

	inline static decltype(&hk_Load) Load;
};
static_assert(sizeof(BSShader) == 0x90);
static_assert_offset(BSShader, m_Type, 0x20);
static_assert_offset(BSShader, m_VertexShaderTable, 0x28);
static_assert_offset(BSShader, m_PixelShaderTable, 0x58);
static_assert_offset(BSShader, m_LoaderType, 0x88);

STATIC_CONSTRUCTOR(CheckBSShaderVtable, []
{
	//assert_vtable_index(&BSShader::~BSShader, 0);
	assert_vtable_index(&BSShader::DeleteThis, 1);
	assert_vtable_index(&BSShader::SetupTechnique, 2);
	assert_vtable_index(&BSShader::RestoreTechnique, 3);
	assert_vtable_index(&BSShader::SetupMaterial, 4);
	assert_vtable_index(&BSShader::RestoreMaterial, 5);
	assert_vtable_index(&BSShader::SetupGeometry, 6);
	assert_vtable_index(&BSShader::RestoreGeometry, 7);
	assert_vtable_index(&BSShader::GetTechniqueName, 8);
	assert_vtable_index(static_cast<void(BSShader::*)(bool)>(&BSShader::ReloadShaders), 9);
});