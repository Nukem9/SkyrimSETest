#pragma once

#include "..\..\..\config.h"

#include <Windows.h>

class __declspec(align(8)) NiRefObject
{
protected:
	volatile LONGLONG m_uiRefCount;

public:
	NiRefObject() : m_uiRefCount(0)
	{
		InterlockedIncrement64(&m_uiRefCount);
	}

	virtual ~NiRefObject()
	{
		InterlockedDecrement64(&m_uiRefCount);
	}

	virtual void DeleteThis()
	{
		if (this)
			this->~NiRefObject();
	}

	uint64_t IncRefCount()
	{
		return (uint64_t)InterlockedIncrement64(&m_uiRefCount);
	}

	uint64_t DecRefCount()
	{
		uint64_t count = (uint64_t)InterlockedDecrement64(&m_uiRefCount);

		if (count <= 0)
			DeleteThis();

		return count;
	}

	void GetViewerStrings(void(*Callback)(const char*, ...), bool Recursive) const
	{
		Callback("-- NiRefObject --\n");
		Callback("This = 0x%p\n", this);
		Callback("Ref Count = %u\n", m_uiRefCount);
	}
};
static_assert(sizeof(NiRefObject) == 0x10);