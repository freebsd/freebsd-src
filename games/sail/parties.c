/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)parties.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include "externs.h"

meleeing(from, to)
struct ship *from;
register struct ship *to;
{
	register struct BP *p = from->file->OBP;
	register struct BP *q = p + NBP;

	for (; p < q; p++)
		if (p->turnsent && p->toship == to)
			return 1;
	return 0;
}

boarding(from, isdefense)
register struct ship *from;
char isdefense;
{
	register struct BP *p = isdefense ? from->file->DBP : from->file->OBP;
	register struct BP *q = p + NBP;

	for (; p < q; p++)
		if (p->turnsent)
			return 1;
	return 0;
}

unboard(ship, to, isdefense)
register struct ship *ship, *to;
register char isdefense;
{
	register struct BP *p = isdefense ? ship->file->DBP : ship->file->OBP;
	register n;

	for (n = 0; n < NBP; p++, n++)
		if (p->turnsent && (p->toship == to || isdefense || ship == to))
			Write(isdefense ? W_DBP : W_OBP, ship, 0, n, 0, 0, 0);
}
