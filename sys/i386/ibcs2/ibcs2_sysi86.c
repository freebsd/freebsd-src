/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_proto.h>

#define IBCS2_FP_NO     0       /* no fp support */
#define IBCS2_FP_SW     1       /* software emulator */
#define IBCS2_FP_287    2       /* 80287 FPU */
#define IBCS2_FP_387    3       /* 80387 FPU */

#define SI86_FPHW	40
#define STIME		54
#define SETNAME		56
#define SI86_MEM	65

extern int hw_float;

int
ibcs2_sysi86(struct thread *td, struct ibcs2_sysi86_args *args)
{
	switch (args->cmd) {
	case SI86_FPHW: {	/* Floating Point information */
		int val, error;

		if (hw_float)
			val = IBCS2_FP_387;
		else
			val = IBCS2_FP_NO;
		if ((error = copyout(&val, args->arg, sizeof(val))) != 0)
			return error;
		return 0;
		}

        case STIME:       /* set the system time given pointer to long */
	  /* gettimeofday; time.tv_sec = *args->arg; settimeofday */
	        return EINVAL;

	case SETNAME:  {  /* set hostname given string w/ len <= 7 chars */
	        int name[2];

		name[0] = CTL_KERN;
		name[1] = KERN_HOSTNAME;
		return (userland_sysctl(td, name, 2, 0, 0, 0, 
		    args->arg, 7, 0, 0));
	}

	case SI86_MEM:	/* size of physical memory */
		td->td_retval[0] = ctob(physmem);
		return 0;

	default:
#ifdef DIAGNOSTIC
		printf("IBCS2: 'sysi86' function %d(0x%x) "
			"not implemented yet\n", args->cmd, args->cmd);
#endif
		return EINVAL;
	}
}
