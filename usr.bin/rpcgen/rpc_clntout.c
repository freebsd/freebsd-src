/* @(#)rpc_clntout.c	2.1 88/08/01 4.0 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#ifndef lint
/*static char sccsid[] = "from: @(#)rpc_clntout.c 1.2 87/06/24 (C) 1987 SMI";*/
static char rcsid[] = "$Id: rpc_clntout.c,v 1.1 1993/09/13 23:20:12 jtc Exp $";
#endif

/*
 * rpc_clntout.c, Client-stub outputter for the RPC protocol compiler
 * Copyright (C) 1987, Sun Microsytsems, Inc.
 */
#include <stdio.h>
#include <strings.h>
#include "rpc_parse.h"
#include "rpc_util.h"

#define DEFAULT_TIMEOUT 25	/* in seconds */

static int write_program(), printbody();


void
write_stubs()
{
	list *l;
	definition *def;

 	f_print(fout,
 		"\n/* Default timeout can be changed using clnt_control() */\n");
 	f_print(fout, "static struct timeval TIMEOUT = { %d, 0 };\n",
		DEFAULT_TIMEOUT);
	for (l = defined; l != NULL; l = l->next) {
		def = (definition *) l->val;
		if (def->def_kind == DEF_PROGRAM) {
			write_program(def);
		}
	}
}


static
write_program(def)
	definition *def;
{
	version_list *vp;
	proc_list *proc;

	for (vp = def->def.pr.versions; vp != NULL; vp = vp->next) {
		for (proc = vp->procs; proc != NULL; proc = proc->next) {
			f_print(fout, "\n");
			ptype(proc->res_prefix, proc->res_type, 1);
			f_print(fout, "*\n");
			pvname(proc->proc_name, vp->vers_num);
			f_print(fout, "(argp, clnt)\n");
			f_print(fout, "\t");
			ptype(proc->arg_prefix, proc->arg_type, 1);
			f_print(fout, "*argp;\n");
			f_print(fout, "\tCLIENT *clnt;\n");
			f_print(fout, "{\n");
			printbody(proc);
			f_print(fout, "}\n\n");
		}
	}
}

static char *
ampr(type)
	char *type;
{
	if (isvectordef(type, REL_ALIAS)) {
		return ("");
	} else {
		return ("&");
	}
}

static
printbody(proc)
	proc_list *proc;
{
	f_print(fout, "\tstatic ");
	if (streq(proc->res_type, "void")) {
		f_print(fout, "char ");
	} else {
		ptype(proc->res_prefix, proc->res_type, 0);
	}
	f_print(fout, "res;\n");
	f_print(fout, "\n");
 	f_print(fout, "\tbzero((char *)%sres, sizeof(res));\n",
 		ampr(proc->res_type));
	f_print(fout,
		"\tif (clnt_call(clnt, %s, xdr_%s, argp, xdr_%s, %sres, TIMEOUT) != RPC_SUCCESS) {\n",
		proc->proc_name, stringfix(proc->arg_type),
		stringfix(proc->res_type), ampr(proc->res_type));
	f_print(fout, "\t\treturn (NULL);\n");
	f_print(fout, "\t}\n");
	if (streq(proc->res_type, "void")) {
		f_print(fout, "\treturn ((void *)%sres);\n",
			ampr(proc->res_type));
	} else {
		f_print(fout, "\treturn (%sres);\n", ampr(proc->res_type));
	}
}
