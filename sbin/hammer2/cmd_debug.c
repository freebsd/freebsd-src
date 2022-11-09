/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2012 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
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

#include "hammer2.h"

#include <crypto/sha2/sha256.h>

#define GIG	(1024LL*1024*1024)

static int show_all_volume_headers = 0;
static int show_tab = 2;
static int show_depth = -1;
static hammer2_tid_t show_min_mirror_tid = 0;
static hammer2_tid_t show_min_modify_tid = 0;

static void count_blocks(hammer2_bmap_data_t *bmap, int value,
		hammer2_off_t *accum16, hammer2_off_t *accum64);

/************************************************************************
 *				    SHOW				*
 ************************************************************************/

static void show_volhdr(hammer2_volume_data_t *voldata, int bi);
static void show_bref(hammer2_volume_data_t *voldata, int tab,
			int bi, hammer2_blockref_t *bref, int norecurse);
static void tabprintf(int tab, const char *ctl, ...);

static hammer2_off_t TotalAccum16[4]; /* includes TotalAccum64 */
static hammer2_off_t TotalAccum64[4];
static hammer2_off_t TotalUnavail;
static hammer2_off_t TotalFreemap;

static
hammer2_off_t
get_next_volume(hammer2_volume_data_t *voldata, hammer2_off_t volu_loff)
{
	hammer2_off_t ret = -1;
	int i;

	for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
		hammer2_off_t tmp = voldata->volu_loff[i];
		if (tmp > volu_loff) {
			ret = tmp;
			break;
		}
	}
	return ret;
}

int
cmd_show(const char *devpath, int which)
{
	hammer2_blockref_t broot;
	hammer2_blockref_t best;
	hammer2_media_data_t media;
	hammer2_media_data_t best_media;
	hammer2_off_t off, volu_loff, next_volu_loff = 0;
	int fd;
	int i;
	int best_i;
	char *env;

	memset(TotalAccum16, 0, sizeof(TotalAccum16));
	memset(TotalAccum64, 0, sizeof(TotalAccum64));
	TotalUnavail = TotalFreemap = 0;

	env = getenv("HAMMER2_SHOW_ALL_VOLUME_HEADERS");
	if (env != NULL) {
		show_all_volume_headers = (int)strtol(env, NULL, 0);
		if (errno)
			show_all_volume_headers = 0;
	}
	env = getenv("HAMMER2_SHOW_TAB");
	if (env != NULL) {
		show_tab = (int)strtol(env, NULL, 0);
		if (errno || show_tab < 0 || show_tab > 8)
			show_tab = 2;
	}
	env = getenv("HAMMER2_SHOW_DEPTH");
	if (env != NULL) {
		show_depth = (int)strtol(env, NULL, 0);
		if (errno || show_depth < 0)
			show_depth = -1;
	}
	env = getenv("HAMMER2_SHOW_MIN_MIRROR_TID");
	if (env != NULL) {
		show_min_mirror_tid = (hammer2_tid_t)strtoull(env, NULL, 16);
		if (errno)
			show_min_mirror_tid = 0;
	}
	env = getenv("HAMMER2_SHOW_MIN_MODIFY_TID");
	if (env != NULL) {
		show_min_modify_tid = (hammer2_tid_t)strtoull(env, NULL, 16);
		if (errno)
			show_min_modify_tid = 0;
	}

	hammer2_init_volumes(devpath, 1);
	int all_volume_headers = VerboseOpt >= 3 || show_all_volume_headers;
next_volume:
	volu_loff = next_volu_loff;
	next_volu_loff = -1;
	printf("%s\n", hammer2_get_volume_path(volu_loff));
	/*
	 * Show the tree using the best volume header.
	 * -vvv will show the tree for all four volume headers.
	 */
	best_i = -1;
	bzero(&best, sizeof(best));
	bzero(&best_media, sizeof(best_media));
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; ++i) {
		bzero(&broot, sizeof(broot));
		broot.data_off = (i * HAMMER2_ZONE_BYTES64) | HAMMER2_PBUFRADIX;
		off = broot.data_off & ~HAMMER2_OFF_MASK_RADIX;
		fd = hammer2_get_volume_fd(volu_loff);
		lseek(fd, off, SEEK_SET);
		if (read(fd, &media, HAMMER2_PBUFSIZE) ==
		    (ssize_t)HAMMER2_PBUFSIZE) {
			broot.mirror_tid = media.voldata.mirror_tid;
			if (best_i < 0 || best.mirror_tid < broot.mirror_tid) {
				best_i = i;
				best = broot;
				best_media = media;
			}
			printf("Volume header %d: mirror_tid=%016jx\n",
			       i, (intmax_t)broot.mirror_tid);

			if (all_volume_headers) {
				switch(which) {
				case 0:
					broot.type = HAMMER2_BREF_TYPE_VOLUME;
					show_bref(&media.voldata, 0, i, &broot,
						  0);
					break;
				case 1:
					broot.type = HAMMER2_BREF_TYPE_FREEMAP;
					show_bref(&media.voldata, 0, i, &broot,
						  0);
					break;
				default:
					show_volhdr(&media.voldata, i);
					if (i == 0)
						next_volu_loff = get_next_volume(&media.voldata, volu_loff);
					break;
				}
				if (i != HAMMER2_NUM_VOLHDRS - 1)
					printf("\n");
			}
		}
	}
	if (next_volu_loff != (hammer2_off_t)-1) {
		printf("---------------------------------------------\n");
		goto next_volume;
	}

	if (!all_volume_headers) {
		switch(which) {
		case 0:
			best.type = HAMMER2_BREF_TYPE_VOLUME;
			show_bref(&best_media.voldata, 0, best_i, &best, 0);
			break;
		case 1:
			best.type = HAMMER2_BREF_TYPE_FREEMAP;
			show_bref(&best_media.voldata, 0, best_i, &best, 0);
			break;
		default:
			show_volhdr(&best_media.voldata, best_i);
			next_volu_loff = get_next_volume(&best_media.voldata, volu_loff);
			if (next_volu_loff != (hammer2_off_t)-1) {
				printf("---------------------------------------------\n");
				goto next_volume;
			}
			break;
		}
	}

	if (which == 1 && VerboseOpt < 3) {
		printf("Total unallocated storage:   %6.3fGiB (%6.3fGiB in 64KB chunks)\n",
		       (double)TotalAccum16[0] / GIG,
		       (double)TotalAccum64[0] / GIG);
		printf("Total possibly free storage: %6.3fGiB (%6.3fGiB in 64KB chunks)\n",
		       (double)TotalAccum16[2] / GIG,
		       (double)TotalAccum64[2] / GIG);
		printf("Total allocated storage:     %6.3fGiB (%6.3fGiB in 64KB chunks)\n",
		       (double)TotalAccum16[3] / GIG,
		       (double)TotalAccum64[3] / GIG);
		printf("Total unavailable storage:   %6.3fGiB\n",
		       (double)TotalUnavail / GIG);
		printf("Total freemap storage:       %6.3fGiB\n",
		       (double)TotalFreemap / GIG);
	}
	hammer2_cleanup_volumes();

	return 0;
}

static void
show_volhdr(hammer2_volume_data_t *voldata, int bi)
{
	uint32_t i;
	char *str;
	const char *name;
	char *buf;
	uuid_t uuid;

	printf("\nVolume header %d {\n", bi);
	printf("    magic          0x%016jx\n", (intmax_t)voldata->magic);
	printf("    boot_beg       0x%016jx\n", (intmax_t)voldata->boot_beg);
	printf("    boot_end       0x%016jx (%6.2fMB)\n",
	       (intmax_t)voldata->boot_end,
	       (double)(voldata->boot_end - voldata->boot_beg) /
	       (1024.0*1024.0));
	printf("    aux_beg        0x%016jx\n", (intmax_t)voldata->aux_beg);
	printf("    aux_end        0x%016jx (%6.2fMB)\n",
	       (intmax_t)voldata->aux_end,
	       (double)(voldata->aux_end - voldata->aux_beg) /
	       (1024.0*1024.0));
	printf("    volu_size      0x%016jx (%6.2fGiB)\n",
	       (intmax_t)voldata->volu_size,
	       (double)voldata->volu_size / GIG);
	printf("    version        %d\n", voldata->version);
	printf("    flags          0x%08x\n", voldata->flags);
	printf("    copyid         %d\n", voldata->copyid);
	printf("    freemap_vers   %d\n", voldata->freemap_version);
	printf("    peer_type      %d\n", voldata->peer_type);
	printf("    volu_id        %d\n", voldata->volu_id);
	printf("    nvolumes       %d\n", voldata->nvolumes);

	str = NULL;
	uuid = voldata->fsid;
	hammer2_uuid_to_str(&uuid, &str);
	printf("    fsid           %s\n", str);
	free(str);

	str = NULL;
	uuid = voldata->fstype;
	hammer2_uuid_to_str(&uuid, &str);
	printf("    fstype         %s\n", str);
	if (!strcmp(str, "5cbb9ad1-862d-11dc-a94d-01301bb8a9f5"))
		name = "DragonFly HAMMER2";
	else
		name = "?";
	printf("                   (%s)\n", name);
	free(str);

	printf("    allocator_size 0x%016jx (%6.2fGiB)\n",
	       voldata->allocator_size,
	       (double)voldata->allocator_size / GIG);
	printf("    allocator_free 0x%016jx (%6.2fGiB)\n",
	       voldata->allocator_free,
	       (double)voldata->allocator_free / GIG);
	printf("    allocator_beg  0x%016jx (%6.2fGiB)\n",
	       voldata->allocator_beg,
	       (double)voldata->allocator_beg / GIG);

	printf("    mirror_tid     0x%016jx\n", voldata->mirror_tid);
	printf("    reserved0080   0x%016jx\n", voldata->reserved0080);
	printf("    reserved0088   0x%016jx\n", voldata->reserved0088);
	printf("    freemap_tid    0x%016jx\n", voldata->freemap_tid);
	printf("    bulkfree_tid   0x%016jx\n", voldata->bulkfree_tid);
	for (i = 0; i < nitems(voldata->reserved00A0); ++i) {
		printf("    reserved00A0/%u 0x%016jx\n",
		       i, voldata->reserved00A0[0]);
	}
	printf("    total_size     0x%016jx\n", voldata->total_size);

	printf("    copyexists    ");
	for (i = 0; i < nitems(voldata->copyexists); ++i)
		printf(" 0x%02x", voldata->copyexists[i]);
	printf("\n");

	/*
	 * NOTE: Index numbers and ICRC_SECTn definitions are not matched,
	 *	 the ICRC for sector 0 actually uses the last index, for
	 *	 example.
	 *
	 * NOTE: The whole voldata CRC does not have to match critically
	 *	 as certain sub-areas of the volume header have their own
	 *	 CRCs.
	 */
	printf("\n");
	for (i = 0; i < nitems(voldata->icrc_sects); ++i) {
		printf("    icrc_sects[%u]  ", i);
		switch(i) {
		case HAMMER2_VOL_ICRC_SECT0:
			printf("0x%08x/0x%08x",
			       hammer2_icrc32((char *)voldata +
					      HAMMER2_VOLUME_ICRC0_OFF,
					      HAMMER2_VOLUME_ICRC0_SIZE),
			       voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT0]);
			if (hammer2_icrc32((char *)voldata +
					   HAMMER2_VOLUME_ICRC0_OFF,
					   HAMMER2_VOLUME_ICRC0_SIZE) ==
			       voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT0]) {
				printf(" (OK)");
			} else {
				printf(" (FAILED)");
			}
			break;
		case HAMMER2_VOL_ICRC_SECT1:
			printf("0x%08x/0x%08x",
			       hammer2_icrc32((char *)voldata +
					      HAMMER2_VOLUME_ICRC1_OFF,
					      HAMMER2_VOLUME_ICRC1_SIZE),
			       voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT1]);
			if (hammer2_icrc32((char *)voldata +
					   HAMMER2_VOLUME_ICRC1_OFF,
					   HAMMER2_VOLUME_ICRC1_SIZE) ==
			       voldata->icrc_sects[HAMMER2_VOL_ICRC_SECT1]) {
				printf(" (OK)");
			} else {
				printf(" (FAILED)");
			}

			break;
		default:
			printf("0x%08x (reserved)", voldata->icrc_sects[i]);
			break;
		}
		printf("\n");
	}
	printf("    icrc_volhdr    0x%08x/0x%08x",
	       hammer2_icrc32((char *)voldata + HAMMER2_VOLUME_ICRCVH_OFF,
			      HAMMER2_VOLUME_ICRCVH_SIZE),
	       voldata->icrc_volheader);
	if (hammer2_icrc32((char *)voldata + HAMMER2_VOLUME_ICRCVH_OFF,
			   HAMMER2_VOLUME_ICRCVH_SIZE) ==
	    voldata->icrc_volheader) {
		printf(" (OK)\n");
	} else {
		printf(" (FAILED - not a critical error)\n");
	}

	/*
	 * The super-root and freemap blocksets (not recursed)
	 */
	printf("\n");
	printf("    sroot_blockset {\n");
	for (i = 0; i < HAMMER2_SET_COUNT; ++i) {
		show_bref(voldata, 16, i,
			  &voldata->sroot_blockset.blockref[i], 2);
	}
	printf("    }\n");

	printf("    freemap_blockset {\n");
	for (i = 0; i < HAMMER2_SET_COUNT; ++i) {
		show_bref(voldata, 16, i,
			  &voldata->freemap_blockset.blockref[i], 2);
	}
	printf("    }\n");

	buf = calloc(1, sizeof(voldata->volu_loff));
	if (bcmp(buf, voldata->volu_loff, sizeof(voldata->volu_loff))) {
		printf("\n");
		for (i = 0; i < HAMMER2_MAX_VOLUMES; ++i) {
			hammer2_off_t loff = voldata->volu_loff[i];
			if (loff != (hammer2_off_t)-1)
				printf("    volu_loff[%d]   0x%016jx\n", i, loff);
		}
	}
	free(buf);

	printf("}\n");
}

static void
show_bref(hammer2_volume_data_t *voldata, int tab, int bi,
	  hammer2_blockref_t *bref, int norecurse)
{
	hammer2_media_data_t media;
	hammer2_blockref_t *bscan;
	hammer2_off_t tmp;
	int i, bcount, namelen, failed, obrace, fd;
	int type_pad;
	size_t bytes;
	const char *type_str;
	char *str = NULL;
	uint32_t cv;
	uint64_t cv64;
	static int init_tab = -1;
	uuid_t uuid;

	SHA256_CTX hash_ctx;
	union {
		uint8_t digest[SHA256_DIGEST_LENGTH];
		uint64_t digest64[SHA256_DIGEST_LENGTH/8];
	} u;

	/* omit if smaller than mininum mirror_tid threshold */
	if (bref->mirror_tid < show_min_mirror_tid)
		return;
	/* omit if smaller than mininum modify_tid threshold */
	if (bref->modify_tid < show_min_modify_tid) {
		if (bref->modify_tid)
			return;
		else if (bref->type == HAMMER2_BREF_TYPE_INODE && !bref->leaf_count)
			return;
	}

	if (init_tab == -1)
		init_tab = tab;

	bytes = (bref->data_off & HAMMER2_OFF_MASK_RADIX);
	if (bytes)
		bytes = (size_t)1 << bytes;
	if (bytes) {
		hammer2_off_t io_off;
		hammer2_off_t io_base;
		size_t io_bytes;
		size_t boff;

		io_off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
		io_base = io_off & ~(hammer2_off_t)(HAMMER2_LBUFSIZE - 1);
		boff = io_off - io_base;

		io_bytes = HAMMER2_LBUFSIZE;
		while (io_bytes + boff < bytes)
			io_bytes <<= 1;

		if (io_bytes > sizeof(media)) {
			printf("(bad block size %zu)\n", bytes);
			return;
		}
		if (bref->type != HAMMER2_BREF_TYPE_DATA || VerboseOpt >= 1) {
			fd = hammer2_get_volume_fd(io_off);
			lseek(fd, io_base - hammer2_get_volume_offset(io_base),
			      SEEK_SET);
			if (read(fd, &media, io_bytes) != (ssize_t)io_bytes) {
				printf("(media read failed)\n");
				return;
			}
			if (boff)
				bcopy((char *)&media + boff, &media, bytes);
		}
	}

	bscan = NULL;
	bcount = 0;
	namelen = 0;
	failed = 0;
	obrace = 1;

	type_str = hammer2_breftype_to_str(bref->type);
	type_pad = 8 - strlen(type_str);
	if (type_pad < 0)
		type_pad = 0;

	switch(bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		assert(bytes);
		if (!(media.ipdata.meta.op_flags & HAMMER2_OPFLAG_DIRECTDATA)) {
			bscan = &media.ipdata.u.blockset.blockref[0];
			bcount = HAMMER2_SET_COUNT;
		}
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		assert(bytes);
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
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		assert(bytes);
		bscan = &media.npdata[0];
		bcount = bytes / sizeof(hammer2_blockref_t);
		break;
	}

	if (QuietOpt > 0) {
		tabprintf(tab,
			  "%s.%-3d %016jx %016jx/%-2d "
			  "vol=%d mir=%016jx mod=%016jx leafcnt=%d ",
			  type_str, bi, (intmax_t)bref->data_off,
			  (intmax_t)bref->key, (intmax_t)bref->keybits,
			  hammer2_get_volume_id(bref->data_off),
			  (intmax_t)bref->mirror_tid,
			  (intmax_t)bref->modify_tid,
			  bref->leaf_count);
	} else {
		tabprintf(tab, "%s.%-3d%*.*s %016jx %016jx/%-2d ",
			  type_str, bi, type_pad, type_pad, "",
			  (intmax_t)bref->data_off,
			  (intmax_t)bref->key, (intmax_t)bref->keybits);
		/*if (norecurse > 1)*/ {
			printf("\n");
			tabprintf(tab + 13, "");
		}
		printf("vol=%d mir=%016jx mod=%016jx lfcnt=%d ",
		       hammer2_get_volume_id(bref->data_off),
		       (intmax_t)bref->mirror_tid, (intmax_t)bref->modify_tid,
		       bref->leaf_count);
		if (/*norecurse > 1 && */ (bcount || bref->flags ||
		    bref->type == HAMMER2_BREF_TYPE_FREEMAP_NODE ||
		    bref->type == HAMMER2_BREF_TYPE_FREEMAP_LEAF)) {
			printf("\n");
			tabprintf(tab + 13, "");
		}
	}

	if (bcount)
		printf("bcnt=%d ", bcount);
	if (bref->flags)
		printf("flags=%02x ", bref->flags);
	if (bref->type == HAMMER2_BREF_TYPE_FREEMAP_NODE ||
	    bref->type == HAMMER2_BREF_TYPE_FREEMAP_LEAF) {
		printf("bigmask=%08x avail=%ju ",
			bref->check.freemap.bigmask,
			(uintmax_t)bref->check.freemap.avail);
	}

	/*
	 * Check data integrity in verbose mode, otherwise we are just doing
	 * a quick meta-data scan.  Meta-data integrity is always checked.
	 * (Also see the check above that ensures the media data is loaded,
	 * otherwise there's no data to check!).
	 *
	 * WARNING! bref->check state may be used for other things when
	 *	    bref has no data (bytes == 0).
	 */
	if (bytes &&
	    (bref->type != HAMMER2_BREF_TYPE_DATA || VerboseOpt >= 1)) {
		if (!(QuietOpt > 0)) {
			/*if (norecurse > 1)*/ {
				printf("\n");
				tabprintf(tab + 13, "");
			}
		}

		switch(HAMMER2_DEC_CHECK(bref->methods)) {
		case HAMMER2_CHECK_NONE:
			printf("meth=%02x ", bref->methods);
			break;
		case HAMMER2_CHECK_DISABLED:
			printf("meth=%02x ", bref->methods);
			break;
		case HAMMER2_CHECK_ISCSI32:
			cv = hammer2_icrc32(&media, bytes);
			if (bref->check.iscsi32.value != cv) {
				printf("(icrc %02x:%08x/%08x failed) ",
				       bref->methods,
				       bref->check.iscsi32.value,
				       cv);
				failed = 1;
			} else {
				printf("meth=%02x iscsi32=%08x ",
				       bref->methods, cv);
			}
			break;
		case HAMMER2_CHECK_XXHASH64:
			cv64 = XXH64(&media, bytes, XXH_HAMMER2_SEED);
			if (bref->check.xxhash64.value != cv64) {
				printf("(xxhash64 %02x:%016jx/%016jx failed) ",
				       bref->methods,
				       bref->check.xxhash64.value,
				       cv64);
				failed = 1;
			} else {
				printf("meth=%02x xxh=%016jx ",
				       bref->methods, cv64);
			}
			break;
		case HAMMER2_CHECK_SHA192:
			SHA256_Init(&hash_ctx);
			SHA256_Update(&hash_ctx, &media, bytes);
			SHA256_Final(u.digest, &hash_ctx);
			u.digest64[2] ^= u.digest64[3];
			if (memcmp(u.digest, bref->check.sha192.data,
			    sizeof(bref->check.sha192.data))) {
				printf("(sha192 failed) ");
				failed = 1;
			} else {
				printf("meth=%02x ", bref->methods);
			}
			break;
		case HAMMER2_CHECK_FREEMAP:
			cv = hammer2_icrc32(&media, bytes);
			if (bref->check.freemap.icrc32 != cv) {
				printf("(fcrc %02x:%08x/%08x failed) ",
					bref->methods,
					bref->check.freemap.icrc32,
					cv);
				failed = 1;
			} else {
				printf("meth=%02x fcrc=%08x ",
					bref->methods, cv);
			}
			break;
		}
	}

	tab += show_tab;

	if (QuietOpt > 0) {
		obrace = 0;
		printf("\n");
		goto skip_data;
	}

	switch(bref->type) {
	case HAMMER2_BREF_TYPE_EMPTY:
		if (norecurse)
			printf("\n");
		obrace = 0;
		break;
	case HAMMER2_BREF_TYPE_DIRENT:
		printf("{\n");
		if (bref->embed.dirent.namlen <= sizeof(bref->check.buf)) {
			tabprintf(tab, "filename \"%*.*s\"\n",
				bref->embed.dirent.namlen,
				bref->embed.dirent.namlen,
				bref->check.buf);
		} else {
			tabprintf(tab, "filename \"%*.*s\"\n",
				bref->embed.dirent.namlen,
				bref->embed.dirent.namlen,
				media.buf);
		}
		tabprintf(tab, "inum 0x%016jx\n",
			  (uintmax_t)bref->embed.dirent.inum);
		tabprintf(tab, "nlen %d\n", bref->embed.dirent.namlen);
		tabprintf(tab, "type %s\n",
			  hammer2_iptype_to_str(bref->embed.dirent.type));
		break;
	case HAMMER2_BREF_TYPE_INODE:
		printf("{\n");
		namelen = media.ipdata.meta.name_len;
		if (namelen > HAMMER2_INODE_MAXNAME)
			namelen = 0;
		tabprintf(tab, "filename \"%*.*s\"\n",
			  namelen, namelen, media.ipdata.filename);
		tabprintf(tab, "version  %d\n", media.ipdata.meta.version);
		if ((media.ipdata.meta.op_flags & HAMMER2_OPFLAG_PFSROOT) ||
		    media.ipdata.meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			tabprintf(tab, "pfs_st   %d (%s)\n",
				  media.ipdata.meta.pfs_subtype,
				  hammer2_pfssubtype_to_str(media.ipdata.meta.pfs_subtype));
		}
		tabprintf(tab, "uflags   0x%08x\n",
			  media.ipdata.meta.uflags);
		if (media.ipdata.meta.rmajor || media.ipdata.meta.rminor) {
			tabprintf(tab, "rmajor   %d\n",
				  media.ipdata.meta.rmajor);
			tabprintf(tab, "rminor   %d\n",
				  media.ipdata.meta.rminor);
		}
		tabprintf(tab, "ctime    %s\n",
			  hammer2_time64_to_str(media.ipdata.meta.ctime, &str));
		tabprintf(tab, "mtime    %s\n",
			  hammer2_time64_to_str(media.ipdata.meta.mtime, &str));
		tabprintf(tab, "atime    %s\n",
			  hammer2_time64_to_str(media.ipdata.meta.atime, &str));
		tabprintf(tab, "btime    %s\n",
			  hammer2_time64_to_str(media.ipdata.meta.btime, &str));
		uuid = media.ipdata.meta.uid;
		tabprintf(tab, "uid      %s\n",
			  hammer2_uuid_to_str(&uuid, &str));
		uuid = media.ipdata.meta.gid;
		tabprintf(tab, "gid      %s\n",
			  hammer2_uuid_to_str(&uuid, &str));
		tabprintf(tab, "type     %s\n",
			  hammer2_iptype_to_str(media.ipdata.meta.type));
		tabprintf(tab, "opflgs   0x%02x\n",
			  media.ipdata.meta.op_flags);
		tabprintf(tab, "capflgs  0x%04x\n",
			  media.ipdata.meta.cap_flags);
		tabprintf(tab, "mode     %-7o\n",
			  media.ipdata.meta.mode);
		tabprintf(tab, "inum     0x%016jx\n",
			  media.ipdata.meta.inum);
		tabprintf(tab, "size     %ju ",
			  (uintmax_t)media.ipdata.meta.size);
		if (media.ipdata.meta.op_flags & HAMMER2_OPFLAG_DIRECTDATA &&
		    media.ipdata.meta.size <= HAMMER2_EMBEDDED_BYTES)
			printf("(embedded data)\n");
		else
			printf("\n");
		tabprintf(tab, "nlinks   %ju\n",
			  (uintmax_t)media.ipdata.meta.nlinks);
		tabprintf(tab, "iparent  0x%016jx\n",
			  (uintmax_t)media.ipdata.meta.iparent);
		tabprintf(tab, "name_key 0x%016jx\n",
			  (uintmax_t)media.ipdata.meta.name_key);
		tabprintf(tab, "name_len %u\n",
			  media.ipdata.meta.name_len);
		tabprintf(tab, "ncopies  %u\n",
			  media.ipdata.meta.ncopies);
		tabprintf(tab, "compalg  %u\n",
			  media.ipdata.meta.comp_algo);
		tabprintf(tab, "target_t %u\n",
			  media.ipdata.meta.target_type);
		tabprintf(tab, "checkalg %u\n",
			  media.ipdata.meta.check_algo);
		if ((media.ipdata.meta.op_flags & HAMMER2_OPFLAG_PFSROOT) ||
		    media.ipdata.meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			tabprintf(tab, "pfs_nmas %u\n",
				  media.ipdata.meta.pfs_nmasters);
			tabprintf(tab, "pfs_type %u (%s)\n",
				  media.ipdata.meta.pfs_type,
				  hammer2_pfstype_to_str(media.ipdata.meta.pfs_type));
			tabprintf(tab, "pfs_inum 0x%016jx\n",
				  (uintmax_t)media.ipdata.meta.pfs_inum);
			uuid = media.ipdata.meta.pfs_clid;
			tabprintf(tab, "pfs_clid %s\n",
				  hammer2_uuid_to_str(&uuid, &str));
			uuid = media.ipdata.meta.pfs_fsid;
			tabprintf(tab, "pfs_fsid %s\n",
				  hammer2_uuid_to_str(&uuid, &str));
			tabprintf(tab, "pfs_lsnap_tid 0x%016jx\n",
				  (uintmax_t)media.ipdata.meta.pfs_lsnap_tid);
		}
		tabprintf(tab, "data_quota  %ju\n",
			  (uintmax_t)media.ipdata.meta.data_quota);
		tabprintf(tab, "data_count  %ju\n",
			  (uintmax_t)bref->embed.stats.data_count);
		tabprintf(tab, "inode_quota %ju\n",
			  (uintmax_t)media.ipdata.meta.inode_quota);
		tabprintf(tab, "inode_count %ju\n",
			  (uintmax_t)bref->embed.stats.inode_count);
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		printf("{\n");
		break;
	case HAMMER2_BREF_TYPE_DATA:
		printf("\n");
		obrace = 0;
		break;
	case HAMMER2_BREF_TYPE_VOLUME:
		printf("mirror_tid=%016jx freemap_tid=%016jx ",
			media.voldata.mirror_tid,
			media.voldata.freemap_tid);
		printf("{\n");
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		printf("mirror_tid=%016jx freemap_tid=%016jx ",
			media.voldata.mirror_tid,
			media.voldata.freemap_tid);
		printf("{\n");
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		printf("{\n");
		tmp = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
		tmp &= HAMMER2_SEGMASK;
		tmp /= HAMMER2_PBUFSIZE;
		assert(tmp >= HAMMER2_ZONE_FREEMAP_00);
		assert(tmp < HAMMER2_ZONE_FREEMAP_END);
		tmp -= HAMMER2_ZONE_FREEMAP_00;
		tmp /= HAMMER2_ZONE_FREEMAP_INC;
		tabprintf(tab, "rotation=%d\n", (int)tmp);

		for (i = 0; i < HAMMER2_FREEMAP_COUNT; ++i) {
			hammer2_off_t data_off = bref->key +
				i * HAMMER2_FREEMAP_LEVEL0_SIZE;
#if HAMMER2_BMAP_ELEMENTS != 8
#error "cmd_debug.c: HAMMER2_BMAP_ELEMENTS expected to be 8"
#endif
			tabprintf(tab + 4, "%016jx %04d.%04x linear=%06x avail=%06x "
				  "%016jx %016jx %016jx %016jx "
				  "%016jx %016jx %016jx %016jx\n",
				  data_off, i, media.bmdata[i].class,
				  media.bmdata[i].linear,
				  media.bmdata[i].avail,
				  media.bmdata[i].bitmapq[0],
				  media.bmdata[i].bitmapq[1],
				  media.bmdata[i].bitmapq[2],
				  media.bmdata[i].bitmapq[3],
				  media.bmdata[i].bitmapq[4],
				  media.bmdata[i].bitmapq[5],
				  media.bmdata[i].bitmapq[6],
				  media.bmdata[i].bitmapq[7]);
		}
		tabprintf(tab, "}\n");
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		printf("{\n");
		tmp = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
		tmp &= HAMMER2_SEGMASK;
		tmp /= HAMMER2_PBUFSIZE;
		assert(tmp >= HAMMER2_ZONE_FREEMAP_00);
		assert(tmp < HAMMER2_ZONE_FREEMAP_END);
		tmp -= HAMMER2_ZONE_FREEMAP_00;
		tmp /= HAMMER2_ZONE_FREEMAP_INC;
		tabprintf(tab, "rotation=%d\n", (int)tmp);
		break;
	default:
		printf("\n");
		obrace = 0;
		break;
	}
	if (str)
		free(str);

skip_data:
	/*
	 * Update statistics.
	 */
	switch(bref->type) {
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		for (i = 0; i < HAMMER2_FREEMAP_COUNT; ++i) {
			hammer2_off_t data_off = bref->key +
				i * HAMMER2_FREEMAP_LEVEL0_SIZE;
			if (data_off >= voldata->aux_end &&
			    data_off < hammer2_get_total_size()) {
				int j;
				for (j = 0; j < 4; ++j)
					count_blocks(&media.bmdata[i], j,
						    &TotalAccum16[j],
						    &TotalAccum64[j]);
			} else
				TotalUnavail += HAMMER2_FREEMAP_LEVEL0_SIZE;
		}
		TotalFreemap += HAMMER2_FREEMAP_LEVEL1_SIZE;
		break;
	default:
		break;
	}

	/*
	 * Recurse if norecurse == 0.  If the CRC failed, pass norecurse = 1.
	 * That is, if an indirect or inode fails we still try to list its
	 * direct children to help with debugging, but go no further than
	 * that because they are probably garbage.
	 */
	if (show_depth == -1 || ((tab - init_tab) / show_tab) < show_depth) {
		for (i = 0; norecurse == 0 && i < bcount; ++i) {
			if (bscan[i].type != HAMMER2_BREF_TYPE_EMPTY) {
				show_bref(voldata, tab, i, &bscan[i],
				    failed);
			}
		}
	}
	tab -= show_tab;
	if (obrace) {
		if (bref->type == HAMMER2_BREF_TYPE_INODE)
			tabprintf(tab, "} (%s.%d, \"%*.*s\")\n",
				  type_str, bi, namelen, namelen,
				  media.ipdata.filename);
		else
			tabprintf(tab, "} (%s.%d)\n", type_str, bi);
	}
}

static
void
count_blocks(hammer2_bmap_data_t *bmap, int value,
	     hammer2_off_t *accum16, hammer2_off_t *accum64)
{
	int i, j, bits;
	hammer2_bitmap_t value16, value64;

	bits = (int)sizeof(hammer2_bitmap_t) * 8;
	assert(bits == 64);

	value16 = value;
	assert(value16 < 4);
	value64 = (value16 << 6) | (value16 << 4) | (value16 << 2) | value16;
	assert(value64 < 256);

	for (i = 0; i < HAMMER2_BMAP_ELEMENTS; ++i) {
		hammer2_bitmap_t bm = bmap->bitmapq[i];
		hammer2_bitmap_t bm_save = bm;
		hammer2_bitmap_t mask;

		mask = 0x03; /* 2 bits per 16KB */
		for (j = 0; j < bits; j += 2) {
			if ((bm & mask) == value16)
				*accum16 += 16384;
			bm >>= 2;
		}

		bm = bm_save;
		mask = 0xFF; /* 8 bits per 64KB chunk */
		for (j = 0; j < bits; j += 8) {
			if ((bm & mask) == value64)
				*accum64 += 65536;
			bm >>= 8;
		}
	}
}

int
cmd_dumpchain(const char *path, u_int flags)
{
	int dummy = (int)flags;
	int ecode = 0;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd >= 0) {
		if (ioctl(fd, HAMMER2IOC_DEBUG_DUMP, &dummy) < 0) {
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
			ecode = 1;
		}
		close(fd);
	} else {
		fprintf(stderr, "unable to open %s\n", path);
		ecode = 1;
	}
	return ecode;
}

static
void
tabprintf(int tab, const char *ctl, ...)
{
	va_list va;

	printf("%*.*s", tab, tab, "");
	va_start(va, ctl);
	vprintf(ctl, va);
	va_end(va);
}
