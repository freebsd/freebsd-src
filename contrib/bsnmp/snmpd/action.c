/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Begemot: bsnmp/snmpd/action.c,v 1.58 2004/08/06 08:47:09 brandt Exp $
 *
 * Variable access for SNMPd
 */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>

#include "snmpmod.h"
#include "snmpd.h"
#include "tree.h"
#include "oid.h"

static const struct asn_oid
	oid_begemotSnmpdModuleTable = OIDX_begemotSnmpdModuleTable;

/*
 * Get a string value from the KERN sysctl subtree.
 */
static char *
act_getkernstring(int id)
{
	int mib[2];
	size_t len;
	char *string;

	mib[0] = CTL_KERN;
	mib[1] = id;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) != 0)
		return (NULL);
	if ((string = malloc(len)) == NULL)
		return (NULL);
	if (sysctl(mib, 2, string, &len, NULL, 0) != 0) {
		free(string);
		return (NULL);
	}
	return (string);
}

/*
 * Get an integer value from the KERN sysctl subtree.
 */
static char *
act_getkernint(int id)
{
	int mib[2];
	size_t len;
	u_long value;
	char *string;

	mib[0] = CTL_KERN;
	mib[1] = id;
	len = sizeof(value);
	if (sysctl(mib, 2, &value, &len, NULL, 0) != 0)
		return (NULL);

	if ((string = malloc(20)) == NULL)
		return (NULL);
	sprintf(string, "%lu", value);
	return (string);
}

/*
 * Initialize global variables of the system group.
 */
int
init_actvals(void)
{
	char *v[4];
	u_int i;
	size_t len;

	if ((systemg.name = act_getkernstring(KERN_HOSTNAME)) == NULL)
		return (-1);

	for (i = 0; i < 4; i++)
		v[1] = NULL;

	if ((v[0] = act_getkernstring(KERN_HOSTNAME)) == NULL)
		goto err;
	if ((v[1] = act_getkernint(KERN_HOSTID)) == NULL)
		goto err;
	if ((v[2] = act_getkernstring(KERN_OSTYPE)) == NULL)
		goto err;
	if ((v[3] = act_getkernstring(KERN_OSRELEASE)) == NULL)
		goto err;

	for (i = 0, len = 0; i < 4; i++)
		len += strlen(v[i]) + 1;

	if ((systemg.descr = malloc(len)) == NULL)
		goto err;
	sprintf(systemg.descr, "%s %s %s %s", v[0], v[1], v[2], v[3]);

	return (0);

  err:
	for (i = 0; i < 4; i++)
		if (v[i] != NULL)
			free(v[i]);
	return (-1);
}



/*************************************************************
 *
 * System group
 */
int
op_system_group(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];

	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		break;

	  case SNMP_OP_SET:
		switch (which) {

		  case LEAF_sysDescr:
			if (community != COMM_INITIALIZE)
				return (SNMP_ERR_NOT_WRITEABLE);
			return (string_save(value, ctx, -1, &systemg.descr));

		  case LEAF_sysObjectId:
			if (community != COMM_INITIALIZE)
				return (SNMP_ERR_NOT_WRITEABLE);
			return (oid_save(value, ctx, &systemg.object_id));

		  case LEAF_sysContact:
			return (string_save(value, ctx, -1, &systemg.contact));

		  case LEAF_sysName:
			return (string_save(value, ctx, -1, &systemg.name));

		  case LEAF_sysLocation:
			return (string_save(value, ctx, -1, &systemg.location));
		}
		return (SNMP_ERR_NO_CREATION);

	  case SNMP_OP_ROLLBACK:
		switch (which) {

		  case LEAF_sysDescr:
			string_rollback(ctx, &systemg.descr);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysObjectId:
			oid_rollback(ctx, &systemg.object_id);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysContact:
			string_rollback(ctx, &systemg.contact);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysName:
			string_rollback(ctx, &systemg.name);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysLocation:
			string_rollback(ctx, &systemg.location);
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_COMMIT:
		switch (which) {

		  case LEAF_sysDescr:
			string_commit(ctx);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysObjectId:
			oid_commit(ctx);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysContact:
			string_commit(ctx);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysName:
			string_commit(ctx);
			return (SNMP_ERR_NOERROR);
		  case LEAF_sysLocation:
			string_commit(ctx);
			return (SNMP_ERR_NOERROR);
		}
		abort();
	}

	/*
	 * Come here for GET.
	 */
	switch (which) {

	  case LEAF_sysDescr:
		return (string_get(value, systemg.descr, -1));
	  case LEAF_sysObjectId:
		return (oid_get(value, &systemg.object_id));
	  case LEAF_sysUpTime:
		value->v.uint32 = get_ticks() - start_tick;
		break;
	  case LEAF_sysContact:
		return (string_get(value, systemg.contact, -1));
	  case LEAF_sysName:
		return (string_get(value, systemg.name, -1));
	  case LEAF_sysLocation:
		return (string_get(value, systemg.location, -1));
	  case LEAF_sysServices:
		value->v.integer = systemg.services;
		break;
	  case LEAF_sysORLastChange:
		value->v.uint32 = systemg.or_last_change;
		break;
	}
	return (SNMP_ERR_NOERROR);
}

/*************************************************************
 *
 * Debug group
 */
int
op_debug(struct snmp_context *ctx, struct snmp_value *value, u_int sub,
    u_int iidx __unused, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];

	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		switch (which) {

		  case LEAF_begemotSnmpdDebugDumpPdus:
			value->v.integer = TRUTH_MK(debug.dump_pdus);
			break;

		  case LEAF_begemotSnmpdDebugSnmpTrace:
			value->v.uint32 = snmp_trace;
			break;

		  case LEAF_begemotSnmpdDebugSyslogPri:
			value->v.integer = debug.logpri;
			break;
		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_SET:
		switch (which) {

		  case LEAF_begemotSnmpdDebugDumpPdus:
			if (!TRUTH_OK(value->v.integer))
				return (SNMP_ERR_WRONG_VALUE);
			ctx->scratch->int1 = debug.dump_pdus;
			debug.dump_pdus = TRUTH_GET(value->v.integer);
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdDebugSnmpTrace:
			ctx->scratch->int1 = snmp_trace;
			snmp_trace = value->v.uint32;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdDebugSyslogPri:
			if (value->v.integer < 0 || value->v.integer > 8)
				return (SNMP_ERR_WRONG_VALUE);
			ctx->scratch->int1 = debug.logpri;
			debug.logpri = (u_int)value->v.integer;
			return (SNMP_ERR_NOERROR);
		}
		return (SNMP_ERR_NO_CREATION);

	  case SNMP_OP_ROLLBACK:
		switch (which) {

		  case LEAF_begemotSnmpdDebugDumpPdus:
			debug.dump_pdus = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdDebugSnmpTrace:
			snmp_trace = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdDebugSyslogPri:
			debug.logpri = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_COMMIT:
		switch (which) {

		  case LEAF_begemotSnmpdDebugDumpPdus:
		  case LEAF_begemotSnmpdDebugSnmpTrace:
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdDebugSyslogPri:
			if (debug.logpri == 0)
				setlogmask(0);
			else
				setlogmask(LOG_UPTO(debug.logpri - 1));
			return (SNMP_ERR_NOERROR);
		}
		abort();
	}
	abort();
}

/*************************************************************
 *
 * OR Table
 */
int
op_or_table(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	struct objres *objres;

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((objres = NEXT_OBJECT_INT(&objres_list, &value->var, sub))
		    == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		value->var.subs[sub] = objres->index;
		value->var.len = sub + 1;
		break;

	  case SNMP_OP_GET:
		if ((objres = FIND_OBJECT_INT(&objres_list, &value->var, sub))
		    == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		if ((objres = FIND_OBJECT_INT(&objres_list, &value->var, sub))
		    == NULL)
			return (SNMP_ERR_NO_CREATION);
		return (SNMP_ERR_NOT_WRITEABLE);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
	  default:
		abort();
	}

	/*
	 * Come here for GET, GETNEXT.
	 */
	switch (value->var.subs[sub - 1]) {

	  case LEAF_sysORID:
		value->v.oid = objres->oid;
		break;

	  case LEAF_sysORDescr:
		return (string_get(value, objres->descr, -1));

	  case LEAF_sysORUpTime:
		value->v.uint32 = objres->uptime;
		break;
	}
	return (SNMP_ERR_NOERROR);
}

/*************************************************************
 *
 * mib-2 snmp
 */
int
op_snmp(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		switch (value->var.subs[sub - 1]) {

		  case LEAF_snmpInPkts:
			value->v.uint32 = snmpd_stats.inPkts;
			break;

		  case LEAF_snmpInBadVersions:
			value->v.uint32 = snmpd_stats.inBadVersions;
			break;

		  case LEAF_snmpInBadCommunityNames:
			value->v.uint32 = snmpd_stats.inBadCommunityNames;
			break;

		  case LEAF_snmpInBadCommunityUses:
			value->v.uint32 = snmpd_stats.inBadCommunityUses;
			break;

		  case LEAF_snmpInASNParseErrs:
			value->v.uint32 = snmpd_stats.inASNParseErrs;
			break;

		  case LEAF_snmpEnableAuthenTraps:
			value->v.integer = TRUTH_MK(snmpd.auth_traps);
			break;

		  case LEAF_snmpSilentDrops:
			value->v.uint32 = snmpd_stats.silentDrops;
			break;

		  case LEAF_snmpProxyDrops:
			value->v.uint32 = snmpd_stats.proxyDrops;
			break;

		  default:
			return (SNMP_ERR_NOSUCHNAME);

		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_SET:
		switch (value->var.subs[sub - 1]) {
		  case LEAF_snmpEnableAuthenTraps:
			if (!TRUTH_OK(value->v.integer))
				return (SNMP_ERR_WRONG_VALUE);
			ctx->scratch->int1 = value->v.integer;
			snmpd.auth_traps = TRUTH_GET(value->v.integer);
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_ROLLBACK:
		switch (value->var.subs[sub - 1]) {
		  case LEAF_snmpEnableAuthenTraps:
			snmpd.auth_traps = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_COMMIT:
		switch (value->var.subs[sub - 1]) {
		  case LEAF_snmpEnableAuthenTraps:
			return (SNMP_ERR_NOERROR);
		}
		abort();
	}
	abort();
}

/*************************************************************
 *
 * SNMPd statistics group
 */
int
op_snmpd_stats(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	switch (op) {

	  case SNMP_OP_GET:
		switch (value->var.subs[sub - 1]) {

		  case LEAF_begemotSnmpdStatsNoRxBufs:
			value->v.uint32 = snmpd_stats.noRxbuf;
			break;

		  case LEAF_begemotSnmpdStatsNoTxBufs:
			value->v.uint32 = snmpd_stats.noTxbuf;
			break;

		  case LEAF_begemotSnmpdStatsInTooLongPkts:
			value->v.uint32 = snmpd_stats.inTooLong;
			break;

		  case LEAF_begemotSnmpdStatsInBadPduTypes:
			value->v.uint32 = snmpd_stats.inBadPduTypes;
			break;

		  default:
			return (SNMP_ERR_NOSUCHNAME);
		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_SET:
	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
	  case SNMP_OP_GETNEXT:
		abort();
	}
	abort();
}

/*
 * SNMPd configuration scalars
 */
int
op_snmpd_config(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];

	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		switch (which) {

		  case LEAF_begemotSnmpdTransmitBuffer:
			value->v.integer = snmpd.txbuf;
			break;
		  case LEAF_begemotSnmpdReceiveBuffer:
			value->v.integer = snmpd.rxbuf;
			break;
		  case LEAF_begemotSnmpdCommunityDisable:
			value->v.integer = TRUTH_MK(snmpd.comm_dis);
			break;
		  case LEAF_begemotSnmpdTrap1Addr:
			return (ip_get(value, snmpd.trap1addr));
		  case LEAF_begemotSnmpdVersionEnable:
			value->v.uint32 = snmpd.version_enable;
			break;
		  default:
			return (SNMP_ERR_NOSUCHNAME);
		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_SET:
		switch (which) {

		  case LEAF_begemotSnmpdTransmitBuffer:
			ctx->scratch->int1 = snmpd.txbuf;
			if (value->v.integer < 484 ||
			    value->v.integer > 65535)
				return (SNMP_ERR_WRONG_VALUE);
			snmpd.txbuf = value->v.integer;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdReceiveBuffer:
			ctx->scratch->int1 = snmpd.rxbuf;
			if (value->v.integer < 484 ||
			    value->v.integer > 65535)
				return (SNMP_ERR_WRONG_VALUE);
			snmpd.rxbuf = value->v.integer;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdCommunityDisable:
			ctx->scratch->int1 = snmpd.comm_dis;
			if (!TRUTH_OK(value->v.integer))
				return (SNMP_ERR_WRONG_VALUE);
			if (TRUTH_GET(value->v.integer)) {
				snmpd.comm_dis = 1;
			} else {
				if (snmpd.comm_dis)
					return (SNMP_ERR_WRONG_VALUE);
			}
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotSnmpdTrap1Addr:
			return (ip_save(value, ctx, snmpd.trap1addr));

		  case LEAF_begemotSnmpdVersionEnable:
			if (community != COMM_INITIALIZE)
				return (SNMP_ERR_NOT_WRITEABLE);
			ctx->scratch->int1 = snmpd.version_enable;
			if (value->v.uint32 == 0 ||
			    (value->v.uint32 & ~VERS_ENABLE_ALL))
				return (SNMP_ERR_WRONG_VALUE);
			snmpd.version_enable = value->v.uint32;
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_ROLLBACK:
		switch (which) {

		  case LEAF_begemotSnmpdTransmitBuffer:
			snmpd.rxbuf = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdReceiveBuffer:
			snmpd.txbuf = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdCommunityDisable:
			snmpd.comm_dis = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdTrap1Addr:
			ip_rollback(ctx, snmpd.trap1addr);
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdVersionEnable:
			snmpd.version_enable = ctx->scratch->int1;
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_COMMIT:
		switch (which) {

		  case LEAF_begemotSnmpdTransmitBuffer:
		  case LEAF_begemotSnmpdReceiveBuffer:
		  case LEAF_begemotSnmpdCommunityDisable:
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdTrap1Addr:
			ip_commit(ctx);
			return (SNMP_ERR_NOERROR);
		  case LEAF_begemotSnmpdVersionEnable:
			return (SNMP_ERR_NOERROR);
		}
		abort();
	}
	abort();
}

/*
 * The community table
 */
int
op_community(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];
	struct community *c;

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((community != COMM_INITIALIZE && snmpd.comm_dis) ||
		    (c = NEXT_OBJECT_OID(&community_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &c->index);
		break;

	  case SNMP_OP_GET:
		if ((community != COMM_INITIALIZE && snmpd.comm_dis) ||
		    (c = FIND_OBJECT_OID(&community_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		if ((community != COMM_INITIALIZE && snmpd.comm_dis) ||
		    (c = FIND_OBJECT_OID(&community_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NO_CREATION);
		if (which != LEAF_begemotSnmpdCommunityString)
			return (SNMP_ERR_NOT_WRITEABLE);
		return (string_save(value, ctx, -1, &c->string));

	  case SNMP_OP_ROLLBACK:
		if (which == LEAF_begemotSnmpdCommunityString) {
			if ((c = FIND_OBJECT_OID(&community_list, &value->var,
			    sub)) == NULL)
				string_free(ctx);
			else
				string_rollback(ctx, &c->string);
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  case SNMP_OP_COMMIT:
		if (which == LEAF_begemotSnmpdCommunityString) {
			if ((c = FIND_OBJECT_OID(&community_list, &value->var,
			    sub)) == NULL)
				string_free(ctx);
			else
				string_commit(ctx);
			return (SNMP_ERR_NOERROR);
		}
		abort();

	  default:
		abort();
	}

	switch (which) {

	  case LEAF_begemotSnmpdCommunityString:
		return (string_get(value, c->string, -1));

	  case LEAF_begemotSnmpdCommunityDescr:
		return (string_get(value, c->descr, -1));
	}
	abort();
}

/*
 * Module table.
 */
struct module_dep {
	struct snmp_dependency dep;
	u_char	section[LM_SECTION_MAX + 1];
	u_char	*path;
	struct lmodule *m;
};

static int
dep_modules(struct snmp_context *ctx, struct snmp_dependency *dep,
    enum snmp_depop op)
{
	struct module_dep *mdep = (struct module_dep *)(void *)dep;

	switch (op) {

	  case SNMP_DEPOP_COMMIT:
		if (mdep->path == NULL) {
			/* unload - find the module */
			TAILQ_FOREACH(mdep->m, &lmodules, link)
				if (strcmp(mdep->m->section,
				    mdep->section) == 0)
					break;
			if (mdep->m == NULL)
				/* no such module - that's ok */
				return (SNMP_ERR_NOERROR);

			/* handle unloading in the finalizer */
			return (SNMP_ERR_NOERROR);
		}
		/* load */
		if ((mdep->m = lm_load(mdep->path, mdep->section)) == NULL) {
			/* could not load */
			return (SNMP_ERR_RES_UNAVAIL);
		}
		/* start in finalizer */
		return (SNMP_ERR_NOERROR);

	  case SNMP_DEPOP_ROLLBACK:
		if (mdep->path == NULL) {
			/* rollback unload - the finalizer takes care */
			return (SNMP_ERR_NOERROR);
		}
		/* rollback load */
		lm_unload(mdep->m);
		return (SNMP_ERR_NOERROR);

	  case SNMP_DEPOP_FINISH:
		if (mdep->path == NULL) {
			if (mdep->m != NULL && ctx->code == SNMP_RET_OK)
				lm_unload(mdep->m);
		} else {
			if (mdep->m != NULL && ctx->code == SNMP_RET_OK &&
			    community != COMM_INITIALIZE)
				lm_start(mdep->m);
			free(mdep->path);
		}
		return (SNMP_ERR_NOERROR);
	}
	abort();
}

int
op_modules(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];
	struct lmodule *m;
	u_char *section, *ptr;
	size_t seclen;
	struct module_dep *mdep;
	struct asn_oid idx;

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((m = NEXT_OBJECT_OID(&lmodules, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &m->index);
		break;

	  case SNMP_OP_GET:
		if ((m = FIND_OBJECT_OID(&lmodules, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		m = FIND_OBJECT_OID(&lmodules, &value->var, sub);
		if (which != LEAF_begemotSnmpdModulePath) {
			if (m == NULL)
				return (SNMP_ERR_NO_CREATION);
			return (SNMP_ERR_NOT_WRITEABLE);
		}

		/* the errors in the next few statements can only happen when
		 * m is NULL, hence the NO_CREATION error. */
		if (index_decode(&value->var, sub, iidx,
		    &section, &seclen))
			return (SNMP_ERR_NO_CREATION);

		/* check the section name */
		if (seclen > LM_SECTION_MAX || seclen == 0) {
			free(section);
			return (SNMP_ERR_NO_CREATION);
		}
		for (ptr = section; ptr < section + seclen; ptr++)
			if (!isascii(*ptr) || !isalnum(*ptr)) {
				free(section);
				return (SNMP_ERR_NO_CREATION);
			}
		if (!isalpha(section[0])) {
			free(section);
			return (SNMP_ERR_NO_CREATION);
		}

		/* check the path */
		for (ptr = value->v.octetstring.octets;
		     ptr < value->v.octetstring.octets + value->v.octetstring.len;
		     ptr++) {
			if (*ptr == '\0') {
				free(section);
				return (SNMP_ERR_WRONG_VALUE);
			}
		}

		if (m == NULL) {
			if (value->v.octetstring.len == 0) {
				free(section);
				return (SNMP_ERR_INCONS_VALUE);
			}
		} else {
			if (value->v.octetstring.len != 0) {
				free(section);
				return (SNMP_ERR_INCONS_VALUE);
			}
		}

		asn_slice_oid(&idx, &value->var, sub, value->var.len);

		/* so far, so good */
		mdep = (struct module_dep *)(void *)snmp_dep_lookup(ctx,
		    &oid_begemotSnmpdModuleTable, &idx,
		    sizeof(*mdep), dep_modules);
		if (mdep == NULL) {
			free(section);
			return (SNMP_ERR_RES_UNAVAIL);
		}

		if (mdep->section[0] != '\0') {
			/* two writes to the same entry - bad */
			free(section);
			return (SNMP_ERR_INCONS_VALUE);
		}

		strncpy(mdep->section, section, seclen);
		mdep->section[seclen] = '\0';
		free(section);

		if (value->v.octetstring.len == 0)
			mdep->path = NULL;
		else {
			if ((mdep->path = malloc(value->v.octetstring.len + 1)) == NULL)
				return (SNMP_ERR_RES_UNAVAIL);
			strncpy(mdep->path, value->v.octetstring.octets,
			    value->v.octetstring.len);
			mdep->path[value->v.octetstring.len] = '\0';
		}
		ctx->scratch->ptr1 = mdep;
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		return (SNMP_ERR_NOERROR);

	  default:
		abort();
	}

	switch (which) {

	  case LEAF_begemotSnmpdModulePath:
		return (string_get(value, m->path, -1));

	  case LEAF_begemotSnmpdModuleComment:
		return (string_get(value, m->config->comment, -1));
	}
	abort();
}

int
op_snmp_set(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	switch (op) {

	  case SNMP_OP_GETNEXT:
		abort();

	  case SNMP_OP_GET:
		switch (value->var.subs[sub - 1]) {

		  case LEAF_snmpSetSerialNo:
			value->v.integer = snmp_serial_no;
			break;

		  default:
			abort();
		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_SET:
		switch (value->var.subs[sub - 1]) {

		  case LEAF_snmpSetSerialNo:
			if (value->v.integer != snmp_serial_no)
				return (SNMP_ERR_INCONS_VALUE);
			break;

		  default:
			abort();
		}
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_ROLLBACK:
		return (SNMP_ERR_NOERROR);

	  case SNMP_OP_COMMIT:
		if (snmp_serial_no++ == 2147483647)
			snmp_serial_no = 0;
		return (SNMP_ERR_NOERROR);
	}
	abort();
}

/*
 * Transport table
 */
int
op_transport_table(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx, enum snmp_op op)
{
	asn_subid_t which = value->var.subs[sub - 1];
	struct transport *t;
	u_char *tname, *ptr;
	size_t tnamelen;

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((t = NEXT_OBJECT_OID(&transport_list, &value->var, sub))
		    == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &t->index);
		break;

	  case SNMP_OP_GET:
		if ((t = FIND_OBJECT_OID(&transport_list, &value->var, sub))
		    == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		t = FIND_OBJECT_OID(&transport_list, &value->var, sub);
		if (which != LEAF_begemotSnmpdTransportStatus) {
			if (t == NULL)
				return (SNMP_ERR_NO_CREATION);
			return (SNMP_ERR_NOT_WRITEABLE);
		}

		/* the errors in the next few statements can only happen when
		 * t is NULL, hence the NO_CREATION error. */
		if (index_decode(&value->var, sub, iidx,
		    &tname, &tnamelen))
			return (SNMP_ERR_NO_CREATION);

		/* check the section name */
		if (tnamelen >= TRANS_NAMELEN || tnamelen == 0) {
			free(tname);
			return (SNMP_ERR_NO_CREATION);
		}
		for (ptr = tname; ptr < tname + tnamelen; ptr++) {
			if (!isascii(*ptr) || !isalnum(*ptr)) {
				free(tname);
				return (SNMP_ERR_NO_CREATION);
			}
		}

		/* for now */
		return (SNMP_ERR_NOT_WRITEABLE);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		return (SNMP_ERR_NOERROR);
	  default:
		abort();
	}

	switch (which) {

	    case LEAF_begemotSnmpdTransportStatus:
		value->v.integer = 1;
		break;

	    case LEAF_begemotSnmpdTransportOid:
		memcpy(&value->v.oid, &t->vtab->id, sizeof(t->vtab->id));
		break;
	}
	return (SNMP_ERR_NOERROR);
}
