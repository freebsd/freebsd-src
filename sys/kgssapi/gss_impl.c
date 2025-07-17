/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>

#include <kgssapi/gssapi.h>
#include <kgssapi/gssapi_impl.h>
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/rpcsec_gss.h>

#include "gssd.h"
#include "kgss_if.h"

MALLOC_DEFINE(M_GSSAPI, "GSS-API", "GSS-API");

struct kgss_mech_list kgss_mechs;
struct mtx kgss_gssd_lock;

KGSS_VNET_DEFINE(CLIENT *, kgss_gssd_handle) = NULL;

static int
kgss_load(void)
{
	CLIENT *cl;

	LIST_INIT(&kgss_mechs);

	cl = client_nl_create("kgss", GSSD, GSSDVERS);
	KASSERT(cl, ("%s: netlink client already exist", __func__));

	/*
	 * The transport default is no retries at all, since there could
	 * be no userland listener to our messages.  We will retry for 5
	 * minutes with 10 second interval.  This will potentially cure hosts
	 * with misconfigured startup, where kernel starts sending GSS queries
	 * before userland had started up the gssd(8) daemon.
	 */
	clnt_control(cl, CLSET_RETRIES, &(int){30});
	clnt_control(cl, CLSET_TIMEOUT, &(struct timeval){.tv_sec = 300});

	/*
	 * We literally wait on gssd(8), let's see that in top(1).
	 */
	clnt_control(cl, CLSET_WAITCHAN, "gssd");

	KGSS_CURVNET_SET_QUIET(KGSS_TD_TO_VNET(curthread));
	mtx_lock(&kgss_gssd_lock);
	KGSS_VNET(kgss_gssd_handle) = cl;
	mtx_unlock(&kgss_gssd_lock);
	KGSS_CURVNET_RESTORE();

	return (0);
}

static void
kgss_unload(void)
{

	KGSS_CURVNET_SET_QUIET(KGSS_TD_TO_VNET(curthread));
	clnt_destroy(KGSS_VNET(kgss_gssd_handle));
	KGSS_CURVNET_RESTORE();
}

int
kgss_oid_equal(const gss_OID oid1, const gss_OID oid2)
{

	if (oid1 == oid2)
		return (1);
	if (!oid1 || !oid2)
		return (0);
	if (oid1->length != oid2->length)
		return (0);
	if (memcmp(oid1->elements, oid2->elements, oid1->length))
		return (0);
	return (1);
}

void
kgss_install_mech(gss_OID mech_type, const char *name, struct kobj_class *cls)
{
	struct kgss_mech *km;

	km = malloc(sizeof(struct kgss_mech), M_GSSAPI, M_WAITOK);
	km->km_mech_type = mech_type;
	km->km_mech_name = name;
	km->km_class = cls;
	LIST_INSERT_HEAD(&kgss_mechs, km, km_link);
}

void
kgss_uninstall_mech(gss_OID mech_type)
{
	struct kgss_mech *km;

	LIST_FOREACH(km, &kgss_mechs, km_link) {
		if (kgss_oid_equal(km->km_mech_type, mech_type)) {
			LIST_REMOVE(km, km_link);
			free(km, M_GSSAPI);
			return;
		}
	}
}

gss_OID
kgss_find_mech_by_name(const char *name)
{
	struct kgss_mech *km;

	LIST_FOREACH(km, &kgss_mechs, km_link) {
		if (!strcmp(km->km_mech_name, name)) {
			return (km->km_mech_type);
		}
	}
	return (GSS_C_NO_OID);
}

const char *
kgss_find_mech_by_oid(const gss_OID oid)
{
	struct kgss_mech *km;

	LIST_FOREACH(km, &kgss_mechs, km_link) {
		if (kgss_oid_equal(km->km_mech_type, oid)) {
			return (km->km_mech_name);
		}
	}
	return (NULL);
}

gss_ctx_id_t
kgss_create_context(gss_OID mech_type)
{
	struct kgss_mech *km;
	gss_ctx_id_t ctx;

	LIST_FOREACH(km, &kgss_mechs, km_link) {
		if (kgss_oid_equal(km->km_mech_type, mech_type))
			break;
	}
	if (!km)
		return (NULL);

	ctx = (gss_ctx_id_t) kobj_create(km->km_class, M_GSSAPI, M_WAITOK);
	KGSS_INIT(ctx);

	return (ctx);
}

void
kgss_delete_context(gss_ctx_id_t ctx, gss_buffer_t output_token)
{

	KGSS_DELETE(ctx, output_token);
	kobj_delete((kobj_t) ctx, M_GSSAPI);
}

OM_uint32
kgss_transfer_context(gss_ctx_id_t ctx)
{
	struct export_sec_context_res res;
	struct export_sec_context_args args;
	enum clnt_stat stat;
	OM_uint32 maj_stat;

	KGSS_CURVNET_SET_QUIET(KGSS_TD_TO_VNET(curthread));
	if (!KGSS_VNET(kgss_gssd_handle)) {
		KGSS_CURVNET_RESTORE();
		return (GSS_S_FAILURE);
	}

	args.ctx = ctx->handle;
	bzero(&res, sizeof(res));
	stat = gssd_export_sec_context_1(&args, &res, KGSS_VNET(kgss_gssd_handle));
	KGSS_CURVNET_RESTORE();
	if (stat != RPC_SUCCESS) {
		return (GSS_S_FAILURE);
	}

	maj_stat = KGSS_IMPORT(ctx, res.format, &res.interprocess_token);
	ctx->handle = 0;

	xdr_free((xdrproc_t) xdr_export_sec_context_res, &res);

	return (maj_stat);
}

void
kgss_copy_buffer(const gss_buffer_t from, gss_buffer_t to)
{
	to->length = from->length;
	if (from->length) {
		to->value = malloc(from->length, M_GSSAPI, M_WAITOK);
		bcopy(from->value, to->value, from->length);
	} else {
		to->value = NULL;
	}
}

/*
 * Acquire the kgss_gssd_handle and return it with a reference count,
 * if it is available.
 */
CLIENT *
kgss_gssd_client(void)
{
	CLIENT *cl;

	KGSS_CURVNET_SET_QUIET(KGSS_TD_TO_VNET(curthread));
	mtx_lock(&kgss_gssd_lock);
	cl = KGSS_VNET(kgss_gssd_handle);
	if (cl != NULL)
		CLNT_ACQUIRE(cl);
	mtx_unlock(&kgss_gssd_lock);
	KGSS_CURVNET_RESTORE();
	return (cl);
}

/*
 * Kernel module glue
 */
static int
kgssapi_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		rpc_gss_entries.rpc_gss_refresh_auth = rpc_gss_refresh_auth;
		rpc_gss_entries.rpc_gss_secfind = rpc_gss_secfind;
		rpc_gss_entries.rpc_gss_secpurge = rpc_gss_secpurge;
		rpc_gss_entries.rpc_gss_seccreate = rpc_gss_seccreate;
		rpc_gss_entries.rpc_gss_set_defaults = rpc_gss_set_defaults;
		rpc_gss_entries.rpc_gss_max_data_length =
		    rpc_gss_max_data_length;
		rpc_gss_entries.rpc_gss_get_error = rpc_gss_get_error;
		rpc_gss_entries.rpc_gss_mech_to_oid = rpc_gss_mech_to_oid;
		rpc_gss_entries.rpc_gss_oid_to_mech = rpc_gss_oid_to_mech;
		rpc_gss_entries.rpc_gss_qop_to_num = rpc_gss_qop_to_num;
		rpc_gss_entries.rpc_gss_get_mechanisms = rpc_gss_get_mechanisms;
		rpc_gss_entries.rpc_gss_get_versions = rpc_gss_get_versions;
		rpc_gss_entries.rpc_gss_is_installed = rpc_gss_is_installed;
		rpc_gss_entries.rpc_gss_set_svc_name = rpc_gss_set_svc_name;
		rpc_gss_entries.rpc_gss_clear_svc_name = rpc_gss_clear_svc_name;
		rpc_gss_entries.rpc_gss_getcred = rpc_gss_getcred;
		rpc_gss_entries.rpc_gss_set_callback = rpc_gss_set_callback;
		rpc_gss_entries.rpc_gss_clear_callback = rpc_gss_clear_callback;
		rpc_gss_entries.rpc_gss_get_principal_name =
		    rpc_gss_get_principal_name;
		rpc_gss_entries.rpc_gss_svc_max_data_length =
		    rpc_gss_svc_max_data_length;
		rpc_gss_entries.rpc_gss_ip_to_srv_principal =
		    rpc_gss_ip_to_srv_principal;
		mtx_init(&kgss_gssd_lock, "kgss_gssd_lock", NULL, MTX_DEF);
		error = kgss_load();
		break;
	case MOD_UNLOAD:
		kgss_unload();
		mtx_destroy(&kgss_gssd_lock);
		/*
		 * Unloading of the kgssapi module is not currently supported.
		 * If somebody wants this, we would need to keep track of
		 * currently executing threads and make sure the count is 0.
		 */
		/* FALLTHROUGH */
	default:
		error = EOPNOTSUPP;
	}
	return (error);
}
static moduledata_t kgssapi_mod = {
	"kgssapi",
	kgssapi_modevent,
	NULL,
};
DECLARE_MODULE(kgssapi, kgssapi_mod, SI_SUB_VFS, SI_ORDER_SECOND);
MODULE_DEPEND(kgssapi, xdr, 1, 1, 1);
MODULE_DEPEND(kgssapi, krpc, 1, 1, 1);
MODULE_VERSION(kgssapi, 1);
