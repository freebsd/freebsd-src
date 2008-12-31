/* $FreeBSD: src/sys/cddl/dev/dtrace/dtrace_hacks.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $ */
/* XXX Hacks.... */

dtrace_cacheid_t dtrace_predcache_id;

int panic_quiesce;
char panic_stack[PANICSTKSIZE];

boolean_t
priv_policy_only(const cred_t *a, int b, boolean_t c)
{
	return 0;
}
