/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

#include <kgssapi/gssapi.h>
#include <kgssapi/gssapi_impl.h>
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/rpcsec_gss.h>

#include "gssd.h"
#include "kgss_if.h"

MALLOC_DEFINE(M_GSSAPI, "GSS-API", "GSS-API");

/*
 * Syscall hooks
 */
static int gssd_syscall_offset = SYS_gssd_syscall;
static struct sysent gssd_syscall_prev_sysent;
MAKE_SYSENT(gssd_syscall);
static bool_t gssd_syscall_registered = FALSE;

struct kgss_mech_list kgss_mechs;
CLIENT *kgss_gssd_handle;

static void
kgss_init(void *dummy)
{
	int error;

	LIST_INIT(&kgss_mechs);
	error = syscall_register(&gssd_syscall_offset, &gssd_syscall_sysent,
	    &gssd_syscall_prev_sysent);
	if (error)
		printf("Can't register GSSD syscall\n");
	else
		gssd_syscall_registered = TRUE;
}
SYSINIT(kgss_init, SI_SUB_LOCK, SI_ORDER_FIRST, kgss_init, NULL);

static void
kgss_uninit(void *dummy)
{

	if (gssd_syscall_registered)
		syscall_deregister(&gssd_syscall_offset,
		    &gssd_syscall_prev_sysent);
}
SYSUNINIT(kgss_uninit, SI_SUB_LOCK, SI_ORDER_FIRST, kgss_uninit, NULL);

int
sys_gssd_syscall(struct thread *td, struct gssd_syscall_args *uap)
{
        struct sockaddr_un sun;
        struct netconfig *nconf;
	char path[MAXPATHLEN];
	int error;
        
	error = priv_check(td, PRIV_NFS_DAEMON);
	if (error)
		return (error);

	if (kgss_gssd_handle)
		CLNT_DESTROY(kgss_gssd_handle);

	error = copyinstr(uap->path, path, sizeof(path), NULL);
	if (error)
		return (error);

        sun.sun_family = AF_LOCAL;
        strcpy(sun.sun_path, path);
        sun.sun_len = SUN_LEN(&sun);
        
        nconf = getnetconfigent("local");
        kgss_gssd_handle = clnt_reconnect_create(nconf,
	    (struct sockaddr *) &sun, GSSD, GSSDVERS,
	    RPC_MAXDATASIZE, RPC_MAXDATASIZE);

	return (0);
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

	if (!kgss_gssd_handle)
		return (GSS_S_FAILURE);

	args.ctx = ctx->handle;
	bzero(&res, sizeof(res));
	stat = gssd_export_sec_context_1(&args, &res, kgss_gssd_handle);
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
 * Kernel module glue
 */
static int
kgssapi_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
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
		break;
	case MOD_UNLOAD:
		/*
		 * Unloading of the kgssapi module is not currently supported.
		 * If somebody wants this, we would need to keep track of
		 * currently executing threads and make sure the count is 0.
		 */
		/* FALLTHROUGH */
	default:
		error = EOPNOTSUPP;
	};
	return (error);
}
static moduledata_t kgssapi_mod = {
	"kgssapi",
	kgssapi_modevent,
	NULL,
};
DECLARE_MODULE(kgssapi, kgssapi_mod, SI_SUB_VFS, SI_ORDER_ANY);
MODULE_DEPEND(kgssapi, krpc, 1, 1, 1);
MODULE_VERSION(kgssapi, 1);
