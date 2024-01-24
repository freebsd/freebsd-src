#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/systm.h> // for copyin, copyout, and uprintf

MALLOC_DEFINE(M_DBQUERY, "dbquery", "Buffer for dbquery system call");
MALLOC_DECLARE(M_DBQUERY); // Declare a new malloc type for our system call

int sys_dbquery (struct thread *td, struct dbquery_args *uap) {
    int error = 0;
    int space_required = 16;
    char *kernel_buffer;

    kernel_buffer = (char *)malloc(space_required, M_DBQUERY, M_WAITOK | M_ZERO);

    if (kernel_buffer == NULL) {
        return ENOMEM;
    }

    sprintf(kernel_buffer, "Not implemented");
    kernel_buffer[15] = '\0';

    error = copyout(kernel_buffer, uap->buf, space_required);

    free(kernel_buffer, M_DBQUERY);

    td->td_retval[0] = 0;
    return error;
}