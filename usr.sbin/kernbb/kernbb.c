/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct bb {
	u_long	zero_one;
	u_long	filename;
	u_long	counts;
	u_long	ncounts;
	u_long	next;
	u_long	addr;
	u_long	nwords;
	u_long	func;
	u_long	lineno;
	u_long	file;
};

struct nlist namelist[] = {
	{ "bbhead", 0, 0, 0, 0 },
	{ NULL, 0, 0, 0, 0 }
};

kvm_t	*kv;

int
main(int argc __unused, char **argv __unused)
{
	int i;
	u_long l1,l2,l4;
	struct bb bb;
	char buf[BUFSIZ], *p;
	FILE *f;

	kv = kvm_open(NULL,NULL,NULL,O_RDWR,"dnc");
	if (!kv) 
		err(1,"kvm_open");
	i = kvm_nlist(kv,namelist);
	if (i)
		err(1,"kvm_nlist");

	l1 = namelist[0].n_value;
	kvm_read(kv,l1,&l2,sizeof l2);
	while(l2) {
		l1 += sizeof l1;
		kvm_read(kv,l2,&bb,sizeof bb);
		l2 = bb.next;
		kvm_read(kv, bb.filename, buf, sizeof(buf));
		p = buf;
		f = fopen(p, "w");
		if (f != NULL) {
			printf("Writing \"%s\"\n", p);
		} else {
			p = strrchr(buf, '/');
			if (p == NULL)
				p = buf;
			else
				p++;
			printf("Writing \"%s\" (spec \"%s\")\n", p, buf);
			f = fopen(p, "w");
		}
		if (f == NULL)
			err(1,"%s", p);
		fwrite(&bb.ncounts, 4, 1, f);
		l4 = 0;
		fwrite(&l4, 4, 1, f);
		p = malloc(bb.ncounts * 8);
		kvm_read(kv, bb.counts, p, bb.ncounts * 8);
		fwrite(p, 8, bb.ncounts, f);
		fclose(f);
		free(p);
	}
	return 0;
}
