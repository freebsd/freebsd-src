/*-
 * Copyright (c) 2011 David Schultz
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * BUFSIZE is the number of bytes of rc4 output to compare.  The probability
 * that this test fails spuriously is 2**(-BUFSIZE * 8).
 */
#define	BUFSIZE		8

/*
 * Test whether arc4random_buf() returns the same sequence of bytes in both
 * parent and child processes.  (Hint: It shouldn't.)
 */
int main(int argc, char *argv[]) {
	struct shared_page {
		char parentbuf[BUFSIZE];
		char childbuf[BUFSIZE];
	} *page;
	pid_t pid;
	char c;

	printf("1..1\n");

	page = mmap(NULL, sizeof(struct shared_page), PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_SHARED, -1, 0);
	if (page == MAP_FAILED) {
		printf("fail 1 - mmap\n");
		exit(1);
	}

	arc4random_buf(&c, 1);

	pid = fork();
	if (pid < 0) {
		printf("fail 1 - fork\n");
		exit(1);
	}
	if (pid == 0) {
		/* child */
		arc4random_buf(page->childbuf, BUFSIZE);
		exit(0);
	} else {
		/* parent */
		int status;
		arc4random_buf(page->parentbuf, BUFSIZE);
		wait(&status);
	}
	if (memcmp(page->parentbuf, page->childbuf, BUFSIZE) == 0) {
		printf("fail 1 - sequences are the same\n");
		exit(1);
	}

	printf("ok 1 - sequences are different\n");
	exit(0);
}
