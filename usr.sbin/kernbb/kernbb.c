/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <kvm.h>
#include <err.h>

#define MAXBB 32768

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
	{ "bbset" },
	{ NULL }
};

u_long	lineno[MAXBB];
u_long	counts[MAXBB];
u_long	addr[MAXBB];
u_long	func[MAXBB];
u_long	file[MAXBB];
char	*fn[MAXBB];
char	*pn[MAXBB];

kvm_t	*kv;

int
main()
{
	int i,j;
	u_long l1,l2,l3;
	struct bb bb;
	char buf[128];

	kv = kvm_open(NULL,NULL,NULL,O_RDWR,"dnc");
	if (!kv) 
		err(1,"kvm_open");
	i = kvm_nlist(kv,namelist);
	if (i)
		err(1,"kvm_nlist");

	l1 = namelist[0].n_value;
	kvm_read(kv,l1,&l2,sizeof l2);
	while(l2--) {
		l1 += sizeof l1;
		kvm_read(kv,l1,&l3,sizeof l3);
		kvm_read(kv,l3,&bb,sizeof bb);
		if (!bb.ncounts)
			continue;
		if (bb.ncounts > MAXBB) {
			fprintf(stderr,"found %lu counts above limit of %u\n",
				bb.ncounts, MAXBB);
			exit (1);
		}
		kvm_read(kv,bb.lineno,lineno, bb.ncounts * sizeof lineno[0]);
		kvm_read(kv,bb.counts,counts, bb.ncounts * sizeof counts[0]);
		kvm_read(kv,bb.addr,  addr,   bb.ncounts * sizeof addr[0]);
		kvm_read(kv,bb.file,  file,   bb.ncounts * sizeof file[0]);
		kvm_read(kv,bb.func,  func,   bb.ncounts * sizeof func[0]);
			if (!counts[i])
				continue;
		for (i=0; i < bb.ncounts; i++) {
			if (!pn[i] && func[i]) {
				kvm_read(kv,func[i], buf, sizeof buf);
				buf[sizeof buf -1] = 0;
				pn[i] = strdup(buf);
				for(j=i+1;j<bb.ncounts;j++)
					if (func[j] == func[i]) {
						pn[j] = pn[i];
						func[j] = 0;
					}
			}
			if (!pn[i])
				pn[i] = "-";
			if (!fn[i] && file[i]) {
				kvm_read(kv,file[i], buf, sizeof buf);
				buf[sizeof buf -1] = 0;
				fn[i] = strdup(buf);
				for(j=i+1;j<bb.ncounts;j++)
					if (file[j] == file[i]) {
						fn[j] = fn[i];
						file[j] = 0;
					}
			}
			if (!fn[i])
				fn[i] = "-";
			printf("%s %5lu %s %lu %lu\n",
				fn[i],lineno[i],pn[i],addr[i],counts[i]);
		}
		for(i=0;i<bb.ncounts;i++) {
			if (func[i] && pn[i]) 
				free(pn[i]);
			if (file[i] && fn[i])
				free(fn[i]);
			pn[i] = 0;
			fn[i] = 0;
		}
	}
	return 0;
}
