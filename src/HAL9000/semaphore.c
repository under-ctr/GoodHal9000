#include "semaphore.h"


void
SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
)
{
	ASSERT(NULL != Semaphore);

	memzero(Semaphore, sizeof(Semaphore));

	MutexInit(&Semaphore->SemaphoreLock);
	InitializeListHead(&Semaphore->WaitingList);

}


void
SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
)
{

}


void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
)
{

}