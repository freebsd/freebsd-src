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

/* #define DEBUG 1 */
/* You can define a particular architecture here if you are debugging. */
/* #define P_DEBUG p_sparc64 */

#define MAX_NO_DISKS	32
/* Max # of disks Disk_Names() will return */

#define MAX_SEC_SIZE    2048  /* maximum sector size that is supported */
#define MIN_SEC_SIZE	512   /* the sector size to end sensing at */

enum platform {
	p_any,			/* for debugging ! */
	p_alpha,
	p_i386,
	p_pc98,
	p_sparc64,
	p_ia64,
	p_ppc,
	p_amd64
};
extern const enum platform platform;

typedef enum {
	whole,
	unknown,

	sun,
	pc98,
	mbr,
	gpt,

	efi,
	fat,
	freebsd,
	extended,
	part,
	spare,
	unused
} chunk_e;

__BEGIN_DECLS
#ifndef __ia64__
struct disk {
	char		*name;
	u_long		bios_cyl;
	u_long		bios_hd;
	u_long		bios_sect;
#ifdef PC98
	u_char		*bootipl;
	size_t		bootipl_size;
	u_char		*bootmenu;
	size_t		bootmenu_size;
#else
	u_char		*bootmgr;
	size_t		bootmgr_size;
#endif
	u_char		*boot1;
#if defined(__i386__) || defined(__amd64__) /* the i386 needs extra help... */
	u_char		*boot2;
#endif
	struct chunk	*chunks;
	u_long		sector_size; /* media sector size, a power of 2 */
};
#else	/* !__ia64__ */
struct disk {
	char		*name;
	struct chunk	*chunks;
	u_long		media_size;
	u_long		sector_size;
	u_long		lba_start;
	u_long		lba_end;
	u_int		gpt_size;	/* Number of entries */
};
#endif

struct chunk {
	struct chunk	*next;
	struct chunk	*part;
	struct disk	*disk;
	long		offset;
	u_long		size;
	u_long		end;
	char		*sname;		/* PC98 field */
	char		*name;
	char		*oname;
	/* Used during Fixup_Names() to avoid renaming more than
	 * absolutely needed.
	 */
	chunk_e		type;
	int		subtype;
	u_long		flags;
	void		(*private_free)(void*);
	void		*(*private_clone)(void*);
	void		*private_data;
	/* For data private to the application, and the management
	 * thereof.  If the functions are not provided, no storage
	 * management is done, Cloning will just copy the pointer
	 * and freeing will just forget it.
	 */
};

/*
 * flags:
 *
 * ALIGN	-	This chunk should be aligned
 * IS_ROOT	-	This 'part' is a rootfs, allocate 'a'
 * ACTIVE	-	This is the active slice in the MBR
 * FORCE_ALL	-	Force a dedicated disk for FreeBSD, bypassing
 *			all BIOS geometry considerations
 * AUTO_SIZE	-	This chunk was auto-sized and can fill-out a
 *			following chunk if the following chunk is deleted.
 * NEWFS	-	newfs pending, used to enable auto-resizing on
 *			delete (along with AUTO_SIZE).
 */

#define CHUNK_ALIGN		0x0008
#define CHUNK_IS_ROOT		0x0010
#define CHUNK_ACTIVE		0x0020
#define CHUNK_FORCE_ALL		0x0040	
#define CHUNK_AUTO_SIZE		0x0080
#define CHUNK_NEWFS		0x0100
#define	CHUNK_HAS_INDEX		0x0200
#define	CHUNK_ITOF(i)		((i & 0xFFFF) << 16)
#define	CHUNK_FTOI(f)		((f >> 16) & 0xFFFF)

#define DELCHUNK_NORMAL		0x0000
#define DELCHUNK_RECOVER	0x0001

const char *chunk_name(chunk_e);

const char *
slice_type_name(int, int);
/* "chunk_n" for subtypes too */

struct disk *
Open_Disk(const char *);
/* Will open the named disk, and return populated tree.  */

void
Free_Disk(struct disk *);
/* Free a tree made with Open_Disk() or Clone_Disk() */

void
Debug_Disk(struct disk *);
/* Print the content of the tree to stdout */

void
Set_Bios_Geom(struct disk *, u_long, u_long, u_long);
/* Set the geometry the bios uses. */

void
Sanitize_Bios_Geom(struct disk *);
/* Set the bios geometry to something sane */

int
Insert_Chunk(struct chunk *, u_long, u_long, const char *, chunk_e, int,
	u_long, const char *);

int
Delete_Chunk2(struct disk *, struct chunk *, int);
/* Free a chunk of disk_space modified by the passed flags. */

int
Delete_Chunk(struct disk *, struct chunk *);
/* Free a chunk of disk_space */

void
Collapse_Disk(struct disk *);
/* Experimental, do not use. */

int
Collapse_Chunk(struct disk *, struct chunk *);
/* Experimental, do not use. */

int
Create_Chunk(struct disk *, u_long, u_long, chunk_e, int, u_long, const char *);
/* Create a chunk with the specified paramters */

void
All_FreeBSD(struct disk *, int);
/*
 * Make one FreeBSD chunk covering the entire disk;
 * if force_all is set, bypass all BIOS geometry
 * considerations.
 */

char *
CheckRules(const struct disk *);
/* Return char* to warnings about broken design rules in this disklayout */

char **
Disk_Names(void);
/*
 * Return char** with all disk's names (wd0, wd1 ...).  You must free
 * each pointer, as well as the array by hand
 */

#ifdef PC98
void
Set_Boot_Mgr(struct disk *, const u_char *, const size_t, const u_char *,
	const size_t);
#else
void
Set_Boot_Mgr(struct disk *, const u_char *, const size_t);
#endif
/*
 * Use this boot-manager on this disk.  Gets written when Write_Disk()
 * is called
 */

int
Set_Boot_Blocks(struct disk *, const u_char *, const u_char *);
/*
 * Use these boot-blocks on this disk.  Gets written when Write_Disk()
 * is called. Returns nonzero upon failure.
 */

int
Write_Disk(const struct disk *);
/* Write all the MBRs, disklabels, bootblocks and boot managers */

u_long
Next_Cyl_Aligned(const struct disk *, u_long);
/* Round offset up to next cylinder according to the bios-geometry */

u_long
Prev_Cyl_Aligned(const struct disk *, u_long);
/* Round offset down to previous cylinder according to the bios-geometry */

int
Track_Aligned(const struct disk *, u_long);
/* Check if offset is aligned on a track according to the bios geometry */

u_long
Next_Track_Aligned(const struct disk *, u_long);
/* Round offset up to next track according to the bios-geometry */

u_long
Prev_Track_Aligned(const struct disk *, u_long);
/* Check if offset is aligned on a track according to the bios geometry */

struct chunk *
Create_Chunk_DWIM(struct disk *, struct chunk *, u_long, chunk_e, int,
	u_long);
/*
 * This one creates a partition inside the given parent of the given
 * size, and returns a pointer to it.  The first unused chunk big
 * enough is used.
 */

char *
ShowChunkFlags(struct chunk *);
/* Return string to show flags. */

/*
 * Implementation details  >>> DO NOT USE <<<
 */

struct disklabel;

void Fill_Disklabel(struct disklabel *, const struct disk *,
	const struct chunk *);
void Debug_Chunk(struct chunk *);
void Free_Chunk(struct chunk *);
struct chunk *Clone_Chunk(const struct chunk *);
int Add_Chunk(struct disk *, long, u_long, const char *, chunk_e, int, u_long,
	const char *);
void *read_block(int, daddr_t, u_long);
int write_block(int, daddr_t, const void *, u_long);
struct disklabel *read_disklabel(int, daddr_t, u_long);
struct disk *Int_Open_Disk(const char *, char *);
int Fixup_Names(struct disk *);
int MakeDevChunk(const struct chunk *, const char *);
__END_DECLS

#define dprintf	printf

/* TODO
 *
 * Need an error string mechanism from the functions instead of warn()
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
 * Debug_Disk(wd0)  flags=0  bios_geom=0/0/0
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
