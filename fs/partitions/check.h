/*
 * add_partition adds a partitions details to the devices partition
 * description.
 */
void add_gd_partition(struct gendisk *hd, int minor, int start, int size);

typedef struct {struct page *v;} Sector;

unsigned char *read_dev_sector(struct block_device *, unsigned long, Sector *);

static inline void put_dev_sector(Sector p)
{
	page_cache_release(p.v);
}

extern int warn_no_part;
