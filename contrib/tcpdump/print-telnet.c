/*	$NetBSD: print-telnet.c,v 1.2 1999/10/11 12:40:12 sjg Exp $ 	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon J. Gerraty.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *      @(#)Copyright (c) 1994, Simon J. Gerraty.
 *      
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code 
 *      is granted subject to the following conditions.
 *      1/ that the above copyright notice and this notice 
 *      are preserved in all copies.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
static const char rcsid[] =
     "@(#) $Header: /tcpdump/master/tcpdump/print-telnet.c,v 1.12 2000/09/29 04:58:51 guy Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>

#include <netinet/in.h>

#define TELCMDS
#define TELOPTS
#include <arpa/telnet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

#ifndef TELCMD_FIRST
# define TELCMD_FIRST SE
#endif

void
telnet_print(register const u_char *sp, u_int length)
{
	static char tnet[128];
	register int i, c, x;
	register u_char *rcp;
	int off, first = 1;
	u_char	*osp;

	off = 0;
	x = 0;
	
	while (length > 0 && *sp == IAC) {
		osp = (u_char *) sp;
		tnet[0] = '\0';
		
		c = *sp++;
		length--;
		switch (*sp) {
		case IAC:			/* <IAC><IAC>! */
			if (length > 1 && sp[1] == IAC) {
				(void)strcpy(tnet, "IAC IAC");
			} else {
				length = 0;
				continue;
			}
			break;
		default:
			c = *sp++;
			length--;
			if ((i = c - TELCMD_FIRST) < 0
			    || i > IAC - TELCMD_FIRST) {
				(void)printf("unknown: ff%02x\n", c);
				return;
			}
			switch (c) {
			case DONT:
			case DO:
			case WONT:
			case WILL:
			case SB:
				x = *sp++; /* option */
				length--;
				if (x >= 0 && x < NTELOPTS) {
					(void)snprintf(tnet, sizeof(tnet),
					    "%s %s", telcmds[i], telopts[x]);
				} else {
					(void)snprintf(tnet, sizeof(tnet),
					    "%s %#x", telcmds[i], x);
				}
				break;
			default:
				(void)snprintf(tnet, sizeof(tnet), "%s",
				    telcmds[i]);
			}
			if (c == SB) {
				c = *sp++;
				length--;
				(void)strcat(tnet, c ? " SEND" : " IS '");
				rcp = (u_char *) sp;
				i = strlen(tnet);
				while (length > 0 && (x = *sp++) != IAC)
					--length;
				if (x == IAC) {
					if (2 < vflag
					    && i + 16 + sp - rcp < sizeof(tnet)) {
						(void)strncpy(&tnet[i], rcp, sp - rcp);
						i += (sp - rcp) - 1;
						tnet[i] = '\0';
					} else if (i + 8 < sizeof(tnet)) {
						(void)strcat(&tnet[i], "...");
					}
					if (*sp++ == SE
					    && i + 4 < sizeof(tnet))
						(void)strcat(tnet, c ? " SE" : "' SE");
				} else if (i + 16 < sizeof(tnet)) {
					(void)strcat(tnet, " truncated!");
				}
			}
			break;
		}
		/*
		 * now print it
		 */
		if (Xflag && 2 < vflag) {
			if (first)
				printf("\nTelnet:\n");
			i = sp - osp;
			hex_print_with_offset(osp, i, off);
			off += i;
			if (i > 8)
				printf("\n\t\t\t\t");
			else
				printf("%*s\t", (8 - i) * 3, "");
			safeputs(tnet);
		} else {
			printf("%s", (first) ? " [telnet " : ", ");
			safeputs(tnet);
		}
		first = 0;
	}
	if (!first) {
		if (Xflag && 2 < vflag)
			printf("\n");
		else
			printf("]");
	}
}
