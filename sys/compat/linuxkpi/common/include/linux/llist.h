/* Public domain. */

#ifndef _LINUXKPI_LINUX_LLIST_H
#define _LINUXKPI_LINUX_LLIST_H

#include <sys/types.h>
#include <machine/atomic.h>

struct llist_node {
	struct llist_node *next;
};

struct llist_head {
	struct llist_node *first;
};

#define	LLIST_HEAD_INIT(name)	{ NULL }
#define	LLIST_HEAD(name)	struct llist_head name = LLIST_HEAD_INIT(name)

#define llist_entry(ptr, type, member) \
	((ptr) ? container_of(ptr, type, member) : NULL)

static inline struct llist_node *
llist_del_all(struct llist_head *head)
{
	return ((void *)atomic_readandclear_ptr((uintptr_t *)&head->first));
}

static inline struct llist_node *
llist_del_first(struct llist_head *head)
{
	struct llist_node *first, *next;

	do {
		first = head->first;
		if (first == NULL)
			return NULL;
		next = first->next;
	} while (atomic_cmpset_ptr((uintptr_t *)&head->first,
	    (uintptr_t)first, (uintptr_t)next) == 0);

	return (first);
}

static inline bool
llist_add(struct llist_node *new, struct llist_head *head)
{
	struct llist_node *first;

	do {
		new->next = first = head->first;
	} while (atomic_cmpset_ptr((uintptr_t *)&head->first,
	    (uintptr_t)first, (uintptr_t)new) == 0);

	return (first == NULL);
}

static inline bool
llist_add_batch(struct llist_node *new_first, struct llist_node *new_last,
    struct llist_head *head)
{
	struct llist_node *first;

	do {
		new_last->next = first = head->first;
	} while (atomic_cmpset_ptr((uintptr_t *)&head->first,
	    (uintptr_t)first, (uintptr_t)new_first) == 0);

	return (first == NULL);
}

static inline void
init_llist_head(struct llist_head *head)
{
	head->first = NULL;
}

static inline bool
llist_empty(struct llist_head *head)
{
	return (head->first == NULL);
}

#define llist_for_each_safe(pos, n, node)				\
	for ((pos) = (node);						\
	    (pos) != NULL &&						\
	    ((n) = (pos)->next, pos);					\
	    (pos) = (n))

#define llist_for_each_entry_safe(pos, n, node, member) 		\
	for (pos = llist_entry((node), __typeof(*pos), member); 	\
	    pos != NULL &&						\
	    (n = llist_entry(pos->member.next, __typeof(*pos), member), pos); \
	    pos = n)

#define llist_for_each_entry(pos, node, member)				\
	for ((pos) = llist_entry((node), __typeof(*(pos)), member);	\
	    (pos) != NULL;						\
	    (pos) = llist_entry((pos)->member.next, __typeof(*(pos)), member))

#endif
