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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/disklabel.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
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
#define Hex(str, ans, tmp) if (hex(str, &tmp, ans)) ans = tmp
#define String(str, ans, len) {char *z = ans; char **dflt = &z; if (string(str, dflt)) strncpy(ans, *dflt, len); }

#define RoundCyl(x) ((((x) + cylsecs - 1) / cylsecs) * cylsecs)

#define MAX_SEC_SIZE 2048	/* maximum section size that is supported */
#define MIN_SEC_SIZE 512	/* the sector size to start sensing at */
int secsize = 0;		/* the sensed sector size */

const char *disk;
const char *disks[] =
{
  "/dev/ad0", "/dev/wd0", "/dev/da0", "/dev/od0", 0
};

struct disklabel disklabel;		/* disk parameters */

int cyls, sectors, heads, cylsecs, disksecs;

struct mboot
{
	unsigned char padding[2]; /* force the longs to be long aligned */
#ifdef PC98
	unsigned char bootinst[510];
#else
	unsigned char bootinst[DOSPARTOFF];
	struct	dos_partition parts[4];
#endif
	unsigned short int	signature;

#ifdef PC98
	struct	dos_partition parts[8];
#endif
	/* room to read in MBRs that are bigger then DEV_BSIZE */
	unsigned char large_sector_overflow[MAX_SEC_SIZE-MIN_SEC_SIZE];
};
struct mboot mboot;

#define ACTIVE 0x80
#define BOOT_MAGIC 0xAA55

int dos_cyls;
int dos_heads;
int dos_sectors;
int dos_cylsecs;

#ifdef PC98
#define DOSSECT(s,c) (s)
#define DOSCYL(c)	(c)
#else
#define DOSSECT(s,c) ((s & 0x3f) | ((c >> 2) & 0xc0))
#define DOSCYL(c)	(c & 0xff)
#endif
static int partition = -1;


#define MAX_ARGS	10

static int	current_line_number;

static int	geom_processed = 0;
static int	part_processed = 0;
static int	active_processed = 0;


typedef struct cmd {
    char		cmd;
    int			n_args;
    struct arg {
	char	argtype;
	int	arg_val;
    }			args[MAX_ARGS];
} CMD;


static int B_flag  = 0;		/* replace boot code */
static int I_flag  = 0;		/* use entire disk for FreeBSD */
static int a_flag  = 0;		/* set active partition */
static char *b_flag = NULL;	/* path to boot code */
static int i_flag  = 0;		/* replace partition data */
static int u_flag  = 0;		/* update partition data */
static int s_flag  = 0;		/* Print a summary and exit */
static int t_flag  = 0;		/* test only, if f_flag is given */
static char *f_flag = NULL;	/* Read config info from file */
static int v_flag  = 0;		/* Be verbose */

struct part_type
{
 unsigned char type;
 char *name;
}part_types[] =
{
	 {0x00, "unused"}
	,{0x01, "Primary DOS with 12 bit FAT"}
#ifdef PC98
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
#else
	,{0x02, "XENIX / file system"}
	,{0x03, "XENIX /usr file system"}
	,{0x04, "Primary DOS with 16 bit FAT (<= 32MB)"}
	,{0x05, "Extended DOS"}
	,{0x06, "Primary 'big' DOS (> 32MB)"}
	,{0x07, "OS/2 HPFS, NTFS, QNX or Advanced UNIX"}
	,{0x08, "AIX file system"}
	,{0x09, "AIX boot partition or Coherent"}
	,{0x0A, "OS/2 Boot Manager or OPUS"}
	,{0x0B, "DOS or Windows 95 with 32 bit FAT"}
	,{0x0C, "DOS or Windows 95 with 32 bit FAT, LBA"}
	,{0x0E, "Primary 'big' DOS (> 32MB, LBA)"}
	,{0x0F, "Extended DOS, LBA"}
	,{0x10, "OPUS"}
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
	,{0x80, "Minix 1.1 ... 1.4a"}
	,{0x81, "Minix 1.4b ... 1.5.10"}
	,{0x82, "Linux swap or Solaris x86"}
	,{0x83, "Linux file system"}
	,{0x93, "Amoeba file system"}
	,{0x94, "Amoeba bad block table"}
	,{0x9F, "BSD/OS"}
	,{0xA5, "FreeBSD/NetBSD/386BSD"}
	,{0xA6, "OpenBSD"}
	,{0xA7, "NEXTSTEP"}
	,{0xA9, "NetBSD"}
	,{0xB7, "BSDI BSD/386 file system"}
	,{0xB8, "BSDI BSD/386 swap"}
	,{0xDB, "Concurrent CPM or C.DOS or CTOS"}
	,{0xE1, "Speed"}
	,{0xE3, "Speed"}
	,{0xE4, "Speed"}
	,{0xF1, "Speed"}
	,{0xF2, "DOS 3.3+ Secondary"}
	,{0xF4, "Speed"}
	,{0xFF, "BBT (Bad Blocks Table)"}
#endif
};

static void print_s0(int which);
static void print_part(int i);
static void init_sector0(unsigned long start);
static void init_boot(void);
static void change_part(int i);
static void print_params();
static void change_active(int which);
static void change_code();
static void get_params_to_use();
#ifdef PC98
static void dos(int sec, int size, unsigned short *c, unsigned char *s,
		unsigned char *h);
#else
static void dos(int sec, int size, unsigned char *c, unsigned char *s,
		unsigned char *h);
#endif
static int open_disk(int u_flag);
static ssize_t read_disk(off_t sector, void *buf);
static ssize_t write_disk(off_t sector, void *buf);
static int get_params();
static int read_s0();
static int write_s0();
static int ok(char *str);
static int decimal(char *str, int *num, int deflt);
static char *get_type(int type);
static int read_config(char *config_file);
static void reset_boot(void);
static void usage(void);
#if 0
static int hex(char *str, int *num, int deflt);
#endif
#ifdef PC98
static int string(char *str, char **ans);
#endif



int
main(int argc, char *argv[])
{
	int	c, i;

#ifdef PC98
	while ((c = getopt(argc, argv, "Bab:f:istuv12345678")) != -1)
#else
	while ((c = getopt(argc, argv, "BIab:f:istuv1234")) != -1)
#endif
		switch (c) {
		case 'B':
			B_flag = 1;
			break;
		case 'I':
			I_flag = 1;
			break;
		case 'a':
			a_flag = 1;
			break;
		case 'b':
			b_flag = optarg;
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

	if (argc > 0)
	{
		static char realname[12];

		if(strncmp(argv[0], _PATH_DEV, sizeof _PATH_DEV - 2) == 0)
			disk = argv[0];
		else
		{
			snprintf(realname, 12, "%s%s", _PATH_DEV, argv[0]);
			disk = realname;
		}
		
		if (open_disk(u_flag) < 0)
			err(1, "cannot open disk %s", disk);
	}
	else
	{
		int rv = 0;

		for(i = 0; disks[i]; i++)
		{
			disk = disks[i];
			rv = open_disk(u_flag);
			if(rv != -2) break;
		}
		if(rv < 0)
			err(1, "cannot open any disk");
	}
	if (s_flag)
	{
		int i;
		struct dos_partition *partp;

		if (read_s0())
			err(1, "read_s0");
		printf("%s: %d cyl %d hd %d sec\n", disk, dos_cyls, dos_heads,
		    dos_sectors);
#ifdef PC98
		printf("Part  %11s %11s SID\n", "Start", "Size");
#else
		printf("Part  %11s %11s Type Flags\n", "Start", "Size");
#endif
		for (i = 0; i < NDOSPART; i++) {
			partp = ((struct dos_partition *) &mboot.parts) + i;
#ifdef PC98
			if (partp->dp_sid == 0)
#else
			if (partp->dp_start == 0 && partp->dp_size == 0)
#endif
				continue;
			printf("%4d: %11lu %11lu 0x%02x\n", i + 1,
#ifdef PC98
			    partp->dp_scyl * cylsecs,
			    (partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs,
				partp->dp_sid);
#else
			    (u_long) partp->dp_start,
			    (u_long) partp->dp_size, partp->dp_typ,
			    partp->dp_flag);
#endif
		}
		exit(0);
	}

	printf("******* Working on device %s *******\n",disk);

#ifndef PC98
	if (I_flag)
	{
		struct dos_partition *partp;

		read_s0();
		reset_boot();
		partp = (struct dos_partition *) (&mboot.parts[0]);
		partp->dp_typ = DOSPTYP_386BSD;
		partp->dp_flag = ACTIVE;
		partp->dp_start = dos_sectors;
		partp->dp_size = disksecs - dos_sectors;

		dos(partp->dp_start, partp->dp_size, 
		    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
		dos(partp->dp_start + partp->dp_size - 1, partp->dp_size,
		    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
		if (v_flag)
			print_s0(-1);
		write_s0();
		exit(0);
	}
#endif
	if (f_flag)
	{
#ifndef PC98
	    if (read_s0() || i_flag)
	    {
		reset_boot();
	    }

	    if (!read_config(f_flag))
	    {
		exit(1);
	    }
#endif
	    if (v_flag)
	    {
		print_s0(-1);
	    }
	    if (!t_flag)
	    {
		write_s0();
	    }
	}
	else
	{
	    if(u_flag)
	    {
		get_params_to_use();
	    }
	    else
	    {
		print_params();
	    }

	    if (read_s0())
		init_sector0(1);

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
		if (!t_flag)
		{
		    printf("\nWe haven't changed the partition table yet.  ");
		    printf("This is your last chance.\n");
		}
		print_s0(-1);
		if (!t_flag)
		{
		    if (ok("Should we write new partition table?"))
			write_s0();
		}
		else
		{
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
		"usage: fdisk [-Batu] [-b bootcode] [-12345678] [disk]\n",
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

static struct dos_partition mtpart = { 0 };

static void
print_part(int i)
{
	struct	  dos_partition *partp;
	u_int64_t part_mb;

	partp = ((struct dos_partition *) &mboot.parts) + i - 1;

	if (!bcmp(partp, &mtpart, sizeof (struct dos_partition))) {
		printf("<UNUSED>\n");
		return;
	}
	/*
	 * Be careful not to overflow.
	 */
#ifdef PC98
	printf("sysmid %d,(%s)\n", partp->dp_mid, get_type(partp->dp_mid));
	printf("    start %d, size %d (%d Meg), sid %d\n",
		partp->dp_scyl * cylsecs ,
		(partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs,
		(partp->dp_ecyl - partp->dp_scyl + 1) * cylsecs * 512 / (1024 * 1024),
		partp->dp_sid);
#else
	part_mb = partp->dp_size;
	part_mb *= secsize;
	part_mb /= (1024 * 1024);
	printf("sysid %d,(%s)\n", partp->dp_typ, get_type(partp->dp_typ));
	printf("    start %lu, size %lu (%qd Meg), flag %x%s\n",
		(u_long)partp->dp_start,
		(u_long)partp->dp_size, 
		part_mb,
		partp->dp_flag,
		partp->dp_flag == ACTIVE ? " (active)" : "");
#endif
	printf("\tbeg: cyl %d/ sector %d/ head %d;\n\tend: cyl %d/ sector %d/ head %d\n"
		,DPCYL(partp->dp_scyl, partp->dp_ssect)
		,DPSECT(partp->dp_ssect)
		,partp->dp_shd
		,DPCYL(partp->dp_ecyl, partp->dp_esect)
		,DPSECT(partp->dp_esect)
		,partp->dp_ehd);
#ifdef PC98
	printf ("\tsystem Name %.16s\n",partp->dp_name);
#endif
}


static void
init_boot(void)
{
#ifndef PC98
	const char *fname;
	int fd;

	fname = b_flag ? b_flag : "/boot/mbr";
	if ((fd = open(fname, O_RDONLY)) == -1 ||
	    read(fd, mboot.bootinst, DOSPARTOFF) == -1 ||
	    close(fd))
		err(1, "%s", fname);
#endif
	mboot.signature = BOOT_MAGIC;
}


static void
init_sector0(unsigned long start)
{
struct dos_partition *partp = (struct dos_partition *) (&mboot.parts[3]);
unsigned long size = disksecs - start;

	init_boot();

#ifdef PC98
	partp->dp_mid = DOSMID_386BSD;
	partp->dp_sid = DOSSID_386BSD;

	dos(start, size, &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
	partp->dp_ipl_cyl = partp->dp_scyl;
	partp->dp_ipl_sct = partp->dp_ssect;
	partp->dp_ipl_head = partp->dp_shd;
	dos(start+size-cylsecs, size, &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
#else
	partp->dp_typ = DOSPTYP_386BSD;
	partp->dp_flag = ACTIVE;
	partp->dp_start = start;
	partp->dp_size = size;

	dos(partp->dp_start, partp->dp_size, 
	    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
	dos(partp->dp_start + partp->dp_size - 1, partp->dp_size,
	    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
#endif
}

static void
change_part(int i)
{
struct dos_partition *partp = ((struct dos_partition *) &mboot.parts) + i - 1;

    printf("The data for partition %d is:\n", i);
    print_part(i);

    if (u_flag && ok("Do you want to change it?")) {
	int tmp;

	if (i_flag) {
		bzero((char *)partp, sizeof (struct dos_partition));
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
#endif
		if (ok("Explicitly specify beg/end address ?"))
		{
			int	tsec,tcyl,thd;
			tcyl = DPCYL(partp->dp_scyl,partp->dp_ssect);
			thd = partp->dp_shd;
			tsec = DPSECT(partp->dp_ssect);
			Decimal("beginning cylinder", tcyl, tmp);
			Decimal("beginning head", thd, tmp);
			Decimal("beginning sector", tsec, tmp);
			partp->dp_scyl = DOSCYL(tcyl);
			partp->dp_ssect = DOSSECT(tsec,tcyl);
			partp->dp_shd = thd;
#ifdef PC98
			partp->dp_ipl_cyl = partp->dp_scyl;
			partp->dp_ipl_sct = partp->dp_ssect;
			partp->dp_ipl_head = partp->dp_shd;
#endif

			tcyl = DPCYL(partp->dp_ecyl,partp->dp_esect);
			thd = partp->dp_ehd;
			tsec = DPSECT(partp->dp_esect);
			Decimal("ending cylinder", tcyl, tmp);
			Decimal("ending head", thd, tmp);
			Decimal("ending sector", tsec, tmp);
			partp->dp_ecyl = DOSCYL(tcyl);
			partp->dp_esect = DOSSECT(tsec,tcyl);
			partp->dp_ehd = thd;
		} else {
#ifdef PC98
			dos(x_start, x_size, &partp->dp_scyl,
			    &partp->dp_ssect, &partp->dp_shd);
			partp->dp_ipl_cyl = partp->dp_scyl;
			partp->dp_ipl_sct = partp->dp_ssect;
			partp->dp_ipl_head = partp->dp_shd;
			dos(x_start+x_size - cylsecs, x_size, &partp->dp_ecyl,
			    &partp->dp_esect, &partp->dp_ehd);
#else
			dos(partp->dp_start, partp->dp_size,
			    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
			dos(partp->dp_start + partp->dp_size - 1, partp->dp_size,
			    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
#endif
		}

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
int i;
#ifdef PC98
int active = 8, tmp;
#else
int active = 4, tmp;
#endif

struct dos_partition *partp = ((struct dos_partition *) &mboot.parts);

	if (a_flag && which != -1)
		active = which;
	if (!ok("Do you want to change the active partition?"))
		return;
setactive:
#ifdef PC98
	active = 4;
#else
	active = 4;
#endif
	do {
		Decimal("active partition", active, tmp);
#ifdef PC98
		if (active < 1 || 8 < active) {
			printf("Active partition number must be in range 1-8."
					"  Try again.\n");
			goto setactive;
		}
#else
		if (active < 1 || 4 < active) {
			printf("Active partition number must be in range 1-4."
					"  Try again.\n");
			goto setactive;
		}
#endif
	} while (!ok("Are you happy with this choice"));
#ifdef PC98
	partp[active].dp_sid |= ACTIVE;
#else
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
static void
dos(sec, size, c, s, h)
int sec, size;
#ifdef PC98
unsigned short *c;
unsigned char *s, *h;
#else
unsigned char *c, *s, *h;
#endif
{
int cy;
int hd;

	if (sec == 0 && size == 0) {
		*s = *c = *h = 0;
		return;
	}

	cy = sec / ( dos_cylsecs );
	sec = sec - cy * ( dos_cylsecs );

	hd = sec / dos_sectors;
#ifdef PC98
	sec = (sec - hd * dos_sectors);

	*h = hd;
	*c = cy;
	*s = sec;
#else
	sec = (sec - hd * dos_sectors) + 1;

	*h = hd;
	*c = cy & 0xff;
	*s = (sec & 0x3f) | ( (cy & 0x300) >> 2);
#endif
}

int fd;

	/* Getting device status */

static int
open_disk(int u_flag)
{
struct stat 	st;

	if (stat(disk, &st) == -1) {
		warnx("can't get file status of %s", disk);
		return -1;
	}
	if ( !(st.st_mode & S_IFCHR) )
		warnx("device %s is not character special", disk);
	if ((fd = open(disk,
	    a_flag || I_flag || B_flag || u_flag ? O_RDWR : O_RDONLY)) == -1) {
		if(errno == ENXIO)
			return -2;
		warnx("can't open device %s", disk);
		return -1;
	}
	if (get_params(0) == -1) {
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
	return read(fd, buf, secsize > MIN_SEC_SIZE ? secsize : MIN_SEC_SIZE * 2);
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
	lseek(fd,(sector * 512), 0);
	/* write out in the size that the read_disk found worked */
#ifdef PC98
	return write(fd, buf, secsize > MIN_SEC_SIZE ? secsize : MIN_SEC_SIZE * 2);
#else
	return write(fd, buf, secsize);
#endif
}

static int
get_params()
{

    if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
	warnx("can't get disk parameters on %s; supplying dummy ones", disk);
	dos_cyls = cyls = 1;
	dos_heads = heads = 1;
	dos_sectors = sectors = 1;
	dos_cylsecs = cylsecs = heads * sectors;
	disksecs = cyls * heads * sectors;
#ifdef PC98
	secsize = disklabel.d_secsize;
#endif
	return disksecs;
    }

    dos_cyls = cyls = disklabel.d_ncylinders;
    dos_heads = heads = disklabel.d_ntracks;
    dos_sectors = sectors = disklabel.d_nsectors;
    dos_cylsecs = cylsecs = heads * sectors;
    disksecs = cyls * heads * sectors;
#ifdef PC98
    secsize = disklabel.d_secsize;
#endif
    return (disksecs);
}


static int
read_s0()
{
	if (read_disk(0, (char *) mboot.bootinst) == -1) {
		warnx("can't read fdisk partition table");
		return -1;
	}
	if (mboot.signature != BOOT_MAGIC) {
		warnx("invalid fdisk partition table found");
		/* So should we initialize things */
		return -1;
	}
	return 0;
}

static int
write_s0()
{
#ifdef NOT_NOW
	int	flag;
#endif
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
#ifdef NOT_NOW
	flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		warn("ioctl DIOCWLABEL");
#endif
	if (write_disk(0, (char *) mboot.bootinst) == -1) {
		warn("can't write fdisk partition table");
		return -1;
#ifdef NOT_NOW
	flag = 0;
	(void) ioctl(fd, DIOCWLABEL, &flag);
#endif
	}
	return(0);
}


static int
ok(str)
char *str;
{
	printf("%s [n] ", str);
	fgets(lbuf, LBUF, stdin);
	lbuf[strlen(lbuf)-1] = 0;

	if (*lbuf &&
		(!strcmp(lbuf, "yes") || !strcmp(lbuf, "YES") ||
		 !strcmp(lbuf, "y") || !strcmp(lbuf, "Y")))
		return 1;
	else
		return 0;
}

static int
decimal(char *str, int *num, int deflt)
{
int acc = 0, c;
char *cp;

	while (1) {
		printf("Supply a decimal value for \"%s\" [%d] ", str, deflt);
		fgets(lbuf, LBUF, stdin);
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

#if 0
static int
hex(char *str, int *num, int deflt)
{
int acc = 0, c;
char *cp;

	while (1) {
		printf("Supply a hex value for \"%s\" [%x] ", str, deflt);
		fgets(lbuf, LBUF, stdin);
		lbuf[strlen(lbuf)-1] = 0;

		if (!*lbuf)
			return 0;

		cp = lbuf;
		while ((c = *cp) && (c == ' ' || c == '\t')) cp++;
		if (!c)
			return 0;
		while ((c = *cp++)) {
			if (c <= '9' && c >= '0')
				acc = (acc << 4) + c - '0';
			else if (c <= 'f' && c >= 'a')
				acc = (acc << 4) + c - 'a' + 10;
			else if (c <= 'F' && c >= 'A')
				acc = (acc << 4) + c - 'A' + 10;
			else
				break;
		}
		if (c == ' ' || c == '\t')
			while ((c = *cp) && (c == ' ' || c == '\t')) cp++;
		if (!c) {
			*num = acc;
			return 1;
		} else
			printf("%s is an invalid hex number.  Try again.\n",
				lbuf);
	}

}
#endif

#ifdef PC98
static int
string(char *str, char **ans)
{
#ifdef PC98
int i;
#endif
int c;
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

#ifdef PC98
		for (i = strlen(*ans); i < 16; i++)
			(*ans)[i] = ' ';
		(*ans)[16] = 0;
#else
		if (c)
			*cp = 0;
#endif
		return 1;
	}
}
#endif

static char *
get_type(int type)
{
	int	numentries = (sizeof(part_types)/sizeof(struct part_type));
	int	counter = 0;
	struct	part_type *ptr = part_types;


	while(counter < numentries)
	{
#ifdef PC98
		if(ptr->type == (type & 0x7f))
#else
		if(ptr->type == type)
#endif
		{
			return(ptr->name);
		}
		ptr++;
		counter++;
	}
	return("unknown");
}

#ifndef PC98
static void
parse_config_line(line, command)
    char	*line;
    CMD		*command;
{
    char	*cp, *end;

    cp = line;
    while (1)	/* dirty trick used to insure one exit point for this
		   function */
    {
	memset(command, 0, sizeof(*command));

	while (isspace(*cp)) ++cp;
	if (*cp == '\0' || *cp == '#')
	{
	    break;
	}
	command->cmd = *cp++;

	/*
	 * Parse args
	 */
	while (1)
	{
	    while (isspace(*cp)) ++cp;
	    if (*cp == '#')
	    {
		break;		/* found comment */
	    }
	    if (isalpha(*cp))
	    {
		command->args[command->n_args].argtype = *cp++;
	    }
	    if (!isdigit(*cp))
	    {
		break;		/* assume end of line */
	    }
	    end = NULL;
	    command->args[command->n_args].arg_val = strtol(cp, &end, 0);
	    if (cp == end)
	    {
		break;		/* couldn't parse number */
	    }
	    cp = end;
	    command->n_args++;
	}
	break;
    }
}


static int
process_geometry(command)
    CMD		*command;
{
    int		status = 1, i;

    while (1)
    {
	geom_processed = 1;
	if (part_processed)
	{
	    warnx(
	"ERROR line %d: the geometry specification line must occur before\n\
    all partition specifications",
		    current_line_number);
	    status = 0;
	    break;
	}
	if (command->n_args != 3)
	{
	    warnx("ERROR line %d: incorrect number of geometry args",
		    current_line_number);
	    status = 0;
	    break;
	}
	dos_cyls = -1;
	dos_heads = -1;
	dos_sectors = -1;
	for (i = 0; i < 3; ++i)
	{
	    switch (command->args[i].argtype)
	    {
	    case 'c':
		dos_cyls = command->args[i].arg_val;
		break;
	    case 'h':
		dos_heads = command->args[i].arg_val;
		break;
	    case 's':
		dos_sectors = command->args[i].arg_val;
		break;
	    default:
		warnx(
		"ERROR line %d: unknown geometry arg type: '%c' (0x%02x)",
			current_line_number, command->args[i].argtype,
			command->args[i].argtype);
		status = 0;
		break;
	    }
	}
	if (status == 0)
	{
	    break;
	}

	dos_cylsecs = dos_heads * dos_sectors;

	/*
	 * Do sanity checks on parameter values
	 */
	if (dos_cyls < 0)
	{
	    warnx("ERROR line %d: number of cylinders not specified",
		    current_line_number);
	    status = 0;
	}
	if (dos_cyls == 0 || dos_cyls > 1024)
	{
	    warnx(
	"WARNING line %d: number of cylinders (%d) may be out-of-range\n\
    (must be within 1-1024 for normal BIOS operation, unless the entire disk\n\
    is dedicated to FreeBSD)",
		    current_line_number, dos_cyls);
	}

	if (dos_heads < 0)
	{
	    warnx("ERROR line %d: number of heads not specified",
		    current_line_number);
	    status = 0;
	}
	else if (dos_heads < 1 || dos_heads > 256)
	{
	    warnx("ERROR line %d: number of heads must be within (1-256)",
		    current_line_number);
	    status = 0;
	}

	if (dos_sectors < 0)
	{
	    warnx("ERROR line %d: number of sectors not specified",
		    current_line_number);
	    status = 0;
	}
	else if (dos_sectors < 1 || dos_sectors > 63)
	{
	    warnx("ERROR line %d: number of sectors must be within (1-63)",
		    current_line_number);
	    status = 0;
	}

	break;
    }
    return (status);
}


static int
process_partition(command)
    CMD		*command;
{
    int				status = 0, partition;
    unsigned long		chunks, adj_size, max_end;
    struct dos_partition	*partp;

    while (1)
    {
	part_processed = 1;
	if (command->n_args != 4)
	{
	    warnx("ERROR line %d: incorrect number of partition args",
		    current_line_number);
	    break;
	}
	partition = command->args[0].arg_val;
	if (partition < 1 || partition > 4)
	{
	    warnx("ERROR line %d: invalid partition number %d",
		    current_line_number, partition);
	    break;
	}
	partp = ((struct dos_partition *) &mboot.parts) + partition - 1;
	bzero((char *)partp, sizeof (struct dos_partition));
	partp->dp_typ = command->args[1].arg_val;
	partp->dp_start = command->args[2].arg_val;
	partp->dp_size = command->args[3].arg_val;
	max_end = partp->dp_start + partp->dp_size;

	if (partp->dp_typ == 0)
	{
	    /*
	     * Get out, the partition is marked as unused.
	     */
	    /*
	     * Insure that it's unused.
	     */
	    bzero((char *)partp, sizeof (struct dos_partition));
	    status = 1;
	    break;
	}

	/*
	 * Adjust start upwards, if necessary, to fall on an head boundary.
	 */
	if (partp->dp_start % dos_sectors != 0)
	{
	    adj_size =
		(partp->dp_start / dos_sectors + 1) * dos_sectors;
	    if (adj_size > max_end)
	    {
		/*
		 * Can't go past end of partition
		 */
		warnx(
	"ERROR line %d: unable to adjust start of partition %d to fall on\n\
    a cylinder boundary",
			current_line_number, partition);
		break;
	    }
	    warnx(
	"WARNING: adjusting start offset of partition '%d' from %lu\n\
    to %lu, to round to an head boundary",
		    partition, (u_long)partp->dp_start, adj_size);
	    partp->dp_start = adj_size;
	}

	/*
	 * Adjust size downwards, if necessary, to fall on a cylinder
	 * boundary.
	 */
	chunks =
	    ((partp->dp_start + partp->dp_size) / dos_cylsecs) * dos_cylsecs;
	adj_size = chunks - partp->dp_start;
	if (adj_size != partp->dp_size)
	{
	    warnx(
	"WARNING: adjusting size of partition '%d' from %lu to %lu,\n\
    to round to a cylinder boundary",
		    partition, (u_long)partp->dp_size, adj_size);
	    if (chunks > 0)
	    {
		partp->dp_size = adj_size;
	    }
	    else
	    {
		partp->dp_size = 0;
	    }
	}
	if (partp->dp_size < 1)
	{
	    warnx("ERROR line %d: size for partition '%d' is zero",
		    current_line_number, partition);
	    break;
	}

	dos(partp->dp_start, partp->dp_size,
	    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
	dos(partp->dp_start+partp->dp_size - 1, partp->dp_size,
	    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
	status = 1;
	break;
    }
    return (status);
}


static int
process_active(command)
    CMD		*command;
{
    int				status = 0, partition, i;
    struct dos_partition	*partp;

    while (1)
    {
	active_processed = 1;
	if (command->n_args != 1)
	{
	    warnx("ERROR line %d: incorrect number of active args",
		    current_line_number);
	    status = 0;
	    break;
	}
	partition = command->args[0].arg_val;
	if (partition < 1 || partition > 4)
	{
	    warnx("ERROR line %d: invalid partition number %d",
		    current_line_number, partition);
	    break;
	}
	/*
	 * Reset active partition
	 */
	partp = ((struct dos_partition *) &mboot.parts);
	for (i = 0; i < NDOSPART; i++)
	    partp[i].dp_flag = 0;
	partp[partition-1].dp_flag = ACTIVE;

	status = 1;
	break;
    }
    return (status);
}


static int
process_line(line)
    char	*line;
{
    CMD		command;
    int		status = 1;

    while (1)
    {
	parse_config_line(line, &command);
	switch (command.cmd)
	{
	case 0:
	    /*
	     * Comment or blank line
	     */
	    break;
	case 'g':
	    /*
	     * Set geometry
	     */
	    status = process_geometry(&command);
	    break;
	case 'p':
	    status = process_partition(&command);
	    break;
	case 'a':
	    status = process_active(&command);
	    break;
	default:
	    status = 0;
	    break;
	}
	break;
    }
    return (status);
}


static int
read_config(config_file)
    char *config_file;
{
    FILE	*fp = NULL;
    int		status = 1;
    char	buf[1010];

    while (1)	/* dirty trick used to insure one exit point for this
		   function */
    {
	if (strcmp(config_file, "-") != 0)
	{
	    /*
	     * We're not reading from stdin
	     */
	    if ((fp = fopen(config_file, "r")) == NULL)
	    {
		status = 0;
		break;
	    }
	}
	else
	{
	    fp = stdin;
	}
	current_line_number = 0;
	while (!feof(fp))
	{
	    if (fgets(buf, sizeof(buf), fp) == NULL)
	    {
		break;
	    }
	    ++current_line_number;
	    status = process_line(buf);
	    if (status == 0)
	    {
		break;
	    }
	}
	break;
    }
    if (fp)
    {
	/*
	 * It doesn't matter if we're reading from stdin, as we've reached EOF
	 */
	fclose(fp);
    }
    return (status);
}


static void
reset_boot(void)
{
    int				i;
    struct dos_partition	*partp;

    init_boot();
    for (i = 0; i < 4; ++i)
    {
	partp = ((struct dos_partition *) &mboot.parts) + i;
	bzero((char *)partp, sizeof (struct dos_partition));
    }
}
#endif
