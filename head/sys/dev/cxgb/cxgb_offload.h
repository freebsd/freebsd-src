
/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/

#ifndef _CXGB_OFFLOAD_H
#define _CXGB_OFFLOAD_H

#include <common/cxgb_tcb.h>
#include <t3cdev.h>

MALLOC_DECLARE(M_CXGB);

struct adapter;
struct cxgb_client;

void cxgb_offload_init(void);
void cxgb_offload_exit(void);

void cxgb_adapter_ofld(struct adapter *adapter);
void cxgb_adapter_unofld(struct adapter *adapter);
int cxgb_offload_activate(struct adapter *adapter);
void cxgb_offload_deactivate(struct adapter *adapter);
int cxgb_ofld_recv(struct t3cdev *dev, struct mbuf **m, int n);

void cxgb_set_dummy_ops(struct t3cdev *dev);


/*
 * Client registration.  Users of T3 driver must register themselves.
 * The T3 driver will call the add function of every client for each T3
 * adapter activated, passing up the t3cdev ptr.  Each client fills out an
 * array of callback functions to process CPL messages.
 */

void cxgb_register_client(struct cxgb_client *client);
void cxgb_unregister_client(struct cxgb_client *client);
void cxgb_add_clients(struct t3cdev *tdev);
void cxgb_remove_clients(struct t3cdev *tdev);

typedef int (*cxgb_cpl_handler_func)(struct t3cdev *dev,
				      struct mbuf *m, void *ctx);

struct l2t_entry;
struct cxgb_client {
	char 			*name;
	void 			(*add) (struct t3cdev *);
	void 			(*remove) (struct t3cdev *);
	cxgb_cpl_handler_func 	*handlers;
	int			(*redirect)(void *ctx, struct rtentry *old,
					    struct rtentry *new,
					    struct l2t_entry *l2t);
	TAILQ_ENTRY(cxgb_client)         client_entry;
};

/*
 * TID allocation services.
 */
int cxgb_alloc_atid(struct t3cdev *dev, struct cxgb_client *client,
		     void *ctx);
int cxgb_alloc_stid(struct t3cdev *dev, struct cxgb_client *client,
		     void *ctx);
void *cxgb_free_atid(struct t3cdev *dev, int atid);
void cxgb_free_stid(struct t3cdev *dev, int stid);
void *cxgb_get_lctx(struct t3cdev *tdev, int stid);
void cxgb_insert_tid(struct t3cdev *dev, struct cxgb_client *client,
		      void *ctx,
	unsigned int tid);
void cxgb_queue_tid_release(struct t3cdev *dev, unsigned int tid);
void cxgb_remove_tid(struct t3cdev *dev, void *ctx, unsigned int tid);

struct toe_tid_entry {
	struct cxgb_client 	*client;
	void 			*ctx;
};

/* CPL message priority levels */
enum {
	CPL_PRIORITY_DATA = 0,     /* data messages */
	CPL_PRIORITY_SETUP = 1,	   /* connection setup messages */
	CPL_PRIORITY_TEARDOWN = 0, /* connection teardown messages */
	CPL_PRIORITY_LISTEN = 1,   /* listen start/stop messages */
	CPL_PRIORITY_ACK = 1,      /* RX ACK messages */
	CPL_PRIORITY_CONTROL = 1   /* offload control messages */
};

/* Flags for return value of CPL message handlers */
enum {
	CPL_RET_BUF_DONE = 1,   // buffer processing done, buffer may be freed
	CPL_RET_BAD_MSG = 2,    // bad CPL message (e.g., unknown opcode)
	CPL_RET_UNKNOWN_TID = 4	// unexpected unknown TID
};

typedef int (*cpl_handler_func)(struct t3cdev *dev, struct mbuf *m);

/*
 * Returns a pointer to the first byte of the CPL header in an sk_buff that
 * contains a CPL message.
 */
static inline void *cplhdr(struct mbuf *m)
{
	return mtod(m, uint8_t *);
}

void t3_register_cpl_handler(unsigned int opcode, cpl_handler_func h);

union listen_entry {
	struct toe_tid_entry toe_tid;
	union listen_entry *next;
};

union active_open_entry {
	struct toe_tid_entry toe_tid;
	union active_open_entry *next;
};

/*
 * Holds the size, base address, free list start, etc of the TID, server TID,
 * and active-open TID tables for a offload device.
 * The tables themselves are allocated dynamically.
 */
struct tid_info {
	struct toe_tid_entry *tid_tab;
	unsigned int ntids;
	volatile unsigned int tids_in_use;

	union listen_entry *stid_tab;
	unsigned int nstids;
	unsigned int stid_base;

	union active_open_entry *atid_tab;
	unsigned int natids;
	unsigned int atid_base;

	/*
	 * The following members are accessed R/W so we put them in their own
	 * cache lines.
	 *
	 * XXX We could combine the atid fields above with the lock here since
	 * atids are use once (unlike other tids).  OTOH the above fields are
	 * usually in cache due to tid_tab.
	 */
	struct mtx atid_lock /* ____cacheline_aligned_in_smp */;
	union active_open_entry *afree;
	unsigned int atids_in_use;

	struct mtx stid_lock /*____cacheline_aligned */;
	union listen_entry *sfree;
	unsigned int stids_in_use;
};

struct t3c_data {
	struct t3cdev *dev;
	unsigned int tx_max_chunk;  /* max payload for TX_DATA */
	unsigned int max_wrs;       /* max in-flight WRs per connection */
	unsigned int nmtus;
	const unsigned short *mtus;
	struct tid_info tid_maps;

	struct toe_tid_entry *tid_release_list;
	struct mtx tid_release_lock;
	struct task tid_release_task;
};

/*
 * t3cdev -> toe_data accessor
 */
#define T3C_DATA(dev) (*(struct t3c_data **)&(dev)->l4opt)

/*
 * Map an ATID or STID to their entries in the corresponding TID tables.
 */
static inline union active_open_entry *atid2entry(const struct tid_info *t,
                                                  unsigned int atid)
{
        return &t->atid_tab[atid - t->atid_base];
}


static inline union listen_entry *stid2entry(const struct tid_info *t,
                                             unsigned int stid)
{
        return &t->stid_tab[stid - t->stid_base];
}

/*
 * Find the connection corresponding to a TID.
 */
static inline struct toe_tid_entry *lookup_tid(const struct tid_info *t,
                                               unsigned int tid)
{
        return tid < t->ntids ? &(t->tid_tab[tid]) : NULL;
}

/*
 * Find the connection corresponding to a server TID.
 */
static inline struct toe_tid_entry *lookup_stid(const struct tid_info *t,
                                                unsigned int tid)
{
        if (tid < t->stid_base || tid >= t->stid_base + t->nstids)
                return NULL;
        return &(stid2entry(t, tid)->toe_tid);
}

/*
 * Find the connection corresponding to an active-open TID.
 */
static inline struct toe_tid_entry *lookup_atid(const struct tid_info *t,
                                                unsigned int tid)
{
        if (tid < t->atid_base || tid >= t->atid_base + t->natids)
                return NULL;
        return &(atid2entry(t, tid)->toe_tid);
}

void *cxgb_alloc_mem(unsigned long size);
void cxgb_free_mem(void *addr);
void cxgb_neigh_update(struct rtentry *rt, uint8_t *enaddr, struct sockaddr *sa);
void cxgb_redirect(struct rtentry *old, struct rtentry *new, struct sockaddr *sa);
int process_rx(struct t3cdev *dev, struct mbuf **m, int n);
int attach_t3cdev(struct t3cdev *dev);
void detach_t3cdev(struct t3cdev *dev);

#define CXGB_UNIMPLEMENTED() panic("IMPLEMENT: %s:%s:%d", __FUNCTION__, __FILE__, __LINE__)
#endif
