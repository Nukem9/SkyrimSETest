#define XBYAK_NO_OP_NAMES

#include "../common.h"

#if !SKYRIM64_CREATIONKIT_ONLY

#include <xbyak/xbyak.h>
#include "../typeinfo/ms_rtti.h"
#include "dinput8.h"
#include "TES/TESForm.h"
#include "TES/BSReadWriteLock.h"
#include "TES/BGSDistantTreeBlock.h"
#include "TES/BSGraphics/BSGraphicsRenderer.h"
#include "TES/BSCullingProcess.h"
#include "TES/BSJobs.h"
#include "TES/BSTaskManager.h"
#include "TES/BSShader/BSShaderManager.h"
#include "TES/BSShader/Shaders/BSBloodSplatterShader.h"
#include "TES/BSShader/Shaders/BSDistantTreeShader.h"
#include "TES/BSShader/Shaders/BSSkyShader.h"
#include "TES/BSShader/Shaders/BSGrassShader.h"
#include "TES/BSShader/Shaders/BSParticleShader.h"
#include "TES/MemoryManager.h"
#include "TES/bhkThreadMemorySource.h"
#include "TES/Setting.h"

void PatchAchievements();
void PatchD3D11();
void PatchLogging();
void PatchSettings();
void PatchSteam();
void PatchThreading();
void PatchWindow();
void PatchFileIO();

void PatchBSThread();
void PatchMemory();
void PatchTESForm();
void TestHook5();
size_t BNetConvertUnicodeString(char *Destination, size_t DestSize, const wchar_t *Source, size_t SourceSize);

bool doCullTest = false;

char __fastcall test1(__int64 a1, __int64(__fastcall ***a2)(__int64, __int64, __int64), unsigned int a3, unsigned int a4)
{
	char result; // al

	if (a3)
		result = (**a2)((__int64)a2, a3, a4);
	else
		result = 1;

	MOC::ForceFlush();
	doCullTest = true;
	return result;
}

void test2()
{
	doCullTest = false;
}

void(*test3_orig)();
void test3()
{
	MOC::SendTraverseCommand(nullptr);
	test3_orig();
}

void Patch_TESV()
{
	MSRTTI::Initialize();

	PatchThreading();
	PatchWindow();
#if SKYRIM64_USE_TRACY
	PatchD3D11();
#endif
	PatchSteam();
	PatchAchievements();
	PatchSettings();
	PatchMemory();
	//PatchFileIO();
	PatchTESForm();
	PatchBSThread();
	PatchLogging();

	//
	// BGSDistantTreeBlock
	//
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x4A8360, &BGSDistantTreeBlock::UpdateBlockVisibility);

	//
	// BSCullingProcess
	//
	//Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xD50310), &BSCullingProcess::hk_Process);

	//Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0x12F93B1), &test1, Detours::X64Option::USE_REL32_JUMP);
	//XUtil::PatchMemory(g_ModuleBase + 0x12F93B1, (PBYTE)"\xE8", 1);

	//Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0x12F96C9), &test2, Detours::X64Option::USE_REL32_JUMP);
	//XUtil::PatchMemory(g_ModuleBase + 0x12F96C9, (PBYTE)"\xE8", 1);

	//
	// BSGraphicsRenderer
	//
	Detours::X64::DetourFunctionClass<void (BSGraphics::Renderer::*)(BSGraphics::TriShape *)>(g_ModuleBase + 0x133EC20, &BSGraphics::Renderer::IncRef);
	Detours::X64::DetourFunctionClass<void (BSGraphics::Renderer::*)(BSGraphics::TriShape *)>(g_ModuleBase + 0xD6B9B0, &BSGraphics::Renderer::DecRef);
	Detours::X64::DetourFunctionClass<void (BSGraphics::Renderer::*)(BSGraphics::DynamicTriShape *)>(g_ModuleBase + 0x133ED50, &BSGraphics::Renderer::IncRef);
	Detours::X64::DetourFunctionClass<void (BSGraphics::Renderer::*)(BSGraphics::DynamicTriShape *)>(g_ModuleBase + 0xD6C7D0, &BSGraphics::Renderer::DecRef);

	//
	// BSJobs
	//
	class jobhook : public Xbyak::CodeGenerator
	{
	public:
		jobhook() : Xbyak::CodeGenerator()
		{
			mov(rcx, qword[rax + rdx * 8 + 8]);
			mov(rdx, qword[rax + rdx * 8]);
			mov(r8, (uintptr_t)&BSJobs::DispatchJobCallback);
			call(r8);

			jmp(ptr[rip]);
			dq(g_ModuleBase + 0xC32111);
		}
	} static jobhookInstance;

	Detours::X64::DetourFunction(g_ModuleBase + 0xC32109, (uintptr_t)jobhookInstance.getCode());

	//
	// BSTaskManager
	//
	Detours::X64::DetourClassVTable(MSRTTI::Find("class IOManager")->VTableAddress, &IOManager::QueueTask, 17);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class AddCellGrassTask")->VTableAddress, &BSTask::GetName_AddCellGrassTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class AttachDistant3DTask")->VTableAddress, &BSTask::GetName_AttachDistant3DTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class AudioLoadForPlaybackTask")->VTableAddress, &BSTask::GetName_AudioLoadForPlaybackTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class AudioLoadToCacheTask")->VTableAddress, &BSTask::GetName_AudioLoadToCacheTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class BGSParticleObjectCloneTask")->VTableAddress, &BSTask::GetName_BGSParticleObjectCloneTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class BSScaleformMovieLoadTask")->VTableAddress, &BSTask::GetName_BSScaleformMovieLoadTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class CheckWithinMultiBoundTask")->VTableAddress, &BSTask::GetName_CheckWithinMultiBoundTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class QueuedFile")->VTableAddress, &BSTask::GetName_QueuedFile, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class QueuedPromoteLocationReferencesTask")->VTableAddress, &BSTask::GetName_QueuedPromoteLocationReferencesTask, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class QueuedPromoteReferencesTask")->VTableAddress, &BSTask::GetName_QueuedPromoteReferencesTask, 4);

	//
	// DirectInput (mouse, keyboard)
	//
	Detours::IATHook(g_ModuleBase, "dinput8.dll", "DirectInput8Create", (uintptr_t)hk_DirectInput8Create);

	//
	// MemoryManager
	//
	XUtil::PatchMemory(g_ModuleBase + 0x59B560, (PBYTE)"\xC3", 1);									// [3GB  ] MemoryManager - Default/Static/File heaps
	XUtil::PatchMemory(g_ModuleBase + 0x59B170, (PBYTE)"\xC3", 1);									// [1GB  ] BSSmallBlockAllocator
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x257D740, &bhkThreadMemorySource::__ctor__);	// [512MB] bhkThreadMemorySource
	XUtil::PatchMemory(g_ModuleBase + 0xC02E60, (PBYTE)"\xC3", 1);									// [64MB ] ScrapHeap init
	XUtil::PatchMemory(g_ModuleBase + 0xC037C0, (PBYTE)"\xC3", 1);									// [64MB ] ScrapHeap deinit
																									// [128MB] BSScaleformSysMemMapper is untouched due to complexity

	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC01DA0, &MemoryManager::Allocate);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC020A0, &MemoryManager::Deallocate);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC02FE0, &ScrapHeap::Allocate);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC03600, &ScrapHeap::Deallocate);

	//
	// Locking
	//
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC06DF0, &BSReadWriteLock::__ctor__);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC06E10, &BSReadWriteLock::LockForRead);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC070D0, &BSReadWriteLock::UnlockRead);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC06E90, &BSReadWriteLock::LockForWrite);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC070E0, &BSReadWriteLock::UnlockWrite);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC07080, &BSReadWriteLock::TryLockForWrite);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC07110, &BSReadWriteLock::IsWritingThread);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC06F90, &BSReadWriteLock::LockForReadAndWrite);

	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC07130, &BSAutoReadAndWriteLock::Initialize);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC07180, &BSAutoReadAndWriteLock::Deinitialize);

	//
	// NiRTTI
	//
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0xC66F80, &NiRTTI::__ctor__);

	//
	// Setting
	//
	//Detours::X64::DetourClassVTable(MSRTTI::Find("class INISettingCollection")->VTableAddress, &INISettingCollection::hk_ReadSetting, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class INISettingCollection")->VTableAddress, &INISettingCollection::hk_Open, 5);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class INISettingCollection")->VTableAddress, &INISettingCollection::hk_Close, 6);

	//Detours::X64::DetourClassVTable(MSRTTI::Find("class INIPrefSettingCollection")->VTableAddress, &INIPrefSettingCollection::hk_ReadSetting, 4);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class INIPrefSettingCollection")->VTableAddress, &INIPrefSettingCollection::hk_Open, 5);
	Detours::X64::DetourClassVTable(MSRTTI::Find("class INIPrefSettingCollection")->VTableAddress, &INIPrefSettingCollection::hk_Close, 6);

	//
	// Shaders
	//
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12ACB20, &BSShaderManager::SetRenderMode);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12AD340, &BSShaderManager::SetCurrentAccumulator);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12AD330, &BSShaderManager::GetCurrentAccumulator);

	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12E0DA0, &BSShaderAccumulator::hk_RegisterObjectDispatch);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12E18B0, &BSShaderAccumulator::hk_FinishAccumulatingDispatch);

	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x1336860, &BSShader::BeginTechnique);
	*(uintptr_t *)&BSShader::Load = Detours::X64::DetourFunctionClass(g_ModuleBase + 0x13364A0, &BSShader::hk_Load);

	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12EF750, &BSBloodSplatterShader::__ctor__);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x1318050, &BSDistantTreeShader::__ctor__);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x13113F0, &BSSkyShader::__ctor__);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x12E4C10, &BSGrassShader::__ctor__);
	Detours::X64::DetourFunctionClass(g_ModuleBase + 0x1336C80, &BSParticleShader::__ctor__);
	TestHook5();// BSLightingShader

	//
	// TESForm (Note: The function itself needs to be hooked, not the vtable)
	//
	Detours::X64::DetourFunctionClass(*(uintptr_t *)(MSRTTI::Find("class TESForm")->VTableAddress + 0xB0), &TESForm::hk_GetFullTypeName);
	Detours::X64::DetourFunctionClass(*(uintptr_t *)(MSRTTI::Find("class TESForm")->VTableAddress + 0x190), &TESForm::hk_GetName);
	Detours::X64::DetourFunctionClass(*(uintptr_t *)(MSRTTI::Find("class TESForm")->VTableAddress + 0x198), &TESForm::hk_SetEditorId);

	for (auto ref : MSRTTI::FindAll("class TESObjectREFR"))
	{
		// TESObjectREFR has multiple virtual tables
		if (ref->VFunctionCount == 162)
			Detours::X64::DetourFunctionClass(*(uintptr_t *)(ref->VTableAddress + 0x190), &TESObjectREFR::hk_GetName);
	}

	//
	// Temporary hack to fix array overflow in BSParticleShader::SetupGeometry
	//
	uint32_t test = 0x2000;
	XUtil::PatchMemory(g_ModuleBase + 0x02493CC + 1, (PBYTE)&test, sizeof(uint32_t));
	XUtil::PatchMemory(g_ModuleBase + 0x0249374 + 1, (PBYTE)&test, sizeof(uint32_t));

	test = 100;
	XUtil::PatchMemory(g_ModuleBase + 0x02494A8 + 2, (PBYTE)&test, sizeof(uint32_t));

	//
	// Misc
	//

	// Broken printf statement that triggers invalid_parameter_handler(), "%08" should really be "%08X"
	const char *newFormat = "World object count changed on object '%s' %08X from %i to %i";

	XUtil::PatchMemory(g_ModuleBase + 0x1696030, (PBYTE)newFormat, strlen(newFormat) + 1);

	//
	// Fix crash when Unicode string conversion fails with bethesda.net http responses
	//
	XUtil::DetourJump(g_ModuleBase + 0x1154B60, &BNetConvertUnicodeString);

	//
	// Bug fix for when TAA/FXAA/DOF are disabled and quicksave doesn't work without
	// opening a menu
	//
	//XUtil::PatchMemory(g_ModuleBase + 0x12AE2DD, (PBYTE)"\x90\x90\x90\x90\x90\x90", 6);	// Kill jnz
	//XUtil::PatchMemory(g_ModuleBase + 0x12AE2E8, (PBYTE)"\x80\xBD\x11\x02\x00\x00\x00", 7);// Rewrite to 'cmp byte ptr [rbp+211h], 0'
	//XUtil::PatchMemory(g_ModuleBase + 0x12AE2EF, (PBYTE)"\x90\x90\x90\x90", 4);			// Extra rewrite padding

	//
	// Memory bug fix during BSShadowDirectionalLight calculations:
	//
	// void *data1 = ScrapHeap::Alloc(32, 8);
	// MessWithData(v40, 8u, v41, &v217, data1);
	// sub_14133E730(..., data1, ...); <- OK
	// ScrapHeap::Free(data1);
	// ....
	// void *data2 = ScrapHeap::Alloc(32, 8);
	// MessWithData(v40, 8u, v41, &v217, data2);
	// sub_14133E730(..., data1, ...); <- USE-AFTER-FREE!! data1 SHOULD BE data2
	// ScrapHeap::Free(data2);
	//
	XUtil::PatchMemory(g_ModuleBase + 0x133D94D, (PBYTE)"\x4D\x8B\xCF\x90\x90\x90\x90", 7);

	*(uintptr_t *)&TESObjectCell::CreateRootMultiBoundNode = Detours::X64::DetourFunctionClass(g_ModuleBase + 0x264230, &TESObjectCell::hk_CreateRootMultiBoundNode);

	//*(PBYTE *)&test3_orig = Detours::X64::DetourFunctionClass((uint8_t *)(g_ModuleBase + 0x5B7AD0), &test3);
}

#endif