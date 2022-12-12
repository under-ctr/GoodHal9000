#pragma once
#include "thread_internal.h"
typedef struct _UM_HANDLE_STRUCT
{
	
    PTHREAD*               Thred;

	QWORD                   Id;

    LIST_ENTRY              AllList;
	
	

} UM_HANDLE_STRUCT, *PUM_HANDLE_STRUCT;

QWORD UM_Handler_Value;

void
_No_competing_thread_
SyscallSystemPreinit(
	void
);

void
SyscallPreinitSystem(
    void
    );

STATUS
SyscallInitSystem(
    void
    );

STATUS
SyscallUninitSystem(
    void
    );

void
SyscallCpuInit(
    void
    );
