#include "../../common.h"
#include "BSReadWriteLock.h"

BSReadWriteLock *BSReadWriteLock::Initialize()
{
	m_Bits = 0;
	m_ThreadId = 0;
	m_WriteCount = 0;
	return this;
}

void BSReadWriteLock::AcquireRead()
{
	ProfileTimer("Read Lock Time");

	for (uint32_t count = 0; !TryAcquireRead();)
	{
		if (++count > 1000)
			YieldProcessor();
	}
}

void BSReadWriteLock::ReleaseRead()
{
	if (IsWriteOwner())
		return;

	m_Bits.fetch_add(-READER, std::memory_order_release);
}

bool BSReadWriteLock::TryAcquireRead()
{
	if (IsWriteOwner())
		return true;

	// fetch_add is considerably (100%) faster than compare_exchange,
	// so here we are optimizing for the common (lock success) case.
	int32_t value = m_Bits.fetch_add(READER, std::memory_order_acquire);

	if (value & WRITER)
	{
		m_Bits.fetch_add(-READER, std::memory_order_release);
		return false;
	}

	return true;
}

void BSReadWriteLock::AcquireWrite()
{
	ProfileTimer("Write Lock Time");

	for (uint32_t count = 0; !TryAcquireWrite();)
	{
		if (++count > 1000)
			YieldProcessor();
	}
}

void BSReadWriteLock::ReleaseWrite()
{
	if (--m_WriteCount > 0)
		return;

	m_ThreadId.store(0, std::memory_order_release);
	m_Bits.fetch_and(~WRITER, std::memory_order_release);
}

bool BSReadWriteLock::TryAcquireWrite()
{
	if (IsWriteOwner())
	{
		m_WriteCount++;
		return true;
	}

	int32_t expect = 0;
	if (m_Bits.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel))
	{
		m_WriteCount = 1;
		m_ThreadId.store((uint16_t)GetCurrentThreadId(), std::memory_order_release);
		return true;
	}

	return false;
}

bool BSReadWriteLock::IsWriteOwner()
{
	return m_ThreadId == (uint16_t)GetCurrentThreadId();
}

void BSReadWriteLock::UpgradeRead()
{
}

BSAutoReadAndWriteLock *BSAutoReadAndWriteLock::Initialize(BSReadWriteLock *Child)
{
	m_Lock = Child;
	m_Lock->AcquireWrite();

	return this;
}

void BSAutoReadAndWriteLock::Deinitialize()
{
	m_Lock->ReleaseWrite();
}

void PatchLocks()
{
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC06DF0), &BSReadWriteLock::Initialize);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC06E10), &BSReadWriteLock::AcquireRead);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC070D0), &BSReadWriteLock::ReleaseRead);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC06E90), &BSReadWriteLock::AcquireWrite);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC070E0), &BSReadWriteLock::ReleaseWrite);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC07080), &BSReadWriteLock::TryAcquireWrite);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC07110), &BSReadWriteLock::IsWriteOwner);

	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC07130), &BSAutoReadAndWriteLock::Initialize);
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC07180), &BSAutoReadAndWriteLock::Deinitialize);

	//Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xBE9010), &BSSpinLock::AcquireWrite);	// EnterUpgradeableReaderLock -- check parent function
	Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xC06F90), &BSReadWriteLock::UpgradeRead);		// UpgdateToWriteLock -- this is a no-op
	//Detours::X64::DetourFunctionClass((PBYTE)(g_ModuleBase + 0xBE9270), &BSSpinLock::ReleaseWrite);	// ExitUpgradeableReaderLock -- name might be wrong

	// C05B30 - Enter any lock mode, returns true if entered write, false if read (no sleep time!), but it's unused by game.
}