/*
 * $FreeBSD: src/sys/contrib/rdma/types.h,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef	__RDMA_TYPES_H_
#define	__RDMA_TYPES_H_
#include <sys/types.h>
#include <sys/malloc.h>


typedef uint8_t 	u8;
typedef uint16_t 	u16;
typedef uint32_t 	u32;
typedef uint64_t 	u64;
 
typedef uint8_t		__u8;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef uint64_t	__u64;
typedef uint8_t		__be8;
typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;

typedef	int32_t		__s32;


#define LINUX_TYPES_DEFINED
#define ERR_PTR(err) ((void *)((long)(err)))
#define IS_ERR(ptr)  ((unsigned long)(ptr) > (unsigned long)(-1000))
#define PTR_ERR(ptr)    ((long)(ptr))

#define PANIC_IF(exp) do {                  \
	if (exp)                            \
		panic("BUG func %s line %u: %s", __FUNCTION__, __LINE__, #exp);      \
} while (0)

#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))

static __inline int
find_first_zero_bit(volatile void *p, int max)
{
        int b;
        volatile int *ptr = (volatile int *)p;

        for (b = 0; b < max; b += 32) {
                if (ptr[b >> 5] != ~0) {
                        for (;;) {
                                if ((ptr[b >> 5] & (1 << (b & 0x1f))) == 0)
                                        return (b);
                                b++;
                        }
                }
        }

        return (max);
}

struct kvl {
        struct kvl *next;
        unsigned int key;
        void *value;
};

#define DEFINE_KVL(x) struct kvl x;

static __inline void *
kvl_lookup(struct kvl *x, uint32_t key)
{
        struct kvl *i;
        for (i=x->next;i;i=i->next) if (i->key==key) return(i->value);
        return(0);
}

static __inline int
kvl_alloc_above(struct kvl *idp, void *ptr, int starting_id, int *id)
{
	int newid = starting_id;
	struct kvl *i;

        for (i=idp->next;i;i=i->next) 
		if (i->key == newid)
			return -EEXIST;

        i=malloc(sizeof(struct kvl),M_TEMP,M_NOWAIT);
        i->key=newid;
        i->value=ptr;
        i->next=idp->next;
        idp->next=i;
	*id = newid;
        return(0);
}

static __inline void
kvl_delete(struct kvl *idp, int id) 
{
        /* leak */
        struct kvl *i, *prev=NULL;
        for (i=idp->next;i;prev=i,i=i->next) 
                if ((i)->key==id) {
			if (!prev)
				idp->next = i->next;
			else
				prev->next = i->next;
			free(i, M_TEMP);
                        return;
                }
}

static __inline void
kvl_free(struct kvl *idp)
{
        struct kvl *i, *tmp;
        for (i=idp->next;i;i=tmp) {
		tmp=i->next;
		free(i, M_TEMP);
	}
	idp->next = NULL;
}


#endif
