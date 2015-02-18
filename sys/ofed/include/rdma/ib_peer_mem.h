#if !defined(IB_PEER_MEM_H)
#define IB_PEER_MEM_H

#include <rdma/peer_mem.h>


struct invalidation_ctx;
struct ib_ucontext;

struct ib_peer_memory_statistics {
	unsigned long num_alloc_mrs;
	unsigned long num_dealloc_mrs;
	unsigned long num_reg_pages;
	unsigned long num_dereg_pages;
	unsigned long num_free_callbacks;
};

struct ib_peer_memory_client {
	const struct peer_memory_client *peer_mem;

	struct list_head	core_peer_list;
	struct list_head    core_ticket_list;
	unsigned long last_ticket;
#ifdef __FreeBSD__
	int holdcount;
	int needwakeup;
	struct cv peer_cv;
#else
	struct srcu_struct peer_srcu;
#endif
	struct mutex lock;
	struct kobject *kobj;
	struct attribute_group peer_mem_attr_group;
	struct ib_peer_memory_statistics stats;
};

struct core_ticket {
	unsigned long key;
	void *context;
	struct list_head   ticket_list;
};

struct ib_peer_memory_client *ib_get_peer_client(struct ib_ucontext *context, unsigned long addr,
						  size_t size, void **peer_client_context,
						  int *srcu_key);

void ib_put_peer_client(struct ib_peer_memory_client *ib_peer_client,
				void *peer_client_context,
				int srcu_key);

unsigned long ib_peer_insert_context(struct ib_peer_memory_client *ib_peer_client,
				void *context);
int ib_peer_remove_context(struct ib_peer_memory_client *ib_peer_client,
				unsigned long key);
struct core_ticket *ib_peer_search_context(struct ib_peer_memory_client *ib_peer_client,
						unsigned long key);
#endif


