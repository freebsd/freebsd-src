/*
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * at_rmx.c,v 1.13 1995/05/30 08:09:31 rgrimes Exp
 */

/* This code generates debugging traces to the radix code */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netatalk/at.h>
#include <netatalk/at_extern.h>

static char hexbuf[256];

char *
prsockaddr(void *v)
{
	char *bp = &hexbuf[0];
	u_char *cp = v;

	if (v) {
		int len = *cp;
		u_char *cplim = cp + len;

		/* return: "(len) hexdump" */

		bp += sprintf(bp, "(%d)", len);
		for (cp++; cp < cplim && bp < hexbuf+252; cp++) {
			*bp++ = "0123456789abcdef"[*cp / 16];
			*bp++ = "0123456789abcdef"[*cp % 16];
		}
	} else {
		bp+= sprintf(bp, "null");
	}
	*bp = '\0';
	
	return &hexbuf[0];
}

static struct radix_node *
at_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
	    struct radix_node *treenodes)
{
	struct radix_node *rn;
	struct sockaddr_at *dst = v_arg, *mask = n_arg;

	printf("at_addroute: v=%s\n", prsockaddr(v_arg));
	printf("at_addroute: n=%s\n", prsockaddr(n_arg));
	printf("at_addroute: head=%x treenodes=%x\n", head, treenodes);

	rn = rn_addroute(v_arg, n_arg, head, treenodes);

	printf("at_addroute: returns rn=%x\n", rn);

	return rn;
}

static struct radix_node *
at_matroute(void *v_arg, struct radix_node_head *head)
{
	struct radix_node *rn;
	struct sockaddr_at *dst = v_arg;

	printf("at_matroute: v=%s\n", prsockaddr(v_arg));
	printf("at_matroute: head=%x\n", head);

	rn = rn_match(v_arg, head);

	printf("at_matroute: returns rn=%x\n", rn);

	return rn;
}

static struct radix_node *
at_lookup(void *v_arg, void *m_arg, struct radix_node_head *head)
{
	struct radix_node *rn;

	printf("at_lookup: v=%s\n", prsockaddr(v_arg));
	printf("at_lookup: n=%s\n", prsockaddr(m_arg));
	printf("at_lookup: head=%x\n", head);

	rn = rn_lookup(v_arg, m_arg, head);

	printf("at_lookup: returns rn=%x\n", rn);

	return rn;
}

static struct radix_node *  
at_delroute(void *v_arg, void *netmask_arg, struct radix_node_head *head)
{
	struct radix_node *rn;

	printf("at_delroute: v=%s\n", prsockaddr(v_arg));
	printf("at_delroute: n=%s\n", prsockaddr(netmask_arg));
	printf("at_delroute: head=%x\n", head);

	rn = rn_delete(v_arg, netmask_arg, head);

	printf("at_delroute: returns rn=%x\n", rn);

	return rn;
}

/*
 * Initialize our routing tree with debugging hooks.
 */
int
at_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if(!rn_inithead(head, off))
		return 0;

	rnh = *head;
	rnh->rnh_addaddr = at_addroute;
	rnh->rnh_deladdr = at_delroute;
	rnh->rnh_matchaddr = at_matroute;
	rnh->rnh_lookup = at_lookup;
	return 1;
}

