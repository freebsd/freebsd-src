/* $FreeBSD$ */
/* $Id: nfs4_idmap.c,v 1.4 2003/11/05 14:58:59 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 *
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 *
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

/* TODO:
 *  o validate ascii
 * */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <sys/fnv_hash.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/libkern.h>

#include <rpc/rpcclnt.h>

#include <nfs4client/nfs4_dev.h>
#include <nfs4client/nfs4_idmap.h>


#ifdef IDMAPVERBOSE
#define IDMAP_DEBUG(...) printf(__VA_ARGS__);
#else
#define IDMAP_DEBUG(...)
#endif

#define IDMAP_HASH_SIZE 37

MALLOC_DEFINE(M_IDMAP, "idmap", "idmap");

#define idmap_entry_get(ID) (ID) = malloc(sizeof(struct idmap_entry), M_IDMAP, M_WAITOK | M_ZERO)
#define idmap_entry_put(ID) free((ID), M_IDMAP)



struct idmap_entry {
	struct idmap_msg id_info;

	TAILQ_ENTRY(idmap_entry) id_entry_id;
	TAILQ_ENTRY(idmap_entry) id_entry_name;
};

struct idmap_hash {
	TAILQ_HEAD(, idmap_entry) hash_name[IDMAP_HASH_SIZE];
	TAILQ_HEAD(, idmap_entry) hash_id[IDMAP_HASH_SIZE];

	struct lock hash_lock;
};

#define IDMAP_RLOCK(lock) lockmgr(lock, LK_SHARED, NULL)
#define IDMAP_WLOCK(lock) lockmgr(lock, LK_EXCLUSIVE, NULL)
#define IDMAP_UNLOCK(lock) lockmgr(lock, LK_RELEASE, NULL)


static struct idmap_hash idmap_uid_hash;
static struct idmap_hash idmap_gid_hash;

static struct idmap_entry * idmap_name_lookup(uint32_t, char *);
static struct idmap_entry * idmap_id_lookup(uint32_t, ident_t);
static int idmap_upcall_name(uint32_t, char *, struct idmap_entry **);
static int idmap_upcall_id(uint32_t , ident_t, struct idmap_entry ** );
static int idmap_add(struct idmap_entry *);

static int
idmap_upcall_name(uint32_t type, char * name, struct idmap_entry ** found)
{
	int error;
	struct idmap_entry * e;
	size_t len, siz;

	if (type > IDMAP_MAX_TYPE || type == 0) {
		IDMAP_DEBUG("bad type %d\n", type);
	 	return EINVAL; /* XXX */
	}

	if (name == NULL || (len = strlen(name)) == 0 || len > IDMAP_MAXNAMELEN) {
		IDMAP_DEBUG("idmap_upcall_name: bad name\n");
		return EFAULT;	/* XXX */
	}

	e = malloc(sizeof(struct idmap_entry), M_IDMAP,
	    M_WAITOK | M_ZERO);

	e->id_info.id_type = type;
	bcopy(name, e->id_info.id_name, len);
	e->id_info.id_namelen = len;


	siz = sizeof(struct idmap_msg);
	error = nfs4dev_call(NFS4DEV_TYPE_IDMAP, (caddr_t)&e->id_info, siz,
	    (caddr_t)&e->id_info, &siz);

	if (error) {
		IDMAP_DEBUG("error %d in nfs4dev_upcall()\n", error);
		*found = NULL;
		return error;
	}

	if (siz != sizeof(struct idmap_msg)) {
	  	IDMAP_DEBUG("bad size of returned message\n");
		*found = NULL;
		return EFAULT;
	}

	*found = e;
	return 0;
}

static int
idmap_upcall_id(uint32_t type, ident_t id, struct idmap_entry ** found)
{
	int error;
	struct idmap_entry * e;
	size_t siz;

	if (type > IDMAP_MAX_TYPE)
	 	panic("bad type"); /* XXX */

	e = malloc(sizeof(struct idmap_entry), M_IDMAP,
	    M_WAITOK | M_ZERO);

	e->id_info.id_type = type;
	e->id_info.id_namelen = 0;	/* should already */
	e->id_info.id_id = id;

	siz = sizeof(struct idmap_msg);
	error = nfs4dev_call(NFS4DEV_TYPE_IDMAP, (caddr_t)&e->id_info, siz,
	    (caddr_t)&e->id_info, &siz);

	if (error) {
		IDMAP_DEBUG("error %d in nfs4dev_upcall()\n", error);
		*found = NULL;
		return error;
	}

	if (siz != sizeof(struct idmap_msg)) {
	  	IDMAP_DEBUG("bad size of returned message\n");
		*found = NULL;
		return EFAULT;
	}

	*found = e;
	return 0;
}

static void
idmap_hashf(struct idmap_entry *e, uint32_t * hval_id, uint32_t * hval_name)
{
	switch (e->id_info.id_type) {
	case IDMAP_TYPE_UID:
		*hval_id = e->id_info.id_id.uid % IDMAP_HASH_SIZE;
                break;
	case IDMAP_TYPE_GID:
		*hval_id = e->id_info.id_id.gid % IDMAP_HASH_SIZE;
                break;
	default:
		/* XXX yikes! */
		panic("hashf: bad type!");
                break;
        }

	if (e->id_info.id_namelen == 0)
		/* XXX */ panic("hashf: bad name");

	*hval_name = fnv_32_str(e->id_info.id_name, FNV1_32_INIT) % IDMAP_HASH_SIZE;
}

static int
idmap_add(struct idmap_entry * e)
{
	struct idmap_hash * hash;
        uint32_t hval_id, hval_name;

	if (e->id_info.id_namelen == 0) {
		printf("idmap_add: name of len 0\n");
		return EINVAL;
	}

        switch (e->id_info.id_type) {
	case IDMAP_TYPE_UID:
		hash = &idmap_uid_hash;
                break;
	case IDMAP_TYPE_GID:
		hash = &idmap_gid_hash;
                break;
	default:
		/* XXX yikes */
		panic("idmap add: bad type!");
                break;
        }

	idmap_hashf(e, &hval_id, &hval_name);

	IDMAP_WLOCK(&hash->hash_lock);

	TAILQ_INSERT_TAIL(&hash->hash_id[hval_id], e, id_entry_id);
	TAILQ_INSERT_TAIL(&hash->hash_name[hval_name], e, id_entry_name);

	IDMAP_UNLOCK(&hash->hash_lock);

	return 0;
}

static struct idmap_entry *
idmap_id_lookup(uint32_t type, ident_t id)
{
	struct idmap_hash * hash;
	uint32_t hval;
	struct idmap_entry * e;

	switch (type) {
	case IDMAP_TYPE_UID:
		hash = &idmap_uid_hash;
		hval = id.uid % IDMAP_HASH_SIZE;
		break;
	case IDMAP_TYPE_GID:
		hash = &idmap_gid_hash;
		hval = id.gid % IDMAP_HASH_SIZE;
		break;
	default:
		/* XXX yikes */
		panic("lookup: bad type!");
		break;
	}


	IDMAP_RLOCK(&hash->hash_lock);

	TAILQ_FOREACH(e, &hash->hash_id[hval], id_entry_name) {
	  	if ((type == IDMAP_TYPE_UID && e->id_info.id_id.uid == id.uid)||
		    (type == IDMAP_TYPE_GID  && e->id_info.id_id.gid == id.gid)) {
			IDMAP_UNLOCK(&hash->hash_lock);
			return e;
		}
	}

	IDMAP_UNLOCK(&hash->hash_lock);
	return NULL;
}

static struct idmap_entry *
idmap_name_lookup(uint32_t type, char * name)
{
	struct idmap_hash * hash;
	uint32_t hval;
	struct idmap_entry * e;
	size_t len;

	switch (type) {
	case IDMAP_TYPE_UID:
	 	hash = &idmap_uid_hash;
		break;
	case IDMAP_TYPE_GID:
	 	hash = &idmap_gid_hash;
		break;
	default:
		/* XXX yikes */
		panic("lookup: bad type!");
		break;
	}

	len = strlen(name);

	if (len == 0 || len > IDMAP_MAXNAMELEN) {
		IDMAP_DEBUG("bad name length %d\n", len);
		return NULL;
	}

	hval = fnv_32_str(name, FNV1_32_INIT) % IDMAP_HASH_SIZE;

	IDMAP_RLOCK(&hash->hash_lock);

	TAILQ_FOREACH(e, &hash->hash_name[hval], id_entry_name) {
		if ((strlen(e->id_info.id_name) == strlen(name)) && strncmp(e->id_info.id_name, name, strlen(name)) == 0) {
			IDMAP_UNLOCK(&hash->hash_lock);
			return e;
		}
	}

	IDMAP_UNLOCK(&hash->hash_lock);
	return NULL;
}

void
idmap_init(void)
{
	unsigned int i;

	for (i=0; i<IDMAP_HASH_SIZE; i++) {
		TAILQ_INIT(&idmap_uid_hash.hash_name[i]);
		TAILQ_INIT(&idmap_uid_hash.hash_id[i]);

		TAILQ_INIT(&idmap_gid_hash.hash_name[i]);
		TAILQ_INIT(&idmap_gid_hash.hash_id[i]);
	}

	lockinit(&idmap_uid_hash.hash_lock, PLOCK, "idmap uid hash table", 0,0);
	lockinit(&idmap_gid_hash.hash_lock, PLOCK, "idmap gid hash table", 0,0);

}

void idmap_uninit(void)
{
  	struct idmap_entry * e;
	int i;

	lockdestroy(&idmap_uid_hash.hash_lock);
	lockdestroy(&idmap_gid_hash.hash_lock);

	for (i=0; i<IDMAP_HASH_SIZE; i++) {
		while(!TAILQ_EMPTY(&idmap_uid_hash.hash_name[i])) {
			e = TAILQ_FIRST(&idmap_uid_hash.hash_name[i]);
			TAILQ_REMOVE(&idmap_uid_hash.hash_name[i], e, id_entry_name);
			TAILQ_REMOVE(&idmap_uid_hash.hash_id[i], e, id_entry_id);
			free(e, M_IDMAP);
		}

		while(!TAILQ_EMPTY(&idmap_gid_hash.hash_name[i])) {
			e = TAILQ_FIRST(&idmap_gid_hash.hash_name[i]);
			TAILQ_REMOVE(&idmap_gid_hash.hash_name[i], e, id_entry_name);
			TAILQ_REMOVE(&idmap_gid_hash.hash_id[i], e, id_entry_id);
			free(e, M_IDMAP);
		}

	}
}

int
idmap_uid_to_name(uid_t uid, char ** name, size_t * len)
{
  	struct idmap_entry * e;
	int error = 0;
	ident_t id;

	id.uid = uid;


	if ((e = idmap_id_lookup(IDMAP_TYPE_UID, id)) == NULL) {
	  	if ((error = idmap_upcall_id(IDMAP_TYPE_UID, id, &e)) != 0) {
			IDMAP_DEBUG("error in upcall\n");
			return error;
		}

		if (e == NULL) {
			IDMAP_DEBUG("no error from upcall, but no data returned\n");
			return EFAULT;
		}

		if (idmap_add(e) != 0) {
			IDMAP_DEBUG("idmap_add failed\n");
			free(e, M_IDMAP);
			return EFAULT;
		}
	}

	*name = e->id_info.id_name;
	*len = e->id_info.id_namelen;
	return 0;
}

int
idmap_gid_to_name(gid_t gid, char ** name, size_t * len)
{
  	struct idmap_entry * e;
	int error = 0;
	ident_t id;

	id.gid = gid;


	if ((e = idmap_id_lookup(IDMAP_TYPE_GID, id)) == NULL) {
	  	if ((error = idmap_upcall_id(IDMAP_TYPE_GID, id, &e))) {
			IDMAP_DEBUG("error in upcall\n");
			return error;
		}

		if (e == NULL) {
			IDMAP_DEBUG("no error from upcall, but no data returned\n");
			return EFAULT;
		}

		if (idmap_add(e) != 0) {
			IDMAP_DEBUG("idmap_add failed\n");
			free(e, M_IDMAP);
		}
	}

	*name = e->id_info.id_name;
	*len  = e->id_info.id_namelen;
	return 0;
}

int
idmap_name_to_uid(char * name, size_t len, uid_t * id)
{
  	struct idmap_entry * e;
	int error = 0;
	char * namestr;

	if (name == NULL )
		return EFAULT;

	if (len == 0 || len > IDMAP_MAXNAMELEN) {
	  	IDMAP_DEBUG("idmap_name_to_uid: bad len\n");
	  	return EINVAL;
	}

	/* XXX hack */
	namestr = malloc(len + 1, M_TEMP, M_WAITOK);
	bcopy(name, namestr, len);
	namestr[len] = '\0';


	if ((e = idmap_name_lookup(IDMAP_TYPE_UID, namestr)) == NULL) {
	  	if ((error = idmap_upcall_name(IDMAP_TYPE_UID, namestr, &e))) {
			free(namestr, M_TEMP);
			return error;
		}

		if (e == NULL) {
			IDMAP_DEBUG("no error from upcall, but no data returned\n");
			free(namestr, M_TEMP);
			return EFAULT;
		}

		if (idmap_add(e) != 0) {
			IDMAP_DEBUG("idmap_add failed\n");
			free(e, M_IDMAP);
		}
	}

	*id = e->id_info.id_id.uid;
	free(namestr, M_TEMP);
	return 0;
}

int
idmap_name_to_gid(char * name, size_t len, gid_t * id)
{
  	struct idmap_entry * e;
	int error = 0;

	char * namestr;

	if (name == NULL )
		return EFAULT;

	if (len == 0 || len > IDMAP_MAXNAMELEN) {
	  	IDMAP_DEBUG("idmap_name_to_uid: bad len\n");
	  	return EINVAL;
	}

	/* XXX hack */
	namestr = malloc(len + 1, M_TEMP, M_WAITOK);
	bcopy(name, namestr, len);
	namestr[len] = '\0';


	if ((e = idmap_name_lookup(IDMAP_TYPE_GID, namestr)) == NULL) {
	  	if ((error = idmap_upcall_name(IDMAP_TYPE_GID, namestr, &e)) != 0) {
			IDMAP_DEBUG("error in upcall\n");
			free(namestr, M_TEMP);
			return error;
		}

		if (e == NULL) {
			IDMAP_DEBUG("no error from upcall, but no data returned\n");
			free(namestr, M_TEMP);
			return EFAULT;
		}

		if (idmap_add(e) != 0) {
			IDMAP_DEBUG("idmap_add failed\n");
			free(e, M_IDMAP);
		}
	}

	*id = e->id_info.id_id.gid;
	free(namestr, M_TEMP);
	return 0;
}
