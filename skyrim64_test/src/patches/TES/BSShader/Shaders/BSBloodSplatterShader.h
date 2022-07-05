#pragma once

#include "../BSShader.h"

class __declspec(align(8)) BSBloodSplatterShader : public BSShader
{
private:
	enum Techniques
	{
		RAW_TECHNIQUE_SPLATTER = 0,
		RAW_TECHNIQUE_FLARE = 1,
	};

	struct TexSlot
	{
		enum
		{
			BloodColor = 0,
			BloodAlpha = 1,
			FlareColor = 2,
			FlareHDR = 3,
		};
	};

	// Is either 0 or 1 [enum], set in SetupTechnique(), used in SetupGeometry()
	uint32_t m_CurrentTechniqueID;

	inline AutoPtr(NiColorA, LightLoc, 0x32573D0);
	inline AutoPtr(int, iAdaptedLightRenderTarget, 0x32573C8);
	inline AutoPtr(float, fGlobalAlpha, 0x1E333C4);
	inline AutoPtr(float, fFlareMult, 0x1E333C0);
	inline AutoPtr(float, fAlpha, 0x32573C0);
	inline AutoPtr(float, fFlareOffsetScale, 0x32573C4);

	const static uintptr_t OriginalVTableBase = 0x1879C98;

public:
	inline AutoPtr(BSBloodSplatterShader *, pInstance, 0x32573B8);

	DECLARE_CONSTRUCTOR_HOOK(BSBloodSplatterShader);

	BSBloodSplatterShader();
	virtual ~BSBloodSplatterShader();
	virtual bool SetupTechnique(uint32_t Technique) override;						// Implemented
	virtual void RestoreTechnique(uint32_t Technique) override;						// Implemented
	virtual void SetupGeometry(BSRenderPass *Pass, uint32_t RenderFlags) override;	// Implemented
	virtual void RestoreGeometry(BSRenderPass *Pass, uint32_t RenderFlags) override;// Implemented

	void CreateAllShaders();
	void CreateVertexShader(uint32_t Technique);
	void CreatePixelShader(uint32_t Technique);

	static uint32_t GetRawTechnique(uint32_t Technique);
	static uint32_t GetVertexTechnique(uint32_t RawTechnique);
	static uint32_t GetPixelTechnique(uint32_t RawTechnique);

	static std::vector<std::pair<const char *, const char *>> GetSourceDefines(uint32_t Technique);
	static std::string GetTechniqueString(uint32_t Technique);
};
static_assert(sizeof(BSBloodSplatterShader) == 0x98);
//static_assert_offset(BSBloodSplatterShader, m_CurrentTechniqueID, 0x90);