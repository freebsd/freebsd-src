
typedef enum {whole, foo, fat, freebsd, extended, part, unused, reserved} chunk_e;
#define CHAR_N static char *chunk_n[] = { \
	"whole","foo","fat","freebsd","extended","part","unused","reserved"};

struct disk {
	char		*name;
	u_long		flags;
#define DISK_ON_TRACK	1
#define DISK_REAL_GEOM	2
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
#define SUBTYPE_BSD_FS          1
#define SUBTYPE_BSD_SWAP        2
#define SUBTYPE_BSD_UNUSED      3
	u_long		flags;
#define CHUNK_PAST_1024		1
	/* this chunk cannot be booted from */
#define CHUNK_BSD_COMPAT	2
	/* this chunk is in the BSD-compatibility, and has a short name
         * too, ie wd0s4f -> wd0f
         */ 
#define CHUNK_BAD144		4
	/* this chunk has bad144 mapping */
#define CHUNK_ALIGN		8
};

struct disk *Open_Disk(char *devname);
void Free_Disk(struct disk *disk);
void Debug_Disk(struct disk *disk);
struct disk *Clone_Disk(struct disk *disk);

struct disk *Set_Phys_Geom(struct disk *disk, u_long cyl, u_long heads, u_long sects);
void Set_Bios_Geom(struct disk *disk, u_long cyl, u_long heads, u_long sects);

int Delete_Chunk(struct disk *disk, u_long offset, u_long end, chunk_e type);
void Collapse_Disk(struct disk *disk);
int Collapse_Chunk(struct disk *disk, struct chunk *chunk);

int Create_Chunk(struct disk *disk, u_long offset, u_long size, chunk_e type, int subtype, u_long flags);

/* Implementation details */

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
