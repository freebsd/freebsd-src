/*
 * linux/fs/hfs/hfs_btree.h
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the declarations of the private B-tree
 * structures and functions.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 */

#ifndef _HFS_BTREE_H
#define _HFS_BTREE_H

#include "hfs.h"

/*================ Variable-like macros ================*/

/* The stickiness of a (struct hfs_bnode) */
#define HFS_NOT_STICKY	0
#define HFS_STICKY	1

/* The number of hash buckets in a B-tree's bnode cache */
#define HFS_CACHELEN	17	/* primes are best? */

/*
 * Legal values for the 'ndType' field of a (struct NodeDescriptor)
 *
 * Reference: _Inside Macintosh: Files_ p. 2-65
 */
#define ndIndxNode	0x00	/* An internal (index) node */
#define ndHdrNode	0x01	/* The tree header node (node 0) */
#define ndMapNode	0x02	/* Holds part of the bitmap of used nodes */
#define ndLeafNode	0xFF	/* A leaf (ndNHeight==1) node */

/*
 * Legal values for the bthAtrb field of a (struct BTHdrRec)
 *
 * Reference: TN 1150
 */
#define bthBadClose     0x00000001  /* b-tree not closed properly. not
                                       used by hfsplus. */
#define bthBigKeys      0x00000002  /* key length is u16 instead of u8.
				       used by hfsplus. */
#define bthVarIndxKeys  0x00000004  /* variable key length instead of
                                       max key length. use din catalog
                                       b-tree but not in extents
                                       b-tree (hfsplus). */

/*================ Function-like macros ================*/

/* Access the cache slot which should contain the desired node */
#define bhash(tree, node) ((tree)->cache[(node) % HFS_CACHELEN])

/* round up to multiple of sizeof(hfs_u16) */
#define ROUND(X) ((X + sizeof(hfs_u16) - 1) & ~(sizeof(hfs_u16)-1))

/* Refer to the (base-1) array of offsets in a bnode */
#define RECTBL(X,N) \
	(((hfs_u16 *)(hfs_buffer_data((X)->buf)+HFS_SECTOR_SIZE))-(N))

/*================ Private data types ================*/

/*
 * struct BTHdrRec
 *
 * The B-tree header record
 *
 * This data structure is stored in the first node (512-byte block) of
 * each B-tree file.  It contains important information about the
 * B-tree.  Most fields vary over the life of the tree and are
 * indicated by a 'V' in the comments.	The other fields are fixed for
 * the life of the tree and are indicated by a 'F'.
 *
 * Reference: _Inside Macintosh: Files_ pp. 2-68 through 2-69 */
struct BTHdrRec {
	hfs_word_t  bthDepth;	/* (V) The number of levels in this B-tree */
	hfs_lword_t bthRoot;	/* (V) The node number of the root node */
	hfs_lword_t bthNRecs;	/* (V) The number of leaf records */
	hfs_lword_t bthFNode;	/* (V) The number of the first leaf node */
	hfs_lword_t bthLNode;	/* (V) The number of the last leaf node */
	hfs_word_t  bthNodeSize;	/* (F) The number of bytes in a node (=512) */
	hfs_word_t  bthKeyLen;	/* (F) The length of a key in an index node */
	hfs_lword_t bthNNodes;	/* (V) The total number of nodes */
	hfs_lword_t bthFree;	/* (V) The number of unused nodes */
        hfs_word_t  bthResv1;   /* reserved */
        hfs_lword_t bthClpSiz;  /* (F) clump size. not usually used. */
        hfs_byte_t  bthType;    /* (F) BTree type */
        hfs_byte_t  bthResv2;   /* reserved */
        hfs_lword_t bthAtrb;    /* (F) attributes */
        hfs_lword_t bthResv3[16]; /* Reserved */
} __attribute__((packed));

/*
 * struct NodeDescriptor
 *
 * The B-tree node descriptor.
 *
 * This structure begins each node in the B-tree file.	It contains
 * important information about the node's contents.  'V' and 'F' in
 * the comments indicate fields that are variable or fixed over the
 * life of a node, where the 'life' of a node is defined as the period
 * between leaving and reentering the free pool.
 *
 * Reference: _Inside Macintosh: Files_ p. 2-64
 */
struct NodeDescriptor {
	hfs_lword_t ndFLink;	/* (V) Number of the next node at this level */
	hfs_lword_t ndBLink;	/* (V) Number of the prev node at this level */
	hfs_byte_t  ndType;	/* (F) The type of node */
	hfs_byte_t  ndNHeight;	/* (F) The level of this node (leaves=1) */
	hfs_word_t  ndNRecs;	/* (V) The number of records in this node */
	hfs_word_t  ndResv2;	/* Reserved */
} __attribute__((packed));

/*
 * typedef hfs_cmpfn
 *
 * The type 'hfs_cmpfn' is a comparison function taking 2 keys and
 * returning a positive, negative or zero integer according to the
 * ordering of the two keys (just like strcmp() does for strings).
 */
typedef int (*hfs_cmpfn)(const void *, const void *);

/*
 * struct hfs_bnode
 *
 * An in-core B-tree node
 *
 * This structure holds information from the NodeDescriptor in native
 * byte-order, a pointer to the buffer which contains the actual
 * node and fields necessary for locking access to the node during
 * updates.  The use of the locking fields is explained with the
 * locking functions.
 */
struct hfs_bnode {
	int		    magic;   /* Magic number to guard against
					wild pointers */
	hfs_buffer	    buf;     /* The buffer containing the
					actual node */
	struct hfs_btree    *tree;   /* The tree to which this node
					belongs */
	struct hfs_bnode    *prev;   /* Next node in this hash bucket */
	struct hfs_bnode    *next;   /* Previous node in this hash
					bucket */
	int		    sticky;  /* Boolean: non-zero means keep
					this node in-core (set for
					root and head) */
	hfs_u32		    node;    /* Node number */
	hfs_u16             nodeSize; /* node size */
        hfs_u16             keyLen;  /* key length */
	/* locking related fields: */
	hfs_wait_queue	    wqueue;  /* Wait queue for write access */
	hfs_wait_queue	    rqueue;  /* Wait queue for read or reserve
					access */
	int		    count;   /* Number of processes accessing
					this node */
	int		    resrv;   /* Boolean, true means a process
					had placed a 'reservation' on
					this node */
	int		    lock;    /* Boolean, true means some
					process has exclusive access,
					so KEEP OUT */
	/* fields from the NodeDescriptor in native byte-order: */
	hfs_u32		    ndFLink;
	hfs_u32		    ndBLink;
	hfs_u16		    ndNRecs;
	hfs_u8		    ndType;
	hfs_u8		    ndNHeight;
};

/*
 * struct hfs_btree
 *
 * An in-core B-tree.
 *
 * This structure holds information from the BTHdrRec, MDB
 * (superblock) and other information needed to work with the B-tree.
 */
struct hfs_btree {
	int			magic;	       /* Magic number to
						  guard against wild
						  pointers */
	hfs_cmpfn		compare;       /* Comparison function
						  for this tree */
	struct hfs_bnode	head;	       /* in-core copy of node 0 */
	struct hfs_bnode	*root;	       /* Pointer to the in-core
						  copy of the root node */
	hfs_sysmdb		sys_mdb;       /* The "device" holding
						  the filesystem */
	int			reserved;      /* bnodes claimed but
						  not yet used */
	struct hfs_bnode		       /* The bnode cache */
				*cache[HFS_CACHELEN];
	struct hfs_cat_entry	entry;	       /* Fake catalog entry */
	int			lock;
	hfs_wait_queue		wait;
	int			dirt;
	int                     keySize;   
	/* Fields from the BTHdrRec in native byte-order: */
	hfs_u32			bthRoot;
	hfs_u32			bthNRecs;
	hfs_u32			bthFNode;
	hfs_u32			bthLNode;
	hfs_u32			bthNNodes;
	hfs_u32			bthFree;
	hfs_u16			bthKeyLen;
	hfs_u16			bthDepth;
};

/*================ Global functions ================*/

/* Convert a (struct hfs_bnode *) and an index to the value of the
   n-th offset in the bnode (N >= 1) to the offset */
extern inline hfs_u16 bnode_offset(const struct hfs_bnode *bnode, int n)
{ return hfs_get_hs(RECTBL(bnode,n)); }

/* Convert a (struct hfs_bnode *) and an index to the size of the
   n-th record in the bnode (N >= 1) */
extern inline hfs_u16 bnode_rsize(const struct hfs_bnode *bnode, int n)
{ return bnode_offset(bnode, n+1) - bnode_offset(bnode, n); }

/* Convert a (struct hfs_bnode *) to the offset of the empty part */
extern inline hfs_u16 bnode_end(const struct hfs_bnode *bnode)
{ return bnode_offset(bnode, bnode->ndNRecs + 1); }

/* Convert a (struct hfs_bnode *) to the number of free bytes it contains */
extern inline hfs_u16 bnode_freespace(const struct hfs_bnode *bnode)
{ return HFS_SECTOR_SIZE - bnode_end(bnode)
	  - (bnode->ndNRecs + 1)*sizeof(hfs_u16); }

/* Convert a (struct hfs_bnode *) X and an index N to
   the address of the record N in the bnode (N >= 1) */
extern inline void *bnode_datastart(const struct hfs_bnode *bnode)
{ return (void *)(hfs_buffer_data(bnode->buf)+sizeof(struct NodeDescriptor)); }

/* Convert a (struct hfs_bnode *) to the address of the empty part */
extern inline void *bnode_dataend(const struct hfs_bnode *bnode)
{ return (void *)(hfs_buffer_data(bnode->buf) + bnode_end(bnode)); }

/* Convert various pointers to address of record's key */
extern inline void *bnode_key(const struct hfs_bnode *bnode, int n)
{ return (void *)(hfs_buffer_data(bnode->buf) + bnode_offset(bnode, n)); }
extern inline void *belem_key(const struct hfs_belem *elem)
{ return bnode_key(elem->bnr.bn, elem->record); }
extern inline void *brec_key(const struct hfs_brec *brec)
{ return belem_key(brec->bottom); }

/* Convert various pointers to the address of a record */
extern inline void *bkey_record(const struct hfs_bkey *key)
{ return (void *)key + ROUND(key->KeyLen + 1); }
extern inline void *bnode_record(const struct hfs_bnode *bnode, int n)
{ return bkey_record(bnode_key(bnode, n)); }
extern inline void *belem_record(const struct hfs_belem *elem)
{ return bkey_record(belem_key(elem)); }
extern inline void *brec_record(const struct hfs_brec *brec)
{ return bkey_record(brec_key(brec)); }

/*================ Function Prototypes ================*/

/* balloc.c */
extern int hfs_bnode_bitop(struct hfs_btree *, hfs_u32, int);
extern struct hfs_bnode_ref hfs_bnode_alloc(struct hfs_btree *);
extern int hfs_bnode_free(struct hfs_bnode_ref *);
extern void hfs_btree_extend(struct hfs_btree *);

/* bins_del.c */
extern void hfs_bnode_update_key(struct hfs_brec *, struct hfs_belem *,
				 struct hfs_bnode *, int);
extern void hfs_bnode_shift_right(struct hfs_bnode *, struct hfs_bnode *, int);
extern void hfs_bnode_shift_left(struct hfs_bnode *, struct hfs_bnode *, int);
extern int hfs_bnode_in_brec(hfs_u32 node, const struct hfs_brec *brec);

/* bnode.c */
extern void hfs_bnode_read(struct hfs_bnode *, struct hfs_btree *,
			   hfs_u32, int);
extern void hfs_bnode_relse(struct hfs_bnode_ref *);
extern struct hfs_bnode_ref hfs_bnode_find(struct hfs_btree *, hfs_u32, int);
extern void hfs_bnode_lock(struct hfs_bnode_ref *, int);
extern void hfs_bnode_delete(struct hfs_bnode *);
extern void hfs_bnode_commit(struct hfs_bnode *);

/* brec.c */
extern void hfs_brec_lock(struct hfs_brec *, struct hfs_belem *);
extern struct hfs_belem *hfs_brec_init(struct hfs_brec *, struct hfs_btree *,
				       int);
extern struct hfs_belem *hfs_brec_next(struct hfs_brec *);

#endif
