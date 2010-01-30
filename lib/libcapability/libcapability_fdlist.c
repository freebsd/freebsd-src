/*-
 * Copyright (c) 2009 Jonathan Anderson
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapability/libcapability_fdlist.c#1 $
 */

#include <errno.h>
#include <libcapability.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>


struct lc_fdlist_entry {

	unsigned int sysoff;	/* offset of e.g. "org.freebsd.rtld-elf-cap" */
	unsigned int syslen;	/* length of above */

	unsigned int idoff;	/* offset of variable ID e.g. "libs" */
	unsigned int idlen;	/* length of above */

	unsigned int nameoff;	/* offset of entry name (e.g. "libc.so.7") */
	unsigned int namelen;	/* length of above */

	int fd;			/* the file descriptor */
};


struct lc_fdlist {

	unsigned int count;		/* number of entries */
	unsigned int capacity;		/* entries that we can hold */

	unsigned int namelen;		/* bytes of name data */
	unsigned int namecapacity;	/* bytes of name data we can hold */

	pthread_mutex_t lock;		/* for thread safety */

	struct lc_fdlist_entry entries[];	/* entries in the descriptor list */

	/* followed by bytes of name data */
};




#define LOCK(l)		pthread_mutex_lock(&((l)->lock));
#define UNLOCK(l)		pthread_mutex_unlock(&((l)->lock));

/* Where an FD list's name byte array starts */
char*	lc_fdlist_names(struct lc_fdlist *l);



#define INITIAL_ENTRIES		16
#define INITIAL_NAMEBYTES	(64 * INITIAL_ENTRIES)


struct lc_fdlist*
lc_fdlist_new(void) {

	int bytes = sizeof(struct lc_fdlist)
		+ INITIAL_ENTRIES * sizeof(struct lc_fdlist_entry)
		+ INITIAL_NAMEBYTES;

	struct lc_fdlist *fdlist = malloc(bytes);
	if (fdlist == NULL) return (NULL);

	fdlist->count = 0;
	fdlist->capacity = INITIAL_ENTRIES;
	fdlist->namelen = 0;
	fdlist->namecapacity = INITIAL_NAMEBYTES;

	if (pthread_mutex_init(&fdlist->lock, NULL)) {
		free(fdlist);
		return NULL;
	}

	return fdlist;
}


struct lc_fdlist*
lc_fdlist_dup(struct lc_fdlist *orig) {

	LOCK(orig);

	int size = lc_fdlist_size(orig);

	struct lc_fdlist *copy = malloc(size);
	if (copy == NULL) return (NULL);

	UNLOCK(orig);

	return copy;
}


void
lc_fdlist_free(struct lc_fdlist *l) {

	LOCK(l);

	pthread_mutex_destroy(&l->lock);
	free(l);
}



int
lc_fdlist_add(struct lc_fdlist **fdlist,
              const char *subsystem, const char *id,
              const char *name, int fd) {

	struct lc_fdlist *l = *fdlist;

	LOCK(l);

	/* do we need more entry space? */
	if (l->count == l->capacity) {

		/* move name data out of the way */
		char *tmp = NULL;
		if (l->namelen > 0) {
			tmp = malloc(l->namelen);
			if (tmp == NULL) {
				UNLOCK(l);
				return (-1);
			}

			memcpy(tmp, lc_fdlist_names(l), l->namelen);
		}

		/* double the number of available entries */
		int namebytes_per_entry = l->namecapacity / l->capacity;
		int newnamebytes = l->capacity * namebytes_per_entry;

		int newsize = lc_fdlist_size(l) + newnamebytes
		               + l->capacity * sizeof(struct lc_fdlist_entry);

		struct lc_fdlist *copy = realloc(l, newsize);
		if (copy == NULL) {
			free(tmp);
			UNLOCK(l);
			return (-1);
		}

		copy->capacity		*= 2;
		copy->namecapacity	+= newnamebytes;

		/* copy name bytes back */
		if (copy->namelen > 0)
			memcpy(lc_fdlist_names(copy), tmp, copy->namelen);

		free(tmp);

		*fdlist = copy;
		l = *fdlist;
	}


	/* do we need more name space? */
	int subsyslen	= strlen(subsystem);
	int idlen	= strlen(id);
	int namelen	= strlen(name);

	if ((l->namelen + subsyslen + idlen + namelen) >= l->namecapacity) {

		/* double the name capacity */
		struct lc_fdlist* enlarged
			= realloc(l, lc_fdlist_size(l) + l->namecapacity);

		if (enlarged == NULL) {
			UNLOCK(l);
			return (-1);
		}

		enlarged->namecapacity *= 2;
		*fdlist = enlarged;
		l = *fdlist;
	}


	/* create the new entry */
	struct lc_fdlist_entry *entry = l->entries + l->count;

	entry->fd = fd;

	char *names = lc_fdlist_names(l);
	char *head = names + l->namelen;

	strncpy(head, subsystem, subsyslen + 1);
	entry->sysoff	= (head - names);
	entry->syslen	= subsyslen;
	head		+= subsyslen + 1;

	strncpy(head, id, idlen + 1);
	entry->idoff	= (head - names);
	entry->idlen	= idlen;
	head		+= idlen + 1;

	strncpy(head, name, namelen + 1);
	entry->nameoff	= (head - names);
	entry->namelen	= namelen + 1;
	head		+= namelen + 1;

	l->count++;
	l->namelen = (head - names);

	UNLOCK(l);

	return 0;
}


int
lc_fdlist_addcap(struct lc_fdlist **fdlist,
                 const char *subsystem, const char *id,
                 const char *name, int fd, cap_rights_t rights) {

	int cap = cap_new(fd, rights);

	return lc_fdlist_add(fdlist, subsystem, id, name, cap);
}


int
lc_fdlist_lookup(struct lc_fdlist *l,
                 const char *subsystem, const char *id, char **name, int *fdp,
                 int *pos) {

	LOCK(l);

	int successful = 0;
	const char *names = lc_fdlist_names(l);

	for (unsigned int i = (pos ? *pos + 1 : 0); i < l->count; i++) {

		struct lc_fdlist_entry *entry = l->entries + i;

		if (!strncmp(subsystem, names + entry->sysoff, entry->syslen + 1)
		    && !strncmp(id, names + entry->idoff, entry->idlen + 1)) {

			/* found a matching entry! */
			*name = malloc(entry->namelen + 1);
			strncpy(*name, names + entry->nameoff, entry->namelen + 1);

			*fdp = entry->fd;

			if (pos) *pos = i;
			successful = 1;

			break;
		}
	}

	UNLOCK(l);

	if (successful) return 0;
	else {
		errno = ENOENT;
		return (-1);
	}
}


int
lc_fdlist_size(struct lc_fdlist* l) {

	LOCK(l);

	if (l == NULL) {
		errno = EINVAL;
		return (-1);
	}

	int size = sizeof(struct lc_fdlist)
		+ l->capacity * sizeof(struct lc_fdlist_entry)
		+ l->namecapacity;

	UNLOCK(l);

	return size;
}


char*
lc_fdlist_names(struct lc_fdlist *l) {

	LOCK(l);

	if (l == NULL) {
		errno = EINVAL;
		return NULL;
	}

	char *names = ((char*) l) + lc_fdlist_size(l) - l->namecapacity;

	UNLOCK(l);

	return names;
}

