/*
 * Copyright 1997 Sean Eric Fagan
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Linux/i386-specific system call handling. */

#include <sys/ptrace.h>

#include <machine/reg.h>
#include <machine/psl.h>

#include <stdio.h>

#include "truss.h"

#include "amd64-linux32_syscalls.h"

static int
amd64_linux32_fetch_args(struct trussinfo *trussinfo, u_int narg)
{
	struct reg regs;
	struct current_syscall *cs;
	lwpid_t tid;

	tid = trussinfo->curthread->tid;
	cs = &trussinfo->curthread->cs;
	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) < 0) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return (-1);
	}

	/*
	 * Linux passes syscall arguments in registers, not
	 * on the stack.  Fortunately, we've got access to the
	 * register set.  Note that we don't bother checking the
	 * number of arguments.	And what does linux do for syscalls
	 * that have more than five arguments?
	 */
	switch (narg) {
	default:
		cs->args[5] = regs.r_rbp;	/* Unconfirmed */
	case 5:
		cs->args[4] = regs.r_rdi;
	case 4:
		cs->args[3] = regs.r_rsi;
	case 3:
		cs->args[2] = regs.r_rdx;
	case 2:
		cs->args[1] = regs.r_rcx;
	case 1:
		cs->args[0] = regs.r_rbx;
	}

	return (0);
}

/*
 * Linux syscalls return negative errno's, we do positive and map them
 */
static const int bsd_to_linux_errno[] = {
	-0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,
	-10, -35, -12, -13, -14, -15, -16, -17, -18, -19,
	-20, -21, -22, -23, -24, -25, -26, -27, -28, -29,
	-30, -31, -32, -33, -34, -11,-115,-114, -88, -89,
	-90, -91, -92, -93, -94, -95, -96, -97, -98, -99,
	-100,-101,-102,-103,-104,-105,-106,-107,-108,-109,
	-110,-111, -40, -36,-112,-113, -39, -11, -87,-122,
	-116, -66,  -6,  -6,  -6,  -6,  -6, -37, -38,  -9,
	-6,
};

static int
amd64_linux32_fetch_retval(struct trussinfo *trussinfo, long *retval,
    int *errorp)
{
	struct reg regs;
	lwpid_t tid;
	size_t i;

	tid = trussinfo->curthread->tid;
	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) < 0) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return (-1);
	}

	retval[0] = regs.r_rax & 0xffffffff;
	retval[1] = regs.r_rdx & 0xffffffff;
	*errorp = !!(regs.r_rflags & PSL_C);

	if (*errorp) {
		for (i = 0; i < nitems(bsd_to_linux_errno); i++) {
			if (retval[0] == bsd_to_linux_errno[i]) {
				retval[0] = i;
				return (0);
			}
		}

		/* XXX: How to handle unknown errors? */
	}
	return (0);
}

static struct procabi amd64_linux32 = {
	"Linux ELF32",
	syscallnames,
	nitems(syscallnames),
	amd64_linux32_fetch_args,
	amd64_linux32_fetch_retval
};

PROCABI(amd64_linux32);
