/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>

typedef long long gcov_type;

#define PARAMS(foo)	foo
#define ATTRIBUTE_UNUSED __unused
#include "gcov-io.h"

struct bbf {
	long	checksum;
	long	arc_count;
	u_long	name;
};

struct bb {
	u_long	zero_one;
	u_long	filename;
	u_long	counts;
	u_long	ncounts;
	u_long	next;
	u_long	sizeof_bb;
	u_long	funcs;
};

struct nlist namelist[] = {
	{ "bbhead", 0, 0, 0, 0 },
	{ NULL, 0, 0, 0, 0 }
};

kvm_t	*kv;

int
main(int argc __unused, char **argv __unused)
{
	int i, funcs;
	u_long l1,l2,l4;
	struct bb bb;
	struct bbf bbf;
	char buf[BUFSIZ], *p;
	gcov_type *q, *qr;
	
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
#if 0
printf("%lx\n%lx\n%lx\n%lx\n%lx\n%lx\n%lx\n",
	bb.zero_one, bb.filename, bb.counts, bb.ncounts, bb.next,
	bb.sizeof_bb, bb.funcs);
#endif

		funcs = 0;
		for (l4 = bb.funcs; ; l4 += sizeof (bbf)) {
			kvm_read(kv, l4, &bbf, sizeof(bbf));
			if (bbf.arc_count == -1)
				break;
			funcs++;
		}
		
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
		__write_long(-123, f, 4);

		__write_long(funcs, f, 4);

		__write_long(4 + 8 + 8 + 4 + 8 + 8, f, 4);

		__write_long(bb.ncounts, f, 4);
		__write_long(0, f, 8);
		__write_long(0, f, 8);

		__write_long(bb.ncounts, f, 4);
		__write_long(0, f, 8);
		__write_long(0, f, 8);

		qr = malloc(bb.ncounts * 8);
		kvm_read(kv, bb.counts, qr, bb.ncounts * 8);
		q = qr;
		for (l4 = bb.funcs; ; l4 += sizeof (bbf)) {
			kvm_read(kv, l4, &bbf, sizeof(bbf));
			if (bbf.arc_count == -1)
				break;
			kvm_read(kv, bbf.name, buf, sizeof(buf));

			__write_gcov_string(buf, strlen(buf), f, -1);
			
			__write_long(bbf.checksum, f, 4);
			__write_long(bbf.arc_count, f, 4);
			for (i = 0; i < bbf.arc_count; i++) {
				__write_gcov_type(*q, f, 8);
				q++;
			}
		}
		fclose(f);
		free(qr);
	}
	return 0;
}
