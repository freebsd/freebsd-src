/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI int2f.c,v 2.2 1996/04/08 19:32:53 bostic Exp
 *
 * $Id: int2f.c,v 1.4 1996/09/22 15:42:56 miff Exp $
 */

#include "doscmd.h"
#include "dispatch.h"

/*
** Multiplex interrupt.
**
** subfunctions 0-0x7f reserved for DOS, some are implemented here.
**
*/

/*
** 2f:00 2f:01 2f:02 2f:03
**
** Various PRINT.COM functions
*/
static int
int2f_printer(regcontext_t *REGS)
{
    debug (D_FILE_OPS, "Called printer function 0x%02x", R_AH);
    R_AL = FUNC_NUM_IVALID;
}

/*
** 2f:12
**
** DOS internal functions.  Only one we support is 0x2e, and then only to
** complain about it.
*/
static int
int2f_dosinternal(regcontext_t *REGS)
{
    switch (R_AL) {
    case 0x2e:		/* XXX - GET/SET ERROR TABLE ADDRESSES */
	switch (R_DL) {
	case 0x00:
	case 0x02:
	case 0x04:
	case 0x06:
	    debug(D_ALWAYS,"DOS program attempted to get internal error table.\n");
	    break;
	    
	case 0x01:
	case 0x03:
	case 0x05:
	case 0x07:
	case 0x09:
	    debug(D_ALWAYS,"DOS program attempted to set error table.\n");
	    break;
	}	
	
    default:
	unknown_int4(0x2f, 0x12, R_AL, R_DL, REGS);
	break;
    }
    R_AL = FUNC_NUM_IVALID;
    return(0);
}

/*
** 2f:16
**
** Windows Enhanced Mode functions.  Aigh!
*/
static int
int2f_windows(regcontext_t *REGS)
{
    switch (R_AL) {
    case 0x80:				/* installation check */
	tty_pause();
	R_AL = 0x00;
	return(0);

    default:
	unknown_int3(0x2f, 0x16, R_AL, REGS);
	break;
    }
    R_AL = FUNC_NUM_IVALID;
    return(0);
}

/*
** 2f:43
**
** XMS interface
*/
static int
int2f_xms(regcontext_t *REGS)
{
    switch(R_AL) {
    case 0:			/* installation check */
	return(0);		/* %al = 0 */
    default:
	R_AL = FUNC_NUM_IVALID;
	return(0);
    }
}
	

static struct intfunc_table int2f_table[] = {

    { 0x00,	IFT_NOSUBFUNC,	int2f_printer,		"printer"},
    { 0x01,	IFT_NOSUBFUNC,	int2f_printer,		"printer"},
    { 0x02,	IFT_NOSUBFUNC,	int2f_printer,		"printer"},
    { 0x03,	IFT_NOSUBFUNC,	int2f_printer,		"printer"},
    { 0x12,	IFT_NOSUBFUNC,	int2f_dosinternal,	"DOS internal function"},
    { 0x16,	IFT_NOSUBFUNC,	int2f_windows,		"Windows detect"},
    { 0x43,	IFT_NOSUBFUNC,	int2f_xms,		"XMS"},
    { -1,	0,		NULL,			NULL}
};

/*
** int2f (multiplex) handler.
**
** Note that due to the widely varied and inconsistent conventions, handlers
** called from here are expected to manage their own return values.
*/
void
int2f(regcontext_t *REGS)
{
    int		index;
   
    /* look up the handler for the current function */
    index = intfunc_search(int2f_table, R_AH, R_AL);

    if (index >= 0) {		/* respond on multiplex chain */
	int2f_table[index].handler(REGS);
    } else {
	unknown_int2(0x2f, R_AH, REGS);
    }
}


    
