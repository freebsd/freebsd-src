/*-
 * Copyright (c) 2007 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	Stand-alone ZFS file reader.
 */

#include <sys/stat.h>

#include "zfsimpl.h"
#include "zfssubr.c"

/*
 * List of all vdevs, chained through v_alllink.
 */
static vdev_list_t zfs_vdevs;

/*
 * List of all pools, chained through spa_link.
 */
static spa_list_t zfs_pools;

static uint64_t zfs_crc64_table[256];
static const dnode_phys_t *dnode_cache_obj = 0;
static uint64_t dnode_cache_bn;
static char *dnode_cache_buf;
static char *zap_scratch;
static char *zfs_temp_buf, *zfs_temp_end, *zfs_temp_ptr;

#define TEMP_SIZE	(1024 * 1024)

static int zio_read(spa_t *spa, const blkptr_t *bp, void *buf);

static void
zfs_init(void)
{
	STAILQ_INIT(&zfs_vdevs);
	STAILQ_INIT(&zfs_pools);

	zfs_temp_buf = malloc(TEMP_SIZE);
	zfs_temp_end = zfs_temp_buf + TEMP_SIZE;
	zfs_temp_ptr = zfs_temp_buf;
	dnode_cache_buf = malloc(SPA_MAXBLOCKSIZE);
	zap_scratch = malloc(SPA_MAXBLOCKSIZE);

	zfs_init_crc();
}

static void *
zfs_alloc(size_t size)
{
	char *ptr;

	if (zfs_temp_ptr + size > zfs_temp_end) {
		printf("ZFS: out of temporary buffer space\n");
		for (;;) ;
	}
	ptr = zfs_temp_ptr;
	zfs_temp_ptr += size;

	return (ptr);
}

static void
zfs_free(void *ptr, size_t size)
{

	zfs_temp_ptr -= size;
	if (zfs_temp_ptr != ptr) {
		printf("ZFS: zfs_alloc()/zfs_free() mismatch\n");
		for (;;) ;
	}
}

static int
xdr_int(const unsigned char **xdr, int *ip)
{
	*ip = ((*xdr)[0] << 24)
		| ((*xdr)[1] << 16)
		| ((*xdr)[2] << 8)
		| ((*xdr)[3] << 0);
	(*xdr) += 4;
	return (0);
}

static int
xdr_u_int(const unsigned char **xdr, u_int *ip)
{
	*ip = ((*xdr)[0] << 24)
		| ((*xdr)[1] << 16)
		| ((*xdr)[2] << 8)
		| ((*xdr)[3] << 0);
	(*xdr) += 4;
	return (0);
}

static int
xdr_uint64_t(const unsigned char **xdr, uint64_t *lp)
{
	u_int hi, lo;

	xdr_u_int(xdr, &hi);
	xdr_u_int(xdr, &lo);
	*lp = (((uint64_t) hi) << 32) | lo;
	return (0);
}

static int
nvlist_find(const unsigned char *nvlist, const char *name, int type,
	    int* elementsp, void *valuep)
{
	const unsigned char *p, *pair;
	int junk;
	int encoded_size, decoded_size;

	p = nvlist;
	xdr_int(&p, &junk);
	xdr_int(&p, &junk);

	pair = p;
	xdr_int(&p, &encoded_size);
	xdr_int(&p, &decoded_size);
	while (encoded_size && decoded_size) {
		int namelen, pairtype, elements;
		const char *pairname;

		xdr_int(&p, &namelen);
		pairname = (const char*) p;
		p += roundup(namelen, 4);
		xdr_int(&p, &pairtype);

		if (!memcmp(name, pairname, namelen) && type == pairtype) {
			xdr_int(&p, &elements);
			if (elementsp)
				*elementsp = elements;
			if (type == DATA_TYPE_UINT64) {
				xdr_uint64_t(&p, (uint64_t *) valuep);
				return (0);
			} else if (type == DATA_TYPE_STRING) {
				int len;
				xdr_int(&p, &len);
				(*(const char**) valuep) = (const char*) p;
				return (0);
			} else if (type == DATA_TYPE_NVLIST
				   || type == DATA_TYPE_NVLIST_ARRAY) {
				(*(const unsigned char**) valuep) =
					 (const unsigned char*) p;
				return (0);
			} else {
				return (EIO);
			}
		} else {
			/*
			 * Not the pair we are looking for, skip to the next one.
			 */
			p = pair + encoded_size;
		}

		pair = p;
		xdr_int(&p, &encoded_size);
		xdr_int(&p, &decoded_size);
	}

	return (EIO);
}

/*
 * Return the next nvlist in an nvlist array.
 */
static const unsigned char *
nvlist_next(const unsigned char *nvlist)
{
	const unsigned char *p, *pair;
	int junk;
	int encoded_size, decoded_size;

	p = nvlist;
	xdr_int(&p, &junk);
	xdr_int(&p, &junk);

	pair = p;
	xdr_int(&p, &encoded_size);
	xdr_int(&p, &decoded_size);
	while (encoded_size && decoded_size) {
		p = pair + encoded_size;

		pair = p;
		xdr_int(&p, &encoded_size);
		xdr_int(&p, &decoded_size);
	}

	return p;
}

#ifdef TEST

static const unsigned char *
nvlist_print(const unsigned char *nvlist, unsigned int indent)
{
	static const char* typenames[] = {
		"DATA_TYPE_UNKNOWN",
		"DATA_TYPE_BOOLEAN",
		"DATA_TYPE_BYTE",
		"DATA_TYPE_INT16",
		"DATA_TYPE_UINT16",
		"DATA_TYPE_INT32",
		"DATA_TYPE_UINT32",
		"DATA_TYPE_INT64",
		"DATA_TYPE_UINT64",
		"DATA_TYPE_STRING",
		"DATA_TYPE_BYTE_ARRAY",
		"DATA_TYPE_INT16_ARRAY",
		"DATA_TYPE_UINT16_ARRAY",
		"DATA_TYPE_INT32_ARRAY",
		"DATA_TYPE_UINT32_ARRAY",
		"DATA_TYPE_INT64_ARRAY",
		"DATA_TYPE_UINT64_ARRAY",
		"DATA_TYPE_STRING_ARRAY",
		"DATA_TYPE_HRTIME",
		"DATA_TYPE_NVLIST",
		"DATA_TYPE_NVLIST_ARRAY",
		"DATA_TYPE_BOOLEAN_VALUE",
		"DATA_TYPE_INT8",
		"DATA_TYPE_UINT8",
		"DATA_TYPE_BOOLEAN_ARRAY",
		"DATA_TYPE_INT8_ARRAY",
		"DATA_TYPE_UINT8_ARRAY"
	};

	unsigned int i, j;
	const unsigned char *p, *pair;
	int junk;
	int encoded_size, decoded_size;

	p = nvlist;
	xdr_int(&p, &junk);
	xdr_int(&p, &junk);

	pair = p;
	xdr_int(&p, &encoded_size);
	xdr_int(&p, &decoded_size);
	while (encoded_size && decoded_size) {
		int namelen, pairtype, elements;
		const char *pairname;

		xdr_int(&p, &namelen);
		pairname = (const char*) p;
		p += roundup(namelen, 4);
		xdr_int(&p, &pairtype);

		for (i = 0; i < indent; i++)
			printf(" ");
		printf("%s %s", typenames[pairtype], pairname);

		xdr_int(&p, &elements);
		switch (pairtype) {
		case DATA_TYPE_UINT64: {
			uint64_t val;
			xdr_uint64_t(&p, &val);
			printf(" = 0x%llx\n", val);
			break;
		}

		case DATA_TYPE_STRING: {
			int len;
			xdr_int(&p, &len);
			printf(" = \"%s\"\n", p);
			break;
		}

		case DATA_TYPE_NVLIST:
			printf("\n");
			nvlist_print(p, indent + 1);
			break;

		case DATA_TYPE_NVLIST_ARRAY:
			for (j = 0; j < elements; j++) {
				printf("[%d]\n", j);
				p = nvlist_print(p, indent + 1);
				if (j != elements - 1) {
					for (i = 0; i < indent; i++)
						printf(" ");
					printf("%s %s", typenames[pairtype], pairname);
				}
			}
			break;

		default:
			printf("\n");
		}

		p = pair + encoded_size;

		pair = p;
		xdr_int(&p, &encoded_size);
		xdr_int(&p, &decoded_size);
	}

	return p;
}

#endif

static int
vdev_read_phys(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t size)
{
	size_t psize;
	int rc;

	if (!vdev->v_phys_read)
		return (EIO);

	if (bp) {
		psize = BP_GET_PSIZE(bp);
	} else {
		psize = size;
	}

	/*printf("ZFS: reading %d bytes at 0x%llx to %p\n", psize, offset, buf);*/
	rc = vdev->v_phys_read(vdev, vdev->v_read_priv, offset, buf, psize);
	if (rc)
		return (rc);
	if (bp && zio_checksum_verify(bp, buf))
		return (EIO);

	return (0);
}

static int
vdev_disk_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{

	return (vdev_read_phys(vdev, bp, buf,
		offset + VDEV_LABEL_START_SIZE, bytes));
}


static int
vdev_mirror_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	vdev_t *kid;
	int rc;

	rc = EIO;
	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		if (kid->v_state != VDEV_STATE_HEALTHY)
			continue;
		rc = kid->v_read(kid, bp, buf, offset, bytes);
		if (!rc)
			return (0);
	}

	return (rc);
}

static int
vdev_replacing_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	vdev_t *kid;

	/*
	 * Here we should have two kids:
	 * First one which is the one we are replacing and we can trust
	 * only this one to have valid data, but it might not be present.
	 * Second one is that one we are replacing with. It is most likely
	 * healthy, but we can't trust it has needed data, so we won't use it.
	 */
	kid = STAILQ_FIRST(&vdev->v_children);
	if (kid == NULL)
		return (EIO);
	if (kid->v_state != VDEV_STATE_HEALTHY)
		return (EIO);
	return (kid->v_read(kid, bp, buf, offset, bytes));
}

static vdev_t *
vdev_find(uint64_t guid)
{
	vdev_t *vdev;

	STAILQ_FOREACH(vdev, &zfs_vdevs, v_alllink)
		if (vdev->v_guid == guid)
			return (vdev);

	return (0);
}

static vdev_t *
vdev_create(uint64_t guid, vdev_read_t *read)
{
	vdev_t *vdev;

	vdev = malloc(sizeof(vdev_t));
	memset(vdev, 0, sizeof(vdev_t));
	STAILQ_INIT(&vdev->v_children);
	vdev->v_guid = guid;
	vdev->v_state = VDEV_STATE_OFFLINE;
	vdev->v_read = read;
	vdev->v_phys_read = 0;
	vdev->v_read_priv = 0;
	STAILQ_INSERT_TAIL(&zfs_vdevs, vdev, v_alllink);

	return (vdev);
}

static int
vdev_init_from_nvlist(const unsigned char *nvlist, vdev_t *pvdev,
    vdev_t **vdevp, int is_newer)
{
	int rc;
	uint64_t guid, id, ashift, nparity;
	const char *type;
	const char *path;
	vdev_t *vdev, *kid;
	const unsigned char *kids;
	int nkids, i, is_new;
	uint64_t is_offline, is_faulted, is_degraded, is_removed, isnt_present;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_GUID,
			DATA_TYPE_UINT64, 0, &guid)
	    || nvlist_find(nvlist, ZPOOL_CONFIG_ID,
			   DATA_TYPE_UINT64, 0, &id)
	    || nvlist_find(nvlist, ZPOOL_CONFIG_TYPE,
			   DATA_TYPE_STRING, 0, &type)) {
		printf("ZFS: can't find vdev details\n");
		return (ENOENT);
	}

	if (strcmp(type, VDEV_TYPE_MIRROR)
	    && strcmp(type, VDEV_TYPE_DISK)
	    && strcmp(type, VDEV_TYPE_RAIDZ)
	    && strcmp(type, VDEV_TYPE_REPLACING)) {
		printf("ZFS: can only boot from disk, mirror, raidz1, raidz2 and raidz3 vdevs\n");
		return (EIO);
	}

	is_offline = is_removed = is_faulted = is_degraded = isnt_present = 0;

	nvlist_find(nvlist, ZPOOL_CONFIG_OFFLINE, DATA_TYPE_UINT64, 0,
			&is_offline);
	nvlist_find(nvlist, ZPOOL_CONFIG_REMOVED, DATA_TYPE_UINT64, 0,
			&is_removed);
	nvlist_find(nvlist, ZPOOL_CONFIG_FAULTED, DATA_TYPE_UINT64, 0,
			&is_faulted);
	nvlist_find(nvlist, ZPOOL_CONFIG_DEGRADED, DATA_TYPE_UINT64, 0,
			&is_degraded);
	nvlist_find(nvlist, ZPOOL_CONFIG_NOT_PRESENT, DATA_TYPE_UINT64, 0,
			&isnt_present);

	vdev = vdev_find(guid);
	if (!vdev) {
		is_new = 1;

		if (!strcmp(type, VDEV_TYPE_MIRROR))
			vdev = vdev_create(guid, vdev_mirror_read);
		else if (!strcmp(type, VDEV_TYPE_RAIDZ))
			vdev = vdev_create(guid, vdev_raidz_read);
		else if (!strcmp(type, VDEV_TYPE_REPLACING))
			vdev = vdev_create(guid, vdev_replacing_read);
		else
			vdev = vdev_create(guid, vdev_disk_read);

		vdev->v_id = id;
		vdev->v_top = pvdev != NULL ? pvdev : vdev;
		if (nvlist_find(nvlist, ZPOOL_CONFIG_ASHIFT,
			DATA_TYPE_UINT64, 0, &ashift) == 0)
			vdev->v_ashift = ashift;
		else
			vdev->v_ashift = 0;
		if (nvlist_find(nvlist, ZPOOL_CONFIG_NPARITY,
			DATA_TYPE_UINT64, 0, &nparity) == 0)
			vdev->v_nparity = nparity;
		else
			vdev->v_nparity = 0;
		if (nvlist_find(nvlist, ZPOOL_CONFIG_PATH,
				DATA_TYPE_STRING, 0, &path) == 0) {
			if (strncmp(path, "/dev/", 5) == 0)
				path += 5;
			vdev->v_name = strdup(path);
		} else {
			if (!strcmp(type, "raidz")) {
				if (vdev->v_nparity == 1)
					vdev->v_name = "raidz1";
				else if (vdev->v_nparity == 2)
					vdev->v_name = "raidz2";
				else if (vdev->v_nparity == 3)
					vdev->v_name = "raidz3";
				else {
					printf("ZFS: can only boot from disk, mirror, raidz1, raidz2 and raidz3 vdevs\n");
					return (EIO);
				}
			} else {
				vdev->v_name = strdup(type);
			}
		}
	} else {
		is_new = 0;
	}

	if (is_new || is_newer) {
		/*
		 * This is either new vdev or we've already seen this vdev,
		 * but from an older vdev label, so let's refresh its state
		 * from the newer label.
		 */
		if (is_offline)
			vdev->v_state = VDEV_STATE_OFFLINE;
		else if (is_removed)
			vdev->v_state = VDEV_STATE_REMOVED;
		else if (is_faulted)
			vdev->v_state = VDEV_STATE_FAULTED;
		else if (is_degraded)
			vdev->v_state = VDEV_STATE_DEGRADED;
		else if (isnt_present)
			vdev->v_state = VDEV_STATE_CANT_OPEN;
	}

	rc = nvlist_find(nvlist, ZPOOL_CONFIG_CHILDREN,
			 DATA_TYPE_NVLIST_ARRAY, &nkids, &kids);
	/*
	 * Its ok if we don't have any kids.
	 */
	if (rc == 0) {
		vdev->v_nchildren = nkids;
		for (i = 0; i < nkids; i++) {
			rc = vdev_init_from_nvlist(kids, vdev, &kid, is_newer);
			if (rc)
				return (rc);
			if (is_new)
				STAILQ_INSERT_TAIL(&vdev->v_children, kid,
						   v_childlink);
			kids = nvlist_next(kids);
		}
	} else {
		vdev->v_nchildren = 0;
	}

	if (vdevp)
		*vdevp = vdev;
	return (0);
}

static void
vdev_set_state(vdev_t *vdev)
{
	vdev_t *kid;
	int good_kids;
	int bad_kids;

	/*
	 * A mirror or raidz is healthy if all its kids are healthy. A
	 * mirror is degraded if any of its kids is healthy; a raidz
	 * is degraded if at most nparity kids are offline.
	 */
	if (STAILQ_FIRST(&vdev->v_children)) {
		good_kids = 0;
		bad_kids = 0;
		STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
			if (kid->v_state == VDEV_STATE_HEALTHY)
				good_kids++;
			else
				bad_kids++;
		}
		if (bad_kids == 0) {
			vdev->v_state = VDEV_STATE_HEALTHY;
		} else {
			if (vdev->v_read == vdev_mirror_read) {
				if (good_kids) {
					vdev->v_state = VDEV_STATE_DEGRADED;
				} else {
					vdev->v_state = VDEV_STATE_OFFLINE;
				}
			} else if (vdev->v_read == vdev_raidz_read) {
				if (bad_kids > vdev->v_nparity) {
					vdev->v_state = VDEV_STATE_OFFLINE;
				} else {
					vdev->v_state = VDEV_STATE_DEGRADED;
				}
			}
		}
	}
}

static spa_t *
spa_find_by_guid(uint64_t guid)
{
	spa_t *spa;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link)
		if (spa->spa_guid == guid)
			return (spa);

	return (0);
}

#ifdef BOOT2

static spa_t *
spa_find_by_name(const char *name)
{
	spa_t *spa;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link)
		if (!strcmp(spa->spa_name, name))
			return (spa);

	return (0);
}

#endif

static spa_t *
spa_create(uint64_t guid)
{
	spa_t *spa;

	spa = malloc(sizeof(spa_t));
	memset(spa, 0, sizeof(spa_t));
	STAILQ_INIT(&spa->spa_vdevs);
	spa->spa_guid = guid;
	STAILQ_INSERT_TAIL(&zfs_pools, spa, spa_link);

	return (spa);
}

static const char *
state_name(vdev_state_t state)
{
	static const char* names[] = {
		"UNKNOWN",
		"CLOSED",
		"OFFLINE",
		"REMOVED",
		"CANT_OPEN",
		"FAULTED",
		"DEGRADED",
		"ONLINE"
	};
	return names[state];
}

#ifdef BOOT2

#define pager_printf printf

#else

static void
pager_printf(const char *fmt, ...)
{
	char line[80];
	va_list args;

	va_start(args, fmt);
	vsprintf(line, fmt, args);
	va_end(args);
	pager_output(line);
}

#endif

#define STATUS_FORMAT	"        %s %s\n"

static void
print_state(int indent, const char *name, vdev_state_t state)
{
	int i;
	char buf[512];

	buf[0] = 0;
	for (i = 0; i < indent; i++)
		strcat(buf, "  ");
	strcat(buf, name);
	pager_printf(STATUS_FORMAT, buf, state_name(state));
	
}

static void
vdev_status(vdev_t *vdev, int indent)
{
	vdev_t *kid;
	print_state(indent, vdev->v_name, vdev->v_state);

	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		vdev_status(kid, indent + 1);
	}
}

static void
spa_status(spa_t *spa)
{
	vdev_t *vdev;
	int good_kids, bad_kids, degraded_kids;
	vdev_state_t state;

	pager_printf("  pool: %s\n", spa->spa_name);
	pager_printf("config:\n\n");
	pager_printf(STATUS_FORMAT, "NAME", "STATE");

	good_kids = 0;
	degraded_kids = 0;
	bad_kids = 0;
	STAILQ_FOREACH(vdev, &spa->spa_vdevs, v_childlink) {
		if (vdev->v_state == VDEV_STATE_HEALTHY)
			good_kids++;
		else if (vdev->v_state == VDEV_STATE_DEGRADED)
			degraded_kids++;
		else
			bad_kids++;
	}

	state = VDEV_STATE_CLOSED;
	if (good_kids > 0 && (degraded_kids + bad_kids) == 0)
		state = VDEV_STATE_HEALTHY;
	else if ((good_kids + degraded_kids) > 0)
		state = VDEV_STATE_DEGRADED;

	print_state(0, spa->spa_name, state);
	STAILQ_FOREACH(vdev, &spa->spa_vdevs, v_childlink) {
		vdev_status(vdev, 1);
	}
}

static void
spa_all_status(void)
{
	spa_t *spa;
	int first = 1;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link) {
		if (!first)
			pager_printf("\n");
		first = 0;
		spa_status(spa);
	}
}

static int
vdev_probe(vdev_phys_read_t *read, void *read_priv, spa_t **spap)
{
	vdev_t vtmp;
	vdev_phys_t *vdev_label = (vdev_phys_t *) zap_scratch;
	spa_t *spa;
	vdev_t *vdev, *top_vdev, *pool_vdev;
	off_t off;
	blkptr_t bp;
	const unsigned char *nvlist;
	uint64_t val;
	uint64_t guid;
	uint64_t pool_txg, pool_guid;
	uint64_t is_log;
	const char *pool_name;
	const unsigned char *vdevs;
	int i, rc, is_newer;
	char *upbuf;
	const struct uberblock *up;

	/*
	 * Load the vdev label and figure out which
	 * uberblock is most current.
	 */
	memset(&vtmp, 0, sizeof(vtmp));
	vtmp.v_phys_read = read;
	vtmp.v_read_priv = read_priv;
	off = offsetof(vdev_label_t, vl_vdev_phys);
	BP_ZERO(&bp);
	BP_SET_LSIZE(&bp, sizeof(vdev_phys_t));
	BP_SET_PSIZE(&bp, sizeof(vdev_phys_t));
	BP_SET_CHECKSUM(&bp, ZIO_CHECKSUM_LABEL);
	BP_SET_COMPRESS(&bp, ZIO_COMPRESS_OFF);
	DVA_SET_OFFSET(BP_IDENTITY(&bp), off);
	ZIO_SET_CHECKSUM(&bp.blk_cksum, off, 0, 0, 0);
	if (vdev_read_phys(&vtmp, &bp, vdev_label, off, 0))
		return (EIO);

	if (vdev_label->vp_nvlist[0] != NV_ENCODE_XDR) {
		return (EIO);
	}

	nvlist = (const unsigned char *) vdev_label->vp_nvlist + 4;

	if (nvlist_find(nvlist,
			ZPOOL_CONFIG_VERSION,
			DATA_TYPE_UINT64, 0, &val)) {
		return (EIO);
	}

	if (val > SPA_VERSION) {
		printf("ZFS: unsupported ZFS version %u (should be %u)\n",
		    (unsigned) val, (unsigned) SPA_VERSION);
		return (EIO);
	}

	if (nvlist_find(nvlist,
			ZPOOL_CONFIG_POOL_STATE,
			DATA_TYPE_UINT64, 0, &val)) {
		return (EIO);
	}

	if (val == POOL_STATE_DESTROYED) {
		/* We don't boot only from destroyed pools. */
		return (EIO);
	}

	if (nvlist_find(nvlist,
			ZPOOL_CONFIG_POOL_TXG,
			DATA_TYPE_UINT64, 0, &pool_txg)
	    || nvlist_find(nvlist,
			   ZPOOL_CONFIG_POOL_GUID,
			   DATA_TYPE_UINT64, 0, &pool_guid)
	    || nvlist_find(nvlist,
			   ZPOOL_CONFIG_POOL_NAME,
			   DATA_TYPE_STRING, 0, &pool_name)) {
		/*
		 * Cache and spare devices end up here - just ignore
		 * them.
		 */
		/*printf("ZFS: can't find pool details\n");*/
		return (EIO);
	}

	is_log = 0;
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_IS_LOG, DATA_TYPE_UINT64, 0,
	    &is_log);
	if (is_log)
		return (EIO);

	/*
	 * Create the pool if this is the first time we've seen it.
	 */
	spa = spa_find_by_guid(pool_guid);
	if (!spa) {
		spa = spa_create(pool_guid);
		spa->spa_name = strdup(pool_name);
	}
	if (pool_txg > spa->spa_txg) {
		spa->spa_txg = pool_txg;
		is_newer = 1;
	} else
		is_newer = 0;

	/*
	 * Get the vdev tree and create our in-core copy of it.
	 * If we already have a vdev with this guid, this must
	 * be some kind of alias (overlapping slices, dangerously dedicated
	 * disks etc).
	 */
	if (nvlist_find(nvlist,
			ZPOOL_CONFIG_GUID,
			DATA_TYPE_UINT64, 0, &guid)) {
		return (EIO);
	}
	vdev = vdev_find(guid);
	if (vdev && vdev->v_phys_read)	/* Has this vdev already been inited? */
		return (EIO);

	if (nvlist_find(nvlist,
			ZPOOL_CONFIG_VDEV_TREE,
			DATA_TYPE_NVLIST, 0, &vdevs)) {
		return (EIO);
	}

	rc = vdev_init_from_nvlist(vdevs, NULL, &top_vdev, is_newer);
	if (rc)
		return (rc);

	/*
	 * Add the toplevel vdev to the pool if its not already there.
	 */
	STAILQ_FOREACH(pool_vdev, &spa->spa_vdevs, v_childlink)
		if (top_vdev == pool_vdev)
			break;
	if (!pool_vdev && top_vdev)
		STAILQ_INSERT_TAIL(&spa->spa_vdevs, top_vdev, v_childlink);

	/*
	 * We should already have created an incomplete vdev for this
	 * vdev. Find it and initialise it with our read proc.
	 */
	vdev = vdev_find(guid);
	if (vdev) {
		vdev->v_phys_read = read;
		vdev->v_read_priv = read_priv;
		vdev->v_state = VDEV_STATE_HEALTHY;
	} else {
		printf("ZFS: inconsistent nvlist contents\n");
		return (EIO);
	}

	/*
	 * Re-evaluate top-level vdev state.
	 */
	vdev_set_state(top_vdev);

	/*
	 * Ok, we are happy with the pool so far. Lets find
	 * the best uberblock and then we can actually access
	 * the contents of the pool.
	 */
	upbuf = zfs_alloc(VDEV_UBERBLOCK_SIZE(vdev));
	up = (const struct uberblock *)upbuf;
	for (i = 0;
	     i < VDEV_UBERBLOCK_COUNT(vdev);
	     i++) {
		off = VDEV_UBERBLOCK_OFFSET(vdev, i);
		BP_ZERO(&bp);
		DVA_SET_OFFSET(&bp.blk_dva[0], off);
		BP_SET_LSIZE(&bp, VDEV_UBERBLOCK_SIZE(vdev));
		BP_SET_PSIZE(&bp, VDEV_UBERBLOCK_SIZE(vdev));
		BP_SET_CHECKSUM(&bp, ZIO_CHECKSUM_LABEL);
		BP_SET_COMPRESS(&bp, ZIO_COMPRESS_OFF);
		ZIO_SET_CHECKSUM(&bp.blk_cksum, off, 0, 0, 0);

		if (vdev_read_phys(vdev, &bp, upbuf, off, 0))
			continue;

		if (up->ub_magic != UBERBLOCK_MAGIC)
			continue;
		if (up->ub_txg < spa->spa_txg)
			continue;
		if (up->ub_txg > spa->spa_uberblock.ub_txg) {
			spa->spa_uberblock = *up;
		} else if (up->ub_txg == spa->spa_uberblock.ub_txg) {
			if (up->ub_timestamp > spa->spa_uberblock.ub_timestamp)
				spa->spa_uberblock = *up;
		}
	}
	zfs_free(upbuf, VDEV_UBERBLOCK_SIZE(vdev));

	if (spap)
		*spap = spa;
	return (0);
}

static int
ilog2(int n)
{
	int v;

	for (v = 0; v < 32; v++)
		if (n == (1 << v))
			return v;
	return -1;
}

static int
zio_read_gang(spa_t *spa, const blkptr_t *bp, void *buf)
{
	blkptr_t gbh_bp;
	zio_gbh_phys_t zio_gb;
	char *pbuf;
	int i;

	/* Artificial BP for gang block header. */
	gbh_bp = *bp;
	BP_SET_PSIZE(&gbh_bp, SPA_GANGBLOCKSIZE);
	BP_SET_LSIZE(&gbh_bp, SPA_GANGBLOCKSIZE);
	BP_SET_CHECKSUM(&gbh_bp, ZIO_CHECKSUM_GANG_HEADER);
	BP_SET_COMPRESS(&gbh_bp, ZIO_COMPRESS_OFF);
	for (i = 0; i < SPA_DVAS_PER_BP; i++)
		DVA_SET_GANG(&gbh_bp.blk_dva[i], 0);

	/* Read gang header block using the artificial BP. */
	if (zio_read(spa, &gbh_bp, &zio_gb))
		return (EIO);

	pbuf = buf;
	for (i = 0; i < SPA_GBH_NBLKPTRS; i++) {
		blkptr_t *gbp = &zio_gb.zg_blkptr[i];

		if (BP_IS_HOLE(gbp))
			continue;
		if (zio_read(spa, gbp, pbuf))
			return (EIO);
		pbuf += BP_GET_PSIZE(gbp);
	}

	if (zio_checksum_verify(bp, buf))
		return (EIO);
	return (0);
}

static int
zio_read(spa_t *spa, const blkptr_t *bp, void *buf)
{
	int cpfunc = BP_GET_COMPRESS(bp);
	uint64_t align, size;
	void *pbuf;
	int i, error;

	error = EIO;

	for (i = 0; i < SPA_DVAS_PER_BP; i++) {
		const dva_t *dva = &bp->blk_dva[i];
		vdev_t *vdev;
		int vdevid;
		off_t offset;

		if (!dva->dva_word[0] && !dva->dva_word[1])
			continue;

		vdevid = DVA_GET_VDEV(dva);
		offset = DVA_GET_OFFSET(dva);
		STAILQ_FOREACH(vdev, &spa->spa_vdevs, v_childlink) {
			if (vdev->v_id == vdevid)
				break;
		}
		if (!vdev || !vdev->v_read)
			continue;

		size = BP_GET_PSIZE(bp);
		if (vdev->v_read == vdev_raidz_read) {
			align = 1ULL << vdev->v_top->v_ashift;
			if (P2PHASE(size, align) != 0)
				size = P2ROUNDUP(size, align);
		}
		if (size != BP_GET_PSIZE(bp) || cpfunc != ZIO_COMPRESS_OFF)
			pbuf = zfs_alloc(size);
		else
			pbuf = buf;

		if (DVA_GET_GANG(dva))
			error = zio_read_gang(spa, bp, pbuf);
		else
			error = vdev->v_read(vdev, bp, pbuf, offset, size);
		if (error == 0) {
			if (cpfunc != ZIO_COMPRESS_OFF)
				error = zio_decompress_data(cpfunc, pbuf,
				    BP_GET_PSIZE(bp), buf, BP_GET_LSIZE(bp));
			else if (size != BP_GET_PSIZE(bp))
				bcopy(pbuf, buf, BP_GET_PSIZE(bp));
		}
		if (buf != pbuf)
			zfs_free(pbuf, size);
		if (error == 0)
			break;
	}
	if (error != 0)
		printf("ZFS: i/o error - all block copies unavailable\n");
	return (error);
}

static int
dnode_read(spa_t *spa, const dnode_phys_t *dnode, off_t offset, void *buf, size_t buflen)
{
	int ibshift = dnode->dn_indblkshift - SPA_BLKPTRSHIFT;
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	int nlevels = dnode->dn_nlevels;
	int i, rc;

	/*
	 * Note: bsize may not be a power of two here so we need to do an
	 * actual divide rather than a bitshift.
	 */
	while (buflen > 0) {
		uint64_t bn = offset / bsize;
		int boff = offset % bsize;
		int ibn;
		const blkptr_t *indbp;
		blkptr_t bp;

		if (bn > dnode->dn_maxblkid)
			return (EIO);

		if (dnode == dnode_cache_obj && bn == dnode_cache_bn)
			goto cached;

		indbp = dnode->dn_blkptr;
		for (i = 0; i < nlevels; i++) {
			/*
			 * Copy the bp from the indirect array so that
			 * we can re-use the scratch buffer for multi-level
			 * objects.
			 */
			ibn = bn >> ((nlevels - i - 1) * ibshift);
			ibn &= ((1 << ibshift) - 1);
			bp = indbp[ibn];
			rc = zio_read(spa, &bp, dnode_cache_buf);
			if (rc)
				return (rc);
			indbp = (const blkptr_t *) dnode_cache_buf;
		}
		dnode_cache_obj = dnode;
		dnode_cache_bn = bn;
	cached:

		/*
		 * The buffer contains our data block. Copy what we
		 * need from it and loop.
		 */ 
		i = bsize - boff;
		if (i > buflen) i = buflen;
		memcpy(buf, &dnode_cache_buf[boff], i);
		buf = ((char*) buf) + i;
		offset += i;
		buflen -= i;
	}

	return (0);
}

/*
 * Lookup a value in a microzap directory. Assumes that the zap
 * scratch buffer contains the directory contents.
 */
static int
mzap_lookup(spa_t *spa, const dnode_phys_t *dnode, const char *name, uint64_t *value)
{
	const mzap_phys_t *mz;
	const mzap_ent_phys_t *mze;
	size_t size;
	int chunks, i;

	/*
	 * Microzap objects use exactly one block. Read the whole
	 * thing.
	 */
	size = dnode->dn_datablkszsec * 512;

	mz = (const mzap_phys_t *) zap_scratch;
	chunks = size / MZAP_ENT_LEN - 1;

	for (i = 0; i < chunks; i++) {
		mze = &mz->mz_chunk[i];
		if (!strcmp(mze->mze_name, name)) {
			*value = mze->mze_value;
			return (0);
		}
	}

	return (ENOENT);
}

/*
 * Compare a name with a zap leaf entry. Return non-zero if the name
 * matches.
 */
static int
fzap_name_equal(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc, const char *name)
{
	size_t namelen;
	const zap_leaf_chunk_t *nc;
	const char *p;

	namelen = zc->l_entry.le_name_length;
			
	nc = &ZAP_LEAF_CHUNK(zl, zc->l_entry.le_name_chunk);
	p = name;
	while (namelen > 0) {
		size_t len;
		len = namelen;
		if (len > ZAP_LEAF_ARRAY_BYTES)
			len = ZAP_LEAF_ARRAY_BYTES;
		if (memcmp(p, nc->l_array.la_array, len))
			return (0);
		p += len;
		namelen -= len;
		nc = &ZAP_LEAF_CHUNK(zl, nc->l_array.la_next);
	}

	return 1;
}

/*
 * Extract a uint64_t value from a zap leaf entry.
 */
static uint64_t
fzap_leaf_value(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc)
{
	const zap_leaf_chunk_t *vc;
	int i;
	uint64_t value;
	const uint8_t *p;

	vc = &ZAP_LEAF_CHUNK(zl, zc->l_entry.le_value_chunk);
	for (i = 0, value = 0, p = vc->l_array.la_array; i < 8; i++) {
		value = (value << 8) | p[i];
	}

	return value;
}

/*
 * Lookup a value in a fatzap directory. Assumes that the zap scratch
 * buffer contains the directory header.
 */
static int
fzap_lookup(spa_t *spa, const dnode_phys_t *dnode, const char *name, uint64_t *value)
{
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	zap_phys_t zh = *(zap_phys_t *) zap_scratch;
	fat_zap_t z;
	uint64_t *ptrtbl;
	uint64_t hash;
	int rc;

	if (zh.zap_magic != ZAP_MAGIC)
		return (EIO);

	z.zap_block_shift = ilog2(bsize);
	z.zap_phys = (zap_phys_t *) zap_scratch;

	/*
	 * Figure out where the pointer table is and read it in if necessary.
	 */
	if (zh.zap_ptrtbl.zt_blk) {
		rc = dnode_read(spa, dnode, zh.zap_ptrtbl.zt_blk * bsize,
			       zap_scratch, bsize);
		if (rc)
			return (rc);
		ptrtbl = (uint64_t *) zap_scratch;
	} else {
		ptrtbl = &ZAP_EMBEDDED_PTRTBL_ENT(&z, 0);
	}

	hash = zap_hash(zh.zap_salt, name);

	zap_leaf_t zl;
	zl.l_bs = z.zap_block_shift;

	off_t off = ptrtbl[hash >> (64 - zh.zap_ptrtbl.zt_shift)] << zl.l_bs;
	zap_leaf_chunk_t *zc;

	rc = dnode_read(spa, dnode, off, zap_scratch, bsize);
	if (rc)
		return (rc);

	zl.l_phys = (zap_leaf_phys_t *) zap_scratch;

	/*
	 * Make sure this chunk matches our hash.
	 */
	if (zl.l_phys->l_hdr.lh_prefix_len > 0
	    && zl.l_phys->l_hdr.lh_prefix
	    != hash >> (64 - zl.l_phys->l_hdr.lh_prefix_len))
		return (ENOENT);

	/*
	 * Hash within the chunk to find our entry.
	 */
	int shift = (64 - ZAP_LEAF_HASH_SHIFT(&zl) - zl.l_phys->l_hdr.lh_prefix_len);
	int h = (hash >> shift) & ((1 << ZAP_LEAF_HASH_SHIFT(&zl)) - 1);
	h = zl.l_phys->l_hash[h];
	if (h == 0xffff)
		return (ENOENT);
	zc = &ZAP_LEAF_CHUNK(&zl, h);
	while (zc->l_entry.le_hash != hash) {
		if (zc->l_entry.le_next == 0xffff) {
			zc = 0;
			break;
		}
		zc = &ZAP_LEAF_CHUNK(&zl, zc->l_entry.le_next);
	}
	if (fzap_name_equal(&zl, zc, name)) {
		*value = fzap_leaf_value(&zl, zc);
		return (0);
	}

	return (ENOENT);
}

/*
 * Lookup a name in a zap object and return its value as a uint64_t.
 */
static int
zap_lookup(spa_t *spa, const dnode_phys_t *dnode, const char *name, uint64_t *value)
{
	int rc;
	uint64_t zap_type;
	size_t size = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;

	rc = dnode_read(spa, dnode, 0, zap_scratch, size);
	if (rc)
		return (rc);

	zap_type = *(uint64_t *) zap_scratch;
	if (zap_type == ZBT_MICRO)
		return mzap_lookup(spa, dnode, name, value);
	else if (zap_type == ZBT_HEADER)
		return fzap_lookup(spa, dnode, name, value);
	printf("ZFS: invalid zap_type=%d\n", (int)zap_type);
	return (EIO);
}

#ifdef BOOT2

/*
 * List a microzap directory. Assumes that the zap scratch buffer contains
 * the directory contents.
 */
static int
mzap_list(spa_t *spa, const dnode_phys_t *dnode)
{
	const mzap_phys_t *mz;
	const mzap_ent_phys_t *mze;
	size_t size;
	int chunks, i;

	/*
	 * Microzap objects use exactly one block. Read the whole
	 * thing.
	 */
	size = dnode->dn_datablkszsec * 512;
	mz = (const mzap_phys_t *) zap_scratch;
	chunks = size / MZAP_ENT_LEN - 1;

	for (i = 0; i < chunks; i++) {
		mze = &mz->mz_chunk[i];
		if (mze->mze_name[0])
			//printf("%-32s 0x%llx\n", mze->mze_name, mze->mze_value);
			printf("%s\n", mze->mze_name);
	}

	return (0);
}

/*
 * List a fatzap directory. Assumes that the zap scratch buffer contains
 * the directory header.
 */
static int
fzap_list(spa_t *spa, const dnode_phys_t *dnode)
{
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	zap_phys_t zh = *(zap_phys_t *) zap_scratch;
	fat_zap_t z;
	int i, j;

	if (zh.zap_magic != ZAP_MAGIC)
		return (EIO);

	z.zap_block_shift = ilog2(bsize);
	z.zap_phys = (zap_phys_t *) zap_scratch;

	/*
	 * This assumes that the leaf blocks start at block 1. The
	 * documentation isn't exactly clear on this.
	 */
	zap_leaf_t zl;
	zl.l_bs = z.zap_block_shift;
	for (i = 0; i < zh.zap_num_leafs; i++) {
		off_t off = (i + 1) << zl.l_bs;
		char name[256], *p;
		uint64_t value;

		if (dnode_read(spa, dnode, off, zap_scratch, bsize))
			return (EIO);

		zl.l_phys = (zap_leaf_phys_t *) zap_scratch;

		for (j = 0; j < ZAP_LEAF_NUMCHUNKS(&zl); j++) {
			zap_leaf_chunk_t *zc, *nc;
			int namelen;

			zc = &ZAP_LEAF_CHUNK(&zl, j);
			if (zc->l_entry.le_type != ZAP_CHUNK_ENTRY)
				continue;
			namelen = zc->l_entry.le_name_length;
			if (namelen > sizeof(name))
				namelen = sizeof(name);
			
			/*
			 * Paste the name back together.
			 */
			nc = &ZAP_LEAF_CHUNK(&zl, zc->l_entry.le_name_chunk);
			p = name;
			while (namelen > 0) {
				int len;
				len = namelen;
				if (len > ZAP_LEAF_ARRAY_BYTES)
					len = ZAP_LEAF_ARRAY_BYTES;
				memcpy(p, nc->l_array.la_array, len);
				p += len;
				namelen -= len;
				nc = &ZAP_LEAF_CHUNK(&zl, nc->l_array.la_next);
			}

			/*
			 * Assume the first eight bytes of the value are
			 * a uint64_t.
			 */
			value = fzap_leaf_value(&zl, zc);

			printf("%s 0x%llx\n", name, value);
		}
	}

	return (0);
}

/*
 * List a zap directory.
 */
static int
zap_list(spa_t *spa, const dnode_phys_t *dnode)
{
	uint64_t zap_type;
	size_t size = dnode->dn_datablkszsec * 512;

	if (dnode_read(spa, dnode, 0, zap_scratch, size))
		return (EIO);

	zap_type = *(uint64_t *) zap_scratch;
	if (zap_type == ZBT_MICRO)
		return mzap_list(spa, dnode);
	else
		return fzap_list(spa, dnode);
}

#endif

static int
objset_get_dnode(spa_t *spa, const objset_phys_t *os, uint64_t objnum, dnode_phys_t *dnode)
{
	off_t offset;

	offset = objnum * sizeof(dnode_phys_t);
	return dnode_read(spa, &os->os_meta_dnode, offset,
		dnode, sizeof(dnode_phys_t));
}

/*
 * Find the object set given the object number of its dataset object
 * and return its details in *objset
 */
static int
zfs_mount_dataset(spa_t *spa, uint64_t objnum, objset_phys_t *objset)
{
	dnode_phys_t dataset;
	dsl_dataset_phys_t *ds;

	if (objset_get_dnode(spa, &spa->spa_mos, objnum, &dataset)) {
		printf("ZFS: can't find dataset %llu\n", objnum);
		return (EIO);
	}

	ds = (dsl_dataset_phys_t *) &dataset.dn_bonus;
	if (zio_read(spa, &ds->ds_bp, objset)) {
		printf("ZFS: can't read object set for dataset %llu\n", objnum);
		return (EIO);
	}

	return (0);
}

/*
 * Find the object set pointed to by the BOOTFS property or the root
 * dataset if there is none and return its details in *objset
 */
static int
zfs_mount_root(spa_t *spa, objset_phys_t *objset)
{
	dnode_phys_t dir, propdir;
	uint64_t props, bootfs, root;

	/*
	 * Start with the MOS directory object.
	 */
	if (objset_get_dnode(spa, &spa->spa_mos, DMU_POOL_DIRECTORY_OBJECT, &dir)) {
		printf("ZFS: can't read MOS object directory\n");
		return (EIO);
	}

	/*
	 * Lookup the pool_props and see if we can find a bootfs.
	 */
	if (zap_lookup(spa, &dir, DMU_POOL_PROPS, &props) == 0
	     && objset_get_dnode(spa, &spa->spa_mos, props, &propdir) == 0
	     && zap_lookup(spa, &propdir, "bootfs", &bootfs) == 0
	     && bootfs != 0)
		return zfs_mount_dataset(spa, bootfs, objset);

	/*
	 * Lookup the root dataset directory
	 */
	if (zap_lookup(spa, &dir, DMU_POOL_ROOT_DATASET, &root)
	    || objset_get_dnode(spa, &spa->spa_mos, root, &dir)) {
		printf("ZFS: can't find root dsl_dir\n");
		return (EIO);
	}

	/*
	 * Use the information from the dataset directory's bonus buffer
	 * to find the dataset object and from that the object set itself.
	 */
	dsl_dir_phys_t *dd = (dsl_dir_phys_t *) &dir.dn_bonus;
	return zfs_mount_dataset(spa, dd->dd_head_dataset_obj, objset);
}

static int
zfs_mount_pool(spa_t *spa)
{

	/*
	 * Find the MOS and work our way in from there.
	 */
	if (zio_read(spa, &spa->spa_uberblock.ub_rootbp, &spa->spa_mos)) {
		printf("ZFS: can't read MOS\n");
		return (EIO);
	}

	/*
	 * Find the root object set
	 */
	if (zfs_mount_root(spa, &spa->spa_root_objset)) {
		printf("Can't find root filesystem - giving up\n");
		return (EIO);
	}

	return (0);
}

static int
zfs_dnode_stat(spa_t *spa, dnode_phys_t *dn, struct stat *sb)
{

	if (dn->dn_bonustype != DMU_OT_SA) {
		znode_phys_t *zp = (znode_phys_t *)dn->dn_bonus;

		sb->st_mode = zp->zp_mode;
		sb->st_uid = zp->zp_uid;
		sb->st_gid = zp->zp_gid;
		sb->st_size = zp->zp_size;
	} else {
		sa_hdr_phys_t *sahdrp;
		int hdrsize;
		size_t size = 0;
		void *buf = NULL;

		if (dn->dn_bonuslen != 0)
			sahdrp = (sa_hdr_phys_t *)DN_BONUS(dn);
		else {
			if ((dn->dn_flags & DNODE_FLAG_SPILL_BLKPTR) != 0) {
				blkptr_t *bp = &dn->dn_spill;
				int error;

				size = BP_GET_LSIZE(bp);
				buf = zfs_alloc(size);
				error = zio_read(spa, bp, buf);
				if (error != 0) {
					zfs_free(buf, size);
					return (error);
				}
				sahdrp = buf;
			} else {
				return (EIO);
			}
		}
		hdrsize = SA_HDR_SIZE(sahdrp);
		sb->st_mode = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_MODE_OFFSET);
		sb->st_uid = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_UID_OFFSET);
		sb->st_gid = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_GID_OFFSET);
		sb->st_size = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_SIZE_OFFSET);
		if (buf != NULL)
			zfs_free(buf, size);
	}

	return (0);
}

/*
 * Lookup a file and return its dnode.
 */
static int
zfs_lookup(spa_t *spa, const char *upath, dnode_phys_t *dnode)
{
	int rc;
	uint64_t objnum, rootnum, parentnum;
	dnode_phys_t dn;
	const char *p, *q;
	char element[256];
	char path[1024];
	int symlinks_followed = 0;
	struct stat sb;

	if (spa->spa_root_objset.os_type != DMU_OST_ZFS) {
		printf("ZFS: unexpected object set type %llu\n",
		       spa->spa_root_objset.os_type);
		return (EIO);
	}

	/*
	 * Get the root directory dnode.
	 */
	rc = objset_get_dnode(spa, &spa->spa_root_objset, MASTER_NODE_OBJ, &dn);
	if (rc)
		return (rc);

	rc = zap_lookup(spa, &dn, ZFS_ROOT_OBJ, &rootnum);
	if (rc)
		return (rc);

	rc = objset_get_dnode(spa, &spa->spa_root_objset, rootnum, &dn);
	if (rc)
		return (rc);

	objnum = rootnum;
	p = upath;
	while (p && *p) {
		while (*p == '/')
			p++;
		if (!*p)
			break;
		q = strchr(p, '/');
		if (q) {
			memcpy(element, p, q - p);
			element[q - p] = 0;
			p = q;
		} else {
			strcpy(element, p);
			p = 0;
		}

		rc = zfs_dnode_stat(spa, &dn, &sb);
		if (rc)
			return (rc);
		if (!S_ISDIR(sb.st_mode))
			return (ENOTDIR);

		parentnum = objnum;
		rc = zap_lookup(spa, &dn, element, &objnum);
		if (rc)
			return (rc);
		objnum = ZFS_DIRENT_OBJ(objnum);

		rc = objset_get_dnode(spa, &spa->spa_root_objset, objnum, &dn);
		if (rc)
			return (rc);

		/*
		 * Check for symlink.
		 */
		rc = zfs_dnode_stat(spa, &dn, &sb);
		if (rc)
			return (rc);
		if (S_ISLNK(sb.st_mode)) {
			if (symlinks_followed > 10)
				return (EMLINK);
			symlinks_followed++;

			/*
			 * Read the link value and copy the tail of our
			 * current path onto the end.
			 */
			if (p)
				strcpy(&path[sb.st_size], p);
			else
				path[sb.st_size] = 0;
			if (sb.st_size + sizeof(znode_phys_t) <= dn.dn_bonuslen) {
				memcpy(path, &dn.dn_bonus[sizeof(znode_phys_t)],
					sb.st_size);
			} else {
				rc = dnode_read(spa, &dn, 0, path, sb.st_size);
				if (rc)
					return (rc);
			}

			/*
			 * Restart with the new path, starting either at
			 * the root or at the parent depending whether or
			 * not the link is relative.
			 */
			p = path;
			if (*p == '/')
				objnum = rootnum;
			else
				objnum = parentnum;
			objset_get_dnode(spa, &spa->spa_root_objset, objnum, &dn);
		}
	}

	*dnode = dn;
	return (0);
}
