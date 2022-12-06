#pragma once

typedef struct _UM_HANDLE_STRUCT
{
	
	struct _THREAD*         Thred;

	QWORD                    Id;
	
	struct _PROCESS*        Process;

} UM_HANDLE_STRUCT, * PUM_HANDLE_STRUCT;

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
