/*
 * $Id: dumpcis.c,v 1.2 1995/08/25 09:45:24 phk Exp $
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

	sprintf(name, "/dev/card%d", slot);
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
