/*-
 * Copyright (c) 2009 Jonathan Anderson
 * Copyright (c) 2010 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_fdlist.c#13 $
 */

#include <sys/mman.h>
#include <sys/stat.h>

#define _WITH_DPRINTF
#include <errno.h>
#include <libcapsicum.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcapsicum_internal.h"
#include "libcapsicum_sandbox_api.h"

struct lc_fdlist_entry {
	u_int sysoff;		/* offset of e.g. "org.freebsd.rtld-elf-cap" */
	u_int syslen;		/* length of above */
	u_int classoff;		/* offset of variable ID e.g. "libs" */
	u_int classnamelen;	/* length of above */
	u_int nameoff;		/* offset of entry name (e.g. "libc.so.7") */
	u_int namelen;		/* length of above */
	int fd;			/* the file descriptor */
};

struct lc_fdlist_storage {
	u_int count;		/* number of entries */
	u_int capacity;		/* entries that we can hold */
	u_int namelen;		/* bytes of name data */
	u_int namecapacity;	/* bytes of name data we can hold */
	struct lc_fdlist_entry entries[]; /* entries in the descriptor list */

	/* followed by bytes of name data */
};

struct lc_fdlist {
	pthread_mutex_t			 lf_lock;	/* for thread safety */
	struct lc_fdlist_storage	*lf_storage;
};

#define LOCK(lfp)	pthread_mutex_lock(&((lfp)->lf_lock));
#define UNLOCK(lfp)	pthread_mutex_unlock(&((lfp)->lf_lock));

/* Where an FD list's name byte array starts */
static char	*lc_fdlist_storage_names(struct lc_fdlist_storage *lfsp);
static u_int	 lc_fdlist_storage_size(struct lc_fdlist_storage *lfsp);

static struct lc_fdlist global_fdlist = {
	.lf_lock = PTHREAD_MUTEX_INITIALIZER,
};

struct lc_fdlist *
lc_fdlist_global(void)
{
	char *env;

	/*
	 * global_fdlist.lf_storage is set to a non-NULL value after the
	 * first call, and will never change; global_fdlist is only valid
	 * once it has non-NULL storage.
	 */
	LOCK(&global_fdlist);
	if (global_fdlist.lf_storage != NULL) {
		UNLOCK(&global_fdlist);
		return (&global_fdlist);
	}
	env = getenv(LIBCAPSICUM_SANDBOX_FDLIST);
	if ((env != NULL) && (strnlen(env, 8) < 7)) {
		struct lc_fdlist_storage *lfsp;
		struct stat sb;
		int fd = -1;

		/* XXX: Should use strtol(3). */
		for (int i = 0; (i < 7) && env[i]; i++) {
			if ((env[i] < '0') || (env[i] > '9'))
				goto fail;
		}
		if (sscanf(env, "%d", &fd) != 1)
			goto fail;
		if (fd < 0)
			goto fail;
		if (fstat(fd, &sb) < 0)
			goto fail;
		lfsp = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE,
		    MAP_NOSYNC | MAP_SHARED, fd, 0);
		if (lfsp == MAP_FAILED)
			goto fail;

		/*
		 * XXX: Should perform additional validation of shared memory
		 * to make sure sizes/etc are internally consistent.
		 */
		global_fdlist.lf_storage = lfsp;
		return (&global_fdlist);
	}

fail:
	/* XXX: We don't always set errno before returning. */
	UNLOCK(&global_fdlist);
	return (NULL);
}

#define INITIAL_ENTRIES		16
#define INITIAL_NAMEBYTES	(64 * INITIAL_ENTRIES)

struct lc_fdlist *
lc_fdlist_new(void)
{
	struct lc_fdlist_storage *lfsp;
	struct lc_fdlist *lfp;
	u_int bytes;

	lfp = malloc(sizeof(*lfp));
	bytes = sizeof(*lfsp) +
	    INITIAL_ENTRIES * sizeof(struct lc_fdlist_entry) +
	    INITIAL_NAMEBYTES;
	lfsp = lfp->lf_storage = malloc(bytes);
	if (lfsp == NULL) {
		free(lfp);
		return (NULL);
	}
	lfsp->count = 0;
	lfsp->capacity = INITIAL_ENTRIES;
	lfsp->namelen = 0;
	lfsp->namecapacity = INITIAL_NAMEBYTES;
	if (pthread_mutex_init(&lfp->lf_lock, NULL) != 0) {
		free(lfp->lf_storage);
		free(lfp);
		return (NULL);
	}
	return (lfp);
}

struct lc_fdlist *
lc_fdlist_dup(struct lc_fdlist *lfp_orig)
{
	struct lc_fdlist *lfp_new;
	u_int size;

	lfp_new = malloc(sizeof(*lfp_new));
	if (lfp_new == NULL)
		return (NULL);
	if (pthread_mutex_init(&lfp_new->lf_lock, NULL) != 0) {
		free(lfp_new);
		return (NULL);
	}
	LOCK(lfp_orig);
	size = lc_fdlist_storage_size(lfp_orig->lf_storage);
	lfp_new->lf_storage = malloc(size);
	if (lfp_new->lf_storage == NULL) {
		UNLOCK(lfp_orig);
		pthread_mutex_destroy(&lfp_new->lf_lock);
		free(lfp_new);
		return (NULL);
	}
	memcpy(lfp_new->lf_storage, lfp_orig->lf_storage, size);
	UNLOCK(lfp_orig);
	return (lfp_new);
}

void
lc_fdlist_free(struct lc_fdlist *lfp)
{

	free(lfp->lf_storage);
	pthread_mutex_destroy(&lfp->lf_lock);
	free(lfp);
}

void
lc_fdlist_print(struct lc_fdlist *lfp, int outFD)
{
	dprintf(outFD, "FD List:\n");
	for(int i = 0; ; )
	{
		char *subsystem, *classname, *name;
		int fd;

		if (lc_fdlist_getentry(lfp, &subsystem, &classname, &name, &fd, &i)
		     < 0)
			break;

		dprintf(outFD, "% 3d:\t'%s'.'%s': '%s'\n",
		        fd, subsystem, classname, name);
	}
}

int
lc_fdlist_add(struct lc_fdlist *lfp, const char *subsystem,
    const char *classname, const char *name, int fd)
{
	struct lc_fdlist_storage *lfsp;

	LOCK(lfp);
	lfsp = lfp->lf_storage;

	/* Do we need more entry space? */
	if (lfsp->count == lfsp->capacity) {
		u_int namebytes_per_entry, newnamebytes, newsize;
		struct lc_fdlist_storage *lfsp_copy;
		char *tmp = NULL;

		/* Copy name data out of the way. */
		if (lfsp->namelen > 0) {
			tmp = malloc(lfsp->namelen);
			if (tmp == NULL) {
				UNLOCK(lfp);
				return (-1);
			}
			memcpy(tmp, lc_fdlist_storage_names(lfsp),
			    lfsp->namelen);
		}

		/* Double the number of available entries. */
		namebytes_per_entry = lfsp->namecapacity / lfsp->capacity;
		newnamebytes = lfsp->capacity * namebytes_per_entry;
		newsize = lc_fdlist_storage_size(lfsp) + newnamebytes
		    + lfsp->capacity * sizeof(struct lc_fdlist_entry);
		lfsp_copy = realloc(lfsp, newsize);
		if (lfsp_copy == NULL) {
			free(tmp);
			UNLOCK(lfp);
			return (-1);
		}

		lfsp_copy->capacity *= 2;
		lfsp_copy->namecapacity += newnamebytes;

		/* Copy name bytes back. */
		if (lfsp_copy->namelen > 0)
			memcpy(lc_fdlist_storage_names(lfsp_copy), tmp,
			    lfsp_copy->namelen);

		lfsp = lfp->lf_storage = lfsp_copy;
		free(tmp);
	}

	/* Do we need more name space? */
	u_int subsyslen = strlen(subsystem);
	u_int classnamelen = strlen(classname);
	u_int namelen = strlen(name);

	if ((lfsp->namelen + subsyslen + classnamelen + namelen) >=
	    lfsp->namecapacity) {

		/* Double the name capacity. */
		struct lc_fdlist_storage *lfsp_enlarged;

		lfsp_enlarged = realloc(lfsp, lc_fdlist_storage_size(lfsp) +
		    lfsp->namecapacity);
		if (lfsp_enlarged == NULL) {
			UNLOCK(lfp);
			return (-1);
		}

		lfsp_enlarged->namecapacity *= 2;
		lfsp = lfp->lf_storage = lfsp_enlarged;
	}

	/* Create the new entry. */
	struct lc_fdlist_entry *entry = lfsp->entries + lfsp->count;

	entry->fd = fd;

	char *names = lc_fdlist_storage_names(lfsp);
	char *head = names + lfsp->namelen;

	strncpy(head, subsystem, subsyslen + 1);
	entry->sysoff = (head - names);
	entry->syslen = subsyslen;
	head += subsyslen + 1;

	strncpy(head, classname, classnamelen + 1);
	entry->classoff	= (head - names);
	entry->classnamelen = classnamelen;
	head += classnamelen + 1;

	strncpy(head, name, namelen + 1);
	entry->nameoff = (head - names);
	entry->namelen = namelen + 1;
	head += namelen + 1;

	lfsp->count++;
	lfsp->namelen = (head - names);

	UNLOCK(lfp);
	return (0);
}

int
lc_fdlist_append(struct lc_fdlist *to, struct lc_fdlist *from)
{
	int pos = 0;
	if (to == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (from == NULL)
		return (0);

	/* Use address to order lc_fdlist locks. */
	if ((uintptr_t)to < (uintptr_t)from) {
		LOCK(to);
		LOCK(from);
	} else {
		LOCK(from);
		LOCK(to);
	}

	for (u_int i = 0; i < from->lf_storage->count; i++) {
		char *subsystem;
		char *classname;
		char *name;
		int fd;

		/*
		 * XXXRW: This recurses the from lock.
		 */
		if (lc_fdlist_getentry(from, &subsystem, &classname, &name,
		    &fd, &pos) < 0)
			goto fail;

		/*
		 * XXXRW: This recurses the to lock.
		 */
		if (lc_fdlist_add(to, subsystem, classname, name, fd) < 0) {
			free(subsystem);
			goto fail;
		}
		free(subsystem);
	}
	return (0);

fail:
	UNLOCK(from);
	UNLOCK(to);
	return (-1);
}

int
lc_fdlist_addcap(struct lc_fdlist *fdlist, const char *subsystem,
    const char *classname, const char *name, int fd, cap_rights_t rights)
{
	int capfd;

	/*
	 * XXXRW: This API isn't particularly caller-friendly, in that it
	 * allocates a descriptor that the caller is responsible for freeing,
	 * but doesn't tell the caller what fd that is.  Not yet clear what
	 * the preferred API is.
	 */
	capfd = cap_new(fd, rights);
	if (capfd < 0)
		return (-1);
	return (lc_fdlist_add(fdlist, subsystem, classname, name, capfd));
}

int
lc_fdlist_find(struct lc_fdlist *lfp, const char *subsystem,
	    const char *classname, const char *filename,
	    const char **relative_name)
{
	int pos = 0;
	int fd = -1;

	/* try to find the file itself in the FD list */
	size_t len = strlen(filename);
	*relative_name = filename + len;

	while (fd == -1)
	{
		char *dirname;

		if (lc_fdlist_lookup(lfp, subsystem, classname,
		                     &dirname, &fd, &pos) == -1)
			break;

		if (strncmp(dirname, filename, len + 1)) fd = -1;
		free(dirname);
	}

	if (fd >= 0) return fd;


	/* now try to find a parent directory and a relative filename */
	*relative_name = NULL;
	pos = 0;

	while (fd == -1)
	{
		char *dirname;

		if (lc_fdlist_lookup(lfp, subsystem, classname,
		                     &dirname, &fd, &pos) == -1)
			return (-1);

		len = strlen(dirname);
		if (strncmp(dirname, filename, len)) fd = -1;
		else
		{
			*relative_name = filename + len;
			if (**relative_name == '/') (*relative_name)++;
		}

		free(dirname);
	}

	return fd;
}


int
lc_fdlist_lookup(struct lc_fdlist *lfp, const char *subsystem,
    const char *classname, char **name, int *fdp, int *pos)
{
	struct lc_fdlist_storage *lfsp;

	LOCK(lfp);
	lfsp = lfp->lf_storage;
	if ((pos != NULL) && (*pos >= (int)lfsp->count)) {
		UNLOCK(lfp);
		errno = EINVAL;
		return (-1);
	}

	int successful = 0;
	const char *names = lc_fdlist_storage_names(lfsp);

	for (u_int i = (pos ? *pos : 0); i < lfsp->count; i++) {
		struct lc_fdlist_entry *entry = lfsp->entries + i;

		if ((!subsystem
		     || !strncmp(subsystem, names + entry->sysoff,
		                 entry->syslen + 1))
		    && (!classname
		        || !strncmp(classname, names + entry->classoff,
		                    entry->classnamelen + 1)))
		{
			/* found a matching entry! */
			successful = 1;
			*fdp = entry->fd;

			if (name) {
				*name = malloc(entry->namelen + 1);
				strncpy(*name, names + entry->nameoff,
				        entry->namelen + 1);
			}
			if (pos) *pos = i + 1;
			break;
		}
	}
	UNLOCK(lfp);
	if (successful)
		return (0);

	errno = ENOENT;
	return (-1);
}

int
lc_fdlist_getentry(struct lc_fdlist *lfp, char **subsystem, char **classname,
    char **name, int *fdp, int *pos)
{
	struct lc_fdlist_storage *lfsp;

	LOCK(lfp);
	lfsp = lfp->lf_storage;

	if ((subsystem == NULL) || (classname == NULL) || (name == NULL) ||
	    (fdp == NULL) || ((pos != NULL) && (*pos >= (int) lfsp->count))) {
		errno = EINVAL;
		return (-1);
	}

	struct lc_fdlist_entry *entry = lfsp->entries + (pos ? *pos : 0);
	char *names = lc_fdlist_storage_names(lfsp);
	int size = entry->syslen + entry->classnamelen + entry->namelen;
	char *head = malloc(size);

	strncpy(head, names + entry->sysoff, entry->syslen + 1);
	*subsystem = head;
	head += size;

	strncpy(head, names + entry->classoff, entry->classnamelen + 1);
	*classname = head;
	head += size;

	strncpy(head, names + entry->nameoff, entry->namelen + 1);
	*name = head;
	head += size;

	*fdp = entry->fd;
	UNLOCK(lfp);
	if (pos)
		(*pos)++;
	return (0);
}

int
lc_fdlist_reorder(struct lc_fdlist *lfp)
{
	struct lc_fdlist_storage *lfsp;

	LOCK(lfp);
	lfsp = lfp->lf_storage;

	/*
	 * Identify the highest source file descriptor we care about so that
	 * when we play the dup2() rearranging game, we don't overwrite any
	 * we care about.
	 */
	int highestfd = -1;
	for (u_int i = 0; i < lfsp->count; i++) {
		if (lfsp->entries[i].fd > highestfd)
			highestfd = lfsp->entries[i].fd;
	}
	highestfd++;	/* Don't tread on the highest */

	/*
	 * First, move all our descriptors up the range.
	 */
	for (u_int i = 0; i < lfsp->count; i++) {
		if (dup2(lfsp->entries[i].fd, highestfd + i) < 0) {
			UNLOCK(lfp);
			return (-1);
		}
	}

	/*
	 * Now put them back.
	 */
	for (u_int i = 0; i < lfsp->count; i++) {
		if (dup2(highestfd + i, i) < 0) {
			UNLOCK(lfp);
			return (-1);
		}

		lfsp->entries[i].fd = i;
	}

	/*
	 * Close the descriptors that we moved, as well as any others that
	 * were left open by the caller.
	 */
	closefrom(lfsp->count);
	UNLOCK(lfp);
	return (0);
}

static u_int
lc_fdlist_storage_size(struct lc_fdlist_storage *lfsp)
{

	return (sizeof(*lfsp) +
	    lfsp->capacity * sizeof(struct lc_fdlist_entry) +
	    lfsp->namecapacity);
}

u_int
lc_fdlist_size(struct lc_fdlist *lfp)
{
	u_int size;

	LOCK(lfp);
	size = lc_fdlist_storage_size(lfp->lf_storage);
	UNLOCK(lfp);
	return (size);
}

static char *
lc_fdlist_storage_names(struct lc_fdlist_storage *lfsp)
{

	return (((char *) lfsp) + lc_fdlist_storage_size(lfsp) -
	    lfsp->namecapacity);
}

void*
_lc_fdlist_getstorage(struct lc_fdlist* lfp)
{

	return (lfp->lf_storage);
}
