/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: libdisk.h,v 1.19.2.2 1995/10/13 08:19:12 jkh Exp $
 *
 */

#define MAX_NO_DISKS	20
	/* Max # of disks Disk_Names() will return */

typedef enum {
	whole,
	unknown,
	fat,
	freebsd,
	extended,
	part,
	unused,
} chunk_e;

__BEGIN_DECLS
struct disk {
	char		*name;
	u_long		flags;
#		define DISK_ON_TRACK	1
#if 0
	u_long		real_cyl;
	u_long		real_hd;
	u_long		real_sect;
#endif
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
	struct disk	*disk;
	long		offset;
	u_long		size;
	u_long		end;
	char		*name;
	char		*oname;
	/* Used during Fixup_Names() to avoid renaming more than
	 * absolutely needed.
	 */
	chunk_e		type;
	int		subtype;
	u_long		flags;
#		define CHUNK_PAST_1024		1
		/* this chunk cannot be booted from because it
		 * extends past cylinder 1024
		 */
#		define CHUNK_BSD_COMPAT	2
		/* this chunk is in the BSD-compatibility, and has a
		 * short name too, ie wd0s4f -> wd0f
		*/
#		define CHUNK_BAD144		4
		/* this chunk has bad144 mapping */
#		define CHUNK_ALIGN		8
		/* This chunk should be aligned */
#		define CHUNK_IS_ROOT		16
		/* This 'part' is a rootfs, allocate 'a' */
#		define CHUNK_ACTIVE		32
		/* This is the active slice in the MBR */
#		define CHUNK_FORCE_ALL		64
		/* Force a dedicated disk for FreeBSD, bypassing
		 * all BIOS geometry considerations
		 */

	void		(*private_free)(void*);
	void		*(*private_clone)(void*);
	void		*private_data;
	/* For data private to the application, and the management
	 * thereof.  If the functions are not provided, no storage
	 * management is done, Cloning will just copy the pointer
	 * and freeing will just forget it.
	 */
};

extern const char *chunk_n[];
extern const u_char *boot1, *boot2;

struct disk *
Open_Disk(const char *devname);
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

#if 0
struct disk *
Set_Phys_Geom(struct disk *disk, u_long cyl, u_long heads, u_long sects);
/* Use a different physical geometry.  Makes sense for ST506 disks only.
 * The tree returned is read from the disk, using this geometry.
 */
#endif

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
Create_Chunk(struct disk *disk, u_long offset, u_long size, chunk_e type,
	int subtype, u_long flags);
/* Create a chunk with the specified paramters
 */

void
All_FreeBSD(struct disk *d, int force_all);
/* Make one FreeBSD chunk covering the entire disk;
 * if force_all is set, bypass all BIOS geometry
 * considerations.
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
Set_Boot_Mgr(struct disk *d, const u_char *bootmgr);
/* Use this boot-manager on this disk.  Gets written when Write_Disk()
 * is called
 */

void
Set_Boot_Blocks(struct disk *d, const u_char *boot1, const u_char *boot2);
/* Use these boot-blocks on this disk.  Gets written when Write_Disk()
 * is called
 */

int
Write_Disk(struct disk *d);
/* Write all the MBRs, disklabels, bootblocks and boot managers
 */

int
Cyl_Aligned(struct disk *d, u_long offset);
/* Check if offset is aligned on a cylinder according to the
 * bios geometry
 */

u_long
Next_Cyl_Aligned(struct disk *d, u_long offset);
/* Round offset up to next cylinder according to the bios-geometry
 */

u_long
Prev_Cyl_Aligned(struct disk *d, u_long offset);
/* Round offset down to previous cylinder according to the bios-
 * geometry
 */

int
Track_Aligned(struct disk *d, u_long offset);
/* Check if offset is aligned on a track according to the
 * bios geometry
 */

u_long
Next_Track_Aligned(struct disk *d, u_long offset);
/* Round offset up to next track according to the bios-geometry
 */

u_long
Prev_Track_Aligned(struct disk *d, u_long offset);
/* Check if offset is aligned on a track according to the
 * bios geometry
 */

struct chunk *
Create_Chunk_DWIM(struct disk *d, struct chunk *parent , u_long size,
	chunk_e type, int subtype, u_long flags);
/* This one creates a partition inside the given parent of the given
 * size, and returns a pointer to it.  The first unused chunk big
 * enough is used.
 */

int
MakeDev(struct chunk *c, const char *path);

int
MakeDevDisk(struct disk *d, const char *path);
/* Make device nodes for all chunks on this disk */

char *
ShowChunkFlags(struct chunk *c);
/* Return string to show flags. */

char *
ChunkCanBeRoot(struct chunk *c);
/* Return NULL if chunk can be /, explanation otherwise */

/*
 * Implementation details  >>> DO NOT USE <<<
 */

void Debug_Chunk(struct chunk *);
void Free_Chunk(struct chunk *);
struct chunk * Clone_Chunk(struct chunk *);
int Add_Chunk(struct disk *, long, u_long, const char *, chunk_e, int, u_long);
void Bios_Limit_Chunk(struct chunk *, u_long);
void * read_block(int, daddr_t);
void write_block(int fd, daddr_t block, void *foo);
struct disklabel * read_disklabel(int, daddr_t);
u_short	dkcksum(struct disklabel *);
struct chunk * Find_Mother_Chunk(struct chunk *, u_long, u_long, chunk_e);
struct disk * Int_Open_Disk(const char *name, u_long size);
void Fixup_Names(struct disk *);
__END_DECLS

#define dprintf	printf

/* TODO
 *
 * Need a error string mechanism from the functions instead of warn()
 *
 * Make sure only FreeBSD start at offset==0
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
 * Make Is_Unchanged(struct disk *d1, struct chunk *c1)
 *
 * don't rename slices unless we have to
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
 * >>>>>>    0x3d2c0     960120         63     960182 -        6 unused   0 0
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
 *		  <wd0s2> --> <unused>
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
