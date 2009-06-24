/*-
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
 * $FreeBSD$
 */

/* This code generates debugging traces to the radix code. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/route.h>

int	at_inithead(void **head, int off);

#if 0
#define	HEXBUF_LEN	256

static const char *
prsockaddr(void *v, char *hexbuf)
{
	char *bp = &hexbuf[0];
	u_char *cp = v;

	if (v != NULL) {
		int len = *cp;
		u_char *cplim = cp + len;

		/* return: "(len) hexdump" */

		bp += sprintf(bp, "(%d)", len);
		for (cp++; cp < cplim && bp < hexbuf + (HEXBUF_LEN - 4);
		    cp++) {
			*bp++ = "0123456789abcdef"[*cp / 16];
			*bp++ = "0123456789abcdef"[*cp % 16];
		}
	} else
		bp+= sprintf(bp, "null");
	*bp = '\0';
	return (hexbuf);
}
#endif

static struct radix_node *
at_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
	    struct radix_node *treenodes)
{

	return (rn_addroute(v_arg, n_arg, head, treenodes));
}

static struct radix_node *
at_matroute(void *v_arg, struct radix_node_head *head)
{

	return (rn_match(v_arg, head));
}

static struct radix_node *
at_lookup(void *v_arg, void *m_arg, struct radix_node_head *head)
{

	return (rn_lookup(v_arg, m_arg, head));
}

static struct radix_node *
at_delroute(void *v_arg, void *netmask_arg, struct radix_node_head *head)
{

	return (rn_delete(v_arg, netmask_arg, head));
}

/*
 * Initialize our routing tree with debugging hooks.
 */
int
at_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if (!rn_inithead(head, off))
		return (0);

	rnh = *head;
	rnh->rnh_addaddr = at_addroute;
	rnh->rnh_deladdr = at_delroute;
	rnh->rnh_matchaddr = at_matroute;
	rnh->rnh_lookup = at_lookup;
	return (1);
}
