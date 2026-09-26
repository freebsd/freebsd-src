#include "kboot_runtime.h"
#include "syscall.h"

/*
 * kboot is single-threaded, so use the same raw host syscall path instead of
 * handling pthread cancellation.
 */
long(__syscall_cp)(syscall_arg_t nr, syscall_arg_t u, syscall_arg_t v,
    syscall_arg_t w, syscall_arg_t x, syscall_arg_t y, syscall_arg_t z)
{
	return (host_syscall(nr, u, v, w, x, y, z));
}
