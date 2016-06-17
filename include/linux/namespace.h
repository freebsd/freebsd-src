#ifndef _NAMESPACE_H_
#define _NAMESPACE_H_
#ifdef __KERNEL__

struct namespace {
	atomic_t		count;
	struct vfsmount *	root;
	struct list_head	list;
	struct rw_semaphore	sem;
};

extern void umount_tree(struct vfsmount *);

static inline void put_namespace(struct namespace *namespace)
{
	if (atomic_dec_and_test(&namespace->count)) {
		down_write(&namespace->sem);
		spin_lock(&dcache_lock);
		umount_tree(namespace->root);
		spin_unlock(&dcache_lock);
		up_write(&namespace->sem);
		kfree(namespace);
	}
}

static inline void exit_namespace(struct task_struct *p)
{
	struct namespace *namespace = p->namespace;
	if (namespace) {
		task_lock(p);
		p->namespace = NULL;
		task_unlock(p);
		put_namespace(namespace);
	}
}
extern int copy_namespace(int, struct task_struct *);

static inline void get_namespace(struct namespace *namespace)
{
	atomic_inc(&namespace->count);
}

#endif
#endif
