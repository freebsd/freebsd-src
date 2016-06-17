/*
	kmod, the new module loader (replaces kerneld)
	Kirk Petersen

	Reorganized not to be a daemon by Adam Richter, with guidance
	from Greg Zornetzer.

	Modified to avoid chroot and file sharing problems.
	Mikael Pettersson

	Limit the concurrent number of kmod modprobes to catch loops from
	"modprobe needs a service that is in a module".
	Keith Owens <kaos@ocs.com.au> December 1999

	Unblock all signals when we exec a usermode process.
	Shuu Yamaguchi <shuu@wondernetworkresources.com> December 2000
*/

#define __KERNEL_SYSCALLS__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/kmod.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/namespace.h>
#include <linux/completion.h>

#include <asm/uaccess.h>

extern int max_threads;

static inline void
use_init_fs_context(void)
{
	struct fs_struct *our_fs, *init_fs;
	struct dentry *root, *pwd;
	struct vfsmount *rootmnt, *pwdmnt;
	struct namespace *our_ns, *init_ns;

	/*
	 * Make modprobe's fs context be a copy of init's.
	 *
	 * We cannot use the user's fs context, because it
	 * may have a different root than init.
	 * Since init was created with CLONE_FS, we can grab
	 * its fs context from "init_task".
	 *
	 * The fs context has to be a copy. If it is shared
	 * with init, then any chdir() call in modprobe will
	 * also affect init and the other threads sharing
	 * init_task's fs context.
	 *
	 * We created the exec_modprobe thread without CLONE_FS,
	 * so we can update the fields in our fs context freely.
	 */

	init_fs = init_task.fs;
	init_ns = init_task.namespace;
	get_namespace(init_ns);
	our_ns = current->namespace;
	current->namespace = init_ns;
	put_namespace(our_ns);
	read_lock(&init_fs->lock);
	rootmnt = mntget(init_fs->rootmnt);
	root = dget(init_fs->root);
	pwdmnt = mntget(init_fs->pwdmnt);
	pwd = dget(init_fs->pwd);
	read_unlock(&init_fs->lock);

	/* FIXME - unsafe ->fs access */
	our_fs = current->fs;
	our_fs->umask = init_fs->umask;
	set_fs_root(our_fs, rootmnt, root);
	set_fs_pwd(our_fs, pwdmnt, pwd);
	write_lock(&our_fs->lock);
	if (our_fs->altroot) {
		struct vfsmount *mnt = our_fs->altrootmnt;
		struct dentry *dentry = our_fs->altroot;
		our_fs->altrootmnt = NULL;
		our_fs->altroot = NULL;
		write_unlock(&our_fs->lock);
		dput(dentry);
		mntput(mnt);
	} else 
		write_unlock(&our_fs->lock);
	dput(root);
	mntput(rootmnt);
	dput(pwd);
	mntput(pwdmnt);
}

int exec_usermodehelper(char *program_path, char *argv[], char *envp[])
{
	int i;
	struct task_struct *curtask = current;

	curtask->session = 1;
	curtask->pgrp = 1;

	use_init_fs_context();

	/* Prevent parent user process from sending signals to child.
	   Otherwise, if the modprobe program does not exist, it might
	   be possible to get a user defined signal handler to execute
	   as the super user right after the execve fails if you time
	   the signal just right.
	*/
	spin_lock_irq(&curtask->sigmask_lock);
	sigemptyset(&curtask->blocked);
	flush_signals(curtask);
	flush_signal_handlers(curtask);
	recalc_sigpending(curtask);
	spin_unlock_irq(&curtask->sigmask_lock);

	for (i = 0; i < curtask->files->max_fds; i++ ) {
		if (curtask->files->fd[i]) close(i);
	}

	switch_uid(INIT_USER);

	/* Give kmod all effective privileges.. */
	curtask->euid = curtask->uid = curtask->suid = curtask->fsuid = 0;
	curtask->egid = curtask->gid = curtask->sgid = curtask->fsgid = 0;

	curtask->ngroups = 0;

	cap_set_full(curtask->cap_effective);

	/* Allow execve args to be in kernel space. */
	set_fs(KERNEL_DS);

	/* Go, go, go... */
	if (execve(program_path, argv, envp) < 0)
		return -errno;
	return 0;
}

#ifdef CONFIG_KMOD

/*
	modprobe_path is set via /proc/sys.
*/
char modprobe_path[256] = "/sbin/modprobe";

static int exec_modprobe(void * module_name)
{
	static char * envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };
	char *argv[] = { modprobe_path, "-s", "-k", "--", (char*)module_name, NULL };
	int ret;

	ret = exec_usermodehelper(modprobe_path, argv, envp);
	if (ret) {
		printk(KERN_ERR
		       "kmod: failed to exec %s -s -k %s, errno = %d\n",
		       modprobe_path, (char*) module_name, errno);
	}
	return ret;
}

/**
 * request_module - try to load a kernel module
 * @module_name: Name of module
 *
 * Load a module using the user mode module loader. The function returns
 * zero on success or a negative errno code on failure. Note that a
 * successful module load does not mean the module did not then unload
 * and exit on an error of its own. Callers must check that the service
 * they requested is now available not blindly invoke it.
 *
 * If module auto-loading support is disabled then this function
 * becomes a no-operation.
 */
int request_module(const char * module_name)
{
	pid_t pid;
	int waitpid_result;
	sigset_t tmpsig;
	int i;
	static atomic_t kmod_concurrent = ATOMIC_INIT(0);
#define MAX_KMOD_CONCURRENT 50	/* Completely arbitrary value - KAO */
	static int kmod_loop_msg;

	/* Don't allow request_module() before the root fs is mounted!  */
	if ( ! current->fs->root ) {
		printk(KERN_ERR "request_module[%s]: Root fs not mounted\n",
			module_name);
		return -EPERM;
	}

	/* If modprobe needs a service that is in a module, we get a recursive
	 * loop.  Limit the number of running kmod threads to max_threads/2 or
	 * MAX_KMOD_CONCURRENT, whichever is the smaller.  A cleaner method
	 * would be to run the parents of this process, counting how many times
	 * kmod was invoked.  That would mean accessing the internals of the
	 * process tables to get the command line, proc_pid_cmdline is static
	 * and it is not worth changing the proc code just to handle this case. 
	 * KAO.
	 */
	i = max_threads/2;
	if (i > MAX_KMOD_CONCURRENT)
		i = MAX_KMOD_CONCURRENT;
	atomic_inc(&kmod_concurrent);
	if (atomic_read(&kmod_concurrent) > i) {
		if (kmod_loop_msg++ < 5)
			printk(KERN_ERR
			       "kmod: runaway modprobe loop assumed and stopped\n");
		atomic_dec(&kmod_concurrent);
		return -ENOMEM;
	}

	pid = kernel_thread(exec_modprobe, (void*) module_name, 0);
	if (pid < 0) {
		printk(KERN_ERR "request_module[%s]: fork failed, errno %d\n", module_name, -pid);
		atomic_dec(&kmod_concurrent);
		return pid;
	}

	/* Block everything but SIGKILL/SIGSTOP */
	spin_lock_irq(&current->sigmask_lock);
	tmpsig = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	waitpid_result = waitpid(pid, NULL, __WCLONE);
	atomic_dec(&kmod_concurrent);

	/* Allow signals again.. */
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = tmpsig;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	if (waitpid_result != pid) {
		printk(KERN_ERR "request_module[%s]: waitpid(%d,...) failed, errno %d\n",
		       module_name, pid, -waitpid_result);
	}
	return 0;
}
#endif /* CONFIG_KMOD */


#ifdef CONFIG_HOTPLUG
/*
	hotplug path is set via /proc/sys
	invoked by hotplug-aware bus drivers,
	with exec_usermodehelper and some thread-spawner

	argv [0] = hotplug_path;
	argv [1] = "usb", "scsi", "pci", "network", etc;
	... plus optional type-specific parameters
	argv [n] = 0;

	envp [*] = HOME, PATH; optional type-specific parameters

	a hotplug bus should invoke this for device add/remove
	events.  the command is expected to load drivers when
	necessary, and may perform additional system setup.
*/
char hotplug_path[256] = "/sbin/hotplug";

EXPORT_SYMBOL(hotplug_path);

#endif /* CONFIG_HOTPLUG */

struct subprocess_info {
	struct completion *complete;
	char *path;
	char **argv;
	char **envp;
	pid_t retval;
};

/*
 * This is the task which runs the usermode application
 */
static int ____call_usermodehelper(void *data)
{
	struct subprocess_info *sub_info = data;
	int retval;

	retval = -EPERM;
	if (current->fs->root)
		retval = exec_usermodehelper(sub_info->path, sub_info->argv, sub_info->envp);

	/* Exec failed? */
	sub_info->retval = (pid_t)retval;
	do_exit(0);
}

/*
 * This is run by keventd.
 */
static void __call_usermodehelper(void *data)
{
	struct subprocess_info *sub_info = data;
	pid_t pid;

	/*
	 * CLONE_VFORK: wait until the usermode helper has execve'd successfully
	 * We need the data structures to stay around until that is done.
	 */
	pid = kernel_thread(____call_usermodehelper, sub_info, CLONE_VFORK | SIGCHLD);
	if (pid < 0)
		sub_info->retval = pid;
	complete(sub_info->complete);
}

/**
 * call_usermodehelper - start a usermode application
 * @path: pathname for the application
 * @argv: null-terminated argument list
 * @envp: null-terminated environment list
 *
 * Runs a user-space application.  The application is started asynchronously.  It
 * runs as a child of keventd.  It runs with full root capabilities.  keventd silently
 * reaps the child when it exits.
 *
 * Must be called from process context.  Returns zero on success, else a negative
 * error code.
 */
int call_usermodehelper(char *path, char **argv, char **envp)
{
	DECLARE_COMPLETION(work);
	struct subprocess_info sub_info = {
		complete:	&work,
		path:		path,
		argv:		argv,
		envp:		envp,
		retval:		0,
	};
	struct tq_struct tqs = {
		routine:	__call_usermodehelper,
		data:		&sub_info,
	};

	if (path[0] == '\0')
		goto out;

	if (current_is_keventd()) {
		/* We can't wait on keventd! */
		__call_usermodehelper(&sub_info);
	} else {
		schedule_task(&tqs);
		wait_for_completion(&work);
	}
out:
	return sub_info.retval;
}

/*
 * This is for the serialisation of device probe() functions
 * against device open() functions
 */
static DECLARE_MUTEX(dev_probe_sem);

void dev_probe_lock(void)
{
	down(&dev_probe_sem);
}

void dev_probe_unlock(void)
{
	up(&dev_probe_sem);
}

EXPORT_SYMBOL(exec_usermodehelper);
EXPORT_SYMBOL(call_usermodehelper);

#ifdef CONFIG_KMOD
EXPORT_SYMBOL(request_module);
#endif

