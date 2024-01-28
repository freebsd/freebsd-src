#include <machine/asm.h>

/*
 * Emulate the Linux system call interface. The system call number is set in
 * %r0, and %r3 -> %r8 have the 6 system call arguments. errno is returned
 * as a negative value, but we use it more as a flag something went wrong
 * rather than using its value.
 *
 * Return value in %r3. If it is positive or < -4096, it's a successful
 * system call. If it is between -1 and -4095 then it's an failed system
 * call with -x as the errno. Errors from the kernel are signaled via the
 * the 'so' bit, but we don't test that here at all. There are at most 6
 * arguments to system calls in Linux.
 *
 * We expose the raw system call result, rather than do the POSIX
 * conversion to -1 and setting errno.
 *
 * Note: The code this replaced used bso to set %r3 to 0 for the read and
 * open system calls for reasons that are still under investigation.
 */
ENTRY(host_syscall)
	mr	%r0, %r3	/* SYS_ number in $r0 */
	mr	%r3, %r4	/* arg2 -> 1 */
	mr	%r4, %r5	/* arg3 -> 2 */
	mr	%r5, %r6	/* arg4 -> 3 */
	mr	%r6, %r7	/* arg5 -> 4 */
	mr	%r7, %r8	/* arg6 -> 5 */
	mr	%r8, %r9	/* arg7 -> 6 */
	sc
	blr
/* Note: We're exposing the raw return value to the caller */	
END(host_syscall)
