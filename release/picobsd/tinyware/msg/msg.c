/*-
 * Copyright (c) 1998 Andrzej Bialecki <abial@freebsd.org>
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
 *
 * $FreeBSD: src/release/picobsd/tinyware/msg/msg.c,v 1.2 1999/08/28 01:33:55 peter Exp $
 */

/*
 * Small replacement for 'dmesg'. It doesn't need libkvm nor /dev/kmem.
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/sysctl.h>

int
main(int argc, char *argv[])
{
	int len,i;
	char *buf,*p;
	char *mib="machdep.msgbuf";

	/* We use sysctlbyname, because the oid is unknown (OID_AUTO) */

	/* get the buffer size */
	i=sysctlbyname(mib,NULL,&len,NULL,NULL);
	if(i) {
		perror("buffer sizing");
		exit(-1);
	}
	buf=(char *)malloc(len*sizeof(char));
	i=sysctlbyname(mib,buf,&len,NULL,NULL);
	if(i) {
		perror("retrieving data");
		exit(-1);
	}
	p=buf;
	i=0;
	while(p<(buf+len)) {
		switch(*p) {
		case '\0':
			/* skip initial NULLs */
			break;
		default:
			putchar(*p);
		}
		p++;
	}
	if(*--p!='\n') putchar('\n');
	free(buf);
	exit(0);
}
