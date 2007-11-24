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
#include <libgeom.h>
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

struct mboot {
	unsigned char padding[2]; /* force the longs to be long aligned */
	unsigned char bootinst[510];
	unsigned short int	signature;
	struct	pc98_partition parts[8];
	unsigned char large_sector_overflow[MAX_SEC_SIZE-MIN_SEC_SIZE];
};

static struct mboot mboot;
static int fd;

#define ACTIVE 0x80

static uint dos_cyls;
static uint dos_heads;
static uint dos_sectors;
static uint dos_cylsecs;

#define MAX_ARGS	10

typedef struct cmd {
    char		cmd;
    int			n_args;
    struct arg {
	char	argtype;
	int	arg_val;
    }			args[MAX_ARGS];
} CMD;

static int B_flag  = 0;		/* replace boot code */
static int a_flag  = 0;		/* set active partition */
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
static void dos(u_int32_t start, u_int32_t size, struct pc98_partition *partp);
static int open_disk(int flag);
static ssize_t read_disk(off_t sector, void *buf);
static int write_disk(off_t sector, void *buf);
static int get_params(void);
static int read_s0(void);
static int write_s0(void);
static int ok(const char *str);
static int decimal(const char *str, int *num, int deflt);
static const char *get_type(int type);
static void usage(void);
static int string(const char *str, char **ans);

int
main(int argc, char *argv[])
{
	struct	stat sb;
	int	c, i;
	int	partition = -1;
	struct	pc98_partition *partp;

	while ((c = getopt(argc, argv, "Ba:f:istuv12345678")) != -1)
		switch (c) {
		case 'B':
			B_flag = 1;
			break;
		case 'a':
			a_flag = 1;
			break;
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
		case '5':
		case '6':
		case '7':
		case '8':
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

	if (s_flag) {
		if (read_s0())
			err(1, "read_s0");
		printf("%s: %d cyl %d hd %d sec\n", disk, dos_cyls, dos_heads,
		    dos_sectors);
		printf("Part  %11s %11s SID\n", "Start", "Size");
		for (i = 0; i < NDOSPART; i++) {
			partp = ((struct pc98_partition *) &mboot.parts) + i;
			if (partp->dp_sid == 0)
				continue;
			printf("%4d: %11u %11u 0x%02x\n", i + 1,
			    partp->dp_scyl * cylsecs,
			    (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs,
				partp->dp_sid);
		}
		exit(0);
	}

	printf("******* Working on device %s *******\n",disk);

	if (f_flag) {
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
	part_sz = (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs;
	part_mb = part_sz * secsize;
	part_mb /= (1024 * 1024);
	printf("sysmid %d (%#04x),(%s)\n", partp->dp_mid, partp->dp_mid,
	    get_type(partp->dp_mid));
	printf("    start %lu, size %lu (%ju Meg), sid %d\n",
		(u_long)(partp->dp_scyl * cylsecs), (u_long)part_sz,
		(uintmax_t)part_mb, partp->dp_sid);
	printf("\tbeg: cyl %d/ head %d/ sector %d;\n\tend: cyl %d/ head %d/ sector %d\n"
		,partp->dp_scyl
		,partp->dp_shd
		,partp->dp_ssect
		,partp->dp_ecyl
		,partp->dp_ehd
		,partp->dp_esect);
	printf ("\tsystem Name %.16s\n", partp->dp_name);
}


static void
init_boot(void)
{

	mboot.signature = DOSMAGIC;
}


static void
init_sector0(unsigned long start)
{
	struct pc98_partition *partp =
		(struct pc98_partition *)(&mboot.parts[0]);

	init_boot();

	partp->dp_mid = DOSMID_386BSD;
	partp->dp_sid = DOSSID_386BSD;

	dos(start, disksecs - start, partp);
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
		if (i == 1) {
			init_sector0(1);
			printf("\nThe static data for the slice 1 has been reinitialized to:\n");
			print_part(i);
		}
	}

	do {
		int x_start = partp->dp_scyl * cylsecs ;
		int x_size  = (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs;
		Decimal("sysmid", partp->dp_mid, tmp);
		Decimal("syssid", partp->dp_sid, tmp);
		String ("system name", partp->dp_name, 16);
		Decimal("start", x_start, tmp);
		Decimal("size", x_size, tmp);

		if (ok("Explicitly specify beg/end address ?"))
		{
			int	tsec,tcyl,thd;
			tcyl = partp->dp_scyl;
			thd = partp->dp_shd;
			tsec = partp->dp_ssect;
			Decimal("beginning cylinder", tcyl, tmp);
			Decimal("beginning head", thd, tmp);
			Decimal("beginning sector", tsec, tmp);
			partp->dp_scyl = tcyl;
			partp->dp_ssect = tsec;
			partp->dp_shd = thd;
			partp->dp_ipl_cyl = partp->dp_scyl;
			partp->dp_ipl_sct = partp->dp_ssect;
			partp->dp_ipl_head = partp->dp_shd;

			tcyl = partp->dp_ecyl;
			thd = partp->dp_ehd;
			tsec = partp->dp_esect;
			Decimal("ending cylinder", tcyl, tmp);
			Decimal("ending head", thd, tmp);
			Decimal("ending sector", tsec, tmp);
			partp->dp_ecyl = tcyl;
			partp->dp_esect = tsec;
			partp->dp_ehd = thd;
		} else
			dos(x_start, x_size, partp);

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
	if (dos_cyls > 65535 || dos_heads > 255 || dos_sectors > 255)
		printf("Figures below won't work with BIOS for partitions not in cyl 1\n");
	printf("parameters to be used for BIOS calculations are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d blks/cyl)\n\n"
		,dos_cyls,dos_heads,dos_sectors,dos_cylsecs);
}

static void
change_active(int which)
{
	struct pc98_partition *partp = &mboot.parts[0];
	int active, i, new, tmp;

	active = -1;
	for (i = 0; i < NDOSPART; i++) {
		if ((partp[i].dp_sid & ACTIVE) == 0)
			continue;
		printf("Partition %d is marked active\n", i + 1);
		if (active == -1)
			active = i + 1;
	}
	if (a_flag && which != -1)
		active = which;
	else if (active == -1)
		active = 1;

	if (!ok("Do you want to change the active partition?"))
		return;
setactive:
	do {
		new = active;
		Decimal("active partition", new, tmp);
		if (new < 1 || new > 8) {
			printf("Active partition number must be in range 1-8."
					"  Try again.\n");
			goto setactive;
		}
		active = new;
	} while (!ok("Are you happy with this choice"));
	if (active > 0 && active <= 8)
		partp[active-1].dp_sid |= ACTIVE;
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
static void
dos(u_int32_t start, u_int32_t size, struct pc98_partition *partp)
{
	u_int32_t end;

	if (partp->dp_mid == 0 && partp->dp_sid == 0 &&
	    start == 0 && size == 0) {
		memcpy(partp, &mtpart, sizeof(*partp));
		return;
	}

	/* Start c/h/s. */
	partp->dp_scyl = partp->dp_ipl_cyl = start / dos_cylsecs;
	partp->dp_shd = partp->dp_ipl_head = start % dos_cylsecs / dos_sectors;
	partp->dp_ssect = partp->dp_ipl_sct = start % dos_sectors;

	/* End c/h/s. */
	end = start + size - cylsecs;
	partp->dp_ecyl = end / dos_cylsecs;
	partp->dp_ehd = end % dos_cylsecs / dos_sectors;
	partp->dp_esect = end % dos_sectors;
}

static int
open_disk(int flag)
{
	struct stat 	st;
	int rwmode;

	if (stat(disk, &st) == -1) {
		if (errno == ENOENT)
			return -2;
		warnx("can't get file status of %s", disk);
		return -1;
	}
	if ( !(st.st_mode & S_IFCHR) )
		warnx("device %s is not character special", disk);
	rwmode = a_flag || B_flag || flag ? O_RDWR : O_RDONLY;
	fd = open(disk, rwmode);
	if (fd == -1 && errno == EPERM && rwmode == O_RDWR)
		fd = open(disk, O_RDONLY);
	if (fd == -1 && errno == ENXIO)
		return -2;
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

	lseek(fd, (sector * 512), 0);
	return read(fd, buf,
		    secsize > MIN_SEC_SIZE ? secsize : MIN_SEC_SIZE * 2);
}

static int
write_disk(off_t sector, void *buf)
{
	int error;
	struct gctl_req *grq;
	const char *q;
	char fbuf[BUFSIZ];
	int i, fdw;

	grq = gctl_get_handle();
	gctl_ro_param(grq, "verb", -1, "write PC98");
	gctl_ro_param(grq, "class", -1, "PC98");
	q = strrchr(disk, '/');
	if (q == NULL)
		q = disk;
	else
		q++;
	gctl_ro_param(grq, "geom", -1, q);
	gctl_ro_param(grq, "data", secsize, buf);
	q = gctl_issue(grq);
	if (q == NULL) {
		gctl_free(grq);
		return(0);
	}
	warnx("%s", q);
	gctl_free(grq);

	error = pwrite(fd, buf, secsize, (sector * 512));
	if (error == secsize)
		return (0);

	for (i = 0; i < NDOSPART; i++) {
		sprintf(fbuf, "%ss%d", disk, i + 1);
		fdw = open(fbuf, O_RDWR, 0);
		if (fdw < 0)
			continue;
		error = ioctl(fdw, DIOCSPC98, buf);
		close(fdw);
		if (error == 0)
			return (0);
	}
	warnx("Failed to write sector zero");
	return(EINVAL);
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
		sectors = dos_sectors = 17;

	error = ioctl(fd, DIOCGFWHEADS, &u);
	if (error == 0)
		heads = dos_heads = u;
	else
		heads = dos_heads = 8;

	dos_cylsecs = cylsecs = heads * sectors;
	disksecs = cyls * heads * sectors;

	error = ioctl(fd, DIOCGSECTORSIZE, &u);
	if (error != 0 || u == 0)
		u = 512;
	secsize = u;

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

	if (read_disk(0, (char *) mboot.bootinst) == -1) {
		warnx("can't read fdisk partition table");
		return -1;
	}
	if (mboot.signature != DOSMAGIC) {
		warnx("invalid fdisk partition table found");
		/* So should we initialize things */
		return -1;
	}

	return 0;
}

static int
write_s0()
{

	if (iotest) {
		print_s0(-1);
		return 0;
	}

	/*
	 * write enable label sector before write (if necessary),
	 * disable after writing.
	 * needed if the disklabel protected area also protects
	 * sector 0. (e.g. empty disk)
	 */
	if (write_disk(0, (char *) mboot.bootinst) == -1) {
		warn("can't write fdisk partition table");
		return -1;
	}

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

static int
string(const char *str, char **ans)
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

static const char *
get_type(int type)
{
	int	numentries = (sizeof(part_types)/sizeof(struct part_type));
	int	counter = 0;
	struct	part_type *ptr = part_types;


	while(counter < numentries) {
		if(ptr->type == (type & 0x7f))
			return(ptr->name);
		ptr++;
		counter++;
	}
	return("unknown");
}

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
