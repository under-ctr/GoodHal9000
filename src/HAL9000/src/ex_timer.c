#include "HAL9000.h"
#include "ex_timer.h"
#include "iomu.h"
#include "thread_internal.h"
#include "base.h"

typedef struct _GLOBAL_TIMER_LIST
{
    // protect the global timer list
    LOCK            TimerListLock;

    // the list 's head
    _Guarded_by_(TimerListLock)
    LIST_ENTRY      TimerListHead;

}GLOBAL_TIMER_LIST, * PGLOBAL_TIMER_LIST;

static  GLOBAL_TIMER_LIST m_globalTimerList;

STATUS
ExTimerInit(
    OUT     PEX_TIMER       Timer,
    IN      EX_TIMER_TYPE   Type,
    IN      QWORD           Time
    )
{   

    //PVOID  context;
    STATUS status;
    INTR_STATE oldState;

    //UNREFERENCED_PARAMETER(context);

    if (NULL == Timer)
    {
        return STATUS_INVALID_PARAMETER1;
    }

    if (Type > ExTimerTypeMax)
    {
        return STATUS_INVALID_PARAMETER2;
    }

    status = STATUS_SUCCESS;

    memzero(Timer, sizeof(EX_TIMER));

    Timer->Type = Type;
    if (Timer->Type != ExTimerTypeAbsolute)
    {
        // relative time

        // if the time trigger time has already passed the timer will
        // be signaled after the first scheduler tick
        Timer->TriggerTimeUs = IomuGetSystemTimeUs() + Time;
        Timer->ReloadTimeUs = Time;
    }
    else
    {
        // absolute
        Timer->TriggerTimeUs = Time;
    }

    status = ExEventInit(&Timer->TimerEvent, ExEventTypeNotification, FALSE);
    LockAcquire(&m_globalTimerList.TimerListLock, &oldState);
    //InsertTailList(&m_globalTimerList.TimerListHead, &Timer->TimerListElem);
    InsertOrderedList(&m_globalTimerList.TimerListHead, &Timer->TimerListElem,(PFUNC_CompareFunction)ExTimerCompareListElems,NULL);
    LockRelease(&m_globalTimerList.TimerListLock, oldState);

    return status;
}

void
ExTimerStart(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = TRUE;
}

void
ExTimerStop(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }
    

    Timer->TimerStarted = FALSE;
    ExEventSignal(&Timer->TimerEvent);
}

void
ExTimerWait(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    ExEventWaitForSignal(&Timer->TimerEvent);

    /*
    while (IomuGetSystemTimeUs() < Timer->TriggerTimeUs && Timer->TimerStarted)
    {
        ThreadYield();
    }
    */
}

void
ExTimerUninit(
    INOUT   PEX_TIMER       Timer
    )
{   
    INTR_STATE oldState;

    ASSERT(Timer != NULL);

    ExTimerStop(Timer);

    Timer->TimerUninited = TRUE;


	LockAcquire(&m_globalTimerList.TimerListLock, &oldState);
    RemoveEntryList(&Timer->TimerListElem);
	LockRelease(&m_globalTimerList.TimerListLock, oldState);
}
void
ExTimerCheck(
	IN          PEX_TIMER       Timer
)
{
	if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs) {
		ExEventSignal(&Timer->TimerEvent);
	}


}

static
STATUS
(_cdecl  _MyTimerFunc)(
	IN          PLIST_ENTRY     ListEntry,
	IN_OPT      PVOID           Context
	)
{
	UNREFERENCED_PARAMETER(Context);
	PEX_TIMER timerEntry = CONTAINING_RECORD(ListEntry, EX_TIMER, TimerListElem);
	ExTimerCheck(timerEntry);
	return STATUS_SUCCESS;

}


void
_No_competing_thread_
ExTimerSystemPreinit(
    void 
) 
{
    memzero(&m_globalTimerList, sizeof(GLOBAL_TIMER_LIST));
    InitializeListHead(&m_globalTimerList.TimerListHead);
    LockInit(&m_globalTimerList.TimerListLock);
}

void
ExTimerCheckAll(
	void
	
)
{
    INTR_STATE oldState;
    
	LockAcquire(&m_globalTimerList.TimerListLock, &oldState);
    ForEachElementExecute(&m_globalTimerList.TimerListHead, _MyTimerFunc, NULL, FALSE);
	LockRelease(&m_globalTimerList.TimerListLock, oldState);

}




INT64
ExTimerCompareTimers(
    IN      PEX_TIMER     FirstElem,
    IN      PEX_TIMER     SecondElem
)
{
    return FirstElem->TriggerTimeUs - SecondElem->TriggerTimeUs;
}


INT64
ExTimerCompareListElems(
    IN      PLIST_ENTRY     Timer1,
    IN      PLIST_ENTRY     Timer2
    //IN_OPT  PVOID           Context
)
{
    
    
    PEX_TIMER firstTimer = CONTAINING_RECORD(Timer1, EX_TIMER, TimerListElem);
    PEX_TIMER secondTimer = CONTAINING_RECORD(Timer2, EX_TIMER, TimerListElem);
    

    return ExTimerCompareTimers(firstTimer, secondTimer);
}
