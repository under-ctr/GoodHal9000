#pragma once
#include "thread_internal.h"

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
