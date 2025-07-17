/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

#ifndef _STRESS_H_
#define _STRESS_H_
extern int setup(int);
extern int test(void);
extern void cleanup(void);
extern void options(int, char **);
extern int random_int(int, int);
/*extern void limits(void);*/

typedef struct {
	int argc;
	char **argv;
	int run_time;
	int load;
	char *wd;
	char *cd;
	int verbose;
	int incarnations;
	int hog;
	int nodelay;
	int kill;
	int64_t kblocks;
	int64_t inodes;
} opt_t;

extern opt_t *op;

extern volatile int done_testing;
extern char *home;
extern void rmval(void);
extern void putval(unsigned long);
extern unsigned long getval(void);
extern void getdf(int64_t *, int64_t *);
extern void reservedf(int64_t, int64_t);
extern void show_status(void);
extern int64_t swap(void);
extern unsigned long usermem(void);
#endif
