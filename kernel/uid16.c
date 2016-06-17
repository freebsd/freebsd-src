/*
 *	Wrapper functions for 16bit uid back compatibility. All nicely tied
 *	together in the faint hope we can take the out in five years time.
 */

#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/mman.h>
#include <linux/smp_lock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/prctl.h>
#include <linux/init.h>
#include <linux/highuid.h>

#include <asm/uaccess.h>

extern asmlinkage long sys_chown(const char *, uid_t,gid_t);
extern asmlinkage long sys_lchown(const char *, uid_t,gid_t);
extern asmlinkage long sys_fchown(unsigned int, uid_t,gid_t);
extern asmlinkage long sys_setregid(gid_t, gid_t);
extern asmlinkage long sys_setgid(gid_t);
extern asmlinkage long sys_setreuid(uid_t, uid_t);
extern asmlinkage long sys_setuid(uid_t);
extern asmlinkage long sys_setresuid(uid_t, uid_t, uid_t);
extern asmlinkage long sys_setresgid(gid_t, gid_t, gid_t);
extern asmlinkage long sys_setfsuid(uid_t);
extern asmlinkage long sys_setfsgid(gid_t);
 
asmlinkage long sys_chown16(const char * filename, old_uid_t user, old_gid_t group)
{
	return sys_chown(filename, low2highuid(user), low2highgid(group));
}

asmlinkage long sys_lchown16(const char * filename, old_uid_t user, old_gid_t group)
{
	return sys_lchown(filename, low2highuid(user), low2highgid(group));
}

asmlinkage long sys_fchown16(unsigned int fd, old_uid_t user, old_gid_t group)
{
	return sys_fchown(fd, low2highuid(user), low2highgid(group));
}

asmlinkage long sys_setregid16(old_gid_t rgid, old_gid_t egid)
{
	return sys_setregid(low2highgid(rgid), low2highgid(egid));
}

asmlinkage long sys_setgid16(old_gid_t gid)
{
	return sys_setgid((gid_t)gid);
}

asmlinkage long sys_setreuid16(old_uid_t ruid, old_uid_t euid)
{
	return sys_setreuid(low2highuid(ruid), low2highuid(euid));
}

asmlinkage long sys_setuid16(old_uid_t uid)
{
	return sys_setuid((uid_t)uid);
}

asmlinkage long sys_setresuid16(old_uid_t ruid, old_uid_t euid, old_uid_t suid)
{
	return sys_setresuid(low2highuid(ruid), low2highuid(euid),
		low2highuid(suid));
}

asmlinkage long sys_getresuid16(old_uid_t *ruid, old_uid_t *euid, old_uid_t *suid)
{
	int retval;

	if (!(retval = put_user(high2lowuid(current->uid), ruid)) &&
	    !(retval = put_user(high2lowuid(current->euid), euid)))
		retval = put_user(high2lowuid(current->suid), suid);

	return retval;
}

asmlinkage long sys_setresgid16(old_gid_t rgid, old_gid_t egid, old_gid_t sgid)
{
	return sys_setresgid(low2highgid(rgid), low2highgid(egid),
		low2highgid(sgid));
}

asmlinkage long sys_getresgid16(old_gid_t *rgid, old_gid_t *egid, old_gid_t *sgid)
{
	int retval;

	if (!(retval = put_user(high2lowgid(current->gid), rgid)) &&
	    !(retval = put_user(high2lowgid(current->egid), egid)))
		retval = put_user(high2lowgid(current->sgid), sgid);

	return retval;
}

asmlinkage long sys_setfsuid16(old_uid_t uid)
{
	return sys_setfsuid((uid_t)uid);
}

asmlinkage long sys_setfsgid16(old_gid_t gid)
{
	return sys_setfsgid((gid_t)gid);
}

asmlinkage long sys_getgroups16(int gidsetsize, old_gid_t *grouplist)
{
	old_gid_t groups[NGROUPS];
	int i,j;

	if (gidsetsize < 0)
		return -EINVAL;
	i = current->ngroups;
	if (gidsetsize) {
		if (i > gidsetsize)
			return -EINVAL;
		for(j=0;j<i;j++)
			groups[j] = current->groups[j];
		if (copy_to_user(grouplist, groups, sizeof(old_gid_t)*i))
			return -EFAULT;
	}
	return i;
}

asmlinkage long sys_setgroups16(int gidsetsize, old_gid_t *grouplist)
{
	old_gid_t groups[NGROUPS];
	int i;

	if (!capable(CAP_SETGID))
		return -EPERM;
	if ((unsigned) gidsetsize > NGROUPS)
		return -EINVAL;
	if (copy_from_user(groups, grouplist, gidsetsize * sizeof(old_gid_t)))
		return -EFAULT;
	for (i = 0 ; i < gidsetsize ; i++)
		current->groups[i] = (gid_t)groups[i];
	current->ngroups = gidsetsize;
	return 0;
}

asmlinkage long sys_getuid16(void)
{
	return high2lowuid(current->uid);
}

asmlinkage long sys_geteuid16(void)
{
	return high2lowuid(current->euid);
}

asmlinkage long sys_getgid16(void)
{
	return high2lowgid(current->gid);
}

asmlinkage long sys_getegid16(void)
{
	return high2lowgid(current->egid);
}
