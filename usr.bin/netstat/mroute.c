/*
 * Print DVMRP multicast routing structures and statistics.
 *
 * MROUTING 1.0
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#define KERNEL 1
#include <sys/socketvar.h>
#include <netinet/ip_mroute.h>
#undef KERNEL

extern int kmem;
extern int nflag;
extern char *routename();
extern char *netname();
extern char *plural();

char *plurales(n)
	int n;
{
	return (n == 1? "" : "es");
}

mroutepr(mrpaddr, mrtaddr, vifaddr)
	off_t mrpaddr, mrtaddr, vifaddr;
{
	u_int mrtproto;
#if BSD >= 199006
	struct mrt *mrttable[MRTHASHSIZ];
	struct mrt *mp;
	struct mrt mb;
	struct mrt *mrt = &mb;
#else
	struct mbuf *mrttable[MRTHASHSIZ];
	struct mbuf *mp;
	struct mbuf mb;
	struct mrt *mrt = mtod(&mb, struct mrt *);
#endif
	struct vif viftable[MAXVIFS];
	register struct vif *v;
	register vifi_t vifi;
	struct in_addr *grp;
	int i, n;
	int banner_printed;
	int saved_nflag;

	if(mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kvm_read(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	    case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	    case IGMP_DVMRP:
		break;

	    default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mrtaddr == 0) {
		printf("mrttable: symbol not in namelist\n");
		return;
	}
	if (vifaddr == 0) {
		printf("viftable: symbol not in namelist\n");
		return;
	}

	saved_nflag = nflag;
	nflag = 1;

	kvm_read(vifaddr, (char *)viftable, sizeof(viftable));
	banner_printed = 0;
	for (vifi = 0, v = viftable; vifi < MAXVIFS; ++vifi, ++v) {
		struct in_addr v_lcl_grps[1024];

		if (v->v_lcl_addr.s_addr == 0) continue;

		if (!banner_printed) {
			printf("\nVirtual Interface Table\n%s%s",
			       " Vif   Threshold   Local-Address   ",
			       "Remote-Address   Groups\n");
			banner_printed = 1;
		}

		printf(" %2u       %3u      %-15.15s",
			vifi, v->v_threshold, routename(v->v_lcl_addr));
		printf(" %-15.15s\n",
			(v->v_flags & VIFF_TUNNEL) ?
				routename(v->v_rmt_addr) : "");

		n = v->v_lcl_grps_n;
		if (n == 0)
			continue;
		if (n < 0 || n > 1024)
			printf("[v_lcl_grps_n = %d!]\n", n);

		kvm_read(v->v_lcl_grps, (char *)v_lcl_grps, 
			 n * sizeof(struct in_addr));
		for (i = 0; i < n; ++i)
			printf("%51s %-15.15s\n", "",
			       routename(v_lcl_grps[i]));
	}
	if (!banner_printed) printf("\nVirtual Interface Table is empty\n");

	kvm_read(mrtaddr, (char *)mrttable, sizeof(mrttable));
	banner_printed = 0;
	for (i = 0; i < MRTHASHSIZ; ++i) {
	    for (mp = mrttable[i]; mp != NULL;
#if BSD >= 199006
		 mp = mb.mrt_next
#else
		 mp = mb.m_next
#endif
		 ) {

		if (!banner_printed) {
			printf("\nMulticast Routing Table\n%s",
			       " Hash  Origin-Subnet  In-Vif  Out-Vifs\n");
			banner_printed = 1;
		}
		kvm_read(mp, (char *)&mb, sizeof(mb));


		printf(" %3u   %-15.15s  %2u   ",
			i,
			netname(mrt->mrt_origin.s_addr,
				ntohl(mrt->mrt_originmask.s_addr)),
			mrt->mrt_parent);
		for (vifi = 0; vifi < MAXVIFS; ++vifi) {
			if (viftable[vifi].v_lcl_addr.s_addr) {
				if (VIFM_ISSET(vifi, mrt->mrt_children)) {
					printf(" %u%c",
						vifi,
						VIFM_ISSET(vifi,
						  mrt->mrt_leaves) ?
						    '*' : ' ');
				} else
					printf("   ");
			}
		}
		printf("\n");
	    }
	}
	if (!banner_printed) printf("\nMulticast Routing Table is empty\n");

	printf("\n");
	nflag = saved_nflag;
}


mrt_stats(mrpaddr, mstaddr)
	off_t mrpaddr, mstaddr;
{
	u_int mrtproto;
	struct mrtstat mrtstat;

	if(mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kvm_read(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	    case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	    case IGMP_DVMRP:
		break;

	    default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mstaddr == 0) {
		printf("mrtstat: symbol not in namelist\n");
		return;
	}

	kvm_read(mstaddr, (char *)&mrtstat, sizeof(mrtstat));
	printf("multicast routing:\n");
	printf(" %10u multicast route lookup%s\n",
	  mrtstat.mrts_mrt_lookups, plural(mrtstat.mrts_mrt_lookups));
	printf(" %10u multicast route cache miss%s\n",
	  mrtstat.mrts_mrt_misses, plurales(mrtstat.mrts_mrt_misses));
	printf(" %10u group address lookup%s\n",
	  mrtstat.mrts_grp_lookups, plural(mrtstat.mrts_grp_lookups));
	printf(" %10u group address cache miss%s\n",
	  mrtstat.mrts_grp_misses, plurales(mrtstat.mrts_grp_misses));
	printf(" %10u datagram%s with no route for origin\n",
	  mrtstat.mrts_no_route, plural(mrtstat.mrts_no_route));
	printf(" %10u datagram%s with malformed tunnel options\n",
	  mrtstat.mrts_bad_tunnel, plural(mrtstat.mrts_bad_tunnel));
	printf(" %10u datagram%s with no room for tunnel options\n",
	  mrtstat.mrts_cant_tunnel, plural(mrtstat.mrts_cant_tunnel));
	printf(" %10u datagram%s arrived on wrong interface\n",
	  mrtstat.mrts_wrong_if, plural(mrtstat.mrts_wrong_if));
}
