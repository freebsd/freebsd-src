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
#include <sys/stat.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gssapi/gssapi.h>
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>

#include "gssd.h"

#ifndef _PATH_GSS_MECH
#define _PATH_GSS_MECH	"/etc/gss/mech"
#endif
#ifndef _PATH_GSSDSOCK
#define _PATH_GSSDSOCK	"/var/run/gssd.sock"
#endif

struct gss_resource {
	LIST_ENTRY(gss_resource) gr_link;
	uint64_t	gr_id;	/* indentifier exported to kernel */
	void*		gr_res;	/* GSS-API resource pointer */
};
LIST_HEAD(gss_resource_list, gss_resource) gss_resources;
int gss_resource_count;
uint32_t gss_next_id;
uint32_t gss_start_time;
int debug_level;

static void gssd_load_mech(void);

extern void gssd_1(struct svc_req *rqstp, SVCXPRT *transp);
extern int gssd_syscall(char *path);

int
main(int argc, char **argv)
{
	/*
	 * We provide an RPC service on a local-domain socket. The
	 * kernel's GSS-API code will pass what it can't handle
	 * directly to us.
	 */
	struct sockaddr_un sun;
	int fd, oldmask, ch, debug;
	SVCXPRT *xprt;

	debug = 0;
	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug_level++;
			break;
		default:
			fprintf(stderr, "usage: %s [-d]\n", argv[0]);
			exit(1);
			break;
		}
	}

	gssd_load_mech();

	if (!debug_level)
		daemon(0, 0);

	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_LOCAL;
	unlink(_PATH_GSSDSOCK);
	strcpy(sun.sun_path, _PATH_GSSDSOCK);
	sun.sun_len = SUN_LEN(&sun);
	fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (!fd) {
		err(1, "Can't create local gssd socket");
	}
	oldmask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *) &sun, sun.sun_len) < 0) {
		err(1, "Can't bind local gssd socket");
	}
	umask(oldmask);
	if (listen(fd, SOMAXCONN) < 0) {
		err(1, "Can't listen on local gssd socket");
	}
	xprt = svc_vc_create(fd, RPC_MAXDATASIZE, RPC_MAXDATASIZE);
	if (!xprt) {
		err(1, "Can't create transport for local gssd socket");
	}
	if (!svc_reg(xprt, GSSD, GSSDVERS, gssd_1, NULL)) {
		err(1, "Can't register service for local gssd socket");
	}

	LIST_INIT(&gss_resources);
	gss_next_id = 1;
	gss_start_time = time(0);

	gssd_syscall(_PATH_GSSDSOCK);
	svc_run();

	return (0);
}

static void
gssd_load_mech(void)
{
	FILE		*fp;
	char		buf[256];
	char		*p;
	char		*name, *oid, *lib, *kobj;

	fp = fopen(_PATH_GSS_MECH, "r");
	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp)) {
		if (*buf == '#')
			continue;
		p = buf;
		name = strsep(&p, "\t\n ");
		if (p) while (isspace(*p)) p++;
		oid = strsep(&p, "\t\n ");
		if (p) while (isspace(*p)) p++;
		lib = strsep(&p, "\t\n ");
		if (p) while (isspace(*p)) p++;
		kobj = strsep(&p, "\t\n ");
		if (!name || !oid || !lib || !kobj)
			continue;

		if (strcmp(kobj, "-")) {
			/*
			 * Attempt to load the kernel module if its
			 * not already present.
			 */
			if (modfind(kobj) < 0) {
				if (kldload(kobj) < 0) {
					fprintf(stderr,
			"%s: can't find or load kernel module %s for %s\n",
					    getprogname(), kobj, name);
				}
			}
		}
	}
	fclose(fp);
}

static void *
gssd_find_resource(uint64_t id)
{
	struct gss_resource *gr;

	if (!id)
		return (NULL);

	LIST_FOREACH(gr, &gss_resources, gr_link)
		if (gr->gr_id == id)
			return (gr->gr_res);

	return (NULL);
}

static uint64_t
gssd_make_resource(void *res)
{
	struct gss_resource *gr;

	if (!res)
		return (0);

	gr = malloc(sizeof(struct gss_resource));
	if (!gr)
		return (0);
	gr->gr_id = (gss_next_id++) + ((uint64_t) gss_start_time << 32);
	gr->gr_res = res;
	LIST_INSERT_HEAD(&gss_resources, gr, gr_link);
	gss_resource_count++;
	if (debug_level > 1)
		printf("%d resources allocated\n", gss_resource_count);

	return (gr->gr_id);
}

static void
gssd_delete_resource(uint64_t id)
{
	struct gss_resource *gr;

	LIST_FOREACH(gr, &gss_resources, gr_link) {
		if (gr->gr_id == id) {
			LIST_REMOVE(gr, gr_link);
			free(gr);
			gss_resource_count--;
			if (debug_level > 1)
				printf("%d resources allocated\n",
				    gss_resource_count);
			return;
		}
	}
}

bool_t
gssd_null_1_svc(void *argp, void *result, struct svc_req *rqstp)
{

	return (TRUE);
}

bool_t
gssd_init_sec_context_1_svc(init_sec_context_args *argp, init_sec_context_res *result, struct svc_req *rqstp)
{
	gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
	gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
	gss_name_t name = GSS_C_NO_NAME;
	char ccname[strlen("FILE:/tmp/krb5cc_") + 6 + 1];

	snprintf(ccname, sizeof(ccname), "FILE:/tmp/krb5cc_%d",
	    (int) argp->uid);
	setenv("KRB5CCNAME", ccname, TRUE);

	memset(result, 0, sizeof(*result));
	if (argp->cred) {
		cred = gssd_find_resource(argp->cred);
		if (!cred) {
			result->major_status = GSS_S_CREDENTIALS_EXPIRED;
			return (TRUE);
		}
	}
	if (argp->ctx) {
		ctx = gssd_find_resource(argp->ctx);
		if (!ctx) {
			result->major_status = GSS_S_CONTEXT_EXPIRED;
			return (TRUE);
		}
	}
	if (argp->name) {
		name = gssd_find_resource(argp->name);
		if (!name) {
			result->major_status = GSS_S_BAD_NAME;
			return (TRUE);
		}
	}

	memset(result, 0, sizeof(*result));
	result->major_status = gss_init_sec_context(&result->minor_status,
	    cred, &ctx, name, argp->mech_type,
	    argp->req_flags, argp->time_req, argp->input_chan_bindings,
	    &argp->input_token, &result->actual_mech_type,
	    &result->output_token, &result->ret_flags, &result->time_rec);

	if (result->major_status == GSS_S_COMPLETE
	    || result->major_status == GSS_S_CONTINUE_NEEDED) {
		if (argp->ctx)
			result->ctx = argp->ctx;
		else
			result->ctx = gssd_make_resource(ctx);
	}

	return (TRUE);
}

bool_t
gssd_accept_sec_context_1_svc(accept_sec_context_args *argp, accept_sec_context_res *result, struct svc_req *rqstp)
{
	gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
	gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
	gss_name_t src_name;
	gss_cred_id_t delegated_cred_handle;

	memset(result, 0, sizeof(*result));
	if (argp->ctx) {
		ctx = gssd_find_resource(argp->ctx);
		if (!ctx) {
			result->major_status = GSS_S_CONTEXT_EXPIRED;
			return (TRUE);
		}
	}
	if (argp->cred) {
		cred = gssd_find_resource(argp->cred);
		if (!cred) {
			result->major_status = GSS_S_CREDENTIALS_EXPIRED;
			return (TRUE);
		}
	}

	memset(result, 0, sizeof(*result));
	result->major_status = gss_accept_sec_context(&result->minor_status,
	    &ctx, cred, &argp->input_token, argp->input_chan_bindings,
	    &src_name, &result->mech_type, &result->output_token,
	    &result->ret_flags, &result->time_rec,
	    &delegated_cred_handle);

	if (result->major_status == GSS_S_COMPLETE
	    || result->major_status == GSS_S_CONTINUE_NEEDED) {
		if (argp->ctx)
			result->ctx = argp->ctx;
		else
			result->ctx = gssd_make_resource(ctx);
		result->src_name = gssd_make_resource(src_name);
		result->delegated_cred_handle =
			gssd_make_resource(delegated_cred_handle);
	}

	return (TRUE);
}

bool_t
gssd_delete_sec_context_1_svc(delete_sec_context_args *argp, delete_sec_context_res *result, struct svc_req *rqstp)
{
	gss_ctx_id_t ctx = gssd_find_resource(argp->ctx);

	if (ctx) {
		result->major_status = gss_delete_sec_context(
			&result->minor_status, &ctx, &result->output_token);
		gssd_delete_resource(argp->ctx);
	} else {
		result->major_status = GSS_S_COMPLETE;
		result->minor_status = 0;
	}

	return (TRUE);
}

bool_t
gssd_export_sec_context_1_svc(export_sec_context_args *argp, export_sec_context_res *result, struct svc_req *rqstp)
{
	gss_ctx_id_t ctx = gssd_find_resource(argp->ctx);

	if (ctx) {
		result->major_status = gss_export_sec_context(
			&result->minor_status, &ctx,
			&result->interprocess_token);
		result->format = KGSS_HEIMDAL_1_1;
		gssd_delete_resource(argp->ctx);
	} else {
		result->major_status = GSS_S_FAILURE;
		result->minor_status = 0;
		result->interprocess_token.length = 0;
		result->interprocess_token.value = NULL;
	}

	return (TRUE);
}

bool_t
gssd_import_name_1_svc(import_name_args *argp, import_name_res *result, struct svc_req *rqstp)
{
	gss_name_t name;

	result->major_status = gss_import_name(&result->minor_status,
	    &argp->input_name_buffer, argp->input_name_type, &name);

	if (result->major_status == GSS_S_COMPLETE)
		result->output_name = gssd_make_resource(name);
	else
		result->output_name = 0;

	return (TRUE);
}

bool_t
gssd_canonicalize_name_1_svc(canonicalize_name_args *argp, canonicalize_name_res *result, struct svc_req *rqstp)
{
	gss_name_t name = gssd_find_resource(argp->input_name);
	gss_name_t output_name;

	memset(result, 0, sizeof(*result));
	if (!name) {
		result->major_status = GSS_S_BAD_NAME;
		return (TRUE);
	}

	result->major_status = gss_canonicalize_name(&result->minor_status,
	    name, argp->mech_type, &output_name);

	if (result->major_status == GSS_S_COMPLETE)
		result->output_name = gssd_make_resource(output_name);
	else
		result->output_name = 0;

	return (TRUE);
}

bool_t
gssd_export_name_1_svc(export_name_args *argp, export_name_res *result, struct svc_req *rqstp)
{
	gss_name_t name = gssd_find_resource(argp->input_name);

	memset(result, 0, sizeof(*result));
	if (!name) {
		result->major_status = GSS_S_BAD_NAME;
		return (TRUE);
	}

	result->major_status = gss_export_name(&result->minor_status,
	    name, &result->exported_name);

	return (TRUE);
}

bool_t
gssd_release_name_1_svc(release_name_args *argp, release_name_res *result, struct svc_req *rqstp)
{
	gss_name_t name = gssd_find_resource(argp->input_name);

	if (name) {
		result->major_status = gss_release_name(&result->minor_status,
		    &name);
		gssd_delete_resource(argp->input_name);
	} else {
		result->major_status = GSS_S_COMPLETE;
		result->minor_status = 0;
	}

	return (TRUE);
}

bool_t
gssd_pname_to_uid_1_svc(pname_to_uid_args *argp, pname_to_uid_res *result, struct svc_req *rqstp)
{
	gss_name_t name = gssd_find_resource(argp->pname);
	uid_t uid;
	char buf[128];
	struct passwd pwd, *pw;

	memset(result, 0, sizeof(*result));
	if (name) {
		result->major_status =
			gss_pname_to_uid(&result->minor_status,
			    name, argp->mech, &uid);
		if (result->major_status == GSS_S_COMPLETE) {
			result->uid = uid;
			getpwuid_r(uid, &pwd, buf, sizeof(buf), &pw);
			if (pw) {
				int len = NGRPS;
				int groups[NGRPS];
				result->gid = pw->pw_gid;
				getgrouplist(pw->pw_name, pw->pw_gid,
				    groups, &len);
				result->gidlist.gidlist_len = len;
				result->gidlist.gidlist_val =
					mem_alloc(len * sizeof(int));
				memcpy(result->gidlist.gidlist_val, groups,
				    len * sizeof(int));
			} else {
				result->gid = 65534;
				result->gidlist.gidlist_len = 0;
				result->gidlist.gidlist_val = NULL;
			}
		}
	} else {
		result->major_status = GSS_S_BAD_NAME;
		result->minor_status = 0;
	}

	return (TRUE);
}

bool_t
gssd_acquire_cred_1_svc(acquire_cred_args *argp, acquire_cred_res *result, struct svc_req *rqstp)
{
	gss_name_t desired_name = GSS_C_NO_NAME;
	gss_cred_id_t cred;
	char ccname[strlen("FILE:/tmp/krb5cc_") + 6 + 1];

	snprintf(ccname, sizeof(ccname), "FILE:/tmp/krb5cc_%d",
	    (int) argp->uid);
	setenv("KRB5CCNAME", ccname, TRUE);

	memset(result, 0, sizeof(*result));
	if (argp->desired_name) {
		desired_name = gssd_find_resource(argp->desired_name);
		if (!desired_name) {
			result->major_status = GSS_S_BAD_NAME;
			return (TRUE);
		}
	}

	result->major_status = gss_acquire_cred(&result->minor_status,
	    desired_name, argp->time_req, argp->desired_mechs,
	    argp->cred_usage, &cred, &result->actual_mechs, &result->time_rec);

	if (result->major_status == GSS_S_COMPLETE)
		result->output_cred = gssd_make_resource(cred);
	else
		result->output_cred = 0;

	return (TRUE);
}

bool_t
gssd_set_cred_option_1_svc(set_cred_option_args *argp, set_cred_option_res *result, struct svc_req *rqstp)
{
	gss_cred_id_t cred = gssd_find_resource(argp->cred);

	memset(result, 0, sizeof(*result));
	if (!cred) {
		result->major_status = GSS_S_CREDENTIALS_EXPIRED;
		return (TRUE);
	}

	result->major_status = gss_set_cred_option(&result->minor_status,
	    &cred, argp->option_name, &argp->option_value);

	return (TRUE);
}

bool_t
gssd_release_cred_1_svc(release_cred_args *argp, release_cred_res *result, struct svc_req *rqstp)
{
	gss_cred_id_t cred = gssd_find_resource(argp->cred);

	if (cred) {
		result->major_status = gss_release_cred(&result->minor_status,
		    &cred);
		gssd_delete_resource(argp->cred);
	} else {
		result->major_status = GSS_S_COMPLETE;
		result->minor_status = 0;
	}

	return (TRUE);
}

bool_t
gssd_display_status_1_svc(display_status_args *argp, display_status_res *result, struct svc_req *rqstp)
{

	result->message_context = argp->message_context;
	result->major_status = gss_display_status(&result->minor_status,
	    argp->status_value, argp->status_type, argp->mech_type,
	    &result->message_context, &result->status_string);

	return (TRUE);
}

int
gssd_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	/*
	 * We don't use XDR to free the results - anything which was
	 * allocated came from GSS-API. We use xdr_result to figure
	 * out what to do.
	 */
	OM_uint32 junk;

	if (xdr_result == (xdrproc_t) xdr_init_sec_context_res) {
		init_sec_context_res *p = (init_sec_context_res *) result;
		gss_release_buffer(&junk, &p->output_token);
	} else if (xdr_result == (xdrproc_t) xdr_accept_sec_context_res) {
		accept_sec_context_res *p = (accept_sec_context_res *) result;
		gss_release_buffer(&junk, &p->output_token);
	} else if (xdr_result == (xdrproc_t) xdr_delete_sec_context_res) {
		delete_sec_context_res *p = (delete_sec_context_res *) result;
		gss_release_buffer(&junk, &p->output_token);
	} else if (xdr_result == (xdrproc_t) xdr_export_sec_context_res) {
		export_sec_context_res *p = (export_sec_context_res *) result;
		if (p->interprocess_token.length)
			memset(p->interprocess_token.value, 0,
			    p->interprocess_token.length);
		gss_release_buffer(&junk, &p->interprocess_token);
	} else if (xdr_result == (xdrproc_t) xdr_export_name_res) {
		export_name_res *p = (export_name_res *) result;
		gss_release_buffer(&junk, &p->exported_name);
	} else if (xdr_result == (xdrproc_t) xdr_acquire_cred_res) {
		acquire_cred_res *p = (acquire_cred_res *) result;
		gss_release_oid_set(&junk, &p->actual_mechs);
	} else if (xdr_result == (xdrproc_t) xdr_pname_to_uid_res) {
		pname_to_uid_res *p = (pname_to_uid_res *) result;
		if (p->gidlist.gidlist_val)
			free(p->gidlist.gidlist_val);
	} else if (xdr_result == (xdrproc_t) xdr_display_status_res) {
		display_status_res *p = (display_status_res *) result;
		gss_release_buffer(&junk, &p->status_string);
	}

	return (TRUE);
}
