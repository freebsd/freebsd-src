/* XXX: use queue(3) instead */

#ifndef _FBSD_LIST_H_
#define _FBSD_LIST_H_

#define	container_of	__containerof

struct list_head {
	struct list_head *next,*last;
};
#define INIT_LIST_HEAD(a) {(a)->next=(a)->last=(a);}
#define list_entry(p,t,m) container_of(p, t, m)

#define list_empty(a) ((a)->next==(a))

#define list_del(a)\
        {(a)->last->next=(a)->next;\
         (a)->next->last=(a)->last;}

#define list_del_rcu(a)\
        {atomic_store_rel_ptr(&(a)->last->next, (a)->next);\
         (a)->next->last=(a)->last;}

#define list_add(a,b)\
        {(a)->last=(b);       (a)->next=(b)->next ;\
         (a)->next->last=(a); (b)->next=(a);}

#endif /* !_FBSD_LIST_H_ */
