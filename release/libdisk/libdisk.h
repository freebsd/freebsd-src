/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: libdisk.h,v 1.2 1995/04/29 01:55:23 phk Exp $
 *
 */

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

/* 
 * Implementation details  >>> DO NOT USE <<<
 */

struct disk *Int_Open_Disk(char *devname, u_long maxsize);

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

#define dprintf	printf
