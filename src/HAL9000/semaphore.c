#include "semaphore.h"
#include "HAL9000.h"
#include "thread_internal.h"


void
SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
)
{
	ASSERT(NULL != Semaphore);


	//MutexInit(&Semaphore->MutexLock, FALSE);
	SpinlockInit(&Semaphore->SpinLock);
	Semaphore->Value = InitialValue;
	InitializeListHead(&Semaphore->WaitingList);

}


void
SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
)
{	
	INTR_STATE oldState;
	INTR_STATE dummyState;
	

	ASSERT(NULL != Semaphore);
	oldState = CpuIntrGetState();

	if (Semaphore->Value >= Value) {
		SpinlockAcquire(&Semaphore->SpinLock, &dummyState);
		Semaphore->Value -= Value;
		SpinlockRelease(&Semaphore->SpinLock, dummyState);
	}
	else {
		PTHREAD pCurrentThread = GetCurrentThread();
		InsertTailList(&Semaphore->WaitingList, &pCurrentThread->WaiterList);
		ThreadTakeBlockLock();
		SpinlockAcquire(&Semaphore->SpinLock, dummyState);
		ThreadBlock();
		SpinlockAcquire(&Semaphore->SpinLock, &dummyState);
	
	}

	

	
}


void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
)
{
	INTR_STATE oldState;
	PTHREAD pThread;

	SpinlockAcquire(&Semaphore->SpinLock, &oldState);
	Semaphore->Value += Value;
	SpinlockRelease(&Semaphore->SpinLock, oldState);

	for (int i = 0; i < Value; i++) {
		SpinlockAcquire(&Semaphore->SpinLock, &oldState);
		PLIST_ENTRY entryThread = RemoveHeadList(&Semaphore->WaitingList);
		if (entryThread != NULL)
			pThread = CONTAINING_RECORD(entryThread, THREAD, WaiterList);
			if(pThread->State == ThreadStateBlocked)
				ThreadUnblock(pThread);
	}
	SpinlockRelease(&Semaphore->SpinLock, oldState);
}