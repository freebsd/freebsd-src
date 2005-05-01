/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/diskpc98.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int iotest;

#define LBUF 100
static char lbuf[LBUF];

#ifndef PC98
#define MBRSIGOFF	510
#endif

/*
 *
 * Ported to 386bsd by Julian Elischer  Thu Oct 15 20:26:46 PDT 1992
 *
 * 14-Dec-89  Robert Baron (rvb) at Carnegie-Mellon University
 *	Copyright (c) 1989	Robert. V. Baron
 *	Created.
 */

#define Decimal(str, ans, tmp) if (decimal(str, &tmp, ans)) ans = tmp
#define String(str, ans, len) {char *z = ans; char **dflt = &z; if (string(str, dflt)) strncpy(ans, *dflt, len); }

#define RoundCyl(x) ((((x) + cylsecs - 1) / cylsecs) * cylsecs)

#define MAX_SEC_SIZE 2048	/* maximum section size that is supported */
#define MIN_SEC_SIZE 512	/* the sector size to start sensing at */
static int secsize = 0;		/* the sensed sector size */

static char *disk;

static int cyls, sectors, heads, cylsecs, disksecs;

#ifdef PC98
struct mboot {
	unsigned char padding[2]; /* force the longs to be long aligned */
	unsigned char bootinst[510];
	unsigned short int	signature;
	struct	pc98_partition parts[8];
	unsigned char large_sector_overflow[MAX_SEC_SIZE-MIN_SEC_SIZE];
};
#else /* PC98 */
struct mboot {
	unsigned char padding[2]; /* force the longs to be long aligned */
  	unsigned char *bootinst;  /* boot code */
  	off_t bootinst_size;
	struct	dos_partition parts[4];
};
#endif /* PC98 */

static struct mboot mboot;
static int fd, fdw;

#define ACTIVE 0x80
#define BOOT_MAGIC 0xAA55

static uint dos_cyls;
static uint dos_heads;
static uint dos_sectors;
static uint dos_cylsecs;

#ifndef PC98
#define DOSSECT(s,c) ((s & 0x3f) | ((c >> 2) & 0xc0))
#define DOSCYL(c)	(c & 0xff)
#endif

#define MAX_ARGS	10

static int	current_line_number;

typedef struct cmd {
    char		cmd;
    int			n_args;
    struct arg {
	char	argtype;
	int	arg_val;
    }			args[MAX_ARGS];
} CMD;

static int B_flag  = 0;		/* replace boot code */
#ifndef PC98
static int I_flag  = 0;		/* use entire disk for FreeBSD */
#endif
static int a_flag  = 0;		/* set active partition */
#ifndef PC98
static char *b_flag = NULL;	/* path to boot code */
#endif
static int i_flag  = 0;		/* replace partition data */
static int u_flag  = 0;		/* update partition data */
static int s_flag  = 0;		/* Print a summary and exit */
static int t_flag  = 0;		/* test only */
static char *f_flag = NULL;	/* Read config info from file */
static int v_flag  = 0;		/* Be verbose */

static struct part_type
{
	unsigned char type;
	const char *name;
} part_types[] = {
#ifdef PC98
	 {0x00, "unused"}
	,{0x01, "Primary DOS with 12 bit FAT"}
	,{0x11, "MSDOS"}
	,{0x20, "MSDOS"}
	,{0x21, "MSDOS"}
	,{0x22, "MSDOS"}
	,{0x23, "MSDOS"}
	,{0x02, "XENIX / file system"}  
	,{0x03, "XENIX /usr file system"}  
	,{0x04, "PC-UX"}   
	,{0x05, "Extended DOS"}   
	,{0x06, "Primary 'big' DOS (> 32MB)"}   
	,{0x07, "OS/2 HPFS, QNX or Advanced UNIX"}  
	,{0x08, "AIX file system"}   
	,{0x09, "AIX boot partition or Coherent"}  
	,{0x0A, "OS/2 Boot Manager or OPUS"}  
	,{0x10, "OPUS"} 
	,{0x14, "FreeBSD/NetBSD/386BSD"}  
	,{0x94, "FreeBSD/NetBSD/386BSD"}
	,{0x40, "VENIX 286"}  
	,{0x50, "DM"}   
	,{0x51, "DM"}   
	,{0x52, "CP/M or Microport SysV/AT"}
	,{0x56, "GB"}
	,{0x61, "Speed"}
	,{0x63, "ISC UNIX, other System V/386, GNU HURD or Mach"}
	,{0x64, "Novell Netware 2.xx"}
	,{0x65, "Novell Netware 3.xx"}
	,{0x75, "PCIX"}
	,{0x40, "Minix"} 
#endif
};

static void print_s0(int which);
static void print_part(int i);
static void init_sector0(unsigned long start);
static void init_boot(void);
static void change_part(int i);
static void print_params(void);
static void change_active(int which);
static void change_code(void);
static void get_params_to_use(void);
static char *get_rootdisk(void);
#ifdef PC98
static void dos(int sec, int size,
		unsigned short *c, unsigned char *s, unsigned char *h);
#else
static void dos(struct dos_partition *partp);
#endif
static int open_disk(int flag);
static ssize_t read_disk(off_t sector, void *buf);
static ssize_t write_disk(off_t sector, void *buf);
static int get_params(void);
static int read_s0(void);
static int write_s0(void);
static int ok(const char *str);
static int decimal(const char *str, int *num, int deflt);
static const char *get_type(int type);
static int read_config(char *config_file);
static void reset_boot(void);
#ifndef PC98
static int sanitize_partition(struct dos_partition *);
#endif
static void usage(void);
#ifdef PC98
static int string(char *str, char **ans);
#endif

int
main(int argc, char *argv[])
{
	struct	stat sb;
	int	c, i;
	int	partition = -1;
	struct	pc98_partition *partp;

#ifdef PC98
	while ((c = getopt(argc, argv, "Ba:f:istuv12345678")) != -1)
#else
	while ((c = getopt(argc, argv, "BIab:f:istuv1234")) != -1)
#endif
		switch (c) {
		case 'B':
			B_flag = 1;
			break;
#ifndef PC98
		case 'I':
			I_flag = 1;
			break;
#endif
		case 'a':
			a_flag = 1;
			break;
#ifndef PC98
		case 'b':
			b_flag = optarg;
			break;
#endif
		case 'f':
			f_flag = optarg;
			break;
		case 'i':
			i_flag = 1;
			break;
		case 's':
			s_flag = 1;
			break;
		case 't':
			t_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'v':
			v_flag = 1;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
#ifdef PC98
		case '5':
		case '6':
		case '7':
		case '8':
#endif
			partition = c - '0';
			break;
		default:
			usage();
		}
	if (f_flag || i_flag)
		u_flag = 1;
	if (t_flag)
		v_flag = 1;
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		disk = get_rootdisk();
	} else {
		if (stat(argv[0], &sb) == 0) {
			/* OK, full pathname given */
			disk = argv[0];
		} else if (errno == ENOENT && argv[0][0] != '/') {
			/* Try prepending "/dev" */
			asprintf(&disk, "%s%s", _PATH_DEV, argv[0]);
			if (disk == NULL)
				errx(1, "out of memory");
		} else {
			/* other stat error, let it fail below */
			disk = argv[0];
		}
	}
	if (open_disk(u_flag) < 0)
		err(1, "cannot open disk %s", disk);

#ifndef PC98
	/* (abu)use mboot.bootinst to probe for the sector size */
	if ((mboot.bootinst = malloc(MAX_SEC_SIZE)) == NULL)
		err(1, "cannot allocate buffer to determine disk sector size");
	read_disk(0, mboot.bootinst);
	free(mboot.bootinst);
	mboot.bootinst = NULL;
#endif

	if (s_flag) {
		if (read_s0())
			err(1, "read_s0");
		printf("%s: %d cyl %d hd %d sec\n", disk, dos_cyls, dos_heads,
		    dos_sectors);
#ifdef PC98
		printf("Part  %11s %11s SID\n", "Start", "Size");
		for (i = 0; i < NDOSPART; i++) {
			partp = ((struct pc98_partition *) &mboot.parts) + i;
			if (partp->dp_sid == 0)
				continue;
			printf("%4d: %11lu %11lu 0x%02x\n", i + 1,
			    partp->dp_scyl * cylsecs,
			    (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs,
				partp->dp_sid);
#else
		printf("Part  %11s %11s Type Flags\n", "Start", "Size");
		for (i = 0; i < NDOSPART; i++) {
			partp = ((struct dos_partition *) &mboot.parts) + i;
			if (partp->dp_start == 0 && partp->dp_size == 0)
				continue;
			printf("%4d: %11lu %11lu 0x%02x 0x%02x\n", i + 1,
			    (u_long) partp->dp_start,
			    (u_long) partp->dp_size, partp->dp_typ,
			    partp->dp_flag);
#endif
		}
		exit(0);
	}

	printf("******* Working on device %s *******\n",disk);

#ifndef PC98
	if (I_flag) {
		read_s0();
		reset_boot();
		partp = (struct dos_partition *) (&mboot.parts[0]);
		partp->dp_typ = DOSPTYP_386BSD;
		partp->dp_flag = ACTIVE;
		partp->dp_start = dos_sectors;
		partp->dp_size = (disksecs / dos_cylsecs) * dos_cylsecs -
		    dos_sectors;
		dos(partp);
		if (v_flag)
			print_s0(-1);
		if (!t_flag)
			write_s0();
		exit(0);
	}
#endif
	if (f_flag) {
#ifndef PC98
	    if (read_s0() || i_flag)
		reset_boot();
	    if (!read_config(f_flag))
		exit(1);
#endif
	    if (v_flag)
		print_s0(-1);
	    if (!t_flag)
		write_s0();
	} else {
	    if(u_flag)
		get_params_to_use();
	    else
		print_params();

	    if (read_s0())
		init_sector0(dos_sectors);

	    printf("Media sector size is %d\n", secsize);
	    printf("Warning: BIOS sector numbering starts with sector 1\n");
	    printf("Information from DOS bootblock is:\n");
	    if (partition == -1)
		for (i = 1; i <= NDOSPART; i++)
		    change_part(i);
	    else
		change_part(partition);

	    if (u_flag || a_flag)
		change_active(partition);

	    if (B_flag)
		change_code();

	    if (u_flag || a_flag || B_flag) {
		if (!t_flag) {
		    printf("\nWe haven't changed the partition table yet.  ");
		    printf("This is your last chance.\n");
		}
		print_s0(-1);
		if (!t_flag) {
		    if (ok("Should we write new partition table?"))
			write_s0();
		} else {
		    printf("\n-t flag specified -- partition table not written.\n");
		}
	    }
	}

	exit(0);
}

static void
usage()
{
	fprintf(stderr, "%s%s",
		"usage: fdisk [-Baistu] [-12345678] [disk]\n",
 		"       fdisk -f configfile [-itv] [disk]\n");
        exit(1);
}

static void
print_s0(int which)
{
	int	i;

	print_params();
	printf("Information from DOS bootblock is:\n");
	if (which == -1)
		for (i = 1; i <= NDOSPART; i++)
			printf("%d: ", i), print_part(i);
	else
		print_part(which);
}

static struct pc98_partition mtpart;

static void
print_part(int i)
{
	struct	  pc98_partition *partp;
	u_int64_t part_sz, part_mb;

	partp = ((struct pc98_partition *) &mboot.parts) + i - 1;

	if (!bcmp(partp, &mtpart, sizeof (struct pc98_partition))) {
		printf("<UNUSED>\n");
		return;
	}
	/*
	 * Be careful not to overflow.
	 */
#ifdef PC98
	part_sz = (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs;
	part_mb = part_sz * secsize;
	part_mb /= (1024 * 1024);
	printf("sysmid %d (%#04x),(%s)\n", partp->dp_mid, partp->dp_mid,
	    get_type(partp->dp_mid));
	printf("    start %lu, size %lu (%ju Meg), sid %d\n",
		(u_long)(partp->dp_scyl * cylsecs), (u_long)part_sz,
		(uintmax_t)part_mb, partp->dp_sid);
#else
	part_mb = partp->dp_size;
	part_mb *= secsize;
	part_mb /= (1024 * 1024);
	printf("sysid %d (%#04x),(%s)\n", partp->dp_typ, partp->dp_typ,
	    get_type(partp->dp_typ));
	printf("    start %lu, size %lu (%ju Meg), flag %x%s\n",
		(u_long)partp->dp_start,
		(u_long)partp->dp_size, 
		(uintmax_t)part_mb,
		partp->dp_flag,
		partp->dp_flag == ACTIVE ? " (active)" : "");
#endif
	printf("\tbeg: cyl %d/ head %d/ sector %d;\n\tend: cyl %d/ head %d/ sector %d\n"
		,partp->dp_scyl
		,partp->dp_shd
		,partp->dp_ssect
		,partp->dp_ecyl
		,partp->dp_ehd
		,partp->dp_esect);
#ifdef PC98
	printf ("\tsystem Name %.16s\n",partp->dp_name);
#endif
}


static void
init_boot(void)
{
#ifdef PC98
	mboot.signature = BOOT_MAGIC;
#else
	const char *fname;
	int fdesc, n;
	struct stat sb;

	fname = b_flag ? b_flag : "/boot/mbr";
	if ((fdesc = open(fname, O_RDONLY)) == -1 ||
	    fstat(fdesc, &sb) == -1)
		err(1, "%s", fname);
	if ((mboot.bootinst_size = sb.st_size) % secsize != 0)
		errx(1, "%s: length must be a multiple of sector size", fname);
	if (mboot.bootinst != NULL)
		free(mboot.bootinst);
	if ((mboot.bootinst = malloc(mboot.bootinst_size = sb.st_size)) == NULL)
		errx(1, "%s: unable to allocate read buffer", fname);
	if ((n = read(fdesc, mboot.bootinst, mboot.bootinst_size)) == -1 ||
	    close(fdesc))
		err(1, "%s", fname);
	if (n != mboot.bootinst_size)
		errx(1, "%s: short read", fname);
#endif
}


static void
init_sector0(unsigned long start)
{
#ifdef PC98
	struct pc98_partition *partp =
		(struct pc98_partition *)(&mboot.parts[3]);
	unsigned long size = disksecs - start;

	init_boot();

	partp->dp_mid = DOSMID_386BSD;
	partp->dp_sid = DOSSID_386BSD;

	dos(start, size, &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
	partp->dp_ipl_cyl = partp->dp_scyl;
	partp->dp_ipl_sct = partp->dp_ssect;
	partp->dp_ipl_head = partp->dp_shd;
	dos(start+size-cylsecs, size,
	    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
#else
	struct dos_partition *partp = (struct dos_partition *) (&mboot.parts[3]);

	init_boot();

	partp->dp_typ = DOSPTYP_386BSD;
	partp->dp_flag = ACTIVE;
	start = ((start + dos_sectors - 1) / dos_sectors) * dos_sectors;
	if(start == 0)
		start = dos_sectors;
	partp->dp_start = start;
	partp->dp_size = (disksecs / dos_cylsecs) * dos_cylsecs - start;

	dos(partp);
#endif
}

static void
change_part(int i)
{
	struct pc98_partition *partp =
		((struct pc98_partition *) &mboot.parts) + i - 1;

    printf("The data for partition %d is:\n", i);
    print_part(i);

    if (u_flag && ok("Do you want to change it?")) {
	int tmp;

	if (i_flag) {
		bzero((char *)partp, sizeof (struct pc98_partition));
		if (i == 4) {
			init_sector0(1);
			printf("\nThe static data for the DOS partition 4 has been reinitialized to:\n");
			print_part(i);
		}
	}

	do {
#ifdef PC98
		int x_start = partp->dp_scyl * cylsecs ;
		int x_size  = (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs;
		Decimal("sysmid", partp->dp_mid, tmp);
		Decimal("syssid", partp->dp_sid, tmp);
		String ("system name", partp->dp_name, 16);
		Decimal("start", x_start, tmp);
		Decimal("size", x_size, tmp);
#else
		Decimal("sysid (165=FreeBSD)", partp->dp_typ, tmp);
		Decimal("start", partp->dp_start, tmp);
		Decimal("size", partp->dp_size, tmp);
		if (!sanitize_partition(partp)) {
			warnx("ERROR: failed to adjust; setting sysid to 0");
			partp->dp_typ = 0;
		}
#endif

		if (ok("Explicitly specify beg/end address ?"))
		{
			int	tsec,tcyl,thd;
			tcyl = partp->dp_scyl;
			thd = partp->dp_shd;
			tsec = partp->dp_ssect;
			Decimal("beginning cylinder", tcyl, tmp);
			Decimal("beginning head", thd, tmp);
			Decimal("beginning sector", tsec, tmp);
#ifdef PC98
			partp->dp_scyl = tcyl;
			partp->dp_ssect = tsec;
			partp->dp_shd = thd;
			partp->dp_ipl_cyl = partp->dp_scyl;
			partp->dp_ipl_sct = partp->dp_ssect;
			partp->dp_ipl_head = partp->dp_shd;
#else
			partp->dp_scyl = DOSCYL(tcyl);
			partp->dp_ssect = DOSSECT(tsec,tcyl);
			partp->dp_shd = thd;
#endif

			tcyl = partp->dp_ecyl;
			thd = partp->dp_ehd;
			tsec = partp->dp_esect;
			Decimal("ending cylinder", tcyl, tmp);
			Decimal("ending head", thd, tmp);
			Decimal("ending sector", tsec, tmp);
#ifdef PC98
			partp->dp_ecyl = tcyl;
			partp->dp_esect = tsec;
			partp->dp_ehd = thd;
#else
			partp->dp_ecyl = DOSCYL(tcyl);
			partp->dp_esect = DOSSECT(tsec,tcyl);
			partp->dp_ehd = thd;
#endif
#ifdef PC98
		} else {
			dos(x_start, x_size, &partp->dp_scyl,
			    &partp->dp_ssect, &partp->dp_shd);
			partp->dp_ipl_cyl = partp->dp_scyl;
			partp->dp_ipl_sct = partp->dp_ssect;
			partp->dp_ipl_head = partp->dp_shd;
			dos(x_start+x_size - cylsecs, x_size, &partp->dp_ecyl,
			    &partp->dp_esect, &partp->dp_ehd);
		}
#else
		} else
			dos(partp);
#endif

		print_part(i);
	} while (!ok("Are we happy with this entry?"));
    }
}

static void
print_params()
{
	printf("parameters extracted from in-core disklabel are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d blks/cyl)\n\n"
			,cyls,heads,sectors,cylsecs);
#ifndef PC98
	if((dos_sectors > 63) || (dos_cyls > 1023) || (dos_heads > 255))
		printf("Figures below won't work with BIOS for partitions not in cyl 1\n");
#endif
	printf("parameters to be used for BIOS calculations are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d blks/cyl)\n\n"
		,dos_cyls,dos_heads,dos_sectors,dos_cylsecs);
}

static void
change_active(int which)
{
#ifdef PC98
	struct pc98_partition *partp = ((struct pc98_partition *) &mboot.parts);
	int active, i, tmp;

	active = 8;
#else
	struct dos_partition *partp = &mboot.parts[0];
	int active, i, new, tmp;

	active = -1;
	for (i = 0; i < NDOSPART; i++) {
		if ((partp[i].dp_flag & ACTIVE) == 0)
			continue;
		printf("Partition %d is marked active\n", i + 1);
		if (active == -1)
			active = i + 1;
	}
#endif
	if (a_flag && which != -1)
		active = which;
#ifndef PC98
	else if (active == -1)
		active = 1;
#endif

	if (!ok("Do you want to change the active partition?"))
		return;
setactive:
#ifdef PC98
	active = 4;
	do {
		Decimal("active partition", active, tmp);
		if (active < 1 || 8 < active) {
			printf("Active partition number must be in range 1-8."
					"  Try again.\n");
			goto setactive;
		}
	} while (!ok("Are you happy with this choice"));
	partp[active].dp_sid |= ACTIVE;
#else
	do {
		new = active;
		Decimal("active partition", new, tmp);
		if (new < 1 || new > 4) {
			printf("Active partition number must be in range 1-4."
					"  Try again.\n");
			goto setactive;
		}
		active = new;
	} while (!ok("Are you happy with this choice"));
	for (i = 0; i < NDOSPART; i++)
		partp[i].dp_flag = 0;
	if (active > 0 && active <= NDOSPART)
		partp[active-1].dp_flag = ACTIVE;
#endif
}

static void
change_code()
{
	if (ok("Do you want to change the boot code?"))
		init_boot();
}

void
get_params_to_use()
{
	int	tmp;
	print_params();
	if (ok("Do you want to change our idea of what BIOS thinks ?"))
	{
		do
		{
			Decimal("BIOS's idea of #cylinders", dos_cyls, tmp);
			Decimal("BIOS's idea of #heads", dos_heads, tmp);
			Decimal("BIOS's idea of #sectors", dos_sectors, tmp);
			dos_cylsecs = dos_heads * dos_sectors;
			print_params();
		}
		while(!ok("Are you happy with this choice"));
	}
}


/***********************************************\
* Change real numbers into strange dos numbers	*
\***********************************************/
#ifdef PC98
static void
dos(int sec, int size, unsigned short *c, unsigned char *s, unsigned char *h)
{
	int cy, hd;

	if (sec == 0 && size == 0) {
		*s = *c = *h = 0;
		return;
	}

	cy = sec / ( dos_cylsecs );
	sec = sec - cy * ( dos_cylsecs );

	hd = sec / dos_sectors;
	sec = (sec - hd * dos_sectors);

	*h = hd;
	*c = cy;
	*s = sec;
}
#else
static void
dos(struct dos_partition *partp)
{
	int cy, sec;
	u_int32_t end;

	if (partp->dp_typ == 0 && partp->dp_start == 0 && partp->dp_size == 0) {
		memcpy(partp, &mtpart, sizeof(*partp));
		return;
	}

	/* Start c/h/s. */
	partp->dp_shd = partp->dp_start % dos_cylsecs / dos_sectors;
	cy = partp->dp_start / dos_cylsecs;
	sec = partp->dp_start % dos_sectors + 1;
	partp->dp_scyl = DOSCYL(cy);
	partp->dp_ssect = DOSSECT(sec, cy);

	/* End c/h/s. */
	end = partp->dp_start + partp->dp_size - 1;
	partp->dp_ehd = end % dos_cylsecs / dos_sectors;
	cy = end / dos_cylsecs;
	sec = end % dos_sectors + 1;
	partp->dp_ecyl = DOSCYL(cy);
	partp->dp_esect = DOSSECT(sec, cy);
}
#endif

static int
open_disk(int flag)
{
	struct stat 	st;
	int rwmode, p;
	char *s;

	fdw = -1;
	if (stat(disk, &st) == -1) {
		if (errno == ENOENT)
			return -2;
		warnx("can't get file status of %s", disk);
		return -1;
	}
	if ( !(st.st_mode & S_IFCHR) )
		warnx("device %s is not character special", disk);
#ifdef PC98
	rwmode = a_flag || B_flag || flag ? O_RDWR : O_RDONLY;
#else
	rwmode = a_flag || I_flag || B_flag || flag ? O_RDWR : O_RDONLY;
#endif
	fd = open(disk, rwmode);
	if (fd == -1 && errno == ENXIO)
		return -2;
	if (fd == -1 && errno == EPERM && rwmode == O_RDWR) {
		fd = open(disk, O_RDONLY);
		if (fd == -1)
			return -3;
		for (p = 0; p < NDOSPART; p++) {
			asprintf(&s, "%ss%d", disk, p + 1);
			fdw = open(s, rwmode);
			free(s);
			if (fdw == -1)
				continue;
			break;
		}
		if (fdw == -1)
			return -4;
	}
	if (fd == -1) {
		warnx("can't open device %s", disk);
		return -1;
	}
	if (get_params() == -1) {
		warnx("can't get disk parameters on %s", disk);
		return -1;
	}
	return fd;
}

static ssize_t
read_disk(off_t sector, void *buf)
{
	lseek(fd,(sector * 512), 0);
#ifdef PC98
	return read(fd, buf,
		    secsize > MIN_SEC_SIZE ? secsize : MIN_SEC_SIZE * 2);
#else
	if( secsize == 0 )
		for( secsize = MIN_SEC_SIZE; secsize <= MAX_SEC_SIZE; secsize *= 2 )
			{
			/* try the read */
			int size = read(fd, buf, secsize);
			if( size == secsize )
				/* it worked so return */
				return secsize;
			}
	else
		return read( fd, buf, secsize );

	/* we failed to read at any of the sizes */
	return -1;
#endif
}

static ssize_t
write_disk(off_t sector, void *buf)
{

#ifdef PC98
	if (fdw != -1) {
		return ioctl(fdw, DIOCSPC98, buf);
	} else {
		lseek(fd,(sector * 512), 0);
		/* write out in the size that the read_disk found worked */
		return write(fd, buf,
		     secsize > MIN_SEC_SIZE ? secsize : MIN_SEC_SIZE * 2);
	}
#else
	if (fdw != -1) {
		return ioctl(fdw, DIOCSMBR, buf);
	} else {
		lseek(fd,(sector * 512), 0);
		/* write out in the size that the read_disk found worked */
		return write(fd, buf, secsize);
	}
#endif
}

static int
get_params()
{
	int error;
	u_int u;
	off_t o;

	error = ioctl(fd, DIOCGFWSECTORS, &u);
	if (error == 0)
		sectors = dos_sectors = u;
	else
		sectors = dos_sectors = 63;

	error = ioctl(fd, DIOCGFWHEADS, &u);
	if (error == 0)
		heads = dos_heads = u;
	else
		heads = dos_heads = 255;

	dos_cylsecs = cylsecs = heads * sectors;
	disksecs = cyls * heads * sectors;

	error = ioctl(fd, DIOCGSECTORSIZE, &u);
	if (error != 0 || u == 0)
		u = 512;
#ifdef PC98
	secsize = u;
#endif

	error = ioctl(fd, DIOCGMEDIASIZE, &o);
	if (error == 0) {
		disksecs = o / u;
		cyls = dos_cyls = o / (u * dos_heads * dos_sectors);
	}

	return (disksecs);
}


static int
read_s0()
{
#ifdef PC98
	if (read_disk(0, (char *) mboot.bootinst) == -1) {
		warnx("can't read fdisk partition table");
		return -1;
	}
	if (mboot.signature != BOOT_MAGIC) {
		warnx("invalid fdisk partition table found");
		/* So should we initialize things */
		return -1;
	}
#else
	mboot.bootinst_size = secsize;
	if (mboot.bootinst != NULL)
		free(mboot.bootinst);
	if ((mboot.bootinst = malloc(mboot.bootinst_size)) == NULL) {
		warnx("unable to allocate buffer to read fdisk "
		      "partition table");
		return -1;
	}
	if (read_disk(0, mboot.bootinst) == -1) {
		warnx("can't read fdisk partition table");
		return -1;
	}
	if (*(uint16_t *)(void *)&mboot.bootinst[MBRSIGOFF] != BOOT_MAGIC) {
		warnx("invalid fdisk partition table found");
		/* So should we initialize things */
		return -1;
	}
	memcpy(mboot.parts, &mboot.bootinst[DOSPARTOFF], sizeof(mboot.parts));
#endif
	return 0;
}

static int
write_s0()
{
#ifndef PC98
	int	sector;
#endif

	if (iotest) {
		print_s0(-1);
		return 0;
	}
#ifndef PC98
	memcpy(&mboot.bootinst[DOSPARTOFF], mboot.parts, sizeof(mboot.parts));
#endif
	/*
	 * write enable label sector before write (if necessary),
	 * disable after writing.
	 * needed if the disklabel protected area also protects
	 * sector 0. (e.g. empty disk)
	 */
#ifdef PC98
	if (write_disk(0, (char *) mboot.bootinst) == -1) {
		warn("can't write fdisk partition table");
		return -1;
	}
#else
	for(sector = 0; sector < mboot.bootinst_size / secsize; sector++) 
		if (write_disk(sector,
			       &mboot.bootinst[sector * secsize]) == -1) {
			warn("can't write fdisk partition table");
			return -1;
		}
#endif
	return(0);
}


static int
ok(const char *str)
{
	printf("%s [n] ", str);
	fflush(stdout);
	if (fgets(lbuf, LBUF, stdin) == NULL)
		exit(1);
	lbuf[strlen(lbuf)-1] = 0;

	if (*lbuf &&
		(!strcmp(lbuf, "yes") || !strcmp(lbuf, "YES") ||
		 !strcmp(lbuf, "y") || !strcmp(lbuf, "Y")))
		return 1;
	else
		return 0;
}

static int
decimal(const char *str, int *num, int deflt)
{
	int acc = 0, c;
	char *cp;

	while (1) {
		printf("Supply a decimal value for \"%s\" [%d] ", str, deflt);
		fflush(stdout);
		if (fgets(lbuf, LBUF, stdin) == NULL)
			exit(1);
		lbuf[strlen(lbuf)-1] = 0;

		if (!*lbuf)
			return 0;

		cp = lbuf;
		while ((c = *cp) && (c == ' ' || c == '\t')) cp++;
		if (!c)
			return 0;
		while ((c = *cp++)) {
			if (c <= '9' && c >= '0')
				acc = acc * 10 + c - '0';
			else
				break;
		}
		if (c == ' ' || c == '\t')
			while ((c = *cp) && (c == ' ' || c == '\t')) cp++;
		if (!c) {
			*num = acc;
			return 1;
		} else
			printf("%s is an invalid decimal number.  Try again.\n",
				lbuf);
	}

}

#ifdef PC98
static int
string(char *str, char **ans)
{
	int i, c;
	char *cp = lbuf;

	while (1) {
		printf("Supply a string value for \"%s\" [%s] ", str, *ans);
		fgets(lbuf, LBUF, stdin);
		lbuf[strlen(lbuf)-1] = 0;

		if (!*lbuf)
			return 0;

		while ((c = *cp) && (c == ' ' || c == '\t')) cp++;
		if (c == '"') {
			c = *++cp;
			*ans = cp;
			while ((c = *cp) && c != '"') cp++;
		} else {
			*ans = cp;
			while ((c = *cp) && c != ' ' && c != '\t') cp++;
		}

		for (i = strlen(*ans); i < 16; i++)
			(*ans)[i] = ' ';
		(*ans)[16] = 0;

		return 1;
	}
}
#endif

static const char *
get_type(int type)
{
	int	numentries = (sizeof(part_types)/sizeof(struct part_type));
	int	counter = 0;
	struct	part_type *ptr = part_types;


	while(counter < numentries) {
#ifdef PC98
		if(ptr->type == (type & 0x7f))
			return(ptr->name);
#else
		if(ptr->type == type)
			return(ptr->name);
#endif
		ptr++;
		counter++;
	}
	return("unknown");
}

#ifndef PC98
static int
sanitize_partition(struct dos_partition *partp)
{
    u_int32_t			prev_head_boundary, prev_cyl_boundary;
    u_int32_t			max_end, size, start;

    start = partp->dp_start;
    size = partp->dp_size;
    max_end = start + size;
    /* Only allow a zero size if the partition is being marked unused. */
    if (size == 0) {
	if (start == 0 && partp->dp_typ == 0)
	    return (1);
	warnx("ERROR: size of partition is zero");
	return (0);
    }
    /* Return if no adjustment is necessary. */
    if (start % dos_sectors == 0 && (start + size) % dos_sectors == 0)
	return (1);

    if (start % dos_sectors != 0)
	warnx("WARNING: partition does not start on a head boundary");
    if ((start  +size) % dos_sectors != 0)
	warnx("WARNING: partition does not end on a cylinder boundary");
    warnx("WARNING: this may confuse the BIOS or some operating systems");
    if (!ok("Correct this automatically?"))
	return (1);

    /*
     * Adjust start upwards, if necessary, to fall on a head boundary.
     */
    if (start % dos_sectors != 0) {
	prev_head_boundary = start / dos_sectors * dos_sectors;
	if (max_end < dos_sectors ||
	    prev_head_boundary >= max_end - dos_sectors) {
	    /*
	     * Can't go past end of partition
	     */
	    warnx(
    "ERROR: unable to adjust start of partition to fall on a head boundary");
	    return (0);
        }
	start = prev_head_boundary + dos_sectors;
    }

    /*
     * Adjust size downwards, if necessary, to fall on a cylinder
     * boundary.
     */
    prev_cyl_boundary = ((start + size) / dos_cylsecs) * dos_cylsecs;
    if (prev_cyl_boundary > start)
	size = prev_cyl_boundary - start;
    else {
	warnx("ERROR: could not adjust partition to start on a head boundary\n\
    and end on a cylinder boundary.");
	return (0);
    }

    /* Finally, commit any changes to partp and return. */
    if (start != partp->dp_start) {
	warnx("WARNING: adjusting start offset of partition to %u",
	    (u_int)start);
	partp->dp_start = start;
    }
    if (size != partp->dp_size) {
	warnx("WARNING: adjusting size of partition to %u", (u_int)size);
	partp->dp_size = size;
    }

    return (1);
}
#endif /* PC98 */

/*
 * Try figuring out the root device's canonical disk name.
 * The following choices are considered:
 *   /dev/ad0s1a     => /dev/ad0
 *   /dev/da0a       => /dev/da0
 *   /dev/vinum/root => /dev/vinum/root
 */
static char *
get_rootdisk(void)
{
	struct statfs rootfs;
	regex_t re;
#define NMATCHES 2
	regmatch_t rm[NMATCHES];
	char *s;
	int rv;

	if (statfs("/", &rootfs) == -1)
		err(1, "statfs(\"/\")");

	if ((rv = regcomp(&re, "^(/dev/[a-z]+[0-9]+)([sp][0-9]+)?[a-h]?$",
		    REG_EXTENDED)) != 0)
		errx(1, "regcomp() failed (%d)", rv);
	if ((rv = regexec(&re, rootfs.f_mntfromname, NMATCHES, rm, 0)) != 0)
		errx(1,
"mounted root fs resource doesn't match expectations (regexec returned %d)",
		    rv);
	if ((s = malloc(rm[1].rm_eo - rm[1].rm_so + 1)) == NULL)
		errx(1, "out of memory");
	memcpy(s, rootfs.f_mntfromname + rm[1].rm_so,
	    rm[1].rm_eo - rm[1].rm_so);
	s[rm[1].rm_eo - rm[1].rm_so] = 0;

	return s;
}
