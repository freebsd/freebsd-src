/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *
 *	from: @(#)output.h	5.1 (Berkeley) 3/7/91
 *	output.h,v 1.4 1993/08/01 18:58:38 mycroft Exp
 */

#ifndef OUTPUT_INCL

struct output {
	char *nextc;
	int nleft;
	char *buf;
	int bufsize;
	short fd;
	short flags;
};

extern struct output output;
extern struct output errout;
extern struct output memout;
extern struct output *out1;
extern struct output *out2;


#ifdef __STDC__
void outstr(char *, struct output *);
void out1str(char *);
void out2str(char *);
void outfmt(struct output *, char *, ...);
void out1fmt(char *, ...);
void fmtstr(char *, int, char *, ...);
/* void doformat(struct output *, char *, va_list); */
void doformat();
void emptyoutbuf(struct output *);
void flushall(void);
void flushout(struct output *);
void freestdout(void);
int xwrite(int, char *, int);
int xioctl(int, int, int);
#else
void outstr();
void out1str();
void out2str();
void outfmt();
void out1fmt();
void fmtstr();
/* void doformat(); */
void doformat();
void emptyoutbuf();
void flushall();
void flushout();
void freestdout();
int xwrite();
int xioctl();
#endif

#define outc(c, file)	(--(file)->nleft < 0? (emptyoutbuf(file), *(file)->nextc++ = (c)) : (*(file)->nextc++ = (c)))
#define out1c(c)	outc(c, out1);
#define out2c(c)	outc(c, out2);

#define OUTPUT_INCL
#endif
