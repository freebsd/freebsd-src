#define	MAXSTR	20
/*
 *	Storage of one tuple.
 */
struct tuple {
	struct tuple *next;
	unsigned char code;
	int     length;
	unsigned char *data;
};

struct tuple_list {
	struct tuple_list *next;
	struct tuple *tuples;
	off_t   offs;
	int     flags;
};

struct tuple_info {
	char   *name;
	unsigned char code;
	unsigned char length;		/* 255 means variable length */
};

/*
 *	Memory device descriptor.
 */
struct dev_mem {
	unsigned char valid;
	unsigned char type;
	unsigned char speed;
	unsigned char wps;
	unsigned char addr;
	unsigned char units;
};

/*
 *	One I/O structure describing a possible I/O map
 *	of the card.
 */
struct cis_ioblk {
	struct cis_ioblk *next;
	unsigned int addr;
	unsigned int size;
};

/*
 *	A structure storing a memory map for the card.
 */
struct cis_memblk {
	struct cis_memblk *next;
	unsigned int address;
	unsigned int length;
	unsigned int host_address;
};

/*
 *	One configuration entry for the card.
 */
struct cis_config {
	struct cis_config *next;
	unsigned int pwr:1;		/* Which values are defined. */
	unsigned int timing:1;
	unsigned int iospace:1;
	unsigned int irq:1;
	unsigned int memspace:1;
	unsigned int misc_valid:1;
	unsigned char id;
	unsigned char io_blks;
	unsigned char io_addr;
	unsigned char io_bus;
	struct cis_ioblk *io;
	unsigned char irqlevel;
	unsigned char irq_flags;
	unsigned irq_mask;
	unsigned char memwins;
	struct cis_memblk *mem;
	unsigned char misc;
};

/*
 *	Structure holding all data retrieved from the
 *	CIS block on the card.
 *	The default configuration contains interface defaults
 *	not listed in each separate configuration.
 */
struct cis {
	struct tuple_list *tlist;
	char    manuf[MAXSTR];
	char    vers[MAXSTR];
	char    add_info1[MAXSTR];
	char    add_info2[MAXSTR];
	unsigned char maj_v, min_v;
	unsigned char last_config;
	unsigned char ccrs;
	unsigned long reg_addr;
	struct dev_mem attr_mem;
	struct dev_mem common_mem;
	struct cis_config *def_config;
	struct cis_config *conf;
};

void   *xmalloc(int);
struct cis *readcis(int);
void    dumpcis(struct cis *);
void    freecis(struct cis *);
struct tuple_list *read_tuples(int);
struct tuple *find_tuple(struct cis *, unsigned char);
struct tuple *find_tuple_in_list(struct tuple_list *, unsigned char);

struct tuple_info *get_tuple_info(unsigned char);
char   *tuple_name(unsigned char);
