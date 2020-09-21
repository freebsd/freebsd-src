/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/kstat.h>

static MALLOC_DEFINE(M_KSTAT, "kstat_data", "Kernel statistics");

SYSCTL_ROOT_NODE(OID_AUTO, kstat, CTLFLAG_RW, 0, "Kernel statistics");

static int
kstat_default_update(kstat_t *ksp, int rw)
{
	KASSERT(ksp != NULL, ("kstat=%p", ksp));

	if (rw == KSTAT_WRITE)
		return (EACCES);

	return (0);
}

kstat_t *
kstat_create(char *module, int instance, char *name, char *class, uchar_t type,
    ulong_t ndata, uchar_t flags)
{
	struct sysctl_oid *root;
	kstat_t *ksp;
	char *pool;

	KASSERT(instance == 0, ("instance=%d", instance));
	KASSERT(type == KSTAT_TYPE_NAMED, ("type=%hhu", type));
	KASSERT(flags == KSTAT_FLAG_VIRTUAL, ("flags=%02hhx", flags));

	/*
	 * Allocate the main structure. We don't need to copy module/class/name
	 * stuff in here, because it is only used for sysctl node creation
	 * done in this function.
	 */
	ksp = malloc(sizeof(*ksp), M_KSTAT, M_WAITOK);
	ksp->ks_ndata = ndata;
	ksp->ks_update = kstat_default_update;

	/*
	 * Some kstats use a module name like "zfs/poolname" to distinguish a
	 * set of kstats belonging to a specific pool.  Split on '/' to add an
	 * extra node for the pool name if needed.
	 */
	pool = strchr(module, '/');
	if (pool != NULL)
		*pool++ = '\0';

	/*
	 * Create sysctl tree for those statistics:
	 *
	 *	kstat.<module>[.<pool>].<class>.<name>
	 */
	sysctl_ctx_init(&ksp->ks_sysctl_ctx);
	root = SYSCTL_ADD_NODE(&ksp->ks_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_kstat), OID_AUTO, module, CTLFLAG_RW, 0,
	    "");
	if (root == NULL) {
		printf("%s: Cannot create kstat.%s tree!\n", __func__, module);
		sysctl_ctx_free(&ksp->ks_sysctl_ctx);
		free(ksp, M_KSTAT);
		return (NULL);
	}
	if (pool != NULL) {
		root = SYSCTL_ADD_NODE(&ksp->ks_sysctl_ctx,
		    SYSCTL_CHILDREN(root), OID_AUTO, pool, CTLFLAG_RW, 0, "");
		if (root == NULL) {
			printf("%s: Cannot create kstat.%s.%s tree!\n",
			    __func__, module, pool);
			sysctl_ctx_free(&ksp->ks_sysctl_ctx);
			free(ksp, M_KSTAT);
			return (NULL);
		}
	}
	root = SYSCTL_ADD_NODE(&ksp->ks_sysctl_ctx, SYSCTL_CHILDREN(root),
	    OID_AUTO, class, CTLFLAG_RW, 0, "");
	if (root == NULL) {
		if (pool != NULL)
			printf("%s: Cannot create kstat.%s.%s.%s tree!\n",
			    __func__, module, pool, class);
		else
			printf("%s: Cannot create kstat.%s.%s tree!\n",
			    __func__, module, class);
		sysctl_ctx_free(&ksp->ks_sysctl_ctx);
		free(ksp, M_KSTAT);
		return (NULL);
	}
	root = SYSCTL_ADD_NODE(&ksp->ks_sysctl_ctx, SYSCTL_CHILDREN(root),
	    OID_AUTO, name, CTLFLAG_RW, 0, "");
	if (root == NULL) {
		if (pool != NULL)
			printf("%s: Cannot create kstat.%s.%s.%s.%s "
			    "tree!\n", __func__, module, pool, class,
			    name);
		else
			printf("%s: Cannot create kstat.%s.%s.%s "
			    "tree!\n", __func__, module, class, name);
		sysctl_ctx_free(&ksp->ks_sysctl_ctx);
		free(ksp, M_KSTAT);
		return (NULL);
	}
	ksp->ks_sysctl_root = root;

	return (ksp);
}

static int
kstat_sysctl(SYSCTL_HANDLER_ARGS)
{
	kstat_t *ksp = arg1;
	kstat_named_t *ksent = ksp->ks_data;
	uint64_t val;

	/* Select the correct element */
	ksent += arg2;
	/* Update the aggsums before reading */
	(void) ksp->ks_update(ksp, KSTAT_READ);
	val = ksent->value.ui64;

	return sysctl_handle_64(oidp, &val, 0, req);
}

static int
kstat_sysctl_string(SYSCTL_HANDLER_ARGS)
{
	kstat_t *ksp = arg1;
	kstat_named_t *ksent = ksp->ks_data;
	char *val;
	uint32_t len = 0;

	/* Select the correct element */
	ksent += arg2;
	/* Update the aggsums before reading */
	(void) ksp->ks_update(ksp, KSTAT_READ);
	val = KSTAT_NAMED_STR_PTR(ksent);
	len = KSTAT_NAMED_STR_BUFLEN(ksent);
	val[len-1] = '\0';

	return (sysctl_handle_string(oidp, val, len, req));
}

void
kstat_install(kstat_t *ksp)
{
	kstat_named_t *ksent;
	u_int i;

	ksent = ksp->ks_data;
	for (i = 0; i < ksp->ks_ndata; i++, ksent++) {
		if (ksent->data_type == KSTAT_DATA_STRING) {
			SYSCTL_ADD_PROC(&ksp->ks_sysctl_ctx,
			    SYSCTL_CHILDREN(ksp->ks_sysctl_root),
			    OID_AUTO, ksent->name,
			    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    ksp, i, kstat_sysctl_string, "A", ksent->desc);
			continue;
		}
		KASSERT(ksent->data_type == KSTAT_DATA_UINT64,
		    ("data_type=%d", ksent->data_type));
		SYSCTL_ADD_PROC(&ksp->ks_sysctl_ctx,
		    SYSCTL_CHILDREN(ksp->ks_sysctl_root), OID_AUTO, ksent->name,
		    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
		    ksp, i, kstat_sysctl, "QU", ksent->desc);
	}
}

void
kstat_delete(kstat_t *ksp)
{

	sysctl_ctx_free(&ksp->ks_sysctl_ctx);
	free(ksp, M_KSTAT);
}

void
kstat_set_string(char *dst, const char *src)
{

	bzero(dst, KSTAT_STRLEN);
	(void) strncpy(dst, src, KSTAT_STRLEN - 1);
}

void
kstat_named_init(kstat_named_t *knp, const char *name, uchar_t data_type)
{

	kstat_set_string(knp->name, name);
	knp->data_type = data_type;
}
