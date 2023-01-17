#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread.h"

extern void SyscallEntry();

#define SYSCALL_IF_VERSION_KM       SYSCALL_IMPLEMENTED_IF_VERSION

typedef struct _UM_HANDLE_SYSTEM_DATA
{
	LOCK                AllUmHandlersLock;

	_Guarded_by_(AllUmHandlersLock)
		LIST_ENTRY          AllUmHandlersList;

  


} UM_HANDLE_SYSTEM_DATA, *PUM_HANDLE_SYSTEM_DATA;


static UM_HANDLE_SYSTEM_DATA m_umHandlerSystemData;

typedef struct _UM_HANDLE_STRUCT
{

    PTHREAD Thred; // acolesa - aici e doar pointer la PTHREAD

    UM_HANDLE              Id; // acolesa - desi si UM_HANDLE a un QWORD, e mai sugestiv ca nume timpul UM_HANDLE

    LIST_ENTRY              AllList;

} UM_HANDLE_STRUCT, * PUM_HANDLE_STRUCT;

QWORD UM_Handler_Value;


void
SyscallHandler(
    INOUT   COMPLETE_PROCESSOR_STATE    *CompleteProcessorState
    )
{
    SYSCALL_ID sysCallId;
    PQWORD pSyscallParameters;
    PQWORD pParameters;
    STATUS status;
    REGISTER_AREA* usermodeProcessorState;

    ASSERT(CompleteProcessorState != NULL);

    // It is NOT ok to setup the FMASK so that interrupts will be enabled when the system call occurs
    // The issue is that we'll have a user-mode stack and we wouldn't want to receive an interrupt on
    // that stack. This is why we only enable interrupts here.
    ASSERT(CpuIntrGetState() == INTR_OFF);
    CpuIntrSetState(INTR_ON);

    LOG_TRACE_USERMODE("The syscall handler has been called!\n");

    status = STATUS_SUCCESS;
    pSyscallParameters = NULL;
    pParameters = NULL;
    usermodeProcessorState = &CompleteProcessorState->RegisterArea;

    __try
    {
        if (LogIsComponentTraced(LogComponentUserMode))
        {
            DumpProcessorState(CompleteProcessorState);
        }

        // Check if indeed the shadow stack is valid (the shadow stack is mandatory)
        pParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp];
        status = MmuIsBufferValid(pParameters, SHADOW_STACK_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            LOG_FUNC_ERROR("MmuIsBufferValid", status);
            __leave;
        }

        sysCallId = usermodeProcessorState->RegisterValues[RegisterR8];

        LOG_TRACE_USERMODE("System call ID is %u\n", sysCallId);

        // The first parameter is the system call ID, we don't care about it => +1
        pSyscallParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp] + 1;

        // Dispatch syscalls
        switch (sysCallId)
        {
        case SyscallIdIdentifyVersion:
            status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
            break;
        case SyscallIdFileWrite:
            status = SyscallFileWrite((UM_HANDLE)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (QWORD)pSyscallParameters[2], (QWORD*)pSyscallParameters[3]);
            break;
        case SyscallIdThreadExit:
            status = SyscallThreadExit((STATUS)pSyscallParameters[0]);
            break;
        case SyscallIdThreadCreate:
            status = SyscallThreadCreate((PFUNC_ThreadStart)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (UM_HANDLE*)pSyscallParameters[2]);
            break;
		case SyscallIdThreadGetTid:
			status = SyscallThreadGetTid((UM_HANDLE)pSyscallParameters[0], (TID*)pSyscallParameters[1]);
			break;
        
		case SyscallIdThreadWaitForTermination:
			status = SyscallThreadWaitForTermination((UM_HANDLE)pSyscallParameters[0], (STATUS*)pSyscallParameters[1]);
			break;
		case SyscallIdThreadCloseHandle:
			status = SyscallThreadCloseHandle((UM_HANDLE)pSyscallParameters[0]);
			break;

		case SyscallIdProcessExit:
				//SyscallProcessExit((STATUS)pSyscallParameters[0]);
				break;

		case SyscallIdProcessCreate:
				//status = SyscallProcessCreate((char*)pSyscallParameters[0], pSyscallParameters[1], (char*)pSyscallParameters[2], pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
				break;
		case SyscallIdProcessGetPid:
				//status = SyscallProcessGetPid((UM_HANDLE)pSyscallParameters[0], (PID*)pSyscallParameters[1]);
				break;
		case SyscallIdProcessWaitForTermination:
				//status = SyscallProcessWaitForTermination(pSyscallParameters[0], (STATUS*)pSyscallParameters[1]);
				break;
		case SyscallIdProcessCloseHandle:
				//status = SyscallProcessCloseHandle(pSyscallParameters[0]);
				break;	
        default:
            LOG_ERROR("Unimplemented syscall called from User-space!\n");
            status = STATUS_UNSUPPORTED;
            break;

        }

    }
    __finally
    {
        LOG_TRACE_USERMODE("Will set UM RAX to 0x%x\n", status);

        usermodeProcessorState->RegisterValues[RegisterRax] = status;

        CpuIntrSetState(INTR_OFF);
    }
}

void
SyscallPreinitSystem(
    void
    )
{

}

STATUS
SyscallInitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

STATUS
SyscallUninitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

void
SyscallCpuInit(
    void
    )
{
    IA32_STAR_MSR_DATA starMsr;
    WORD kmCsSelector;
    WORD umCsSelector;

    memzero(&starMsr, sizeof(IA32_STAR_MSR_DATA));

    kmCsSelector = GdtMuGetCS64Supervisor();
    ASSERT(kmCsSelector + 0x8 == GdtMuGetDS64Supervisor());

    umCsSelector = GdtMuGetCS32Usermode();
    /// DS64 is the same as DS32
    ASSERT(umCsSelector + 0x8 == GdtMuGetDS32Usermode());
    ASSERT(umCsSelector + 0x10 == GdtMuGetCS64Usermode());

    // Syscall RIP <- IA32_LSTAR
    __writemsr(IA32_LSTAR, (QWORD) SyscallEntry);

    LOG_TRACE_USERMODE("Successfully set LSTAR to 0x%X\n", (QWORD) SyscallEntry);

    // Syscall RFLAGS <- RFLAGS & ~(IA32_FMASK)
    __writemsr(IA32_FMASK, RFLAGS_INTERRUPT_FLAG_BIT);

    LOG_TRACE_USERMODE("Successfully set FMASK to 0x%X\n", RFLAGS_INTERRUPT_FLAG_BIT);

    // Syscall CS.Sel <- IA32_STAR[47:32] & 0xFFFC
    // Syscall DS.Sel <- (IA32_STAR[47:32] + 0x8) & 0xFFFC
    starMsr.SyscallCsDs = kmCsSelector;

    // Sysret CS.Sel <- (IA32_STAR[63:48] + 0x10) & 0xFFFC
    // Sysret DS.Sel <- (IA32_STAR[63:48] + 0x8) & 0xFFFC
    starMsr.SysretCsDs = umCsSelector;

    __writemsr(IA32_STAR, starMsr.Raw);

    LOG_TRACE_USERMODE("Successfully set STAR to 0x%X\n", starMsr.Raw);
}

// SyscallIdIdentifyVersion
STATUS
SyscallValidateInterface(
    IN  SYSCALL_IF_VERSION          InterfaceVersion
)
{
    LOG_TRACE_USERMODE("Will check interface version 0x%x from UM against 0x%x from KM\n",
        InterfaceVersion, SYSCALL_IF_VERSION_KM);

    if (InterfaceVersion != SYSCALL_IF_VERSION_KM)
    {
        LOG_ERROR("Usermode interface 0x%x incompatible with KM!\n", InterfaceVersion);
        return STATUS_INCOMPATIBLE_INTERFACE;
    }

    return STATUS_SUCCESS;
}


STATUS
SyscallFileWrite(
    IN UM_HANDLE FileHandle,
    IN_READS_BYTES(BytesToWrite) PVOID Buffer, 
    IN QWORD BytesToWrite, 
    OUT QWORD* BytesWritten
)
{   
    UNREFERENCED_PARAMETER(FileHandle);
    
    if (FileHandle == UM_FILE_HANDLE_STDOUT)
    {
        LOG("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);
    }

    *BytesWritten = BytesToWrite;

    return STATUS_SUCCESS;
}

//STATUS
//SyscallFileWrite(
//    IN UM_HANDLE File_Handle,
//    IN_READS_BYTES(BytesToWrite)
//    PVOID         buffer,
//    IN QWORD       BytesToWrite,
//    OUT QWORD* BytesWritten
//
//)
//{
//    UNREFERENCED_PARAMETER(File_Handle);
//    UNREFERENCED_PARAMETER(BytesToWrite);
//    LOG("[%s]:[%s]\n", ProcessGetName(NULL), buffer);
//    *BytesWritten = 10;
//
//    return STATUS_SUCCESS;
//}

//INT64
//ExTimerCompareTimers(
//	IN      PEX_TIMER     FirstElem,
//	IN      PEX_TIMER     SecondElem
//)
//{
//	return FirstElem->TriggerTimeUs - SecondElem->TriggerTimeUs;
//}

void
_No_competing_thread_
SyscallSystemPreinit(
	void
)
{
	memzero(&m_umHandlerSystemData, sizeof(UM_HANDLE_SYSTEM_DATA));
	InitializeListHead(&m_umHandlerSystemData.AllUmHandlersList);
	LockInit(&m_umHandlerSystemData.AllUmHandlersLock);
	UM_Handler_Value = 4;
}

STATUS
SyscallThreadCreate(
    IN      PFUNC_ThreadStart       StartFunction,
    IN_OPT  PVOID                   Context,
    OUT     UM_HANDLE*              ThreadHandle
) {
    

    PTHREAD newThred; // acolesa - aici ai nevoie de pointer la o structura THREAD, care e alocata in ThreadCreateEx, si trebuie sa ti se dea in aceasta variabila adresa acelei structuri
    char* name = "name";
    PUM_HANDLE_STRUCT UmHandler;
    INTR_STATE oldIntrState;
    
	if (StartFunction == NULL) {
		return STATUS_INVALID_PARAMETER1;
	}

	

	if (ThreadHandle == NULL) {
		return STATUS_INVALID_PARAMETER3;
	}

    //LOG("1\n");

    newThred = NULL;
    // acolesa - fara sa alociu sturctura de handle, da mai jos page fault, daca incerci sa scri in campurile ei
    UmHandler = ExAllocatePoolWithTag(PoolAllocateZeroMemory, sizeof(UM_HANDLE_STRUCT), HEAP_THREAD_TAG, 0);
    if (NULL == UmHandler)
    {
        LOG_FUNC_ERROR_ALLOC("HeapAllocatePoolWithTag", sizeof(UM_HANDLE_STRUCT));
        return STATUS_HEAP_INSUFFICIENT_RESOURCES;
    };
    //LOG("2\n");

    STATUS s = ThreadCreateEx(name, ThreadPriorityDefault, StartFunction, Context, &newThred, GetCurrentProcess()); // acolesa - aici dai adresa lui newThread, ca sa se poata modifica valoarei ei, adica sa se scrie in ea adresa structurii THREAD alocata de fc ThreadCreateEx
    //ThreadHandle = (UM_HANDLE)newThred->Id;
    UmHandler->Thred = newThred;
    UmHandler->Id = UM_Handler_Value++;
    //o variabila pe care sa o initializez cu zero atunci cand porneste sistemu si incrementata cu 1 
    *ThreadHandle = UmHandler->Id; // acolesa e *THreadHandle, ca sa scrii la adresa pointata de ThreadHandle valoarea handlelului

    //LOG("3\n");
    if (s != STATUS_SUCCESS)
        return STATUS_UNSUCCESSFUL;


	LockAcquire(&m_umHandlerSystemData.AllUmHandlersLock, &oldIntrState);
	InsertTailList(&m_umHandlerSystemData.AllUmHandlersList, &UmHandler->AllList);
	LockRelease(&m_umHandlerSystemData.AllUmHandlersLock, oldIntrState);

    //LOG("4\n");


    return STATUS_SUCCESS;


}

STATUS
SyscallThreadGetTid(
    IN_OPT  UM_HANDLE               ThreadHandle,
    OUT     TID*                    ThreadId
) {
    PTHREAD newThred;
    INTR_STATE oldIntrState;
    LIST_ITERATOR iterator;
    PLIST_ENTRY pListEntry;
    PUM_HANDLE_STRUCT UmHandler;

	newThred = NULL;
	UmHandler = NULL;
   

    

	if (ThreadHandle == UM_INVALID_HANDLE_VALUE) {
		newThred = GetCurrentThread();

		if (newThred == NULL) {
			return STATUS_UNSUCCESSFUL;
		}

		*ThreadId = newThred->Id;
		return STATUS_SUCCESS;
	}
    
	LockAcquire(&m_umHandlerSystemData.AllUmHandlersLock, &oldIntrState);
	//ListSearchForElement(&m_umHandlerSystemData.AllUmHandlersList, (PLIST_ENTRY)ThreadHandle,FALSE,compare,context);
    ListIteratorInit(&m_umHandlerSystemData.AllUmHandlersList, &iterator);
    while ((pListEntry = ListIteratorNext(&iterator)) != NULL )
    {
        UmHandler = CONTAINING_RECORD(pListEntry, UM_HANDLE_STRUCT, AllList);
        if (UmHandler->Id == ThreadHandle) {
            newThred = (PTHREAD)(UmHandler->Thred);
           
            break;
        }
    }
    
	LockRelease(&m_umHandlerSystemData.AllUmHandlersLock, oldIntrState);
    
    if (newThred != NULL) 
    {
        *ThreadId = newThred->Id;
        return STATUS_SUCCESS;
        
    }

    return STATUS_INVALID_PARAMETER1;
        
}

STATUS
SyscallThreadCloseHandle(
    IN      UM_HANDLE               ThreadHandle
) {

	PTHREAD newThred;
	INTR_STATE oldIntrState;

	LIST_ITERATOR iterator;
	PLIST_ENTRY pListEntry;
	PUM_HANDLE_STRUCT UmHandler;

	newThred = NULL;
	UmHandler = NULL;


	if (ThreadHandle == UM_INVALID_HANDLE_VALUE) {

		return STATUS_INVALID_PARAMETER1;
	}
    
    //LOG("1\n");

	LockAcquire(&m_umHandlerSystemData.AllUmHandlersLock, &oldIntrState);
	//ListSearchForElement(&m_umHandlerSystemData.AllUmHandlersList, (PLIST_ENTRY)ThreadHandle,FALSE,compare,context);
	ListIteratorInit(&m_umHandlerSystemData.AllUmHandlersList, &iterator);
    //LOG("2\n");

	while ((pListEntry = ListIteratorNext(&iterator)) != NULL)
	{
		UmHandler = CONTAINING_RECORD(pListEntry, UM_HANDLE_STRUCT, AllList);
		if (UmHandler->Id == ThreadHandle) {
			//nu stiu cum sa sterg din lista asa ca pun id-ul ca si handler invalid
            newThred = (PTHREAD)(UmHandler->Thred);
            UmHandler->Id = UM_INVALID_HANDLE_VALUE;
            //LOG("3\n");

			break;
		}
	}
	LockRelease(&m_umHandlerSystemData.AllUmHandlersLock, oldIntrState);
    //LOG("4\n");

    if (newThred != NULL)
    {
        ThreadCloseHandle(newThred);
        return STATUS_SUCCESS;
    }
    //LOG("5\n");
    return STATUS_UNSUCCESSFUL;
}

STATUS
SyscallThreadWaitForTermination(
    IN      UM_HANDLE               ThreadHandle,
    OUT     STATUS* TerminationStatus
) {
	PTHREAD newThred;
	INTR_STATE oldIntrState;
	
	LIST_ITERATOR iterator;
	PLIST_ENTRY pListEntry;
	PUM_HANDLE_STRUCT UmHandler;

	newThred = NULL;
	UmHandler = NULL;

	if (ThreadHandle == UM_INVALID_HANDLE_VALUE) {

		return STATUS_INVALID_PARAMETER1;
	}

	LockAcquire(&m_umHandlerSystemData.AllUmHandlersLock, &oldIntrState);
	//ListSearchForElement(&m_umHandlerSystemData.AllUmHandlersList, (PLIST_ENTRY)ThreadHandle,FALSE,compare,context);
	ListIteratorInit(&m_umHandlerSystemData.AllUmHandlersList, &iterator);
	while ((pListEntry = ListIteratorNext(&iterator)) != NULL)
	{
        UmHandler = CONTAINING_RECORD(pListEntry, UM_HANDLE_STRUCT, AllList);
		if (UmHandler->Id == ThreadHandle) {
            newThred = (PTHREAD)(UmHandler->Thred);
			
			break;
		}
	}
	LockRelease(&m_umHandlerSystemData.AllUmHandlersLock, oldIntrState);
    if (newThred != NULL)
    {
        ThreadWaitForTermination(newThred, TerminationStatus);
        return STATUS_SUCCESS;
    }
    return STATUS_UNSUCCESSFUL;
}


STATUS
SyscallThreadExit(
    IN      STATUS      ExitStatus
) {
    ThreadExit(ExitStatus);
    return ExitStatus;
}
//functii: IsBufferValid si isStringValid

// STUDENT TODO: implement the rest of the syscalls