#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/systm.h> // for copyin, copyout, and uprintf
    
MALLOC_DEFINE(M_SYSHW, "helloworld", "Buffer for helloworld system call");
MALLOC_DECLARE(M_SYSHW); // Declare a new malloc type for our system call

int sys_helloworld (struct thread *td, struct helloworld_args *uap) {
    int i;
    int error = 0;
    int space_required = 11 + uap->len + 1;
    char *kernel_buffer;

    kernel_buffer = (char *)malloc(space_required, M_SYSHW, M_WAITOK | M_ZERO);

    if (kernel_buffer == NULL) {
        return ENOMEM;
    }

    sprintf(kernel_buffer, "Hello World");
    for (i=0; i< uap->len; i++) {
        kernel_buffer[11+i] = '!';
    }
    kernel_buffer[11 + uap->len] = '\0';

    error = copyout(kernel_buffer, uap->buf, space_required);

    free(kernel_buffer, M_SYSHW);

    td->td_retval[0] = 0;
    return error;
}