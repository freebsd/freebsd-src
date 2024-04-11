#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/systm.h> // for copyin, copyout, and uprintf

int sys_dbquery (struct thread *td, struct dbquery_args *uap) {
    // Return positive value to indicate failure
    return 1;
}
