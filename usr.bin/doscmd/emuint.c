/*-
 * Copyright (c) 1997 Helmut Wirth <hfwirth@ping.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, witout modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <ctype.h>
#include "doscmd.h"
#include "emuint.h"

/* The central entry point for the emulator interrupt. This is used by
 * different special programs to call the emulator from VM86 space. 
 * Look at emuint.h for definitions and a list of the currently defined
 * subfunctions.
 * To call emuint from VM86 space do:
 *	push ax		   Save original ax value (*must be done* !)
 *	mov  ah, funcnum   Emuint function number to ah
 *	mov  al, subfunc   Subfunction number, optional, depending on func
 *	int  0xff		
 *	..
 *	..
 * Emuint saves the function and subfunction numbers internally, then
 * pops ax off the stack and calls the function handler with the original
 * value in ax.
 */
void
emuint(regcontext_t *REGS)
{
    u_short func, subfunc;

    /* Remove function number from stack */
    func = R_AH;
    subfunc = R_AL;

    R_AX = POP(REGS);

    /* Call the function handler, subfunction is ignored, if unused */
    switch (func)
    {
	/* The redirector call */
	case EMU_REDIR:
	    intff(REGS);
	    break;

	/* EMS call, used by emsdriv.sys */
	case EMU_EMS:
	{
	    switch (subfunc) 
	    {
	    	case EMU_EMS_CTL:
		    R_AX = (u_short)ems_init();
		    break;

	    	case EMU_EMS_CALL:
		    ems_entry(REGS);
		    break;

		default:
		    debug(D_ALWAYS, "Undefined subfunction for EMS call\n");
		    break;
	    }
	    break;
	}

	default:
	    debug(D_ALWAYS, "Emulator interrupt called with undefined function %02x\n", func);

            /* 
             * XXX
             * temporary backwards compatibility with instbsdi.exe
             * remove after a while.
             */
            fprintf(stderr, "***\n*** WARNING - unknown emuint function\n");
            fprintf(stderr, "*** Continuing; assuming instbsdi redirector.\n");
            fprintf(stderr, "*** Please install the new redirector");
            fprintf(stderr, " `redir.com' as soon as possible.\n");
            fprintf(stderr, "*** This compatibility hack is not permanent.\n");
            fprintf(stderr, "***\n");
            PUSH(R_AX, REGS);
            R_BX = R_ES;
            R_DX = R_DI;
            R_DI = R_DS;
            intff(REGS);
	    break;
    }
}
