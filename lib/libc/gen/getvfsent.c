/*
 * getvfsent.c - get a listing of installed filesystems
 * Written September 1994 by Garrett A. Wollman
 * This file is in the public domain.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <paths.h>

/* XXX hide some compatibility problems. */
#undef getvfsbyname
#define vfsconf		ovfsconf

#define _PATH_MODLOAD	"/sbin/modload"	/* XXX should be in header file */

static struct vfsconf *_vfslist = 0;
static struct vfsconf _vfsconf;
static size_t _vfslistlen = 0;
static int _vfs_keeplist = 0;
static int _vfs_index = 0;

static int
initvfs(void)
{
	int mib[2] = { CTL_VFS, VFS_VFSCONF };
	size_t size = 0;
	int rv;

	rv = sysctl(mib, 2, (void *)0, &size, (void *)0, (size_t)0);
	if(rv < 0)
		return 0;

	if(_vfslist)
		free(_vfslist);
	_vfslist = malloc(size);
	if(!_vfslist)
		return 0;

	rv = sysctl(mib, 2, _vfslist, &size, (void *)0, (size_t)0);
	if(rv < 0) {
		free(_vfslist);
		_vfslist = 0;
		return 0;
	}

	_vfslistlen = size / sizeof _vfslist[0];
	return 1;
}

struct vfsconf *
getvfsent(void)
{
	if(!_vfslist && !initvfs()) {
		return 0;
	}

	do {
		if(_vfs_index >= _vfslistlen) {
			return 0;
		}

		_vfsconf = _vfslist[_vfs_index++];
	} while(!_vfsconf.vfc_vfsops);

	if(!_vfs_keeplist) {
		free(_vfslist);
		_vfslist = 0;
	}
	return &_vfsconf;
}

struct vfsconf *
getvfsbyname(const char *name)
{
	int i;

	if(!_vfslist && !initvfs()) {
		return 0;
	}

	for(i = 0; i < _vfslistlen; i++) {
		if( ! strcmp(_vfslist[i].vfc_name, name) )
			break;
	}

	if(i < _vfslistlen) {
		_vfsconf = _vfslist[i];
	}

	if(!_vfs_keeplist) {
		free(_vfslist);
		_vfslist = 0;
	}

	if(i < _vfslistlen) {
		return &_vfsconf;
	} else {
		return 0;
	}
}

struct vfsconf *
getvfsbytype(int type)
{
	int i;

	if(!_vfslist && !initvfs()) {
		return 0;
	}

	for(i = 0; i < _vfslistlen; i++) {
		if(_vfslist[i].vfc_index == type)
			break;
	}

	if(i < _vfslistlen) {
		_vfsconf = _vfslist[i];
	}

	if(!_vfs_keeplist) {
		free(_vfslist);
		_vfslist = 0;
	}

	if(i < _vfslistlen) {
		return &_vfsconf;
	} else {
		return 0;
	}
}

void
setvfsent(int keep)
{
	if(_vfslist && !keep) {
		free(_vfslist);
		_vfslist = 0;
	}

	_vfs_keeplist = keep;
	_vfs_index = 0;
}

void
endvfsent(void)
{
	if(_vfslist) {
		free(_vfslist);
		_vfslist = 0;
	}

	_vfs_index = 0;
}

static const char *vfs_lkmdirs[] = {
	"/lkm",
	"/usr/lkm",
	0,
	0
};
#define NLKMDIRS ((sizeof vfs_lkmdirs) / (sizeof vfs_lkmdirs[0]))

static const char *
vfspath(const char *name)
{
	static char pnbuf[MAXPATHLEN];
	char *userdir = getenv("LKMDIR");
	int i;

	if(userdir && getuid() == geteuid() && getuid() == 0) {
		vfs_lkmdirs[NLKMDIRS - 2] = userdir;
	}

	for(i = 0; vfs_lkmdirs[i]; i++) {
		snprintf(pnbuf, sizeof pnbuf, "%s/%s_mod.o", vfs_lkmdirs[i], name);
		if( ! access(pnbuf, R_OK) )
			return pnbuf;
	}

	return 0;
}

int
vfsisloadable(const char *name)
{
	int fd;

	fd = open("/dev/lkm", O_RDWR, 0);
	if(fd < 0) {
		return 0;
	}
	close(fd);

	return !!vfspath(name);
}

int
vfsload(const char *name)
{
	const char *path = vfspath(name);
	char name_mod[sizeof("_mod") + strlen(name)];
	pid_t pid;
	int status;

	if(!path) {
		errno = ENOENT;
		return -1;
	}

	pid = fork();
	if(pid < 0)
		return -1;

	if(pid > 0) {
		waitpid(pid, &status, 0);
		if(WIFEXITED(status)) {
			errno = WEXITSTATUS(status);
			return errno ? -1 : 0;
		}
		errno = EINVAL;	/* not enough errno values, >sigh< */
		return -1;
	}

	status = -1;
	unsetenv("TMPDIR");
	if(status) {
		status = chdir(_PATH_VARTMP);
	}
	if(status) {
		status = chdir(_PATH_TMP);
	}
	if(status) {
		exit(errno);
	}

	snprintf(name_mod, sizeof name_mod, "%s%s", name, "_mod");
	status = execl(_PATH_MODLOAD, "modload", "-e", name_mod, "-o",
		       name_mod, "-u", "-q", path, (const char *)0);

	exit(status ? errno : 0);
}

