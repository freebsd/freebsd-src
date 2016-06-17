/* net/atm/atm_misc.c - Various functions for use by ATM drivers */

/* Written 1995-2000 by Werner Almesberger, EPFL ICA */


#include <linux/module.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/skbuff.h>
#include <linux/sonet.h>
#include <linux/bitops.h>
#include <asm/atomic.h>
#include <asm/errno.h>


int atm_charge(struct atm_vcc *vcc,int truesize)
{
	atm_force_charge(vcc,truesize);
	if (atomic_read(&vcc->sk->rmem_alloc) <= vcc->sk->rcvbuf) return 1;
	atm_return(vcc,truesize);
	atomic_inc(&vcc->stats->rx_drop);
	return 0;
}


struct sk_buff *atm_alloc_charge(struct atm_vcc *vcc,int pdu_size,
    int gfp_flags)
{
	int guess = atm_guess_pdu2truesize(pdu_size);

	atm_force_charge(vcc,guess);
	if (atomic_read(&vcc->sk->rmem_alloc) <= vcc->sk->rcvbuf) {
		struct sk_buff *skb = alloc_skb(pdu_size,gfp_flags);

		if (skb) {
			atomic_add(skb->truesize-guess,&vcc->sk->rmem_alloc);
			return skb;
		}
	}
	atm_return(vcc,guess);
	atomic_inc(&vcc->stats->rx_drop);
	return NULL;
}


static int check_ci(struct atm_vcc *vcc,short vpi,int vci)
{
	struct sock *s;
	struct atm_vcc *walk;

	for (s = vcc_sklist; s; s = s->next) {
		walk = s->protinfo.af_atm;
		if (walk->dev != vcc->dev)
			continue;
		if (test_bit(ATM_VF_ADDR,&walk->flags) && walk->vpi == vpi &&
		    walk->vci == vci && ((walk->qos.txtp.traffic_class !=
		    ATM_NONE && vcc->qos.txtp.traffic_class != ATM_NONE) ||
		    (walk->qos.rxtp.traffic_class != ATM_NONE &&
		    vcc->qos.rxtp.traffic_class != ATM_NONE)))
			return -EADDRINUSE;
	}
		/* allow VCCs with same VPI/VCI iff they don't collide on
		   TX/RX (but we may refuse such sharing for other reasons,
		   e.g. if protocol requires to have both channels) */
	return 0;
}


int atm_find_ci(struct atm_vcc *vcc,short *vpi,int *vci)
{
	static short p = 0; /* poor man's per-device cache */
	static int c = 0;
	short old_p;
	int old_c;
	int err;

	read_lock(&vcc_sklist_lock);
	if (*vpi != ATM_VPI_ANY && *vci != ATM_VCI_ANY) {
		err = check_ci(vcc,*vpi,*vci);
		read_unlock(&vcc_sklist_lock);
		return err;
	}
	/* last scan may have left values out of bounds for current device */
	if (*vpi != ATM_VPI_ANY) p = *vpi;
	else if (p >= 1 << vcc->dev->ci_range.vpi_bits) p = 0;
	if (*vci != ATM_VCI_ANY) c = *vci;
	else if (c < ATM_NOT_RSV_VCI || c >= 1 << vcc->dev->ci_range.vci_bits)
			c = ATM_NOT_RSV_VCI;
	old_p = p;
	old_c = c;
	do {
		if (!check_ci(vcc,p,c)) {
			*vpi = p;
			*vci = c;
			read_unlock(&vcc_sklist_lock);
			return 0;
		}
		if (*vci == ATM_VCI_ANY) {
			c++;
			if (c >= 1 << vcc->dev->ci_range.vci_bits)
				c = ATM_NOT_RSV_VCI;
		}
		if ((c == ATM_NOT_RSV_VCI || *vci != ATM_VCI_ANY) &&
		    *vpi == ATM_VPI_ANY) {
			p++;
			if (p >= 1 << vcc->dev->ci_range.vpi_bits) p = 0;
		}
	}
	while (old_p != p || old_c != c);
	read_unlock(&vcc_sklist_lock);
	return -EADDRINUSE;
}


/*
 * atm_pcr_goal returns the positive PCR if it should be rounded up, the
 * negative PCR if it should be rounded down, and zero if the maximum available
 * bandwidth should be used.
 *
 * The rules are as follows (* = maximum, - = absent (0), x = value "x",
 * (x+ = x or next value above x, x- = x or next value below):
 *
 *	min max pcr	result		min max pcr	result
 *	-   -   -	* (UBR only)	x   -   -	x+
 *	-   -   *	*		x   -   *	*
 *	-   -   z	z-		x   -   z	z-
 *	-   *   -	*		x   *   -	x+
 *	-   *   *	*		x   *   *	*
 *	-   *   z	z-		x   *   z	z-
 *	-   y   -	y-		x   y   -	x+
 *	-   y   *	y-		x   y   *	y-
 *	-   y   z	z-		x   y   z	z-
 *
 * All non-error cases can be converted with the following simple set of rules:
 *
 *   if pcr == z then z-
 *   else if min == x && pcr == - then x+
 *     else if max == y then y-
 *	 else *
 */


int atm_pcr_goal(struct atm_trafprm *tp)
{
	if (tp->pcr && tp->pcr != ATM_MAX_PCR) return -tp->pcr;
	if (tp->min_pcr && !tp->pcr) return tp->min_pcr;
	if (tp->max_pcr != ATM_MAX_PCR) return -tp->max_pcr;
	return 0;
}


void sonet_copy_stats(struct k_sonet_stats *from,struct sonet_stats *to)
{
#define __HANDLE_ITEM(i) to->i = atomic_read(&from->i)
	__SONET_ITEMS
#undef __HANDLE_ITEM
}


void sonet_subtract_stats(struct k_sonet_stats *from,struct sonet_stats *to)
{
#define __HANDLE_ITEM(i) atomic_sub(to->i,&from->i)
	__SONET_ITEMS
#undef __HANDLE_ITEM
}


EXPORT_SYMBOL(atm_charge);
EXPORT_SYMBOL(atm_alloc_charge);
EXPORT_SYMBOL(atm_find_ci);
EXPORT_SYMBOL(atm_pcr_goal);
EXPORT_SYMBOL(sonet_copy_stats);
EXPORT_SYMBOL(sonet_subtract_stats);
