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

	memzero(Semaphore, sizeof(Semaphore));

	LockInit(&Semaphore->SemaphoreLock);
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
	oldState = CpuIntrDisable();

	if (Semaphore->Value >= Value) {
		LockAcquire(&Semaphore->SemaphoreLock, &dummyState);
		Semaphore->Value -= Value;
		LockRelease(&Semaphore->SemaphoreLock, dummyState);
	}
	else {
		PTHREAD pCurrentThread = GetCurrentThread();
		InsertTailList(&Semaphore->WaitingList, &pCurrentThread->ReadyList);
		ThreadTakeBlockLock();
		LockRelease(&Semaphore->SemaphoreLock, dummyState);
		ThreadBlock();
		LockAcquire(&Semaphore->SemaphoreLock, &dummyState);
	
	}

	LockRelease(&Semaphore->SemaphoreLock, dummyState);

	CpuIntrSetState(oldState);
}


void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
)
{
	INTR_STATE oldState;
	PTHREAD pThread;

	LockAcquire(&Semaphore->SemaphoreLock, &oldState);
	Semaphore->Value += Value;
	LockRelease(&Semaphore->SemaphoreLock, oldState);

	for (int i = 0; i < Value; i++) {
		pThread = RemoveHeadList(&Semaphore->WaitingList);
		if(pThread != NULL)
			ThreadUnblock(pThread);
	}

}