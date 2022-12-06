#pragma once

typedef DWORD SYSCALL_IF_VERSION;

#define SYSCALL_IMPLEMENTED_IF_VERSION      0x1

#define UM_INVALID_HANDLE_VALUE             0

#define UM_FILE_HANDLE_STDOUT               (UM_HANDLE)0x1

typedef QWORD UM_HANDLE;

typedef struct _UM_HANDLE_SYSTEM_DATA
{
	LOCK                AllUmHandlersLock;

	_Guarded_by_(AllUmHandlersLock)
		LIST_ENTRY          AllUmHandlersList;

	
} UM_HANDLE_SYSTEM_DATA, * PUM_HANDLE_SYSTEM_DATA;

#include "mem_structures.h"
#include "thread_defs.h"
#include "process_defs.h"
