/*	$NetBSD: bootconfig.c,v 1.2 2002/03/10 19:56:39 lukem Exp $	*/

/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <sys/systm.h>

#include <machine/bootconfig.h>


/*
 * Function to identify and process different types of boot argument
 */

int
get_bootconf_option(opts, opt, type, result)
	char *opts;
	char *opt;
	int type;
	void *result;
{
	char *ptr;
	char *optstart;
	int not;

	ptr = opts;

	while (*ptr) {
		/* Find start of option */
		while (*ptr == ' ' || *ptr == '\t')
			++ptr;

		if (*ptr == 0)
			break;

		not = 0;

		/* Is it a negate option */
		if ((type & BOOTOPT_TYPE_MASK) == BOOTOPT_TYPE_BOOLEAN && *ptr == '!') {
			not = 1;
			++ptr;
		}

		/* Find the end of option */
		optstart = ptr;
		while (*ptr != 0 && *ptr != ' ' && *ptr != '\t' && *ptr != '=')
			++ptr;

		if ((*ptr == '=')
		    || (*ptr != '=' && ((type & BOOTOPT_TYPE_MASK) == BOOTOPT_TYPE_BOOLEAN))) {
			/* compare the option */
			if (strncmp(optstart, opt, (ptr - optstart)) == 0) {
				/* found */

				if (*ptr == '=')
					++ptr;

				switch(type & BOOTOPT_TYPE_MASK) {
				case BOOTOPT_TYPE_BOOLEAN :
					if (*(ptr - 1) == '=')
						*((int *)result) = ((u_int)strtoul(ptr, NULL, 10) != 0);
					else
						*((int *)result) = !not;
					break;
				case BOOTOPT_TYPE_STRING :
					*((char **)result) = ptr;
					break;			
				case BOOTOPT_TYPE_INT :
					*((int *)result) = (u_int)strtoul(ptr, NULL, 10);
					break;
				case BOOTOPT_TYPE_BININT :
					*((int *)result) = (u_int)strtoul(ptr, NULL, 2);
					break;
				case BOOTOPT_TYPE_HEXINT :
					*((int *)result) = (u_int)strtoul(ptr, NULL, 16);
					break;
				default:
					return(0);
				}
				return(1);
			}
		}
		/* skip to next option */
		while (*ptr != ' ' && *ptr != '\t' && *ptr != 0)
			++ptr;
	}
	return(0);
}
