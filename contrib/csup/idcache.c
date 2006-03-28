/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
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
 * $FreeBSD$
 */
#include <sys/types.h>

#include <assert.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include "idcache.h"
#include "misc.h"

/*
 * Constants and data structures used to implement the thread-safe
 * group and password file caches.  Cache sizes must be prime.
 */
#define	UIDTONAME_SZ		317	/* Size of uid -> user name cache */
#define	NAMETOUID_SZ		317	/* Size of user name -> uid cache */
#define	GIDTONAME_SZ		317	/* Size of gid -> group name cache */
#define	NAMETOGID_SZ		317	/* Size of group name -> gid cache */

/* Node structures used to cache lookups. */
struct uidc {
	char *name;		/* user name */
	uid_t uid;		/* cached uid */
	int valid;		/* is this a valid or a miss entry */
	struct uidc *next;	/* for collisions */
};

struct gidc {
	char *name;		/* group name */
	gid_t gid;		/* cached gid */
	int valid;		/* is this a valid or a miss entry */
	struct gidc *next;	/* for collisions */
};

static struct uidc **uidtoname;	/* uid to user name cache */
static struct gidc **gidtoname;	/* gid to group name cache */
static struct uidc **nametouid;	/* user name to uid cache */
static struct gidc **nametogid;	/* group name to gid cache */

static pthread_mutex_t uid_mtx;
static pthread_mutex_t gid_mtx;

static void		uid_lock(void);
static void		uid_unlock(void);
static void		gid_lock(void);
static void		gid_unlock(void);

static uint32_t		hash(const char *);

/* A 32-bit version of Peter Weinberger's (PJW) hash algorithm,
    as used by ELF for hashing function names. */
static uint32_t
hash(const char *name)
{
	uint32_t g, h;

	h = 0;
	while(*name != '\0') {
		h = (h << 4) + *name++;
		if ((g = h & 0xF0000000)) {
			h ^= g >> 24;
			h &= 0x0FFFFFFF;
		}
	}
	return (h);
}

static void
uid_lock(void)
{
	int error;

	error = pthread_mutex_lock(&uid_mtx);
	assert(!error);
}

static void
uid_unlock(void)
{
	int error;

	error = pthread_mutex_unlock(&uid_mtx);
	assert(!error);
}

static void
gid_lock(void)
{
	int error;

	error = pthread_mutex_lock(&gid_mtx);
	assert(!error);
}

static void
gid_unlock(void)
{
	int error;

	error = pthread_mutex_unlock(&gid_mtx);
	assert(!error);
}

static void
uidc_insert(struct uidc **tbl, struct uidc *uidc, uint32_t key)
{

	uidc->next = tbl[key];
	tbl[key] = uidc;
}

static void
gidc_insert(struct gidc **tbl, struct gidc *gidc, uint32_t key)
{

	gidc->next = tbl[key];
	tbl[key] = gidc;
}

/* Return the user name for this uid, or NULL if it's not found. */
char *
getuserbyid(uid_t uid)
{
	struct passwd *pw;
	struct uidc *uidc, *uidc2;
	uint32_t key, key2;

	key = uid % UIDTONAME_SZ;
	uid_lock();
	uidc = uidtoname[key];
	while (uidc != NULL) {
		if (uidc->uid == uid)
			break;
		uidc = uidc->next;
	}

	if (uidc == NULL) {
		/* We didn't find this uid, look it up and add it. */
		uidc = xmalloc(sizeof(struct uidc));
		uidc->uid = uid;
		pw = getpwuid(uid);
		if (pw != NULL) {
			/* This uid is in the password file. */
			uidc->name = xstrdup(pw->pw_name);
			uidc->valid = 1;
			/* Also add it to the name -> gid table. */
			uidc2 = xmalloc(sizeof(struct uidc));
			uidc2->uid = uid;
			uidc2->name = uidc->name; /* We reuse the pointer. */
			uidc2->valid = 1;
			key2 = hash(uidc->name) % NAMETOUID_SZ;
			uidc_insert(nametouid, uidc2, key2);
		} else {
			/* Add a miss entry for this uid. */
			uidc->name = NULL;
			uidc->valid = 0;
		}
		uidc_insert(uidtoname, uidc, key);
	}
	/* It is safe to unlock here since the cache structure
	   is not going to get freed or changed. */
	uid_unlock();
	return (uidc->name);
}

/* Return the group name for this gid, or NULL if it's not found. */
char *
getgroupbyid(gid_t gid)
{
	struct group *gr;
	struct gidc *gidc, *gidc2;
	uint32_t key, key2;

	key = gid % GIDTONAME_SZ;
	gid_lock();
	gidc = gidtoname[key];
	while (gidc != NULL) {
		if (gidc->gid == gid)
			break;
		gidc = gidc->next;
	}

	if (gidc == NULL) {
		/* We didn't find this gid, look it up and add it. */
		gidc = xmalloc(sizeof(struct gidc));
		gidc->gid = gid;
		gr = getgrgid(gid);
		if (gr != NULL) {
			/* This gid is in the group file. */
			gidc->name = xstrdup(gr->gr_name);
			gidc->valid = 1;
			/* Also add it to the name -> gid table. */
			gidc2 = xmalloc(sizeof(struct gidc));
			gidc2->gid = gid;
			gidc2->name = gidc->name; /* We reuse the pointer. */
			gidc2->valid = 1;
			key2 = hash(gidc->name) % NAMETOGID_SZ;
			gidc_insert(nametogid, gidc2, key2);
		} else {
			/* Add a miss entry for this gid. */
			gidc->name = NULL;
			gidc->valid = 0;
		}
		gidc_insert(gidtoname, gidc, key);
	}
	/* It is safe to unlock here since the cache structure
	   is not going to get freed or changed. */
	gid_unlock();
	return (gidc->name);
}

/* Finds the uid for this user name.  If it's found, the gid is stored
   in *uid and 0 is returned.  Otherwise, -1 is returned. */
int
getuidbyname(const char *name, uid_t *uid)
{
	struct passwd *pw;
	struct uidc *uidc, *uidc2;
	uint32_t key, key2;

	uid_lock();
	key = hash(name) % NAMETOUID_SZ;
	uidc = nametouid[key];
	while (uidc != NULL) {
		if (strcmp(uidc->name, name) == 0)
			break;
		uidc = uidc->next;
	}

	if (uidc == NULL) {
		uidc = xmalloc(sizeof(struct uidc));
		uidc->name = xstrdup(name);
		pw = getpwnam(name);
		if (pw != NULL) {
			/* This user name is in the password file. */
			uidc->valid = 1;
			uidc->uid = pw->pw_uid;
			/* Also add it to the uid -> name table. */
			uidc2 = xmalloc(sizeof(struct uidc));
			uidc2->name = uidc->name; /* We reuse the pointer. */
			uidc2->uid = uidc->uid;
			uidc2->valid = 1;
			key2 = uidc2->uid % UIDTONAME_SZ;
			uidc_insert(uidtoname, uidc2, key2);
		} else {
			/* Add a miss entry for this user name. */
			uidc->valid = 0;
			uidc->uid = (uid_t)-1; /* Should not be accessed. */
		}
		uidc_insert(nametouid, uidc, key);
	}
	/* It is safe to unlock here since the cache structure
	   is not going to get freed or changed. */
	uid_unlock();
	if (!uidc->valid)
		return (-1);
	*uid = uidc->uid;
	return (0);
}

/* Finds the gid for this group name.  If it's found, the gid is stored
   in *gid and 0 is returned.  Otherwise, -1 is returned. */
int
getgidbyname(const char *name, gid_t *gid)
{
	struct group *gr;
	struct gidc *gidc, *gidc2;
	uint32_t key, key2;

	gid_lock();
	key = hash(name) % NAMETOGID_SZ;
	gidc = nametogid[key];
	while (gidc != NULL) {
		if (strcmp(gidc->name, name) == 0)
			break;
		gidc = gidc->next;
	}

	if (gidc == NULL) {
		gidc = xmalloc(sizeof(struct gidc));
		gidc->name = xstrdup(name);
		gr = getgrnam(name);
		if (gr != NULL) {
			/* This group name is in the group file. */
			gidc->gid = gr->gr_gid;
			gidc->valid = 1;
			/* Also add it to the gid -> name table. */
			gidc2 = xmalloc(sizeof(struct gidc));
			gidc2->name = gidc->name; /* We reuse the pointer. */
			gidc2->gid = gidc->gid;
			gidc2->valid = 1;
			key2 = gidc2->gid % GIDTONAME_SZ;
			gidc_insert(gidtoname, gidc2, key2);
		} else {
			/* Add a miss entry for this group name. */
			gidc->gid = (gid_t)-1; /* Should not be accessed. */
			gidc->valid = 0;
		}
		gidc_insert(nametogid, gidc, key);
	}
	/* It is safe to unlock here since the cache structure
	   is not going to get freed or changed. */
	gid_unlock();
	if (!gidc->valid)
		return (-1);
	*gid = gidc->gid;
	return (0);
}

/* Initialize the cache structures. */
void
idcache_init(void)
{

	pthread_mutex_init(&uid_mtx, NULL);
	pthread_mutex_init(&gid_mtx, NULL);
	uidtoname = xmalloc(UIDTONAME_SZ * sizeof(struct uidc *));
	gidtoname = xmalloc(GIDTONAME_SZ * sizeof(struct gidc *));
	nametouid = xmalloc(NAMETOUID_SZ * sizeof(struct uidc *));
	nametogid = xmalloc(NAMETOGID_SZ * sizeof(struct gidc *));
	memset(uidtoname, 0, UIDTONAME_SZ * sizeof(struct uidc *));
	memset(gidtoname, 0, GIDTONAME_SZ * sizeof(struct gidc *));
	memset(nametouid, 0, NAMETOUID_SZ * sizeof(struct uidc *));
	memset(nametogid, 0, NAMETOGID_SZ * sizeof(struct gidc *));
}

/* Cleanup the cache structures. */
void
idcache_fini(void)
{
	struct uidc *uidc, *uidc2;
	struct gidc *gidc, *gidc2;
	size_t i;

	for (i = 0; i < UIDTONAME_SZ; i++) {
		uidc = uidtoname[i];
		while (uidc != NULL) {
			if (uidc->name != NULL) {
				assert(uidc->valid);
				free(uidc->name);
			}
			uidc2 = uidc->next;
			free(uidc);
			uidc = uidc2;
		}
	}
	free(uidtoname);
	for (i = 0; i < NAMETOUID_SZ; i++) {
		uidc = nametouid[i];
		while (uidc != NULL) {
			assert(uidc->name != NULL);
			/* If it's a valid entry, it has been added to both the
			   uidtoname and nametouid tables, and the name pointer
			   has been reused for both entries.  Thus, the name
			   pointer has already been freed in the loop above. */
			if (!uidc->valid)
				free(uidc->name);
			uidc2 = uidc->next;
			free(uidc);
			uidc = uidc2;
		}
	}
	free(nametouid);
	for (i = 0; i < GIDTONAME_SZ; i++) {
		gidc = gidtoname[i];
		while (gidc != NULL) {
			if (gidc->name != NULL) {
				assert(gidc->valid);
				free(gidc->name);
			}
			gidc2 = gidc->next;
			free(gidc);
			gidc = gidc2;
		}
	}
	free(gidtoname);
	for (i = 0; i < NAMETOGID_SZ; i++) {
		gidc = nametogid[i];
		while (gidc != NULL) {
			assert(gidc->name != NULL);
			/* See above comment. */
			if (!gidc->valid)
				free(gidc->name);
			gidc2 = gidc->next;
			free(gidc);
			gidc = gidc2;
		}
	}
	free(nametogid);
	pthread_mutex_destroy(&uid_mtx);
	pthread_mutex_destroy(&gid_mtx);
}
