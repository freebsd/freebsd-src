#include <machine/asm.h>

/*
 * Emulate the Linux system call interface. The system call number is set in
 * %rax, and %rdi, %rsi, %rdx, %r10, %r8, %r9 have the 6 system call
 * arguments. errno is returned as a negative value, but we use it more as a
 * flag something went wrong rather than using its value.
 *
 * Note: For system calls, we use %r10 instead of %rcx for the 4th argument.
 * See section A.2.1 for the Linux calling conventions of the ABI spec
 * https://web.archive.org/web/20160801075146/http://www.x86-64.org/documentation/abi.pdf
 * In addition to the below, %r11 and %rcx are destroyed, negative
 * values are ERRNO for %rax between -1 and -4095 otherwise the system
 * call is successful. Unlike other Unix systems, carry isn't used to
 * signal an error in the system call. We expose the raw system call
 * result, rather than do the POSIX converion to -1 and setting errno.
 */
ENTRY(host_syscall)
	movq %rdi, %rax		/* SYS_ number in %rax */
	movq %rsi, %rdi		/* arg2 -> 1 */
	movq %rdx, %rsi		/* arg3 -> 2 */
	movq %rcx, %rdx		/* arg4 -> 3 */
	movq %r8, %r10		/* arg5 -> 4 */
	movq %r9, %r8		/* arg6 -> 5 */
        movq 8(%rsp),%r9        /* arg7 -> 6 from stack.  */
        syscall
	ret
/* Note: We're exposing the raw return value to the caller */
END(host_syscall)

	.section .note.GNU-stack,"",%progbits
