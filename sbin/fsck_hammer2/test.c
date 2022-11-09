/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2019 The DragonFly Project
 * All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/ttycom.h>
#include <sys/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <crypto/sha2/sha256.h>

#include <fs/hammer2/hammer2_disk.h>
#include <fs/hammer2/hammer2_xxhash.h>

#include "hammer2_subs.h"
#include "fsck_hammer2.h"

struct blockref_msg {
	TAILQ_ENTRY(blockref_msg) entry;
	hammer2_blockref_t bref;
	void *msg;
};

TAILQ_HEAD(blockref_list, blockref_msg);

struct blockref_entry {
	RB_ENTRY(blockref_entry) entry;
	hammer2_off_t data_off;
	struct blockref_list head;
};

static int
blockref_cmp(struct blockref_entry *b1, struct blockref_entry *b2)
{
	if (b1->data_off < b2->data_off)
		return -1;
	if (b1->data_off > b2->data_off)
		return 1;
	return 0;
}

RB_HEAD(blockref_tree, blockref_entry);
RB_PROTOTYPE(blockref_tree, blockref_entry, entry, blockref_cmp);
RB_GENERATE(blockref_tree, blockref_entry, entry, blockref_cmp);

typedef struct {
	struct blockref_tree root;
	uint8_t type; /* HAMMER2_BREF_TYPE_VOLUME or FREEMAP */
	uint64_t total_blockref;
	uint64_t total_empty;
	uint64_t total_bytes;
	union {
		/* use volume or freemap depending on type value */
		struct {
			uint64_t total_inode;
			uint64_t total_indirect;
			uint64_t total_data;
			uint64_t total_dirent;
		} volume;
		struct {
			uint64_t total_freemap_node;
			uint64_t total_freemap_leaf;
		} freemap;
	};
} blockref_stats_t;

typedef struct {
	uint64_t total_blockref;
	uint64_t total_empty;
	uint64_t total_bytes;
	struct {
		uint64_t total_inode;
		uint64_t total_indirect;
		uint64_t total_data;
		uint64_t total_dirent;
	} volume;
	struct {
		uint64_t total_freemap_node;
		uint64_t total_freemap_leaf;
	} freemap;
	long count;
} delta_stats_t;

static void print_blockref_entry(struct blockref_tree *);
static void init_blockref_stats(blockref_stats_t *, uint8_t);
static void cleanup_blockref_stats(blockref_stats_t *);
static void init_delta_root(struct blockref_tree *);
static void cleanup_delta_root(struct blockref_tree *);
static void print_blockref_stats(const blockref_stats_t *, bool);
static int verify_volume_header(const hammer2_volume_data_t *);
static int read_media(const hammer2_blockref_t *, hammer2_media_data_t *,
    size_t *);
static int verify_blockref(const hammer2_volume_data_t *,
    const hammer2_blockref_t *, bool, blockref_stats_t *,
    struct blockref_tree *, delta_stats_t *, int, int);
static void print_pfs(const hammer2_inode_data_t *);
static char *get_inode_filename(const hammer2_inode_data_t *);
static int init_pfs_blockref(const hammer2_volume_data_t *,
    const hammer2_blockref_t *, struct blockref_list *);
static void cleanup_pfs_blockref(struct blockref_list *);
static void print_media(FILE *, int, const hammer2_blockref_t *,
    const hammer2_media_data_t *, size_t);

static int best_zone = -1;

#define TAB 8

static void
tfprintf(FILE *fp, int tab, const char *ctl, ...)
{
	va_list va;
	int ret;

	ret = fprintf(fp, "%*s", tab * TAB, "");
	if (ret < 0)
		return;

	va_start(va, ctl);
	vfprintf(fp, ctl, va);
	va_end(va);
}

static void
tsnprintf(char *str, size_t siz, int tab, const char *ctl, ...)
{
	va_list va;
	int ret;

	ret = snprintf(str, siz, "%*s", tab * TAB, "");
	if (ret < 0 || ret >= (int)siz)
		return;

	va_start(va, ctl);
	vsnprintf(str + ret, siz - ret, ctl, va);
	va_end(va);
}

static void
tprintf_zone(int tab, int i, const hammer2_blockref_t *bref)
{
	tfprintf(stdout, tab, "zone.%d %016jx%s\n",
	    i, (uintmax_t)bref->data_off,
	    (!ScanBest && i == best_zone) ? " (best)" : "");
}

static int
init_root_blockref(int i, uint8_t type, hammer2_blockref_t *bref)
{
	hammer2_off_t off;

	assert(type == HAMMER2_BREF_TYPE_EMPTY ||
		type == HAMMER2_BREF_TYPE_VOLUME ||
		type == HAMMER2_BREF_TYPE_FREEMAP);
	memset(bref, 0, sizeof(*bref));
	bref->type = type;
	bref->data_off = (i * HAMMER2_ZONE_BYTES64) | HAMMER2_PBUFRADIX;
	off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;

	return lseek(hammer2_get_root_volume_fd(),
	    off - hammer2_get_root_volume_offset(), SEEK_SET);
}

static int
find_best_zone(void)
{
	hammer2_blockref_t best;
	int i, best_i = -1;

	memset(&best, 0, sizeof(best));

	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		hammer2_volume_data_t voldata;
		hammer2_blockref_t broot;
		ssize_t ret;

		if (i * HAMMER2_ZONE_BYTES64 >=
		    hammer2_get_root_volume_size())
			break;
		init_root_blockref(i, HAMMER2_BREF_TYPE_EMPTY, &broot);
		ret = read(hammer2_get_root_volume_fd(), &voldata,
		    HAMMER2_PBUFSIZE);
		if (ret == HAMMER2_PBUFSIZE) {
			if ((voldata.magic != HAMMER2_VOLUME_ID_HBO) &&
			    (voldata.magic != HAMMER2_VOLUME_ID_ABO))
				continue;
			broot.mirror_tid = voldata.mirror_tid;
			if (best_i < 0 || best.mirror_tid < broot.mirror_tid) {
				best_i = i;
				best = broot;
			}
		} else if (ret == -1) {
			perror("read");
			return -1;
		} else {
			tfprintf(stderr, 1, "Failed to read volume header\n");
			return -1;
		}
	}

	return best_i;
}

static int
test_volume_header(void)
{
	bool failed = false;
	int i;

	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		hammer2_volume_data_t voldata;
		hammer2_blockref_t broot;
		ssize_t ret;

		if (ScanBest && i != best_zone)
			continue;
		if (i * HAMMER2_ZONE_BYTES64 >=
		    hammer2_get_root_volume_size()) {
			tfprintf(stderr, 0, "zone.%d exceeds volume size\n", i);
			break;
		}
		init_root_blockref(i, HAMMER2_BREF_TYPE_EMPTY, &broot);
		ret = read(hammer2_get_root_volume_fd(), &voldata,
		    HAMMER2_PBUFSIZE);
		if (ret == HAMMER2_PBUFSIZE) {
			tprintf_zone(0, i, &broot);
			if (verify_volume_header(&voldata) == -1)
				failed = true;
		} else if (ret == -1) {
			perror("read");
			return -1;
		} else {
			tfprintf(stderr, 1, "Failed to read volume header\n");
			return -1;
		}
	}

	return failed ? -1 : 0;
}

static int
test_blockref(uint8_t type)
{
	struct blockref_tree droot;
	bool failed = false;
	int i;

	init_delta_root(&droot);
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		hammer2_volume_data_t voldata;
		hammer2_blockref_t broot;
		ssize_t ret;

		if (ScanBest && i != best_zone)
			continue;
		if (i * HAMMER2_ZONE_BYTES64 >=
		    hammer2_get_root_volume_size()) {
			tfprintf(stderr, 0, "zone.%d exceeds volume size\n", i);
			break;
		}
		init_root_blockref(i, type, &broot);
		ret = read(hammer2_get_root_volume_fd(), &voldata,
		    HAMMER2_PBUFSIZE);
		if (ret == HAMMER2_PBUFSIZE) {
			blockref_stats_t bstats;
			init_blockref_stats(&bstats, type);
			delta_stats_t ds;
			memset(&ds, 0, sizeof(ds));
			tprintf_zone(0, i, &broot);
			if (verify_blockref(&voldata, &broot, false, &bstats,
			    &droot, &ds, 0, 0) == -1)
				failed = true;
			print_blockref_stats(&bstats, true);
			print_blockref_entry(&bstats.root);
			cleanup_blockref_stats(&bstats);
		} else if (ret == -1) {
			perror("read");
			failed = true;
			goto end;
		} else {
			tfprintf(stderr, 1, "Failed to read volume header\n");
			failed = true;
			goto end;
		}
	}
end:
	cleanup_delta_root(&droot);
	return failed ? -1 : 0;
}

static int
test_pfs_blockref(void)
{
	struct blockref_tree droot;
	uint8_t type = HAMMER2_BREF_TYPE_VOLUME;
	bool failed = false;
	int i;

	init_delta_root(&droot);
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		hammer2_volume_data_t voldata;
		hammer2_blockref_t broot;
		ssize_t ret;

		if (ScanBest && i != best_zone)
			continue;
		if (i * HAMMER2_ZONE_BYTES64 >=
		    hammer2_get_root_volume_size()) {
			tfprintf(stderr, 0, "zone.%d exceeds volume size\n", i);
			break;
		}
		init_root_blockref(i, type, &broot);
		ret = read(hammer2_get_root_volume_fd(), &voldata,
		    HAMMER2_PBUFSIZE);
		if (ret == HAMMER2_PBUFSIZE) {
			struct blockref_list blist;
			struct blockref_msg *p;
			int count = 0;

			tprintf_zone(0, i, &broot);
			TAILQ_INIT(&blist);
			if (init_pfs_blockref(&voldata, &broot, &blist) == -1) {
				tfprintf(stderr, 1, "Failed to read PFS "
				    "blockref\n");
				failed = true;
				continue;
			}
			if (TAILQ_EMPTY(&blist)) {
				tfprintf(stderr, 1, "Failed to find PFS "
				    "blockref\n");
				failed = true;
				continue;
			}
			TAILQ_FOREACH(p, &blist, entry) {
				blockref_stats_t bstats;
				bool found = false;
				char *f = get_inode_filename(p->msg);
				if (NumPFSNames) {
					int j;
					for (j = 0; j < NumPFSNames; j++)
						if (!strcmp(PFSNames[j], f))
							found = true;
				} else
					found = true;
				if (!found) {
					free(f);
					continue;
				}
				count++;
				if (PrintPFS) {
					print_pfs(p->msg);
					free(f);
					continue;
				}
				tfprintf(stdout, 1, "%s\n", f);
				free(f);
				init_blockref_stats(&bstats, type);
				delta_stats_t ds;
				memset(&ds, 0, sizeof(ds));
				if (verify_blockref(&voldata, &p->bref, false,
				    &bstats, &droot, &ds, 0, 0) == -1)
					failed = true;
				print_blockref_stats(&bstats, true);
				print_blockref_entry(&bstats.root);
				cleanup_blockref_stats(&bstats);
			}
			cleanup_pfs_blockref(&blist);
			if (NumPFSNames && !count) {
				tfprintf(stderr, 1, "PFS not found\n");
				failed = true;
			}
		} else if (ret == -1) {
			perror("read");
			failed = true;
			goto end;
		} else {
			tfprintf(stderr, 1, "Failed to read volume header\n");
			failed = true;
			goto end;
		}
	}
end:
	cleanup_delta_root(&droot);
	return failed ? -1 : 0;
}

static int
charsperline(void)
{
	int columns;
	char *cp;
	struct winsize ws;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;	/* last resort */

	return columns;
}

static void
cleanup_blockref_msg(struct blockref_list *head)
{
	struct blockref_msg *p;

	while ((p = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, p, entry);
		free(p->msg);
		free(p);
	}
	assert(TAILQ_EMPTY(head));
}

static void
cleanup_blockref_entry(struct blockref_tree *root)
{
	struct blockref_entry *e;

	while ((e = RB_ROOT(root)) != NULL) {
		RB_REMOVE(blockref_tree, root, e);
		cleanup_blockref_msg(&e->head);
		free(e);
	}
	assert(RB_EMPTY(root));
}

static void
add_blockref_msg(struct blockref_list *head, const hammer2_blockref_t *bref,
    const void *msg, size_t siz)
{
	struct blockref_msg *m;
	void *p;

	m = calloc(1, sizeof(*m));
	assert(m);
	m->bref = *bref;
	p = calloc(1, siz);
	assert(p);
	memcpy(p, msg, siz);
	m->msg = p;

	TAILQ_INSERT_TAIL(head, m, entry);
}

static void
add_blockref_entry(struct blockref_tree *root, const hammer2_blockref_t *bref,
    const void *msg, size_t siz)
{
	struct blockref_entry *e, bref_find;

	memset(&bref_find, 0, sizeof(bref_find));
	bref_find.data_off = bref->data_off;
	e = RB_FIND(blockref_tree, root, &bref_find);
	if (!e) {
		e = calloc(1, sizeof(*e));
		assert(e);
		TAILQ_INIT(&e->head);
		e->data_off = bref->data_off;
	}

	add_blockref_msg(&e->head, bref, msg, siz);

	RB_INSERT(blockref_tree, root, e);
}

static void
__print_blockref(FILE *fp, int tab, const hammer2_blockref_t *bref,
    const char *msg)
{
	tfprintf(fp, tab, "%016jx %-12s %016jx/%-2d%s%s\n",
	    (uintmax_t)bref->data_off,
	    hammer2_breftype_to_str(bref->type),
	    (uintmax_t)bref->key,
	    bref->keybits,
	    msg ? " " : "",
	    msg ? msg : "");
}

static void
print_blockref(FILE *fp, const hammer2_blockref_t *bref, const char *msg)
{
	__print_blockref(fp, 1, bref, msg);
}

static void
print_blockref_debug(FILE *fp, int depth, int index,
    const hammer2_blockref_t *bref, const char *msg)
{
	if (DebugOpt > 1) {
		char buf[256];
		int i;

		memset(buf, 0, sizeof(buf));
		for (i = 0; i < depth * 2; i++)
			strlcat(buf, " ", sizeof(buf));
		tfprintf(fp, 1, buf);
		fprintf(fp, "%-2d %-3d ", depth, index);
		__print_blockref(fp, 0, bref, msg);
	} else if (DebugOpt > 0)
		print_blockref(fp, bref, msg);
}

static void
print_blockref_msg(const struct blockref_list *head)
{
	struct blockref_msg *m;

	TAILQ_FOREACH(m, head, entry) {
		hammer2_blockref_t *bref = &m->bref;
		print_blockref(stderr, bref, m->msg);
		if (VerboseOpt > 0) {
			hammer2_media_data_t media;
			size_t bytes;
			if (!read_media(bref, &media, &bytes))
				print_media(stderr, 2, bref, &media, bytes);
			else
				tfprintf(stderr, 2, "Failed to read media\n");
		}
	}
}

static void
print_blockref_entry(struct blockref_tree *root)
{
	struct blockref_entry *e;

	RB_FOREACH(e, blockref_tree, root)
		print_blockref_msg(&e->head);
}

static void
init_blockref_stats(blockref_stats_t *bstats, uint8_t type)
{
	memset(bstats, 0, sizeof(*bstats));
	RB_INIT(&bstats->root);
	bstats->type = type;
}

static void
cleanup_blockref_stats(blockref_stats_t *bstats)
{
	cleanup_blockref_entry(&bstats->root);
}

static void
init_delta_root(struct blockref_tree *droot)
{
	RB_INIT(droot);
}

static void
cleanup_delta_root(struct blockref_tree *droot)
{
	cleanup_blockref_entry(droot);
}

static void
print_blockref_stats(const blockref_stats_t *bstats, bool newline)
{
	size_t siz = charsperline();
	char *buf = calloc(1, siz);
	char emptybuf[128];

	assert(buf);

	if (CountEmpty)
		snprintf(emptybuf, sizeof(emptybuf), ", %ju empty",
		    (uintmax_t)bstats->total_empty);
	else
		strlcpy(emptybuf, "", sizeof(emptybuf));

	switch (bstats->type) {
	case HAMMER2_BREF_TYPE_VOLUME:
		tsnprintf(buf, siz, 1, "%ju blockref (%ju inode, %ju indirect, "
		    "%ju data, %ju dirent%s), %s",
		    (uintmax_t)bstats->total_blockref,
		    (uintmax_t)bstats->volume.total_inode,
		    (uintmax_t)bstats->volume.total_indirect,
		    (uintmax_t)bstats->volume.total_data,
		    (uintmax_t)bstats->volume.total_dirent,
		    emptybuf,
		    sizetostr(bstats->total_bytes));
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		tsnprintf(buf, siz, 1, "%ju blockref (%ju node, %ju leaf%s), "
		    "%s",
		    (uintmax_t)bstats->total_blockref,
		    (uintmax_t)bstats->freemap.total_freemap_node,
		    (uintmax_t)bstats->freemap.total_freemap_leaf,
		    emptybuf,
		    sizetostr(bstats->total_bytes));
		break;
	default:
		assert(0);
		break;
	}

	if (newline) {
		printf("%s\n", buf);
	} else {
		printf("%s\r", buf);
		fflush(stdout);
	}
	free(buf);
}

static int
verify_volume_header(const hammer2_volume_data_t *voldata)
{
	hammer2_crc32_t crc0, crc1;
	const char *p = (const char*)voldata;

	if ((voldata->magic != HAMMER2_VOLUME_ID_HBO) &&
	    (voldata->magic != HAMMER2_VOLUME_ID_ABO)) {
		tfprintf(stderr, 1, "Bad magic %jX\n", voldata->magic);
		return -1;
	}

	if (voldata->magic == HAMMER2_VOLUME_ID_ABO)
		tfprintf(stderr, 1, "Reverse endian\n");

	crc0 = voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT0];
	crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRC0_OFF,
	    HAMMER2_VOLUME_ICRC0_SIZE);
	if (crc0 != crc1) {
		tfprintf(stderr, 1, "Bad HAMMER2_VOL_ICRC_SECT0 CRC\n");
		return -1;
	}

	crc0 = voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT1];
	crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRC1_OFF,
	    HAMMER2_VOLUME_ICRC1_SIZE);
	if (crc0 != crc1) {
		tfprintf(stderr, 1, "Bad HAMMER2_VOL_ICRC_SECT1 CRC\n");
		return -1;
	}

	crc0 = voldata->icrc_volheader;
	crc1 = hammer2_icrc32(p + HAMMER2_VOLUME_ICRCVH_OFF,
	    HAMMER2_VOLUME_ICRCVH_SIZE);
	if (crc0 != crc1) {
		tfprintf(stderr, 1, "Bad volume header CRC\n");
		return -1;
	}

	return 0;
}

static int
read_media(const hammer2_blockref_t *bref, hammer2_media_data_t *media,
    size_t *media_bytes)
{
	hammer2_off_t io_off, io_base;
	size_t bytes, io_bytes, boff;
	int fd;

	bytes = (bref->data_off & HAMMER2_OFF_MASK_RADIX);
	if (bytes)
		bytes = (size_t)1 << bytes;
	if (media_bytes)
		*media_bytes = bytes;

	if (!bytes)
		return 0;

	io_off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
	io_base = io_off & ~(hammer2_off_t)(HAMMER2_LBUFSIZE - 1);
	boff = io_off - io_base;

	io_bytes = HAMMER2_LBUFSIZE;
	while (io_bytes + boff < bytes)
		io_bytes <<= 1;

	if (io_bytes > sizeof(*media))
		return -1;
	fd = hammer2_get_volume_fd(io_off);
	if (lseek(fd, io_base - hammer2_get_volume_offset(io_base), SEEK_SET)
	    == -1)
		return -2;
	if (read(fd, media, io_bytes) != (ssize_t)io_bytes)
		return -2;
	if (boff)
		memmove(media, (char *)media + boff, bytes);

	return 0;
}

static void
load_delta_stats(blockref_stats_t *bstats, const delta_stats_t *dstats)
{
	bstats->total_blockref += dstats->total_blockref;
	bstats->total_empty += dstats->total_empty;
	bstats->total_bytes += dstats->total_bytes;

	switch (bstats->type) {
	case HAMMER2_BREF_TYPE_VOLUME:
		bstats->volume.total_inode += dstats->volume.total_inode;
		bstats->volume.total_indirect += dstats->volume.total_indirect;
		bstats->volume.total_data += dstats->volume.total_data;
		bstats->volume.total_dirent += dstats->volume.total_dirent;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		bstats->freemap.total_freemap_node +=
		    dstats->freemap.total_freemap_node;
		bstats->freemap.total_freemap_leaf +=
		    dstats->freemap.total_freemap_leaf;
		break;
	default:
		assert(0);
		break;
	}
}

static void
accumulate_delta_stats(delta_stats_t *dst, const delta_stats_t *src)
{
	dst->total_blockref += src->total_blockref;
	dst->total_empty += src->total_empty;
	dst->total_bytes += src->total_bytes;

	dst->volume.total_inode += src->volume.total_inode;
	dst->volume.total_indirect += src->volume.total_indirect;
	dst->volume.total_data += src->volume.total_data;
	dst->volume.total_dirent += src->volume.total_dirent;

	dst->freemap.total_freemap_node += src->freemap.total_freemap_node;
	dst->freemap.total_freemap_leaf += src->freemap.total_freemap_leaf;

	dst->count += src->count;
}

static int
verify_blockref(const hammer2_volume_data_t *voldata,
    const hammer2_blockref_t *bref, bool norecurse, blockref_stats_t *bstats,
    struct blockref_tree *droot, delta_stats_t *dstats, int depth, int index)
{
	hammer2_media_data_t media;
	hammer2_blockref_t *bscan;
	int i, bcount;
	bool failed = false;
	size_t bytes;
	uint32_t cv;
	uint64_t cv64;
	char msg[256];
	SHA256_CTX hash_ctx;
	union {
		uint8_t digest[SHA256_DIGEST_LENGTH];
		uint64_t digest64[SHA256_DIGEST_LENGTH/8];
	} u;

	/* only for DebugOpt > 1 */
	if (DebugOpt > 1)
		print_blockref_debug(stdout, depth, index, bref, NULL);

	if (bref->data_off) {
		struct blockref_entry *e, bref_find;
		memset(&bref_find, 0, sizeof(bref_find));
		bref_find.data_off = bref->data_off;
		e = RB_FIND(blockref_tree, droot, &bref_find);
		if (e) {
			struct blockref_msg *m;
			TAILQ_FOREACH(m, &e->head, entry) {
				delta_stats_t *ds = m->msg;
				if (!memcmp(&m->bref, bref, sizeof(*bref))) {
					/* delta contains cached delta */
					accumulate_delta_stats(dstats, ds);
					load_delta_stats(bstats, ds);
					print_blockref_debug(stdout, depth,
					    index, &m->bref, "cache-hit");
					return 0;
				}
			}
		}
	}

	bstats->total_blockref++;
	dstats->total_blockref++;

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_EMPTY:
		if (CountEmpty) {
			bstats->total_empty++;
			dstats->total_empty++;
		} else {
			bstats->total_blockref--;
			dstats->total_blockref--;
		}
		break;
	case HAMMER2_BREF_TYPE_INODE:
		bstats->volume.total_inode++;
		dstats->volume.total_inode++;
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bstats->volume.total_indirect++;
		dstats->volume.total_indirect++;
		break;
	case HAMMER2_BREF_TYPE_DATA:
		bstats->volume.total_data++;
		dstats->volume.total_data++;
		break;
	case HAMMER2_BREF_TYPE_DIRENT:
		bstats->volume.total_dirent++;
		dstats->volume.total_dirent++;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		bstats->freemap.total_freemap_node++;
		dstats->freemap.total_freemap_node++;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		bstats->freemap.total_freemap_leaf++;
		dstats->freemap.total_freemap_leaf++;
		break;
	case HAMMER2_BREF_TYPE_VOLUME:
		bstats->total_blockref--;
		dstats->total_blockref--;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		bstats->total_blockref--;
		dstats->total_blockref--;
		break;
	default:
		snprintf(msg, sizeof(msg), "Invalid blockref type %d",
		    bref->type);
		add_blockref_entry(&bstats->root, bref, msg, strlen(msg) + 1);
		print_blockref_debug(stdout, depth, index, bref, msg);
		failed = true;
		break;
	}

	switch (read_media(bref, &media, &bytes)) {
	case -1:
		strlcpy(msg, "Bad I/O bytes", sizeof(msg));
		add_blockref_entry(&bstats->root, bref, msg, strlen(msg) + 1);
		print_blockref_debug(stdout, depth, index, bref, msg);
		return -1;
	case -2:
		strlcpy(msg, "Failed to read media", sizeof(msg));
		add_blockref_entry(&bstats->root, bref, msg, strlen(msg) + 1);
		print_blockref_debug(stdout, depth, index, bref, msg);
		return -1;
	default:
		break;
	}

	if (bref->type != HAMMER2_BREF_TYPE_VOLUME &&
	    bref->type != HAMMER2_BREF_TYPE_FREEMAP) {
		bstats->total_bytes += bytes;
		dstats->total_bytes += bytes;
	}

	if (!CountEmpty && bref->type == HAMMER2_BREF_TYPE_EMPTY) {
		assert(bytes == 0);
		bstats->total_bytes -= bytes;
		dstats->total_bytes -= bytes;
	}

	if (!DebugOpt && QuietOpt <= 0 && (bstats->total_blockref % 100) == 0)
		print_blockref_stats(bstats, false);

	if (!bytes)
		goto end;

	switch (HAMMER2_DEC_CHECK(bref->methods)) {
	case HAMMER2_CHECK_ISCSI32:
		cv = hammer2_icrc32(&media, bytes);
		if (bref->check.iscsi32.value != cv) {
			strlcpy(msg, "Bad HAMMER2_CHECK_ISCSI32", sizeof(msg));
			add_blockref_entry(&bstats->root, bref, msg,
			    strlen(msg) + 1);
			print_blockref_debug(stdout, depth, index, bref, msg);
			failed = true;
		}
		break;
	case HAMMER2_CHECK_XXHASH64:
		cv64 = XXH64(&media, bytes, XXH_HAMMER2_SEED);
		if (bref->check.xxhash64.value != cv64) {
			strlcpy(msg, "Bad HAMMER2_CHECK_XXHASH64", sizeof(msg));
			add_blockref_entry(&bstats->root, bref, msg,
			    strlen(msg) + 1);
			print_blockref_debug(stdout, depth, index, bref, msg);
			failed = true;
		}
		break;
	case HAMMER2_CHECK_SHA192:
		SHA256_Init(&hash_ctx);
		SHA256_Update(&hash_ctx, &media, bytes);
		SHA256_Final(u.digest, &hash_ctx);
		u.digest64[2] ^= u.digest64[3];
		if (memcmp(u.digest, bref->check.sha192.data,
		    sizeof(bref->check.sha192.data))) {
			strlcpy(msg, "Bad HAMMER2_CHECK_SHA192", sizeof(msg));
			add_blockref_entry(&bstats->root, bref, msg,
			    strlen(msg) + 1);
			print_blockref_debug(stdout, depth, index, bref, msg);
			failed = true;
		}
		break;
	case HAMMER2_CHECK_FREEMAP:
		cv = hammer2_icrc32(&media, bytes);
		if (bref->check.freemap.icrc32 != cv) {
			strlcpy(msg, "Bad HAMMER2_CHECK_FREEMAP", sizeof(msg));
			add_blockref_entry(&bstats->root, bref, msg,
			    strlen(msg) + 1);
			print_blockref_debug(stdout, depth, index, bref, msg);
			failed = true;
		}
		break;
	}

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		if (!(media.ipdata.meta.op_flags & HAMMER2_OPFLAG_DIRECTDATA)) {
			bscan = &media.ipdata.u.blockset.blockref[0];
			bcount = HAMMER2_SET_COUNT;
		} else {
			bscan = NULL;
			bcount = 0;
		}
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bscan = &media.npdata[0];
		bcount = bytes / sizeof(hammer2_blockref_t);
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		bscan = &media.npdata[0];
		bcount = bytes / sizeof(hammer2_blockref_t);
		break;
	case HAMMER2_BREF_TYPE_VOLUME:
		bscan = &media.voldata.sroot_blockset.blockref[0];
		bcount = HAMMER2_SET_COUNT;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		bscan = &media.voldata.freemap_blockset.blockref[0];
		bcount = HAMMER2_SET_COUNT;
		break;
	default:
		bscan = NULL;
		bcount = 0;
		break;
	}

	if (ForceOpt)
		norecurse = false;
	/*
	 * If failed, no recurse, but still verify its direct children.
	 * Beyond that is probably garbage.
	 */
	for (i = 0; norecurse == false && i < bcount; ++i) {
		delta_stats_t ds;
		memset(&ds, 0, sizeof(ds));
		if (verify_blockref(voldata, &bscan[i], failed, bstats, droot,
		    &ds, depth + 1, i) == -1)
			return -1;
		if (!failed)
			accumulate_delta_stats(dstats, &ds);
	}
end:
	if (failed)
		return -1;

	dstats->count++;
	if (bref->data_off && BlockrefCacheCount > 0 &&
	    dstats->count >= BlockrefCacheCount) {
		assert(bytes);
		add_blockref_entry(droot, bref, dstats, sizeof(*dstats));
		print_blockref_debug(stdout, depth, index, bref, "cache-add");
	}

	return 0;
}

static void
print_pfs(const hammer2_inode_data_t *ipdata)
{
	const hammer2_inode_meta_t *meta = &ipdata->meta;
	char *f, *pfs_id_str = NULL;
	const char *type_str;
	uuid_t uuid;

	f = get_inode_filename(ipdata);
	uuid = meta->pfs_clid;
	hammer2_uuid_to_str(&uuid, &pfs_id_str);
	if (meta->pfs_type == HAMMER2_PFSTYPE_MASTER) {
		if (meta->pfs_subtype == HAMMER2_PFSSUBTYPE_NONE)
			type_str = "MASTER";
		else
			type_str = hammer2_pfssubtype_to_str(meta->pfs_subtype);
	} else {
		type_str = hammer2_pfstype_to_str(meta->pfs_type);
	}
	tfprintf(stdout, 1, "%-11s %s %s\n", type_str, pfs_id_str, f);

	free(f);
	free(pfs_id_str);
}

static char*
get_inode_filename(const hammer2_inode_data_t *ipdata)
{
	char *p = malloc(HAMMER2_INODE_MAXNAME + 1);

	memcpy(p, ipdata->filename, sizeof(ipdata->filename));
	p[HAMMER2_INODE_MAXNAME] = '\0';

	return p;
}

static void
__add_pfs_blockref(const hammer2_blockref_t *bref, struct blockref_list *blist,
    const hammer2_inode_data_t *ipdata)
{
	struct blockref_msg *newp, *p;

	newp = calloc(1, sizeof(*newp));
	newp->bref = *bref;
	newp->msg = calloc(1, sizeof(*ipdata));
	memcpy(newp->msg, ipdata, sizeof(*ipdata));

	p = TAILQ_FIRST(blist);
	while (p) {
		char *f1 = get_inode_filename(newp->msg);
		char *f2 = get_inode_filename(p->msg);
		if (strcmp(f1, f2) <= 0) {
			TAILQ_INSERT_BEFORE(p, newp, entry);
			free(f1);
			free(f2);
			break;
		}
		p = TAILQ_NEXT(p, entry);
		free(f1);
		free(f2);
	}
	if (!p)
		TAILQ_INSERT_TAIL(blist, newp, entry);
}

static int
init_pfs_blockref(const hammer2_volume_data_t *voldata,
    const hammer2_blockref_t *bref, struct blockref_list *blist)
{
	hammer2_media_data_t media;
	hammer2_inode_data_t ipdata;
	hammer2_blockref_t *bscan;
	int i, bcount;
	size_t bytes;

	if (read_media(bref, &media, &bytes))
		return -1;
	if (!bytes)
		return 0;

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		ipdata = media.ipdata;
		if (ipdata.meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			bscan = &ipdata.u.blockset.blockref[0];
			bcount = HAMMER2_SET_COUNT;
		} else {
			bscan = NULL;
			bcount = 0;
			if (ipdata.meta.op_flags & HAMMER2_OPFLAG_PFSROOT)
				__add_pfs_blockref(bref, blist, &ipdata);
			else
				assert(0); /* should only see SUPROOT or PFS */
		}
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bscan = &media.npdata[0];
		bcount = bytes / sizeof(hammer2_blockref_t);
		break;
	case HAMMER2_BREF_TYPE_VOLUME:
		bscan = &media.voldata.sroot_blockset.blockref[0];
		bcount = HAMMER2_SET_COUNT;
		break;
	default:
		bscan = NULL;
		bcount = 0;
		break;
	}

	for (i = 0; i < bcount; ++i)
		if (init_pfs_blockref(voldata, &bscan[i], blist) == -1)
			return -1;
	return 0;
}

static void
cleanup_pfs_blockref(struct blockref_list *blist)
{
	cleanup_blockref_msg(blist);
}

static void
print_media(FILE *fp, int tab, const hammer2_blockref_t *bref,
    const hammer2_media_data_t *media, size_t media_bytes)
{
	const hammer2_blockref_t *bscan;
	const hammer2_inode_data_t *ipdata;
	int i, bcount, namelen;
	char *str = NULL;
	uuid_t uuid;

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		ipdata = &media->ipdata;
		namelen = ipdata->meta.name_len;
		if (namelen > HAMMER2_INODE_MAXNAME)
			namelen = 0;
		tfprintf(fp, tab, "filename \"%*.*s\"\n", namelen, namelen,
		    ipdata->filename);
		tfprintf(fp, tab, "version %d\n", ipdata->meta.version);
		if ((ipdata->meta.op_flags & HAMMER2_OPFLAG_PFSROOT) ||
		    ipdata->meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT)
			tfprintf(fp, tab, "pfs_subtype %d (%s)\n",
			    ipdata->meta.pfs_subtype,
			    hammer2_pfssubtype_to_str(ipdata->meta.pfs_subtype));
		tfprintf(fp, tab, "uflags 0x%08x\n", ipdata->meta.uflags);
		if (ipdata->meta.rmajor || ipdata->meta.rminor) {
			tfprintf(fp, tab, "rmajor %d\n", ipdata->meta.rmajor);
			tfprintf(fp, tab, "rminor %d\n", ipdata->meta.rminor);
		}
		tfprintf(fp, tab, "ctime %s\n",
		    hammer2_time64_to_str(ipdata->meta.ctime, &str));
		tfprintf(fp, tab, "mtime %s\n",
		    hammer2_time64_to_str(ipdata->meta.mtime, &str));
		tfprintf(fp, tab, "atime %s\n",
		    hammer2_time64_to_str(ipdata->meta.atime, &str));
		tfprintf(fp, tab, "btime %s\n",
		    hammer2_time64_to_str(ipdata->meta.btime, &str));
		uuid = ipdata->meta.uid;
		tfprintf(fp, tab, "uid %s\n", hammer2_uuid_to_str(&uuid, &str));
		uuid = ipdata->meta.gid;
		tfprintf(fp, tab, "gid %s\n", hammer2_uuid_to_str(&uuid, &str));
		tfprintf(fp, tab, "type %s\n",
		    hammer2_iptype_to_str(ipdata->meta.type));
		tfprintf(fp, tab, "op_flags 0x%02x\n", ipdata->meta.op_flags);
		tfprintf(fp, tab, "cap_flags 0x%04x\n", ipdata->meta.cap_flags);
		tfprintf(fp, tab, "mode %-7o\n", ipdata->meta.mode);
		tfprintf(fp, tab, "inum 0x%016jx\n", ipdata->meta.inum);
		tfprintf(fp, tab, "size %ju ", (uintmax_t)ipdata->meta.size);
		if (ipdata->meta.op_flags & HAMMER2_OPFLAG_DIRECTDATA &&
		    ipdata->meta.size <= HAMMER2_EMBEDDED_BYTES)
			printf("(embedded data)\n");
		else
			printf("\n");
		tfprintf(fp, tab, "nlinks %ju\n",
		    (uintmax_t)ipdata->meta.nlinks);
		tfprintf(fp, tab, "iparent 0x%016jx\n",
		    (uintmax_t)ipdata->meta.iparent);
		tfprintf(fp, tab, "name_key 0x%016jx\n",
		    (uintmax_t)ipdata->meta.name_key);
		tfprintf(fp, tab, "name_len %u\n", ipdata->meta.name_len);
		tfprintf(fp, tab, "ncopies %u\n", ipdata->meta.ncopies);
		tfprintf(fp, tab, "comp_algo %u\n", ipdata->meta.comp_algo);
		tfprintf(fp, tab, "target_type %u\n", ipdata->meta.target_type);
		tfprintf(fp, tab, "check_algo %u\n", ipdata->meta.check_algo);
		if ((ipdata->meta.op_flags & HAMMER2_OPFLAG_PFSROOT) ||
		    ipdata->meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			tfprintf(fp, tab, "pfs_nmasters %u\n",
			    ipdata->meta.pfs_nmasters);
			tfprintf(fp, tab, "pfs_type %u (%s)\n",
			    ipdata->meta.pfs_type,
			    hammer2_pfstype_to_str(ipdata->meta.pfs_type));
			tfprintf(fp, tab, "pfs_inum 0x%016jx\n",
			    (uintmax_t)ipdata->meta.pfs_inum);
			uuid = ipdata->meta.pfs_clid;
			tfprintf(fp, tab, "pfs_clid %s\n",
			    hammer2_uuid_to_str(&uuid, &str));
			uuid = ipdata->meta.pfs_fsid;
			tfprintf(fp, tab, "pfs_fsid %s\n",
			    hammer2_uuid_to_str(&uuid, &str));
			tfprintf(fp, tab, "pfs_lsnap_tid 0x%016jx\n",
			    (uintmax_t)ipdata->meta.pfs_lsnap_tid);
		}
		tfprintf(fp, tab, "data_quota %ju\n",
		    (uintmax_t)ipdata->meta.data_quota);
		tfprintf(fp, tab, "data_count %ju\n",
		    (uintmax_t)bref->embed.stats.data_count);
		tfprintf(fp, tab, "inode_quota %ju\n",
		    (uintmax_t)ipdata->meta.inode_quota);
		tfprintf(fp, tab, "inode_count %ju\n",
		    (uintmax_t)bref->embed.stats.inode_count);
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bcount = media_bytes / sizeof(hammer2_blockref_t);
		for (i = 0; i < bcount; ++i) {
			bscan = &media->npdata[i];
			tfprintf(fp, tab, "%3d %016jx %-12s %016jx/%-2d\n",
			    i, (uintmax_t)bscan->data_off,
			    hammer2_breftype_to_str(bscan->type),
			    (uintmax_t)bscan->key,
			    bscan->keybits);
		}
		break;
	case HAMMER2_BREF_TYPE_DIRENT:
		if (bref->embed.dirent.namlen <= sizeof(bref->check.buf)) {
			tfprintf(fp, tab, "filename \"%*.*s\"\n",
			    bref->embed.dirent.namlen,
			    bref->embed.dirent.namlen,
			    bref->check.buf);
		} else {
			tfprintf(fp, tab, "filename \"%*.*s\"\n",
			    bref->embed.dirent.namlen,
			    bref->embed.dirent.namlen,
			    media->buf);
		}
		tfprintf(fp, tab, "inum 0x%016jx\n",
		    (uintmax_t)bref->embed.dirent.inum);
		tfprintf(fp, tab, "namlen %d\n",
		    (uintmax_t)bref->embed.dirent.namlen);
		tfprintf(fp, tab, "type %s\n",
		    hammer2_iptype_to_str(bref->embed.dirent.type));
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		bcount = media_bytes / sizeof(hammer2_blockref_t);
		for (i = 0; i < bcount; ++i) {
			bscan = &media->npdata[i];
			tfprintf(fp, tab, "%3d %016jx %-12s %016jx/%-2d\n",
			    i, (uintmax_t)bscan->data_off,
			    hammer2_breftype_to_str(bscan->type),
			    (uintmax_t)bscan->key,
			    bscan->keybits);
		}
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		for (i = 0; i < HAMMER2_FREEMAP_COUNT; ++i) {
			hammer2_off_t data_off = bref->key +
				i * HAMMER2_FREEMAP_LEVEL0_SIZE;
#if HAMMER2_BMAP_ELEMENTS != 8
#error "HAMMER2_BMAP_ELEMENTS != 8"
#endif
			tfprintf(fp, tab, "%016jx %04d.%04x (avail=%7d) "
			    "%016jx %016jx %016jx %016jx "
			    "%016jx %016jx %016jx %016jx\n",
			    data_off, i, media->bmdata[i].class,
			    media->bmdata[i].avail,
			    media->bmdata[i].bitmapq[0],
			    media->bmdata[i].bitmapq[1],
			    media->bmdata[i].bitmapq[2],
			    media->bmdata[i].bitmapq[3],
			    media->bmdata[i].bitmapq[4],
			    media->bmdata[i].bitmapq[5],
			    media->bmdata[i].bitmapq[6],
			    media->bmdata[i].bitmapq[7]);
		}
		break;
	default:
		break;
	}
	if (str)
		free(str);
}

int
test_hammer2(const char *devpath)
{
	bool failed = false;

	hammer2_init_volumes(devpath, 1);

	best_zone = find_best_zone();
	if (best_zone == -1)
		fprintf(stderr, "Failed to find best zone\n");

	if (PrintPFS) {
		if (test_pfs_blockref() == -1)
			failed = true;
		goto end; /* print PFS info and exit */
	}

	printf("volume header\n");
	if (test_volume_header() == -1) {
		failed = true;
		if (!ForceOpt)
			goto end;
	}

	printf("freemap\n");
	if (test_blockref(HAMMER2_BREF_TYPE_FREEMAP) == -1) {
		failed = true;
		if (!ForceOpt)
			goto end;
	}
	printf("volume\n");
	if (!ScanPFS) {
		if (test_blockref(HAMMER2_BREF_TYPE_VOLUME) == -1) {
			failed = true;
			if (!ForceOpt)
				goto end;
		}
	} else {
		if (test_pfs_blockref() == -1) {
			failed = true;
			if (!ForceOpt)
				goto end;
		}
	}
end:
	hammer2_cleanup_volumes();

	return failed ? -1 : 0;
}
