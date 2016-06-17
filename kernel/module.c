#include <linux/config.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <asm/module.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/smp_lock.h>
#include <asm/pgalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/seq_file.h>

/*
 * Originally by Anonymous (as far as I know...)
 * Linux version by Bas Laarhoven <bas@vimec.nl>
 * 0.99.14 version by Jon Tombs <jon@gtex02.us.es>,
 * Heavily modified by Bjorn Ekwall <bj0rn@blox.se> May 1994 (C)
 * Rewritten by Richard Henderson <rth@tamu.edu> Dec 1996
 * Add MOD_INITIALIZING Keith Owens <kaos@ocs.com.au> Nov 1999
 * Add kallsyms support, Keith Owens <kaos@ocs.com.au> Apr 2000
 * Add asm/module support, IA64 has special requirements.  Keith Owens <kaos@ocs.com.au> Sep 2000
 * Fix assorted bugs in module verification.  Keith Owens <kaos@ocs.com.au> Sep 2000
 * Fix sys_init_module race, Andrew Morton <andrewm@uow.edu.au> Oct 2000
 *     http://www.uwsg.iu.edu/hypermail/linux/kernel/0008.3/0379.html
 * Replace xxx_module_symbol with inter_module_xxx.  Keith Owens <kaos@ocs.com.au> Oct 2000
 * Add a module list lock for kernel fault race fixing. Alan Cox <alan@redhat.com>
 *
 * This source is covered by the GNU GPL, the same as all kernel sources.
 */

#if defined(CONFIG_MODULES) || defined(CONFIG_KALLSYMS)

extern struct module_symbol __start___ksymtab[];
extern struct module_symbol __stop___ksymtab[];

extern const struct exception_table_entry __start___ex_table[];
extern const struct exception_table_entry __stop___ex_table[];

extern const char __start___kallsyms[] __attribute__ ((weak));
extern const char __stop___kallsyms[] __attribute__ ((weak));

struct module kernel_module =
{
	size_of_struct:		sizeof(struct module),
	name: 			"",
	uc:	 		{ATOMIC_INIT(1)},
	flags:			MOD_RUNNING,
	syms:			__start___ksymtab,
	ex_table_start:		__start___ex_table,
	ex_table_end:		__stop___ex_table,
	kallsyms_start:		__start___kallsyms,
	kallsyms_end:		__stop___kallsyms,
};

struct module *module_list = &kernel_module;

#endif	/* defined(CONFIG_MODULES) || defined(CONFIG_KALLSYMS) */

/* inter_module functions are always available, even when the kernel is
 * compiled without modules.  Consumers of inter_module_xxx routines
 * will always work, even when both are built into the kernel, this
 * approach removes lots of #ifdefs in mainline code.
 */

static struct list_head ime_list = LIST_HEAD_INIT(ime_list);
static spinlock_t ime_lock = SPIN_LOCK_UNLOCKED;
static int kmalloc_failed;

/*
 *	This lock prevents modifications that might race the kernel fault
 *	fixups. It does not prevent reader walks that the modules code
 *	does. The kernel lock does that.
 *
 *	Since vmalloc fault fixups occur in any context this lock is taken
 *	irqsave at all times.
 */
 
spinlock_t modlist_lock = SPIN_LOCK_UNLOCKED;

/**
 * inter_module_register - register a new set of inter module data.
 * @im_name: an arbitrary string to identify the data, must be unique
 * @owner: module that is registering the data, always use THIS_MODULE
 * @userdata: pointer to arbitrary userdata to be registered
 *
 * Description: Check that the im_name has not already been registered,
 * complain if it has.  For new data, add it to the inter_module_entry
 * list.
 */
void inter_module_register(const char *im_name, struct module *owner, const void *userdata)
{
	struct list_head *tmp;
	struct inter_module_entry *ime, *ime_new;

	if (!(ime_new = kmalloc(sizeof(*ime), GFP_KERNEL))) {
		/* Overloaded kernel, not fatal */
		printk(KERN_ERR
			"Aiee, inter_module_register: cannot kmalloc entry for '%s'\n",
			im_name);
		kmalloc_failed = 1;
		return;
	}
	memset(ime_new, 0, sizeof(*ime_new));
	ime_new->im_name = im_name;
	ime_new->owner = owner;
	ime_new->userdata = userdata;

	spin_lock(&ime_lock);
	list_for_each(tmp, &ime_list) {
		ime = list_entry(tmp, struct inter_module_entry, list);
		if (strcmp(ime->im_name, im_name) == 0) {
			spin_unlock(&ime_lock);
			kfree(ime_new);
			/* Program logic error, fatal */
			printk(KERN_ERR "inter_module_register: duplicate im_name '%s'", im_name);
			BUG();
		}
	}
	list_add(&(ime_new->list), &ime_list);
	spin_unlock(&ime_lock);
}

/**
 * inter_module_unregister - unregister a set of inter module data.
 * @im_name: an arbitrary string to identify the data, must be unique
 *
 * Description: Check that the im_name has been registered, complain if
 * it has not.  For existing data, remove it from the
 * inter_module_entry list.
 */
void inter_module_unregister(const char *im_name)
{
	struct list_head *tmp;
	struct inter_module_entry *ime;

	spin_lock(&ime_lock);
	list_for_each(tmp, &ime_list) {
		ime = list_entry(tmp, struct inter_module_entry, list);
		if (strcmp(ime->im_name, im_name) == 0) {
			list_del(&(ime->list));
			spin_unlock(&ime_lock);
			kfree(ime);
			return;
		}
	}
	spin_unlock(&ime_lock);
	if (kmalloc_failed) {
		printk(KERN_ERR
			"inter_module_unregister: no entry for '%s', "
			"probably caused by previous kmalloc failure\n",
			im_name);
		return;
	}
	else {
		/* Program logic error, fatal */
		printk(KERN_ERR "inter_module_unregister: no entry for '%s'", im_name);
		BUG();
	}
}

/**
 * inter_module_get - return arbitrary userdata from another module.
 * @im_name: an arbitrary string to identify the data, must be unique
 *
 * Description: If the im_name has not been registered, return NULL.
 * Try to increment the use count on the owning module, if that fails
 * then return NULL.  Otherwise return the userdata.
 */
const void *inter_module_get(const char *im_name)
{
	struct list_head *tmp;
	struct inter_module_entry *ime;
	const void *result = NULL;

	spin_lock(&ime_lock);
	list_for_each(tmp, &ime_list) {
		ime = list_entry(tmp, struct inter_module_entry, list);
		if (strcmp(ime->im_name, im_name) == 0) {
			if (try_inc_mod_count(ime->owner))
				result = ime->userdata;
			break;
		}
	}
	spin_unlock(&ime_lock);
	return(result);
}

/**
 * inter_module_get_request - im get with automatic request_module.
 * @im_name: an arbitrary string to identify the data, must be unique
 * @modname: module that is expected to register im_name
 *
 * Description: If inter_module_get fails, do request_module then retry.
 */
const void *inter_module_get_request(const char *im_name, const char *modname)
{
	const void *result = inter_module_get(im_name);
	if (!result) {
		request_module(modname);
		result = inter_module_get(im_name);
	}
	return(result);
}

/**
 * inter_module_put - release use of data from another module.
 * @im_name: an arbitrary string to identify the data, must be unique
 *
 * Description: If the im_name has not been registered, complain,
 * otherwise decrement the use count on the owning module.
 */
void inter_module_put(const char *im_name)
{
	struct list_head *tmp;
	struct inter_module_entry *ime;

	spin_lock(&ime_lock);
	list_for_each(tmp, &ime_list) {
		ime = list_entry(tmp, struct inter_module_entry, list);
		if (strcmp(ime->im_name, im_name) == 0) {
			if (ime->owner)
				__MOD_DEC_USE_COUNT(ime->owner);
			spin_unlock(&ime_lock);
			return;
		}
	}
	spin_unlock(&ime_lock);
	printk(KERN_ERR "inter_module_put: no entry for '%s'", im_name);
	BUG();
}


#if defined(CONFIG_MODULES)	/* The rest of the source */

static long get_mod_name(const char *user_name, char **buf);
static void put_mod_name(char *buf);
struct module *find_module(const char *name);
void free_module(struct module *, int tag_freed);


/*
 * Called at boot time
 */

void __init init_modules(void)
{
	kernel_module.nsyms = __stop___ksymtab - __start___ksymtab;

	arch_init_modules(&kernel_module);
}

/*
 * Copy the name of a module from user space.
 */

static inline long
get_mod_name(const char *user_name, char **buf)
{
	unsigned long page;
	long retval;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = strncpy_from_user((char *)page, user_name, PAGE_SIZE);
	if (retval > 0) {
		if (retval < PAGE_SIZE) {
			*buf = (char *)page;
			return retval;
		}
		retval = -ENAMETOOLONG;
	} else if (!retval)
		retval = -EINVAL;

	free_page(page);
	return retval;
}

static inline void
put_mod_name(char *buf)
{
	free_page((unsigned long)buf);
}

/*
 * Allocate space for a module.
 */

asmlinkage unsigned long
sys_create_module(const char *name_user, size_t size)
{
	char *name;
	long namelen, error;
	struct module *mod;
	unsigned long flags;

	if (!capable(CAP_SYS_MODULE))
		return -EPERM;
	lock_kernel();
	if ((namelen = get_mod_name(name_user, &name)) < 0) {
		error = namelen;
		goto err0;
	}
	if (size < sizeof(struct module)+namelen+1) {
		error = -EINVAL;
		goto err1;
	}
	if (find_module(name) != NULL) {
		error = -EEXIST;
		goto err1;
	}
	if ((mod = (struct module *)module_map(size)) == NULL) {
		error = -ENOMEM;
		goto err1;
	}

	memset(mod, 0, sizeof(*mod));
	mod->size_of_struct = sizeof(*mod);
	mod->name = (char *)(mod + 1);
	mod->size = size;
	memcpy((char*)(mod+1), name, namelen+1);

	put_mod_name(name);

	spin_lock_irqsave(&modlist_lock, flags);
	mod->next = module_list;
	module_list = mod;	/* link it in */
	spin_unlock_irqrestore(&modlist_lock, flags);

	error = (long) mod;
	goto err0;
err1:
	put_mod_name(name);
err0:
	unlock_kernel();
	return error;
}

/*
 * Initialize a module.
 */

asmlinkage long
sys_init_module(const char *name_user, struct module *mod_user)
{
	struct module mod_tmp, *mod, *mod2 = NULL;
	char *name, *n_name, *name_tmp = NULL;
	long namelen, n_namelen, i, error;
	unsigned long mod_user_size, flags;
	struct module_ref *dep;

	if (!capable(CAP_SYS_MODULE))
		return -EPERM;
	lock_kernel();
	if ((namelen = get_mod_name(name_user, &name)) < 0) {
		error = namelen;
		goto err0;
	}
	if ((mod = find_module(name)) == NULL) {
		error = -ENOENT;
		goto err1;
	}

	/* Check module header size.  We allow a bit of slop over the
	   size we are familiar with to cope with a version of insmod
	   for a newer kernel.  But don't over do it. */
	if ((error = get_user(mod_user_size, &mod_user->size_of_struct)) != 0)
		goto err1;
	if (mod_user_size < (unsigned long)&((struct module *)0L)->persist_start
	    || mod_user_size > sizeof(struct module) + 16*sizeof(void*)) {
		printk(KERN_ERR "init_module: Invalid module header size.\n"
		       KERN_ERR "A new version of the modutils is likely "
				"needed.\n");
		error = -EINVAL;
		goto err1;
	}

	/* Hold the current contents while we play with the user's idea
	   of righteousness.  */
	mod_tmp = *mod;
	name_tmp = kmalloc(strlen(mod->name) + 1, GFP_KERNEL);	/* Where's kstrdup()? */
	if (name_tmp == NULL) {
		error = -ENOMEM;
		goto err1;
	}
	strcpy(name_tmp, mod->name);

	/* Copying mod_user directly over mod breaks the module_list chain and
	 * races against search_exception_table.  copy_from_user may sleep so it
	 * cannot be under modlist_lock, do the copy in two stages.
	 */
	if (!(mod2 = vmalloc(mod_user_size))) {
		error = -ENOMEM;
		goto err2;
	}
	error = copy_from_user(mod2, mod_user, mod_user_size);
	if (error) {
		error = -EFAULT;
		goto err2;
	}
	spin_lock_irqsave(&modlist_lock, flags);
	memcpy(mod, mod2, mod_user_size);
	mod->next = mod_tmp.next;
	spin_unlock_irqrestore(&modlist_lock, flags);

	/* Sanity check the size of the module.  */
	error = -EINVAL;

	if (mod->size > mod_tmp.size) {
		printk(KERN_ERR "init_module: Size of initialized module "
				"exceeds size of created module.\n");
		goto err2;
	}

	/* Make sure all interesting pointers are sane.  */

	if (!mod_bound(mod->name, namelen, mod)) {
		printk(KERN_ERR "init_module: mod->name out of bounds.\n");
		goto err2;
	}
	if (mod->nsyms && !mod_bound(mod->syms, mod->nsyms, mod)) {
		printk(KERN_ERR "init_module: mod->syms out of bounds.\n");
		goto err2;
	}
	if (mod->ndeps && !mod_bound(mod->deps, mod->ndeps, mod)) {
		printk(KERN_ERR "init_module: mod->deps out of bounds.\n");
		goto err2;
	}
	if (mod->init && !mod_bound(mod->init, 0, mod)) {
		printk(KERN_ERR "init_module: mod->init out of bounds.\n");
		goto err2;
	}
	if (mod->cleanup && !mod_bound(mod->cleanup, 0, mod)) {
		printk(KERN_ERR "init_module: mod->cleanup out of bounds.\n");
		goto err2;
	}
	if (mod->ex_table_start > mod->ex_table_end
	    || (mod->ex_table_start &&
		!((unsigned long)mod->ex_table_start >= ((unsigned long)mod + mod->size_of_struct)
		  && ((unsigned long)mod->ex_table_end
		      < (unsigned long)mod + mod->size)))
	    || (((unsigned long)mod->ex_table_start
		 - (unsigned long)mod->ex_table_end)
		% sizeof(struct exception_table_entry))) {
		printk(KERN_ERR "init_module: mod->ex_table_* invalid.\n");
		goto err2;
	}
	if (mod->flags & ~MOD_AUTOCLEAN) {
		printk(KERN_ERR "init_module: mod->flags invalid.\n");
		goto err2;
	}
	if (mod_member_present(mod, can_unload)
	    && mod->can_unload && !mod_bound(mod->can_unload, 0, mod)) {
		printk(KERN_ERR "init_module: mod->can_unload out of bounds.\n");
		goto err2;
	}
	if (mod_member_present(mod, kallsyms_end)) {
	    if (mod->kallsyms_end &&
		(!mod_bound(mod->kallsyms_start, 0, mod) ||
		 !mod_bound(mod->kallsyms_end, 0, mod))) {
		printk(KERN_ERR "init_module: mod->kallsyms out of bounds.\n");
		goto err2;
	    }
	    if (mod->kallsyms_start > mod->kallsyms_end) {
		printk(KERN_ERR "init_module: mod->kallsyms invalid.\n");
		goto err2;
	    }
	}
	if (mod_member_present(mod, archdata_end)) {
	    if (mod->archdata_end &&
		(!mod_bound(mod->archdata_start, 0, mod) ||
		 !mod_bound(mod->archdata_end, 0, mod))) {
		printk(KERN_ERR "init_module: mod->archdata out of bounds.\n");
		goto err2;
	    }
	    if (mod->archdata_start > mod->archdata_end) {
		printk(KERN_ERR "init_module: mod->archdata invalid.\n");
		goto err2;
	    }
	}
	if (mod_member_present(mod, kernel_data) && mod->kernel_data) {
	    printk(KERN_ERR "init_module: mod->kernel_data must be zero.\n");
	    goto err2;
	}

	/* Check that the user isn't doing something silly with the name.  */

	if ((n_namelen = get_mod_name(mod->name - (unsigned long)mod
				      + (unsigned long)mod_user,
				      &n_name)) < 0) {
		printk(KERN_ERR "init_module: get_mod_name failure.\n");
		error = n_namelen;
		goto err2;
	}
	if (namelen != n_namelen || strcmp(n_name, name_tmp) != 0) {
		printk(KERN_ERR "init_module: changed module name to "
				"`%s' from `%s'\n",
		       n_name, name_tmp);
		goto err3;
	}

	/* Ok, that's about all the sanity we can stomach; copy the rest.  */

	if (copy_from_user((char *)mod+mod_user_size,
			   (char *)mod_user+mod_user_size,
			   mod->size-mod_user_size)) {
		error = -EFAULT;
		goto err3;
	}

	if (module_arch_init(mod))
		goto err3;

	/* On some machines it is necessary to do something here
	   to make the I and D caches consistent.  */
	flush_icache_range((unsigned long)mod, (unsigned long)mod + mod->size);

	mod->refs = NULL;

	/* Sanity check the module's dependents */
	for (i = 0, dep = mod->deps; i < mod->ndeps; ++i, ++dep) {
		struct module *o, *d = dep->dep;

		/* Make sure the indicated dependencies are really modules.  */
		if (d == mod) {
			printk(KERN_ERR "init_module: self-referential "
					"dependency in mod->deps.\n");
			goto err3;
		}

		/* Scan the current modules for this dependency */
		for (o = module_list; o != &kernel_module && o != d; o = o->next)
			;

		if (o != d) {
			printk(KERN_ERR "init_module: found dependency that is "
				"(no longer?) a module.\n");
			goto err3;
		}
	}

	/* Update module references.  */
	for (i = 0, dep = mod->deps; i < mod->ndeps; ++i, ++dep) {
		struct module *d = dep->dep;

		dep->ref = mod;
		dep->next_ref = d->refs;
		d->refs = dep;
		/* Being referenced by a dependent module counts as a
		   use as far as kmod is concerned.  */
		d->flags |= MOD_USED_ONCE;
	}

	/* Free our temporary memory.  */
	put_mod_name(n_name);
	put_mod_name(name);

	/* Initialize the module.  */
	atomic_set(&mod->uc.usecount,1);
	mod->flags |= MOD_INITIALIZING;
	if (mod->init && (error = mod->init()) != 0) {
		atomic_set(&mod->uc.usecount,0);
		mod->flags &= ~MOD_INITIALIZING;
		if (error > 0)	/* Buggy module */
			error = -EBUSY;
		goto err0;
	}
	atomic_dec(&mod->uc.usecount);

	/* And set it running.  */
	mod->flags = (mod->flags | MOD_RUNNING) & ~MOD_INITIALIZING;
	error = 0;
	goto err0;

err3:
	put_mod_name(n_name);
err2:
	*mod = mod_tmp;
	strcpy((char *)mod->name, name_tmp);	/* We know there is room for this */
err1:
	put_mod_name(name);
err0:
	if (mod2)
		vfree(mod2);
	unlock_kernel();
	kfree(name_tmp);
	return error;
}

static spinlock_t unload_lock = SPIN_LOCK_UNLOCKED;
int try_inc_mod_count(struct module *mod)
{
	int res = 1;
	if (mod) {
		spin_lock(&unload_lock);
		if (mod->flags & MOD_DELETED)
			res = 0;
		else
			__MOD_INC_USE_COUNT(mod);
		spin_unlock(&unload_lock);
	}
	return res;
}

asmlinkage long
sys_delete_module(const char *name_user)
{
	struct module *mod, *next;
	char *name;
	long error;
	int something_changed;

	if (!capable(CAP_SYS_MODULE))
		return -EPERM;

	lock_kernel();
	if (name_user) {
		if ((error = get_mod_name(name_user, &name)) < 0)
			goto out;
		error = -ENOENT;
		if ((mod = find_module(name)) == NULL) {
			put_mod_name(name);
			goto out;
		}
		put_mod_name(name);
		error = -EBUSY;
		if (mod->refs != NULL)
			goto out;

		spin_lock(&unload_lock);
		if (!__MOD_IN_USE(mod)) {
			mod->flags |= MOD_DELETED;
			spin_unlock(&unload_lock);
			free_module(mod, 0);
			error = 0;
		} else {
			spin_unlock(&unload_lock);
		}
		goto out;
	}

	/* Do automatic reaping */
restart:
	something_changed = 0;
	
	for (mod = module_list; mod != &kernel_module; mod = next) {
		next = mod->next;
		spin_lock(&unload_lock);
		if (mod->refs == NULL
		    && (mod->flags & MOD_AUTOCLEAN)
		    && (mod->flags & MOD_RUNNING)
		    && !(mod->flags & MOD_DELETED)
		    && (mod->flags & MOD_USED_ONCE)
		    && !__MOD_IN_USE(mod)) {
			if ((mod->flags & MOD_VISITED)
			    && !(mod->flags & MOD_JUST_FREED)) {
				spin_unlock(&unload_lock);
				mod->flags &= ~MOD_VISITED;
			} else {
				mod->flags |= MOD_DELETED;
				spin_unlock(&unload_lock);
				free_module(mod, 1);
				something_changed = 1;
			}
		} else {
			spin_unlock(&unload_lock);
		}
	}
	
	if (something_changed)
		goto restart;
		
	for (mod = module_list; mod != &kernel_module; mod = mod->next)
		mod->flags &= ~MOD_JUST_FREED;
	
	error = 0;
out:
	unlock_kernel();
	return error;
}

/* Query various bits about modules.  */

static int
qm_modules(char *buf, size_t bufsize, size_t *ret)
{
	struct module *mod;
	size_t nmod, space, len;

	nmod = space = 0;

	for (mod=module_list; mod != &kernel_module; mod=mod->next, ++nmod) {
		len = strlen(mod->name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, mod->name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nmod, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((mod = mod->next) != &kernel_module)
		space += strlen(mod->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_deps(struct module *mod, char *buf, size_t bufsize, size_t *ret)
{
	size_t i, space, len;

	if (mod == &kernel_module)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = 0;
	for (i = 0; i < mod->ndeps; ++i) {
		const char *dep_name = mod->deps[i].dep->name;

		len = strlen(dep_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, dep_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(i, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while (++i < mod->ndeps)
		space += strlen(mod->deps[i].dep->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_refs(struct module *mod, char *buf, size_t bufsize, size_t *ret)
{
	size_t nrefs, space, len;
	struct module_ref *ref;

	if (mod == &kernel_module)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = 0;
	for (nrefs = 0, ref = mod->refs; ref ; ++nrefs, ref = ref->next_ref) {
		const char *ref_name = ref->ref->name;

		len = strlen(ref_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, ref_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nrefs, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((ref = ref->next_ref) != NULL)
		space += strlen(ref->ref->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_symbols(struct module *mod, char *buf, size_t bufsize, size_t *ret)
{
	size_t i, space, len;
	struct module_symbol *s;
	char *strings;
	unsigned long *vals;

	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = mod->nsyms * 2*sizeof(void *);

	i = len = 0;
	s = mod->syms;

	if (space > bufsize)
		goto calc_space_needed;

	if (!access_ok(VERIFY_WRITE, buf, space))
		return -EFAULT;

	bufsize -= space;
	vals = (unsigned long *)buf;
	strings = buf+space;

	for (; i < mod->nsyms ; ++i, ++s, vals += 2) {
		len = strlen(s->name)+1;
		if (len > bufsize)
			goto calc_space_needed;

		if (copy_to_user(strings, s->name, len)
		    || __put_user(s->value, vals+0)
		    || __put_user(space, vals+1))
			return -EFAULT;

		strings += len;
		bufsize -= len;
		space += len;
	}
	if (put_user(i, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	for (; i < mod->nsyms; ++i, ++s)
		space += strlen(s->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_info(struct module *mod, char *buf, size_t bufsize, size_t *ret)
{
	int error = 0;

	if (mod == &kernel_module)
		return -EINVAL;

	if (sizeof(struct module_info) <= bufsize) {
		struct module_info info;
		info.addr = (unsigned long)mod;
		info.size = mod->size;
		info.flags = mod->flags;
		
		/* usecount is one too high here - report appropriately to
		   compensate for locking */
		info.usecount = (mod_member_present(mod, can_unload)
				 && mod->can_unload ? -1 : atomic_read(&mod->uc.usecount)-1);

		if (copy_to_user(buf, &info, sizeof(struct module_info)))
			return -EFAULT;
	} else
		error = -ENOSPC;

	if (put_user(sizeof(struct module_info), ret))
		return -EFAULT;

	return error;
}

asmlinkage long
sys_query_module(const char *name_user, int which, char *buf, size_t bufsize,
		 size_t *ret)
{
	struct module *mod;
	int err;

	lock_kernel();
	if (name_user == NULL)
		mod = &kernel_module;
	else {
		long namelen;
		char *name;

		if ((namelen = get_mod_name(name_user, &name)) < 0) {
			err = namelen;
			goto out;
		}
		err = -ENOENT;
		if ((mod = find_module(name)) == NULL) {
			put_mod_name(name);
			goto out;
		}
		put_mod_name(name);
	}

	/* __MOD_ touches the flags. We must avoid that */
	
	atomic_inc(&mod->uc.usecount);
		
	switch (which)
	{
	case 0:
		err = 0;
		break;
	case QM_MODULES:
		err = qm_modules(buf, bufsize, ret);
		break;
	case QM_DEPS:
		err = qm_deps(mod, buf, bufsize, ret);
		break;
	case QM_REFS:
		err = qm_refs(mod, buf, bufsize, ret);
		break;
	case QM_SYMBOLS:
		err = qm_symbols(mod, buf, bufsize, ret);
		break;
	case QM_INFO:
		err = qm_info(mod, buf, bufsize, ret);
		break;
	default:
		err = -EINVAL;
		break;
	}
	atomic_dec(&mod->uc.usecount);
	
out:
	unlock_kernel();
	return err;
}

/*
 * Copy the kernel symbol table to user space.  If the argument is
 * NULL, just return the size of the table.
 *
 * This call is obsolete.  New programs should use query_module+QM_SYMBOLS
 * which does not arbitrarily limit the length of symbols.
 */

asmlinkage long
sys_get_kernel_syms(struct kernel_sym *table)
{
	struct module *mod;
	int i;
	struct kernel_sym ksym;

	lock_kernel();
	for (mod = module_list, i = 0; mod; mod = mod->next) {
		/* include the count for the module name! */
		i += mod->nsyms + 1;
	}

	if (table == NULL)
		goto out;

	/* So that we don't give the user our stack content */
	memset (&ksym, 0, sizeof (ksym));

	for (mod = module_list, i = 0; mod; mod = mod->next) {
		struct module_symbol *msym;
		unsigned int j;

		if (!MOD_CAN_QUERY(mod))
			continue;

		/* magic: write module info as a pseudo symbol */
		ksym.value = (unsigned long)mod;
		ksym.name[0] = '#';
		strncpy(ksym.name+1, mod->name, sizeof(ksym.name)-1);
		ksym.name[sizeof(ksym.name)-1] = '\0';

		if (copy_to_user(table, &ksym, sizeof(ksym)) != 0)
			goto out;
		++i, ++table;

		if (mod->nsyms == 0)
			continue;

		for (j = 0, msym = mod->syms; j < mod->nsyms; ++j, ++msym) {
			ksym.value = msym->value;
			strncpy(ksym.name, msym->name, sizeof(ksym.name));
			ksym.name[sizeof(ksym.name)-1] = '\0';

			if (copy_to_user(table, &ksym, sizeof(ksym)) != 0)
				goto out;
			++i, ++table;
		}
	}
out:
	unlock_kernel();
	return i;
}

/*
 * Look for a module by name, ignoring modules marked for deletion.
 */

struct module *
find_module(const char *name)
{
	struct module *mod;

	for (mod = module_list; mod ; mod = mod->next) {
		if (mod->flags & MOD_DELETED)
			continue;
		if (!strcmp(mod->name, name))
			break;
	}

	return mod;
}

/*
 * Free the given module.
 */

void
free_module(struct module *mod, int tag_freed)
{
	struct module_ref *dep;
	unsigned i;
	unsigned long flags;

	/* Let the module clean up.  */

	if (mod->flags & MOD_RUNNING)
	{
		if(mod->cleanup)
			mod->cleanup();
		mod->flags &= ~MOD_RUNNING;
	}

	/* Remove the module from the dependency lists.  */

	for (i = 0, dep = mod->deps; i < mod->ndeps; ++i, ++dep) {
		struct module_ref **pp;
		for (pp = &dep->dep->refs; *pp != dep; pp = &(*pp)->next_ref)
			continue;
		*pp = dep->next_ref;
		if (tag_freed && dep->dep->refs == NULL)
			dep->dep->flags |= MOD_JUST_FREED;
	}

	/* And from the main module list.  */

	spin_lock_irqsave(&modlist_lock, flags);
	if (mod == module_list) {
		module_list = mod->next;
	} else {
		struct module *p;
		for (p = module_list; p->next != mod; p = p->next)
			continue;
		p->next = mod->next;
	}
	spin_unlock_irqrestore(&modlist_lock, flags);

	/* And free the memory.  */

	module_unmap(mod);
}

/*
 * Called by the /proc file system to return a current list of modules.
 */

int get_module_list(char *p)
{
	size_t left = PAGE_SIZE;
	struct module *mod;
	char tmpstr[64];
	struct module_ref *ref;

	for (mod = module_list; mod != &kernel_module; mod = mod->next) {
		long len;
		const char *q;

#define safe_copy_str(str, len)						\
		do {							\
			if (left < len)					\
				goto fini;				\
			memcpy(p, str, len); p += len, left -= len;	\
		} while (0)
#define safe_copy_cstr(str)	safe_copy_str(str, sizeof(str)-1)

		len = strlen(mod->name);
		safe_copy_str(mod->name, len);

		if ((len = 20 - len) > 0) {
			if (left < len)
				goto fini;
			memset(p, ' ', len);
			p += len;
			left -= len;
		}

		len = sprintf(tmpstr, "%8lu", mod->size);
		safe_copy_str(tmpstr, len);

		if (mod->flags & MOD_RUNNING) {
			len = sprintf(tmpstr, "%4ld",
				      (mod_member_present(mod, can_unload)
				       && mod->can_unload
				       ? -1L : (long)atomic_read(&mod->uc.usecount)));
			safe_copy_str(tmpstr, len);
		}

		if (mod->flags & MOD_DELETED)
			safe_copy_cstr(" (deleted)");
		else if (mod->flags & MOD_RUNNING) {
			if (mod->flags & MOD_AUTOCLEAN)
				safe_copy_cstr(" (autoclean)");
			if (!(mod->flags & MOD_USED_ONCE))
				safe_copy_cstr(" (unused)");
		}
		else if (mod->flags & MOD_INITIALIZING)
			safe_copy_cstr(" (initializing)");
		else
			safe_copy_cstr(" (uninitialized)");

		if ((ref = mod->refs) != NULL) {
			safe_copy_cstr(" [");
			while (1) {
				q = ref->ref->name;
				len = strlen(q);
				safe_copy_str(q, len);

				if ((ref = ref->next_ref) != NULL)
					safe_copy_cstr(" ");
				else
					break;
			}
			safe_copy_cstr("]");
		}
		safe_copy_cstr("\n");

#undef safe_copy_str
#undef safe_copy_cstr
	}

fini:
	return PAGE_SIZE - left;
}

/*
 * Called by the /proc file system to return a current list of ksyms.
 */

struct mod_sym {
	struct module *mod;
	int index;
};

/* iterator */

static void *s_start(struct seq_file *m, loff_t *pos)
{
	struct mod_sym *p = kmalloc(sizeof(*p), GFP_KERNEL);
	struct module *v;
	loff_t n = *pos;

	if (!p)
		return ERR_PTR(-ENOMEM);
	lock_kernel();
	for (v = module_list, n = *pos; v; n -= v->nsyms, v = v->next) {
		if (n < v->nsyms) {
			p->mod = v;
			p->index = n;
			return p;
		}
	}
	unlock_kernel();
	kfree(p);
	return NULL;
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct mod_sym *v = p;
	(*pos)++;
	if (++v->index >= v->mod->nsyms) {
		do {
			v->mod = v->mod->next;
			if (!v->mod) {
				unlock_kernel();
				kfree(p);
				return NULL;
			}
		} while (!v->mod->nsyms);
		v->index = 0;
	}
	return p;
}

static void s_stop(struct seq_file *m, void *p)
{
	if (p && !IS_ERR(p)) {
		unlock_kernel();
		kfree(p);
	}
}

static int s_show(struct seq_file *m, void *p)
{
	struct mod_sym *v = p;
	struct module_symbol *sym;

	if (!MOD_CAN_QUERY(v->mod))
		return 0;
	sym = &v->mod->syms[v->index];
	if (*v->mod->name)
		seq_printf(m, "%0*lx %s\t[%s]\n", (int)(2*sizeof(void*)),
			       sym->value, sym->name, v->mod->name);
	else
		seq_printf(m, "%0*lx %s\n", (int)(2*sizeof(void*)),
			       sym->value, sym->name);
	return 0;
}

struct seq_operations ksyms_op = {
	start:	s_start,
	next:	s_next,
	stop:	s_stop,
	show:	s_show
};

#else		/* CONFIG_MODULES */

/* Dummy syscalls for people who don't want modules */

asmlinkage unsigned long
sys_create_module(const char *name_user, size_t size)
{
	return -ENOSYS;
}

asmlinkage long
sys_init_module(const char *name_user, struct module *mod_user)
{
	return -ENOSYS;
}

asmlinkage long
sys_delete_module(const char *name_user)
{
	return -ENOSYS;
}

asmlinkage long
sys_query_module(const char *name_user, int which, char *buf, size_t bufsize,
		 size_t *ret)
{
	/* Let the program know about the new interface.  Not that
	   it'll do them much good.  */
	if (which == 0)
		return 0;

	return -ENOSYS;
}

asmlinkage long
sys_get_kernel_syms(struct kernel_sym *table)
{
	return -ENOSYS;
}

int try_inc_mod_count(struct module *mod)
{
	return 1;
}

#endif	/* CONFIG_MODULES */
