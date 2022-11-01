#pragma once

#include <common_lib.h>
#include "synch.h"
#include <list.h>

typedef struct _SEMAPHORE
{

	//MUTEX			MutexLock;
	DWORD           Value;

	SPINLOCK		SpinLock;

	//LOCK			SemaphoreLock;

	//_Guarded_by_(SpinLock)
	LIST_ENTRY		WaitingList;

	// ... add more fields here ...
} SEMAPHORE, * PSEMAPHORE;

void
SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
);


void
SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);


void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);