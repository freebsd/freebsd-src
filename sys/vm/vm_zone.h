
#if !defined(_SYS_ZONE_H)

#define _SYS_ZONE_H

#define ZONE_COLOR 1
#define ZONE_INTERRUPT 2
#define ZONE_WAIT 4
#define ZONE_PREALLOCATE 8
#define ZONE_BOOT 16

#include	<machine/param.h>
#include	<sys/lock.h>


#define CACHE_LINE_SIZE 32

typedef struct vm_zone {
	struct	simplelock		zlock;			/* lock for data structure */
	void					*zitems;		/* linked list of items */
	int						zfreemin;		/* minimum number of free entries */
	int						zfreecnt;		/* free entries */
	vm_offset_t				zkva;			/* Base kva of zone */
	int						zpagecount;		/* Total # of allocated pages */
	int						zpagemax;		/* Max address space */
	int						zsize;			/* size of each entry */
	int						zalloc;			/* hint for # of pages to alloc */
	int						zflags;			/* flags for zone */
	int						zallocflag;		/* flag for allocation */
	struct	vm_object		*zobj;			/* object to hold zone */
	char					*zname;			/* name for diags */
} *vm_zone_t;


vm_zone_t zinit(char *name, int size, int nentries, int flags, int zalloc);
int _zinit(vm_zone_t z, struct vm_object *obj, char *name, int size,
		int nentries, int flags, int zalloc);
static void * zalloc(vm_zone_t z);
static void zfree(vm_zone_t z, void *item);
void * zalloci(vm_zone_t z) __attribute__((regparm(1)));
void zfreei(vm_zone_t z, void *item) __attribute__((regparm(2)));
void _zbootinit(vm_zone_t z, char *name, int size, void *item, int nitems) ;
void * zget(vm_zone_t z, int s) __attribute__((regparm(2)));

#if SMP > 1

static __inline__ void *
zalloc(vm_zone_t z) {
	return zalloci(z);
}

static __inline__ void
zfree(vm_zone_t z, void *item) {
	zfreei(z, item);
}

#else

static __inline__ void *
zalloc(vm_zone_t z) {
	int s;
	void *item;

	if (z->zfreecnt <= z->zfreemin) {
		return zget(z, s);
	}
	
	item = z->zitems;
	z->zitems = *(void **) item;
	--z->zfreecnt;
	return item;
}

static __inline__ void
zfree(vm_zone_t z, void *item) {
	* (void **) item = z->zitems;
	z->zitems = item;
	++z->zfreecnt;
}
#endif

#endif
