#include "common_lib.h"
#include "syscall_if.h"
#include "um_lib_helper.h"

STATUS
__main(
    DWORD       argc,
    char**      argv
)   
{
    UNREFERENCED_PARAMETER(argc);
    
    UNREFERENCED_PARAMETER(argv);
   // char buffer = 2;
    //QWORD   val;
    //SyscallFileWrite(NULL,buffer,NULL,&val);
    
    return STATUS_SUCCESS;
}