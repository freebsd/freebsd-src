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

#include <sys/types.h>
#include <sys/disklabel.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
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
  "/dev/rwd0", "/dev/rsd0", "/dev/rod0", 0
};

char *name;

struct disklabel disklabel;		/* disk parameters */

int cyls, sectors, heads, cylsecs, disksecs;

struct mboot
{
	unsigned char padding[2]; /* force the longs to be long alligned */
	unsigned char bootinst[DOSPARTOFF];
	struct	dos_partition parts[4];
	unsigned short int	signature;
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

#define DOSSECT(s,c) ((s & 0x3f) | ((c >> 2) & 0xc0))
#define DOSCYL(c)	(c & 0xff)
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


static int a_flag  = 0;		/* set active partition */
static int i_flag  = 0;		/* replace partition data */
static int u_flag  = 0;		/* update partition data */
static int t_flag  = 0;		/* test only, if f_flag is given */
static char *f_flag = NULL;	/* Read config info from file */
static int v_flag  = 0;		/* Be verbose */

static unsigned char bootcode[] = {
0x33, 0xc0, 0xfa, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x8e, 0xc0, 0x8e, 0xd8, 0xfb, 0x8b, 0xf4, 0xbf,
0x00, 0x06, 0xb9, 0x00, 0x02, 0xfc, 0xf3, 0xa4, 0xea, 0x1d, 0x06, 0x00, 0x00, 0xb0, 0x04, 0xbe,
0xbe, 0x07, 0x80, 0x3c, 0x80, 0x74, 0x0c, 0x83, 0xc6, 0x10, 0xfe, 0xc8, 0x75, 0xf4, 0xbe, 0xbd,
0x06, 0xeb, 0x43, 0x8b, 0xfe, 0x8b, 0x14, 0x8b, 0x4c, 0x02, 0x83, 0xc6, 0x10, 0xfe, 0xc8, 0x74,
0x0a, 0x80, 0x3c, 0x80, 0x75, 0xf4, 0xbe, 0xbd, 0x06, 0xeb, 0x2b, 0xbd, 0x05, 0x00, 0xbb, 0x00,
0x7c, 0xb8, 0x01, 0x02, 0xcd, 0x13, 0x73, 0x0c, 0x33, 0xc0, 0xcd, 0x13, 0x4d, 0x75, 0xef, 0xbe,
0x9e, 0x06, 0xeb, 0x12, 0x81, 0x3e, 0xfe, 0x7d, 0x55, 0xaa, 0x75, 0x07, 0x8b, 0xf7, 0xea, 0x00,
0x7c, 0x00, 0x00, 0xbe, 0x85, 0x06, 0x2e, 0xac, 0x0a, 0xc0, 0x74, 0x06, 0xb4, 0x0e, 0xcd, 0x10,
0xeb, 0xf4, 0xfb, 0xeb, 0xfe,
'M', 'i', 's', 's', 'i', 'n', 'g', ' ',
	'o', 'p', 'e', 'r', 'a', 't', 'i', 'n', 'g', ' ', 's', 'y', 's', 't', 'e', 'm', 0,
'E', 'r', 'r', 'o', 'r', ' ', 'l', 'o', 'a', 'd', 'i', 'n', 'g', ' ',
	'o', 'p', 'e', 'r', 'a', 't', 'i', 'n', 'g', ' ', 's', 'y', 's', 't', 'e', 'm', 0,
'I', 'n', 'v', 'a', 'l', 'i', 'd', ' ',
	'p', 'a', 'r', 't', 'i', 't', 'i', 'o', 'n', ' ', 't', 'a', 'b', 'l', 'e', 0,
'A', 'u', 't', 'h', 'o', 'r', ' ', '-', ' ',
	'S', 'i', 'e', 'g', 'm', 'a', 'r', ' ', 'S', 'c', 'h', 'm', 'i', 'd', 't', 0,0,0,

  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

struct part_type
{
 unsigned char type;
 char *name;
}part_types[] =
{
	 {0x00, "unused"}
	,{0x01, "Primary DOS with 12 bit FAT"}
	,{0x02, "XENIX / filesystem"}
	,{0x03, "XENIX /usr filesystem"}
	,{0x04, "Primary DOS with 16 bit FAT"}
	,{0x05, "Extended DOS"}
	,{0x06, "Primary 'big' DOS (> 32MB)"}
	,{0x07, "OS/2 HPFS, QNX or Advanced UNIX"}
	,{0x08, "AIX filesystem"}
	,{0x09, "AIX boot partition or Coherent"}
	,{0x0A, "OS/2 Boot Manager or OPUS"}
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
	,{0x82, "Linux swap"}
	,{0x83, "Linux filesystem"}
	,{0x93, "Amoeba filesystem"}
	,{0x94, "Amoeba bad block table"}
	,{0xA5, "FreeBSD/NetBSD/386BSD"}
	,{0xA6, "OpenBSD"}
	,{0xA7, "NEXTSTEP"}
	,{0xB7, "BSDI BSD/386 filesystem"}
	,{0xB8, "BSDI BSD/386 swap"}
	,{0xDB, "Concurrent CPM or C.DOS or CTOS"}
	,{0xE1, "Speed"}
	,{0xE3, "Speed"}
	,{0xE4, "Speed"}
	,{0xF1, "Speed"}
	,{0xF2, "DOS 3.3+ Secondary"}
	,{0xF4, "Speed"}
	,{0xFF, "BBT (Bad Blocks Table)"}
};

static void print_s0(int which);
static void print_part(int i);
static void init_sector0(unsigned long start);
static void init_boot(void);
static void change_part(int i);
static void print_params();
static void change_active(int which);
static void get_params_to_use();
static void dos(int sec, int size, unsigned char *c, unsigned char *s,
		unsigned char *h);
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
#if 0
static int hex(char *str, int *num, int deflt);
static int string(char *str, char **ans);
#endif


int
main(int argc, char *argv[])
{
	int	i;

	name = *argv;
	{register char *cp = name;
		while (*cp) if (*cp++ == '/') name = cp;
	}

	for ( argv++ ; --argc ; argv++ ) { register char *token = *argv;
		if (*token++ != '-' || !*token)
			break;
		else { register int flag;
			for ( ; (flag = *token++) ; ) {
				switch (flag) {
				case '1':
					partition = 1;
					break;
				case '2':
					partition = 2;
					break;
				case '3':
					partition = 3;
					break;
				case '4':
					partition = 4;
					break;
				case 'a':
					a_flag = 1;
					break;
				case 'f':
					if (*token)
					{
					    f_flag = token;
					    token = "";
					}
					else
					{
					    if (argc == 1)
					    {
						goto usage;
					    }
					    --argc;
					    f_flag = *++argv;
					}
					/*
					 * u_flag is needed, because we're
					 * writing to the disk.
					 */
					u_flag = 1;
					break;
				case 'i':
					i_flag = 1;
				case 'u':
					u_flag = 1;
					break;
				case 't':
					t_flag = 1;
				case 'v':
					v_flag = 1;
					break;
				default:
					goto usage;
				}
			}
		}
	}

	if (argc > 0)
	{
		static char realname[12];

		if(strncmp(argv[0], "/dev", 4) == 0)
			disk = argv[0];
		else
		{
			snprintf(realname, 12, "/dev/r%s", argv[0]);
			disk = realname;
		}
		
		if (open_disk(u_flag) < 0)
		{
			fprintf(stderr, "Cannot open disk %s (%s)\n",
				disk, sys_errlist[errno]);
			exit(1);
		}
	}
	else
	{
		int i, rv = 0;

		for(i = 0; disks[i]; i++)
		{
			disk = disks[i];
			rv = open_disk(u_flag);
			if(rv != -2) break;
		}
		if(rv < 0)
		{
			fprintf(stderr, "Cannot open any disk (%s)\n",
				sys_errlist[errno]);
			exit(1);
		}
	}

	printf("******* Working on device %s *******\n",disk);

	if (f_flag)
	{
	    if (read_s0() || i_flag)
	    {
		reset_boot();
	    }

	    if (!read_config(f_flag))
	    {
		exit(1);
	    }
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

	    if (u_flag || a_flag) {
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

usage:
	printf("fdisk {-a|-i|-u} [-f <config file> [-t] [-v]] [-{1,2,3,4}] [disk]\n");
	return(1);
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
	part_mb = partp->dp_size;
	part_mb *= secsize;
	part_mb /= (1024 * 1024);
	printf("sysid %d,(%s)\n", partp->dp_typ, get_type(partp->dp_typ));
	printf("    start %ld, size %ld (%qd Meg), flag %x\n",
		partp->dp_start,
		partp->dp_size, 
		part_mb,
		partp->dp_flag);
	printf("\tbeg: cyl %d/ sector %d/ head %d;\n\tend: cyl %d/ sector %d/ head %d\n"
		,DPCYL(partp->dp_scyl, partp->dp_ssect)
		,DPSECT(partp->dp_ssect)
		,partp->dp_shd
		,DPCYL(partp->dp_ecyl, partp->dp_esect)
		,DPSECT(partp->dp_esect)
		,partp->dp_ehd);
}


static void
init_boot(void)
{
	memcpy(mboot.bootinst, bootcode, sizeof(bootcode));
	mboot.signature = BOOT_MAGIC;
}


static void
init_sector0(unsigned long start)
{
struct dos_partition *partp = (struct dos_partition *) (&mboot.parts[3]);
unsigned long size = disksecs - start;

	init_boot();

	partp->dp_typ = DOSPTYP_386BSD;
	partp->dp_flag = ACTIVE;
	partp->dp_start = start;
	partp->dp_size = size;

	dos(partp->dp_start, partp->dp_size, 
	    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
	dos(partp->dp_start + partp->dp_size - 1, partp->dp_size,
	    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
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
		Decimal("sysid", partp->dp_typ, tmp);
		Decimal("start", partp->dp_start, tmp);
		Decimal("size", partp->dp_size, tmp);

		if (ok("Explicitly specifiy beg/end address ?"))
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
			dos(partp->dp_start, partp->dp_size,
			    &partp->dp_scyl, &partp->dp_ssect, &partp->dp_shd);
			dos(partp->dp_start + partp->dp_size - 1, partp->dp_size,
			    &partp->dp_ecyl, &partp->dp_esect, &partp->dp_ehd);
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
	if((dos_sectors > 63) || (dos_cyls > 1023) || (dos_heads > 255))
		printf("Figures below won't work with BIOS for partitions not in cyl 1\n");
	printf("parameters to be used for BIOS calculations are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d blks/cyl)\n\n"
		,dos_cyls,dos_heads,dos_sectors,dos_cylsecs);
}

static void
change_active(int which)
{
int i;
int active = 4, tmp;
struct dos_partition *partp = ((struct dos_partition *) &mboot.parts);

	if (a_flag && which != -1)
		active = which;
	if (!ok("Do you want to change the active partition?"))
		return;
	do
		Decimal("active partition", active, tmp);
	while (!ok("Are you happy with this choice"));
	for (i = 0; i < NDOSPART; i++)
		partp[i].dp_flag = 0;
	if (active > 0 && active <= NDOSPART)
		partp[active-1].dp_flag = ACTIVE;
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
unsigned char *c, *s, *h;
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
	sec = (sec - hd * dos_sectors) + 1;

	*h = hd;
	*c = cy & 0xff;
	*s = (sec & 0x3f) | ( (cy & 0x300) >> 2);
}

int fd;

	/* Getting device status */

static int
open_disk(int u_flag)
{
struct stat 	st;

	if (stat(disk, &st) == -1) {
		fprintf(stderr, "%s: Can't get file status of %s\n",
			name, disk);
		return -1;
	}
	if ( !(st.st_mode & S_IFCHR) )
		fprintf(stderr,"%s: Device %s is not character special\n",
			name, disk);
	if ((fd = open(disk, a_flag || u_flag ? O_RDWR : O_RDONLY)) == -1) {
		if(errno == ENXIO)
			return -2;
		fprintf(stderr,"%s: Can't open device %s\n", name, disk);
		return -1;
	}
	if (get_params(0) == -1) {
		fprintf(stderr, "%s: Can't get disk parameters on %s\n",
			name, disk);
		return -1;
	}
	return fd;
}

static ssize_t
read_disk(off_t sector, void *buf)
{
	lseek(fd,(sector * 512), 0);
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
}

static ssize_t
write_disk(off_t sector, void *buf)
{
	lseek(fd,(sector * 512), 0);
	/* write out in the size that the read_disk found worked */
	return write(fd, buf, secsize);
}

static int
get_params()
{

    if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
	fprintf(stderr,
		"%s: Can't get disk parameters on %s; supplying dummy ones\n",
		name, disk);
	dos_cyls = cyls = 1;
	dos_heads = heads = 1;
	dos_sectors = sectors = 1;
	dos_cylsecs = cylsecs = heads * sectors;
	disksecs = cyls * heads * sectors;
	return disksecs;
    }

    dos_cyls = cyls = disklabel.d_ncylinders;
    dos_heads = heads = disklabel.d_ntracks;
    dos_sectors = sectors = disklabel.d_nsectors;
    dos_cylsecs = cylsecs = heads * sectors;
    disksecs = cyls * heads * sectors;

    return (disksecs);
}


static int
read_s0()
{
	if (read_disk(0, (char *) mboot.bootinst) == -1) {
		fprintf(stderr, "%s: Can't read fdisk partition table\n", name);
		return -1;
	}
	if (mboot.signature != BOOT_MAGIC) {
		fprintf(stderr, "%s: Invalid fdisk partition table found\n",
			name);
		/* So should we initialize things */
		return -1;
	}
	return 0;
}

static int
write_s0()
{
	int	flag;
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
	flag = 1;
#ifdef NOT_NOW
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		perror("ioctl DIOCWLABEL");
#endif
	if (write_disk(0, (char *) mboot.bootinst) == -1) {
		fprintf(stderr, "%s: Can't write fdisk partition table\n",
			name);
		return -1;
	flag = 0;
#ifdef NOT_NOW
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
			printf("%s is an invalid decimal number.  Try again\n",
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
			printf("%s is an invalid hex number.  Try again\n",
				lbuf);
	}

}

static int
string(char *str, char **ans)
{
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

		if (c)
			*cp = 0;
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
		if(ptr->type == type)
		{
			return(ptr->name);
		}
		ptr++;
		counter++;
	}
	return("unknown");
}


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
	    fprintf(stderr,
		    "%s: ERROR line %d: the geometry specification line must occur before\n\
    all partition specifications.\n",
		    name, current_line_number);
	    status = 0;
	    break;
	}
	if (command->n_args != 3)
	{
	    fprintf(stderr,
		    "%s: ERROR line %d: incorrect number of geometry args\n",
		    name, current_line_number);
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
		fprintf(stderr,
			"%s: ERROR line %d: unknown geometry arg type: '%c' (0x%02x)\n",
			name, current_line_number, command->args[i].argtype,
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
	    fprintf(stderr,
		    "%s: ERROR line %d: number of cylinders not specified\n",
		    name, current_line_number);
	    status = 0;
	}
	if (dos_cyls == 0 || dos_cyls > 1024)
	{
	    fprintf(stderr,
		    "%s: WARNING line %d: number of cylinders (%d) may be out-of-range\n\
    (must be within 1-1024 for normal BIOS operation, unless the entire disk\n\
    is dedicated to FreeBSD).\n",
		    name, current_line_number, dos_cyls);
	}

	if (dos_heads < 0)
	{
	    fprintf(stderr,
		    "%s: ERROR line %d: number of heads not specified\n",
		    name, current_line_number);
	    status = 0;
	}
	else if (dos_heads < 1 || dos_heads > 256)
	{
	    fprintf(stderr,
		    "%s: ERROR line %d: number of heads must be within (1-256)\n",
		    name, current_line_number);
	    status = 0;
	}

	if (dos_sectors < 0)
	{
	    fprintf(stderr, "%s: ERROR line %d: number of sectors not specified\n",
		    name, current_line_number);
	    status = 0;
	}
	else if (dos_sectors < 1 || dos_sectors > 63)
	{
	    fprintf(stderr,
		    "%s: ERROR line %d: number of sectors must be within (1-63)\n",
		    name, current_line_number);
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
	    fprintf(stderr,
		    "%s: ERROR line %d: incorrect number of partition args\n",
		    name, current_line_number);
	    break;
	}
	partition = command->args[0].arg_val;
	if (partition < 1 || partition > 4)
	{
	    fprintf(stderr, "%s: ERROR line %d: invalid partition number %d\n",
		    name, current_line_number, partition);
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
		fprintf(stderr,
			"%s: ERROR line %d: unable to adjust start of partition %d to fall on\n\
    a cylinder boundary.\n",
			name, current_line_number, partition);
		break;
	    }
	    fprintf(stderr,
		    "%s: WARNING: adjusting start offset of partition '%d' from %d\n\
    to %d, to round to an head boundary.\n",
		    name, partition, partp->dp_start, adj_size);
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
	    fprintf(stderr,
		    "%s: WARNING: adjusting size of partition '%d' from %d to %d,\n\
    to round to a cylinder boundary.\n",
		    name, partition, partp->dp_size, adj_size);
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
	    fprintf(stderr,
		    "%s: ERROR line %d: size for partition '%d' is zero.\n",
		    name, current_line_number, partition);
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
	    fprintf(stderr,
		    "%s: ERROR line %d: incorrect number of active args\n",
		    name, current_line_number);
	    status = 0;
	    break;
	}
	partition = command->args[0].arg_val;
	if (partition < 1 || partition > 4)
	{
	    fprintf(stderr, "%s: ERROR line %d: invalid partition number %d\n",
		    name, current_line_number, partition);
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
