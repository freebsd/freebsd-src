/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 *
 * $FreeBSD$
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>
#include "readcis.h"

int     nocards;

void
scan(slot)
	int     slot;
{
	int     fd;
	char    name[64];
	struct cis *cp;
	struct slotstate st;

	sprintf(name, CARD_DEVICE, slot);
	fd = open(name, 0);
	if (fd < 0)
		return;
	nocards++;
	ioctl(fd, PIOCGSTATE, &st);
	if (st.state == filled) {
		cp = readcis(fd);
		if (cp) {
			printf("Configuration data for card in slot %d\n",
			    slot);
			dumpcis(cp);
			freecis(cp);
		}
	}
}

void
dump(p, sz)
	unsigned char *p;
	int     sz;
{
	int     ad = 0, i;

	while (sz > 0) {
		printf("%03x: ", ad);
		for (i = 0; i < ((sz < 16) ? sz : 16); i++)
			printf(" %02x", p[i]);
		printf("\n");
		sz -= 16;
		p += 16;
		ad += 16;
	}
}

void *
xmalloc(int sz)
{
	void   *p;

	sz = (sz + 7) & ~7;
	p = malloc(sz);
	if (p)
		bzero(p, sz);
	else {
		perror("malloc");
		exit(1);
	}
	return (p);
}

int
dumpcis_main(int argc, char **argv)
{
	int     node;

	for (node = 0; node < 8; node++)
		scan(node);
	printf("%d slots found\n", nocards);
	return 0;
}
