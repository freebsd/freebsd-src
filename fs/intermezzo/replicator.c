/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
 * Copyright (C) 2001 Tacit Networks, Inc. <phil@off.net>
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Manage RCVD records for clients in the kernel
 *
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <stdarg.h>
#include <asm/uaccess.h>

#include <linux/errno.h>

#include <linux/intermezzo_fs.h>

/*
 * this file contains a hash table of replicators/clients for a
 * fileset. It allows fast lookup and update of reintegration status
 */

struct izo_offset_rec {
	struct list_head or_list;
	char             or_uuid[16];
	loff_t           or_offset;
};

#define RCACHE_BITS 8
#define RCACHE_SIZE (1 << RCACHE_BITS)
#define RCACHE_MASK (RCACHE_SIZE - 1)

static struct list_head *
izo_rep_cache(void)
{
	int i;
	struct list_head *cache;
	PRESTO_ALLOC(cache, sizeof(struct list_head) * RCACHE_SIZE);
	if (cache == NULL) {
		CERROR("intermezzo-fatal: no memory for replicator cache\n");
                return NULL;
	}
	memset(cache, 0, sizeof(struct list_head) * RCACHE_SIZE);
	for (i = 0; i < RCACHE_SIZE; i++)
		INIT_LIST_HEAD(&cache[i]);

	return cache;
}

static struct list_head *
izo_rep_hash(struct list_head *cache, char *uuid)
{
        return &cache[(RCACHE_MASK & uuid[1])];
}

static void
izo_rep_cache_clean(struct presto_file_set *fset)
{
	int i;
	struct list_head *bucket;
	struct list_head *tmp;

        if (fset->fset_clients == NULL)
		return;
        for (i = 0; i < RCACHE_SIZE; i++) {
		tmp = bucket = &fset->fset_clients[i];

		tmp = tmp->next;
                while (tmp != bucket) {
			struct izo_offset_rec *offrec;
			tmp = tmp->next;
			list_del(tmp);
			offrec = list_entry(tmp, struct izo_offset_rec,
					    or_list);
			PRESTO_FREE(offrec, sizeof(struct izo_offset_rec));
		}
	}
}

struct izo_offset_rec *
izo_rep_cache_find(struct presto_file_set *fset, char *uuid)
{
	struct list_head *buck = izo_rep_hash(fset->fset_clients, uuid);
	struct list_head *tmp = buck;
        struct izo_offset_rec *rec = NULL;

        while ( (tmp = tmp->next) != buck ) {
		rec = list_entry(tmp, struct izo_offset_rec, or_list);
                if ( memcmp(rec->or_uuid, uuid, sizeof(rec->or_uuid)) == 0 )
			return rec;
	}

	return NULL;
}

static int
izo_rep_cache_add(struct presto_file_set *fset, struct izo_rcvd_rec *rec,
                  loff_t offset)
{
        struct izo_offset_rec *offrec;

        if (izo_rep_cache_find(fset, rec->lr_uuid)) {
                CERROR("izo: duplicate client entry %s off %Ld\n",
                       fset->fset_name, offset);
                return -EINVAL;
        }

        PRESTO_ALLOC(offrec, sizeof(*offrec));
        if (offrec == NULL) {
                CERROR("izo: cannot allocate offrec\n");
                return -ENOMEM;
        }

        memcpy(offrec->or_uuid, rec->lr_uuid, sizeof(rec->lr_uuid));
        offrec->or_offset = offset;

        list_add(&offrec->or_list,
                 izo_rep_hash(fset->fset_clients, rec->lr_uuid));
        return 0;
}

int
izo_rep_cache_init(struct presto_file_set *fset)
{
	struct izo_rcvd_rec rec;
        loff_t offset = 0, last_offset = 0;

	fset->fset_clients = izo_rep_cache();
        if (fset->fset_clients == NULL) {
		CERROR("Error initializing client cache\n");
		return -ENOMEM;
	}

        while ( presto_fread(fset->fset_rcvd.fd_file, (char *)&rec,
                             sizeof(rec), &offset) == sizeof(rec) ) {
                int rc;

                if ((rc = izo_rep_cache_add(fset, &rec, last_offset)) < 0) {
			izo_rep_cache_clean(fset);
			return rc;
		}

                last_offset = offset;
	}

	return 0;
}

/*
 * Return local last_rcvd record for the client. Update or create 
 * if necessary.
 *
 * XXX: After this call, any -EINVAL from izo_rcvd_get is a real error.
 */
int
izo_repstatus(struct presto_file_set *fset,  __u64 client_kmlsize, 
              struct izo_rcvd_rec *lr_client, struct izo_rcvd_rec *lr_server)
{
        int rc;
        rc = izo_rcvd_get(lr_server, fset, lr_client->lr_uuid);
        if (rc < 0 && rc != -EINVAL) {
                return rc;
        }

        /* client is new or has been reset. */
        if (rc < 0 || (client_kmlsize == 0 && lr_client->lr_remote_offset == 0)) {
                memset(lr_server, 0, sizeof(*lr_server));
                memcpy(lr_server->lr_uuid, lr_client->lr_uuid, sizeof(lr_server->lr_uuid));
                rc = izo_rcvd_write(fset, lr_server);
                if (rc < 0)
                        return rc;
        }

        /* update intersync */
        rc = izo_upc_repstatus(presto_f2m(fset), fset->fset_name, lr_server);
        return rc;
}

loff_t
izo_rcvd_get(struct izo_rcvd_rec *rec, struct presto_file_set *fset, char *uuid)
{
        struct izo_offset_rec *offrec;
        struct izo_rcvd_rec tmprec;
        loff_t offset;

        offrec = izo_rep_cache_find(fset, uuid);
        if (offrec == NULL) {
                CDEBUG(D_SPECIAL, "izo_get_rcvd: uuid not in hash.\n");
                return -EINVAL;
        }
        offset = offrec->or_offset;

        if (rec == NULL)
                return offset;

        if (presto_fread(fset->fset_rcvd.fd_file, (char *)&tmprec,
                         sizeof(tmprec), &offset) != sizeof(tmprec)) {
                CERROR("izo_get_rcvd: Unable to read from last_rcvd file offset "
                       "%Lu\n", offset);
                return -EIO;
        }

        memcpy(rec->lr_uuid, tmprec.lr_uuid, sizeof(tmprec.lr_uuid));
        rec->lr_remote_recno = le64_to_cpu(tmprec.lr_remote_recno);
        rec->lr_remote_offset = le64_to_cpu(tmprec.lr_remote_offset);
        rec->lr_local_recno = le64_to_cpu(tmprec.lr_local_recno);
        rec->lr_local_offset = le64_to_cpu(tmprec.lr_local_offset);
        rec->lr_last_ctime = le64_to_cpu(tmprec.lr_last_ctime);

        return offrec->or_offset;
}

/* Try to lookup the UUID in the hash.  Insert it if it isn't found.  Write the
 * data to the file.
 *
 * Returns the offset of the beginning of the record in the last_rcvd file. */
loff_t
izo_rcvd_write(struct presto_file_set *fset, struct izo_rcvd_rec *rec)
{
        struct izo_offset_rec *offrec;
        loff_t offset, rc;

        ENTRY;

        offrec = izo_rep_cache_find(fset, rec->lr_uuid);
        if (offrec == NULL) {
                /* I don't think it should be possible for an entry to be not in
                 * the hash table without also having an invalid offset, but we
                 * handle it gracefully regardless. */
                write_lock(&fset->fset_rcvd.fd_lock);
                offset = fset->fset_rcvd.fd_offset;
                fset->fset_rcvd.fd_offset += sizeof(*rec);
                write_unlock(&fset->fset_rcvd.fd_lock);

                rc = izo_rep_cache_add(fset, rec, offset);
                if (rc < 0) {
                        EXIT;
                        return rc;
                }
        } else
                offset = offrec->or_offset;
        

        rc = presto_fwrite(fset->fset_rcvd.fd_file, (char *)rec, sizeof(*rec),
                           &offset);
        if (rc == sizeof(*rec))
                /* presto_fwrite() advances 'offset' */
                rc = offset - sizeof(*rec);

        EXIT;
        return rc;
}

loff_t
izo_rcvd_upd_remote(struct presto_file_set *fset, char * uuid,  __u64 remote_recno, 
                    __u64 remote_offset)
{
        struct izo_rcvd_rec rec;
        
        loff_t rc;

        ENTRY;
        rc = izo_rcvd_get(&rec, fset, uuid);
        if (rc < 0)
                return rc;
        rec.lr_remote_recno = remote_recno;
        rec.lr_remote_offset = remote_offset;

        rc = izo_rcvd_write(fset, &rec);
        EXIT;
        if (rc < 0)
                return rc;
        return 0;
}
