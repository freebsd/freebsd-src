/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: libdisk.h,v 1.3 1995/04/29 04:00:55 phk Exp $
 *
 */

#define MAX_NO_DISKS	20
	/* Max # of disks Disk_Names() will return */

typedef enum {whole, foo, fat, freebsd, extended, part, unused, reserved} chunk_e;

#define CHAR_N static char *chunk_n[] = { \
	"whole","foo","fat","freebsd","extended","part","unused","reserved",0};

struct disk {
	char		*name;
	u_long		flags;
#		define DISK_ON_TRACK	1
#		define DISK_REAL_GEOM	2
	u_long		real_cyl;
	u_long		real_hd;
	u_long		real_sect;
	u_long		bios_cyl;
	u_long		bios_hd;
	u_long		bios_sect;
	u_char		*bootmgr;
	u_char		*boot1;
	u_char		*boot2;
	struct chunk	*chunks;
};

struct chunk {
	struct chunk	*next;
	struct chunk	*part;
	u_long		offset;
	u_long		size;
	u_long		end;
	char		*name;
	chunk_e		type;
	int		subtype;
#		define SUBTYPE_BSD_FS          1
#		define SUBTYPE_BSD_SWAP        2
#		define SUBTYPE_BSD_UNUSED      3
	u_long		flags;
#		define CHUNK_PAST_1024		1
			/* this chunk cannot be booted from */
#		define CHUNK_BSD_COMPAT	2
			/* this chunk is in the BSD-compatibility, and has a 
			 * short name too, ie wd0s4f -> wd0f
         		*/ 
#		define CHUNK_BAD144		4
			/* this chunk has bad144 mapping */
#		define CHUNK_ALIGN		8
};

struct disk *
Open_Disk(char *devname);
	/* Will open the named disk, and return populated tree.
	 */	

struct disk *
Clone_Disk(struct disk *disk);
	/* Clone a copy of a tree.  Useful for "Undo" functionality
	 */

void 
Free_Disk(struct disk *disk);
	/* Free a tree made with Open_Disk() or Clone_Disk()
	 */

void
Debug_Disk(struct disk *disk);
	/* Print the content of the tree to stdout
	 */

struct disk *
Set_Phys_Geom(struct disk *disk, u_long cyl, u_long heads, u_long sects);
	/* Use a different physical geometry.  Makes sense for ST506 disks only.
	 * The tree returned is read from the disk, using this geometry.
	 */

void 
Set_Bios_Geom(struct disk *disk, u_long cyl, u_long heads, u_long sects);
	/* Set the geometry the bios uses.
	 */

int 
Delete_Chunk(struct disk *disk, struct chunk *);
	/* Free a chunk of disk_space 
	 */

void
Collapse_Disk(struct disk *disk);
	/* Experimental, do not use.
	 */
int
Collapse_Chunk(struct disk *disk, struct chunk *chunk);
	/* Experimental, do not use.
	 */

int 
Create_Chunk(struct disk *disk, u_long offset, u_long size, chunk_e type, int subtype, u_long flags);
	/* Create a chunk with the specified paramters
	 */

void
All_FreeBSD(struct disk *d);
	/* Make one FreeBSD chunk covering the entire disk
	 */

char * 
CheckRules(struct disk *);
	/* Return char* to warnings about broken design rules in this disklayout
	 */

char **
Disk_Names();
	/* Return char** with all disk's names (wd0, wd1 ...).  You must free
	 * each pointer, as well as the array by hand
	 */

void
Set_Boot_Mgr(struct disk *d, u_char *bootmgr);
	/* Use this boot-manager on this disk.  Gets written when Write_Disk()
	 * is called
	 */

void
Set_Boot_Blocks(struct disk *d, u_char *boot1, u_char *boot2);
	/* Use these boot-blocks on this disk.  Gets written when Write_Disk()
	 * is called
	 */

/* 
 * Implementation details  >>> DO NOT USE <<<
 */

void Debug_Chunk(struct chunk *);
void Free_Chunk(struct chunk *);
struct chunk * Clone_Chunk(struct chunk *);
int Add_Chunk(struct disk *, u_long , u_long , char *, chunk_e, int , u_long);
void Bios_Limit_Chunk(struct chunk *, u_long);
void * read_block(int, daddr_t );
struct disklabel * read_disklabel(int, daddr_t);
u_short	dkcksum(struct disklabel *);
int Aligned(struct disk *d, u_long offset);
u_long Next_Aligned(struct disk *d, u_long offset);
u_long Prev_Aligned(struct disk *d, u_long offset);
struct chunk * Find_Mother_Chunk(struct chunk *, u_long , u_long , chunk_e);
struct disk * Int_Open_Disk(char *name, u_long size);

#define dprintf	printf

/* TODO
 *
 * Need a error string mechanism from the functions instead of warn()
 * 
 * Make sure only FreeBSD start at offset==0
 * 
 * Make sure all MBR+extended children are aligned at create.
 * 
 * Collapse must align.
 * 
 * Make Write_Disk(struct disk*)
 * 
 * Consider booting from OnTrack'ed disks.
 *
 * Get Bios-geom, ST506 & OnTrack from driver (or otherwise)
 *
 * Make Create_DWIM().
 *
 *
 *Sample output from tst01:
 *
 * Debug_Disk(wd0)  flags=0  real_geom=0/0/0  bios_geom=0/0/0
 * >>        0x3d040          0    1411200    1411199 wd0      0 whole    0 0
 * >>>>      0x3d080          0     960120     960119 wd0s1    3 freebsd  0 8
 * >>>>>>    0x3d100          0      40960      40959 wd0s1a   5 part     0 0
 * >>>>>>    0x3d180      40960     131072     172031 wd0s1b   5 part     0 0
 * >>>>>>    0x3d1c0     172032     409600     581631 wd0s1e   5 part     0 0
 * >>>>>>    0x3d200     581632     378488     960119 wd0s1f   5 part     0 0
 * >>>>      0x3d140     960120       5670     965789 wd0s2    4 extended 0 8
 * >>>>>>    0x3d240     960120          1     960120 -        7 reserved 0 8
 * >>>>>>    0x3d2c0     960121         62     960182 -        6 unused   0 0
 * >>>>>>    0x3d0c0     960183       5607     965789 wd0s5    2 fat      0 8
 * >>>>      0x3d280     965790       1890     967679 wd0s3    1 foo      -2 8
 * >>>>      0x3d300     967680     443520    1411199 wd0s4    3 freebsd  0 8
 * >>>>>>    0x3d340     967680     443520    1411199 wd0s4a   5 part     0 0
 *
 * ^            ^           ^          ^          ^     ^      ^ ^        ^ ^
 * level    chunkptr      start      size        end  name    type  subtype flags
 *
 * Underlying data structure:
 *
 *	Legend:
 *		<struct chunk> --> part
 *			|
 *			v next
 *
 *	<wd0> --> <wd0s1> --> <wd0s1a>
 *		     |           |
 *		     |           v
 *		     |        <wd0s1b>
 *		     |           |
 *		     |           v
 *		     |        <wd0s1e>
 *		     |           |
 *		     |           v
 *		     |        <wd0s1f>
 *		     |
 *		     v
 *		  <wd0s2> --> <reserved>
 *		     |           |
 *		     |           v
 *		     |        <unused>
 *		     |           |
 *		     |           v
 *		     |        <wd0s5>
 *		     |
 *		     v
 *		  <wd0s3>	
 *		     |
 *		     v
 *		  <wd0s4> --> <wd0s4a>
 *
 *
 */
