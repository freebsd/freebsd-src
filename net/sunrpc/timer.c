#include <linux/version.h>
#include <linux/types.h>
#include <linux/unistd.h>

#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/timer.h>

#define RPC_RTO_MAX (60*HZ)
#define RPC_RTO_INIT (HZ/5)
#define RPC_RTO_MIN (HZ/10)

void
rpc_init_rtt(struct rpc_rtt *rt, long timeo)
{
	long t = (timeo - RPC_RTO_INIT) << 3;
	int i;
	rt->timeo = timeo;
	if (t < 0)
		t = 0;
	for (i = 0; i < 5; i++) {
		rt->srtt[i] = t;
		rt->sdrtt[i] = RPC_RTO_INIT;
	}
	memset(rt->ntimeouts, 0, sizeof(rt->ntimeouts));
}

void
rpc_update_rtt(struct rpc_rtt *rt, int timer, long m)
{
	long *srtt, *sdrtt;

	if (timer-- == 0)
		return;

	if (m == 0)
		m = 1;
	srtt = &rt->srtt[timer];
	m -= *srtt >> 3;
	*srtt += m;
	if (m < 0)
		m = -m;
	sdrtt = &rt->sdrtt[timer];
	m -= *sdrtt >> 2;
	*sdrtt += m;
	/* Set lower bound on the variance */
	if (*sdrtt < RPC_RTO_MIN)
		*sdrtt = RPC_RTO_MIN;
}

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup,
 * read, write, commit     - A+4D
 * other                   - timeo
 */

long
rpc_calc_rto(struct rpc_rtt *rt, int timer)
{
	long res;
	if (timer-- == 0)
		return rt->timeo;
	res = (rt->srtt[timer] >> 3) + rt->sdrtt[timer];
	if (res > RPC_RTO_MAX)
		res = RPC_RTO_MAX;
	return res;
}
