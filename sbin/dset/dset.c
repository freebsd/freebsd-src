/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Device configuration to kernel image saving utility.
 */

#include <stdio.h>
#include <nlist.h>
#include <paths.h>
#include <unistd.h>
#include <fcntl.h>
#include <a.out.h>
#include <kvm.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <machine/param.h>
#include "i386/isa/isa_device.h"

#define TRUE	1
#define FALSE 	0

extern int      errno;

struct nlist    nl[] = {
#define N_TABTTY 	0
	{"_isa_devtab_tty"},
#define N_TABBIO	1
	{"_isa_devtab_bio"},
#define N_TABNET	2
	{"_isa_devtab_net"},
#define N_TABNULL	3
	{"_isa_devtab_null"},
	"",
};
#define N_TABLAST	N_TABNULL

struct nlist    nlk[] = {
	{"_isa_devlist"},
	"",
};

int             quiet = FALSE;

void
fatal(name, str)
	char           *name, *str;
{
	if (quiet)
		exit(1);
	if (str)
		fprintf(stderr, "%s : %s\n", name, str);
	else
		perror(name);
	exit(1);
}

void
error(name, str)
	char           *name, *str;
{
	if (quiet)
		return;
	if (str)
		fprintf(stderr, "%s : %s\n", name, str);
	else
		perror(name);
}

void
usage(char *title)
{
	fprintf(stderr, "usage: %s [-qtv]\n", title);
}

main(ac, av)
	int             ac;
	char          **av;
{
	int             f, res, s;
	int             modified,dev_found;
	int             sym;
	u_long          pos, entry, pos1, pos_t;
	u_long          flags;
	struct isa_device buf, buf1;
	struct isa_driver dbuf;
	char            nbuf[5];
	struct stat     fst;
	struct exec     es;
	kvm_t          *kd;
	static char     errb[_POSIX2_LINE_MAX];
	const char     *kernel;

	extern char    *optarg;
	char            ch;
	int             testonly = FALSE;
	int             verbose = FALSE;

	while ((ch = getopt(ac, av, "qtv")) != EOF)
		switch (ch) {
		case 'q':
			quiet = TRUE;
			break;
		case 't':
			testonly = TRUE;
			/* In test mode we want to be verbose */
		case 'v':
			verbose = TRUE;
			break;
		case '?':
		default:
			usage(av[0]);
			exit(1);
		}


	kernel = getbootfile();
	if (verbose)
		printf("Boot image: %s\n", kernel);

	if (!(kd = kvm_open(NULL, NULL, NULL, O_RDONLY, errb)))
		fatal("kvm_open", NULL);

	if (kvm_nlist(kd, nlk) != 0)
		fatal("kvm_nlist", NULL);

	if (nlk[0].n_type == 0)
		fatal("kvm_nlist", "bad symbol type");

	if (nlist(kernel, nl) != 0)
		fatal("nlist", NULL);

	if (nl[0].n_type == 0)
		fatal("nlist", "bad symbol type");

	if (stat(kernel, &fst) < 0)
		fatal("stat", NULL);

	flags = fst.st_flags;

	if (chflags(kernel, (u_long) 0) < 0)
		fatal("chflags", NULL);

	if ((f = open(kernel, O_RDWR)) <= 0)
		fatal("open", NULL);

	if (read(f, &es, sizeof(struct exec)) <= 0)
		fatal("read header", NULL);

	entry = es.a_entry;

	for (sym = 0; sym <= N_TABLAST; sym++) {
		if (verbose)
			printf("\nTable: %s\n", nl[sym].n_name);
		pos = nl[sym].n_value + getpagesize() - entry;

		pos1 = nlk[0].n_value;

		if (lseek(f, pos, SEEK_SET) != pos)
			fatal("seek", NULL);

		if (verbose)
			printf("----------------------------------------------------\n");

		do {
			if ((res = read(f, (char *) &buf, sizeof(struct isa_device)))
			    <= 0)
				fatal("read", NULL);



		if (kvm_read(kd, pos1, &pos_t, sizeof(u_long)) < 0)
			fatal("kvmread", NULL);
		dev_found = 0;

		while(pos_t!=NULL) {
			if (kvm_read(kd, pos_t, &buf1, sizeof(struct isa_device)) < 0)
				fatal("kvmread", NULL);

			if (buf1.id_id != buf.id_id) {
				pos_t = (u_long)(buf1.id_next);
				continue;
			} else
				dev_found=1;

			if (buf1.id_driver)
				if (kvm_read(kd, (u_long) buf1.id_driver,
					     &dbuf, sizeof(struct isa_driver)) < 0) {
					error("kvm_read", "no driver");
				} else {
					if (kvm_read(kd, (u_long) dbuf.name,
					  nbuf, sizeof(nbuf)) < 0) {
						error("kvm_read", NULL);
					} else {
						nbuf[sizeof(nbuf) - 1] = 0;
						if (verbose)
							printf("Device: %s%d\n", nbuf, buf1.id_unit);
					}
				}
			else
				error("kvm_read", "no driver");
			break;

		};

		if (!dev_found)
			continue;

			if (buf1.id_id != 0)
				if (verbose)
					printf(
  "kernel: id=%u io=%X irq=%d drq=%d maddr=%X msize=%d flags=%X enabled=%X \n",
	buf1.id_id, buf1.id_iobase, buf1.id_irq, buf1.id_drq,
	buf1.id_maddr, buf1.id_msize, buf1.id_flags, buf1.id_enabled);

			if (buf.id_id != 0)
				if (verbose)
					printf(
  "file: id=%u io=%X irq=%d drq=%d maddr=%X msize=%d flags=%X enabled=%X \n",
	buf.id_id, buf.id_iobase, buf.id_irq, buf.id_drq,
	buf.id_maddr, buf.id_msize, buf.id_flags, buf.id_enabled);


			/*
			 * OK,now we'd compare values and set'em from kernel.
			 */
			modified = FALSE;

			if (buf.id_iobase != -1 && buf.id_iobase !=
			    buf1.id_iobase) {
				if (verbose)
					printf("Setting IO addr\n");
				buf.id_iobase = buf1.id_iobase;
				modified = TRUE;
			}
			if (buf.id_irq != (u_short)-1 && buf.id_irq != buf1.id_irq) {
				if (verbose)
					printf("Setting IRQ\n");
				buf.id_irq = buf1.id_irq;
				modified = TRUE;
			}
			if (buf.id_drq != -1 && buf.id_drq != buf1.id_drq) {
				if (verbose)
					printf("Setting DRQ\n");
				buf.id_drq = buf1.id_drq;
				modified = TRUE;
			}
			if (buf.id_maddr != (caddr_t)-1 && buf.id_maddr != buf1.id_maddr) {
				if (verbose)
					printf("Setting memory addres\n");
				buf.id_maddr = buf1.id_maddr;
				modified = TRUE;
			}
			if (buf.id_msize != buf1.id_msize) {
				if (verbose)
					printf("Setting msize\n");
				buf.id_msize = buf1.id_msize;
				modified = TRUE;
			}
			if (buf.id_flags != buf1.id_flags) {
				if (verbose)
					printf("Setting flags\n");
				buf.id_flags = buf1.id_flags;
				modified = TRUE;
			}
			if (buf.id_enabled != buf1.id_enabled) {
				if (verbose)
					printf("Setting device enable/disable\n");
				buf.id_enabled = buf1.id_enabled;
				modified = TRUE;
			}
			if (verbose)
				printf("----------------------------------------------------\n");
			if (modified && !testonly) {

				res = lseek(f, -(off_t) sizeof(struct isa_device),
					    SEEK_CUR);
				if (write(f, &buf, sizeof(struct isa_device)) <= 0)
					fatal("write", NULL);

			}
		} while (buf.id_id != 0 && buf1.id_id != 0);
	}

	if (chflags(kernel, flags) < 0)
		fatal("chflags restore", NULL);

	kvm_close(kd);
	close(f);
}
