/*
 * Copyright (c) 1993 Christoph M. Robitschko
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
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

/*
 * cf_defs.h
 * typedefs and defines for the configuration subsystem.
 */

struct Command	{
		char	*name;		/* command name / value description   */
		short	level;		/* level of command nesting           */
		u_char	type;		/* type of entry (exact, string, int) */
		u_char	flags;		/* additional flags                   */
		void	*var;		/* Pointer to variable to assign to   */
		union	{
			int	intval;	/* Integer value to assign to *var    */
			char	*chval; /* String ...                         */
			} val;
		};


/* defines for the level field */
#define SUB	0x100		/* There must be another argument */
#define OPT	0x200		/* There can be another argument (not yet impl) */
#define LEVEL	0x0ff		/* Flags masked off */

/* Valid types */
#define T_EX	0x00		/* Must match exact */
#define T_STR	0x01		/* Matches any string */
#define T_INT	0x02		/* Matches an integer */
#define T_BYTE	0x03		/* Matches a byte expression (1m = 1024k == 1024 == 1048576b) */
#define T_TIME	0x04		/* Matches a time expression (1:00:00 == 1h == 60m == 3600s == 3600) */

/* values for the flags field */
#define NRAISE	0x01		/* Variable can be lowered but not raised */
#define MAXVAL	0x02		/* intval contains the maximum allowed value */
#define CFUNC	0x04		/* Call the function *(void (*)())var(argc, argv) */


#define NOVAR		NULL, {0}
