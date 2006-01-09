/*-
 * Copyright (c) 2005-2006 The FreeBSD Project
 * All rights reserved.
 *
 * Author: Victor Cruceru <soc-victor@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
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

/*
 * Host Resources MIB: hrPartitionTable implementation for SNMPd.
 */

#include <sys/types.h>
#include <sys/limits.h>

#include <assert.h>
#include <err.h>
#include <libdisk.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "hostres_snmp.h"
#include "hostres_oid.h"
#include "hostres_tree.h"

/*
 * One row in the hrPartitionTable
 */
struct partition_entry {
	struct asn_oid	index;
	u_char		label[128 + 1];
	u_char		id[128 + 1];
	int32_t		size;
	int32_t		fs_Index;
	TAILQ_ENTRY(partition_entry) link;
#define HR_PARTITION_FOUND		0x001
	uint32_t	flags;
};
TAILQ_HEAD(partition_tbl, partition_entry);

/*
 * This table is used to get a consistent indexing. It saves the name -> index
 * mapping while we rebuild the partition table.
 */
struct partition_map_entry {
	int32_t		index;		/* hrPartitionTblEntry::index */
	u_char		id[128 + 1];

	/*
	 * next may be NULL if the respective partition_entry
	 * is (temporally) gone.
	 */
	struct partition_entry	*entry;
	STAILQ_ENTRY(partition_map_entry) link;
};
STAILQ_HEAD(partition_map, partition_map_entry);

/* Mapping table for consistent indexing */
static struct partition_map partition_map =
    STAILQ_HEAD_INITIALIZER(partition_map);

/* THE partition table. */
static struct partition_tbl partition_tbl =
    TAILQ_HEAD_INITIALIZER(partition_tbl);

/* next int available for indexing the hrPartitionTable */
static uint32_t next_partition_index = 1;

/**
 * Create a new partition table entry
 */
static struct partition_entry *
partition_entry_create(int32_t ds_index, const struct chunk *chunk)
{
	struct partition_entry *entry;
	struct partition_map_entry *map = NULL;

	/* sanity checks */
	assert(chunk != NULL);
	assert(chunk->name != NULL);
	if (chunk == NULL || chunk->name == NULL || chunk->name[0] == '\0')
		return (NULL);

	if ((entry = malloc(sizeof(*entry))) == NULL) {
		syslog(LOG_WARNING, "hrPartitionTable: %s: %m", __func__);
		return (NULL);
	}
	memset(entry, 0, sizeof(*entry));

	/* check whether we already have seen this partition */
	STAILQ_FOREACH(map, &partition_map, link)
		if (strcmp(map->id, chunk->name) == 0 ) {
			map->entry = entry;
			break;
		}

	if (map == NULL) {
		/* new object - get a new index and create a map */
		if (next_partition_index > INT_MAX) {
		        syslog(LOG_ERR, "%s: hrPartitionTable index wrap",
			    __func__);
			errx(1, "hrPartitionTable index wrap");
		}

		if ((map = malloc(sizeof(*map))) == NULL) {
			syslog(LOG_ERR, "hrPartitionTable: %s: %m", __func__);
			free(entry);
			return (NULL);
		}

		map->index = next_partition_index++;

		memset(map->id, 0, sizeof(map->id));
		strncpy(map->id, chunk->name, sizeof(map->id) - 1);

		map->entry = entry;
		STAILQ_INSERT_TAIL(&partition_map, map, link);

		HRDBG("%s added into hrPartitionMap at index=%d",
		    chunk->name, map->index);

	} else {
		HRDBG("%s exists in hrPartitionMap index=%d",
		    chunk->name, map->index);
	}

	/* create the index */
	entry->index.len = 2;
	entry->index.subs[0] = ds_index;
	entry->index.subs[1] = map->index;

	memset(&entry->id[0], 0, sizeof(entry->id));
	strncpy(entry->id, chunk->name, sizeof(entry->id) - 1);

	snprintf(entry->label, sizeof(entry->label) - 1,
	    "%s%s", _PATH_DEV, chunk->name);

	INSERT_OBJECT_OID(entry, &partition_tbl);

	return (entry);
}

/**
 * Delete a partition table entry but keep the map entry intact.
 */
static void
partition_entry_delete(struct partition_entry *entry)
{
	struct partition_map_entry *map;

	assert(entry != NULL);

	TAILQ_REMOVE(&partition_tbl, entry, link);
	STAILQ_FOREACH(map, &partition_map, link)
		if (map->entry == entry) {
			map->entry = NULL;
			break;
		}

	free(entry);
}

/**
 * Find a partition table entry by name. If none is found, return NULL.
 */
static struct partition_entry *
partition_entry_find_by_name(const char *name)
{
	struct partition_entry *entry =  NULL;

	TAILQ_FOREACH(entry, &partition_tbl, link)
		if (strcmp(entry->id, name) == 0)
			return (entry);

	return (NULL);
}

/**
 * Find a partition table entry by label. If none is found, return NULL.
 */
static struct partition_entry *
partition_entry_find_by_label(const char *name)
{
	struct partition_entry *entry =  NULL;

	TAILQ_FOREACH(entry, &partition_tbl, link)
		if (strcmp(entry->label, name) == 0)
			return (entry);

	return (NULL);
}

/**
 * Process a chunk from libdisk. A chunk is either a slice or a partition.
 * If necessary create a new partition table entry for it. In any case
 * set the size field of the entry and set the FOUND flag.
 */
static void
handle_chunk(int32_t ds_index, const struct chunk* chunk,
    const struct disk *disk)
{
	struct partition_entry *entry = NULL;
	daddr_t k_size;

	assert(chunk != NULL);
	if (chunk == NULL)
		return;

	if (chunk->type == unused) {
		HRDBG("SKIP unused chunk %s", chunk->name);
		return;
	}
	HRDBG("ANALYZE chunk %s", chunk->name);

	if ((entry = partition_entry_find_by_name(chunk->name)) == NULL)
		if ((entry = partition_entry_create(ds_index, chunk)) == NULL)
			return;

	entry->flags |= HR_PARTITION_FOUND;

	/* actual size may overflow the SNMP type */
	k_size = chunk->size / (1024 / disk->sector_size);
	entry->size = (k_size > (daddr_t)INT_MAX ? INT_MAX : k_size);
}

/**
 * Start refreshing the partition table. A call to this function will
 * be followed by a call to handleDiskStorage() for every disk, followed
 * by a single call to the post_refresh function.
 */
void
partition_tbl_pre_refresh(void)
{
	struct partition_entry *entry = NULL;

	/* mark each entry as missing */
	TAILQ_FOREACH(entry, &partition_tbl, link)
		entry->flags &= ~HR_PARTITION_FOUND;
}

/**
 * Called from the DiskStorage table for every row. Open the device and
 * process all the partitions in it. ds_index is the index into the DiskStorage
 * table.
 */
void
partition_tbl_handle_disk(int32_t ds_index, const char *disk_dev_name)
{
	struct disk *disk;
	struct chunk *chunk;
     	struct chunk *partt;

	assert(disk_dev_name != NULL);
	assert(ds_index > 0);

     	if ((disk = Open_Disk(disk_dev_name)) == NULL) {
		syslog(LOG_ERR, "%s: cannot Open_Disk()", disk_dev_name);
		return;
	}

     	for (chunk = disk->chunks->part; chunk != NULL; chunk = chunk->next) {
     		handle_chunk(ds_index, chunk, disk);
		for (partt = chunk->part; partt != NULL; partt = partt->next)
     			handle_chunk(ds_index, partt, disk);
     	}
     	Free_Disk(disk);
}

/**
 * Finish refreshing the table.
 */
void
partition_tbl_post_refresh(void)
{
	struct partition_entry *e, *etmp;

	/*
	 * Purge items that disappeared
	 */
	TAILQ_FOREACH_SAFE(e, &partition_tbl, link, etmp)
		if (!(e->flags & HR_PARTITION_FOUND))
			partition_entry_delete(e);
}

/*
 * Finalization routine for hrPartitionTable
 * It destroys the lists and frees any allocated heap memory
 */
void
fini_partition_tbl(void)
{
	struct partition_map_entry *m;

     	while ((m = STAILQ_FIRST(&partition_map)) != NULL) {
		STAILQ_REMOVE_HEAD(&partition_map, link);
		if(m->entry != NULL) {
			TAILQ_REMOVE(&partition_tbl, m->entry, link);
			free(m->entry);
		}
		free(m);
     	}
	assert(TAILQ_EMPTY(&partition_tbl));
}

/**
 * Called from the file system code to insert the file system table index
 * into the partition table entry. Note, that an partition table entry exists
 * only for local file systems.
 */
void
handle_partition_fs_index(const char *name, int32_t fs_idx)
{
	struct partition_entry *entry;

	if ((entry = partition_entry_find_by_label(name)) == NULL) {
		HRDBG("%s IS MISSING from hrPartitionTable", name);
		return;
	}
	HRDBG("%s [FS index = %d] IS in hrPartitionTable", name, fs_idx);
	entry->fs_Index = fs_idx;
}

/*
 * This is the implementation for a generated (by our SNMP tool)
 * function prototype, see hostres_tree.h
 * It handles the SNMP operations for hrPartitionTable
 */
int
op_hrPartitionTable(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	struct partition_entry *entry;

	/*
	 * Refresh the disk storage table (which refreshes the partition
	 * table) if necessary.
	 */
	refresh_disk_storage_tbl(0);

	switch (op) {

	case SNMP_OP_GETNEXT:
		if ((entry = NEXT_OBJECT_OID(&partition_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);

		index_append(&value->var, sub, &entry->index);
		goto get;

	case SNMP_OP_GET:
		if ((entry = FIND_OBJECT_OID(&partition_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		goto get;

	case SNMP_OP_SET:
		if ((entry = FIND_OBJECT_OID(&partition_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NOT_WRITEABLE);
		return (SNMP_ERR_NO_CREATION);

	case SNMP_OP_ROLLBACK:
	case SNMP_OP_COMMIT:
		abort();
	}
	abort();

  get:
	switch (value->var.subs[sub - 1]) {

	case LEAF_hrPartitionIndex:
		value->v.integer = entry->index.subs[1];
		return (SNMP_ERR_NOERROR);

	case LEAF_hrPartitionLabel:
		return (string_get(value, entry->label, -1));

	case LEAF_hrPartitionID:
		return(string_get(value, entry->id, -1));

	case LEAF_hrPartitionSize:
		value->v.integer = entry->size;
		return (SNMP_ERR_NOERROR);

	case LEAF_hrPartitionFSIndex:
		value->v.integer = entry->fs_Index;
		return (SNMP_ERR_NOERROR);
	}
	abort();
}
