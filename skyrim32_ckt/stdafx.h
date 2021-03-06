#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Objbase.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "resource.h"
#include "MemoryModule.h"
#include "stb.h"

void *__fastcall MemoryManager_Alloc(void *Thisptr, void *_EDX, uint32_t Size, uint32_t Alignment, bool Aligned);
void __fastcall MemoryManager_Free(void *Thisptr, void *_EDX, void *Ptr, bool Aligned);
void *ScrapHeap_Alloc(uint32_t Size);
void ScrapHeap_Free(void *Ptr);

void PatchMemory(ULONG_PTR Address, PBYTE Data, SIZE_T Size);
void PatchJump(uintptr_t Address, uintptr_t Target);