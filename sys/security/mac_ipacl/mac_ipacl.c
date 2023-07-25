/*-
 * Copyright (c) 2003-2004 Networks Associates Technology, Inc.
 * Copyright (c) 2006 SPARTA, Inc.
 * Copyright (c) 2019, 2023 Shivank Garg <shivank@FreeBSD.org>
 *
 * This software was developed for the FreeBSD Project by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract
 * N66001-04-C-6019 ("SEFOS").
 *
 * This code was developed as a Google Summer of Code 2019 project
 * under the guidance of Bjoern A. Zeeb.
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
 *
 * $FreeBSD$
 */

/*
 * The IP address access control policy module - mac_ipacl allows the root of
 * the host to limit the VNET jail's privileges of setting IPv4 and IPv6
 * addresses via sysctl(8) interface. So, the host can define rules for jails
 * and their interfaces about IP addresses.
 * sysctl(8) is to be used to modify the rules string in following format-
 * "jail_id,allow,interface,address_family,IP_addr/prefix_length[@jail_id,...]"
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/jail.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet6/scope6_var.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

static SYSCTL_NODE(_security_mac, OID_AUTO, ipacl, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TrustedBSD mac_ipacl policy controls");

#ifdef INET
static int ipacl_ipv4 = 1;
SYSCTL_INT(_security_mac_ipacl, OID_AUTO, ipv4, CTLFLAG_RWTUN,
    &ipacl_ipv4, 0, "Enforce mac_ipacl for IPv4 addresses");
#endif

#ifdef INET6
static int ipacl_ipv6 = 1;
SYSCTL_INT(_security_mac_ipacl, OID_AUTO, ipv6, CTLFLAG_RWTUN,
    &ipacl_ipv6, 0, "Enforce mac_ipacl for IPv6 addresses");
#endif

static MALLOC_DEFINE(M_IPACL, "ipacl_rule", "Rules for mac_ipacl");

#define	MAC_RULE_STRING_LEN	1024

struct ipacl_addr {
	union {
#ifdef INET
		struct in_addr	ipv4;
#endif
#ifdef INET6
		struct in6_addr	ipv6;
#endif
		u_int8_t	addr8[16];
		u_int16_t	addr16[8];
		u_int32_t	addr32[4];
	} ipa; /* 128 bit address*/
#ifdef INET
#define v4	ipa.ipv4
#endif
#ifdef INET6
#define v6	ipa.ipv6
#endif
#define addr8	ipa.addr8
#define addr16	ipa.addr16
#define addr32	ipa.addr32
};

struct ip_rule {
	int			jid;
	bool			allow;
	bool			subnet_apply; /* Apply rule on whole subnet. */
	char			if_name[IFNAMSIZ];
	int			af; /* Address family. */
	struct	ipacl_addr	addr;
	struct	ipacl_addr	mask;
	TAILQ_ENTRY(ip_rule)	r_entries;
};

static struct mtx			rule_mtx;
static TAILQ_HEAD(rulehead, ip_rule)	rule_head;
static char				rule_string[MAC_RULE_STRING_LEN];

static void
destroy_rules(struct rulehead *head)
{
	struct ip_rule *rule;

	while ((rule = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, rule, r_entries);
		free(rule, M_IPACL);
	}
}

static void
ipacl_init(struct mac_policy_conf *conf)
{
	mtx_init(&rule_mtx, "rule_mtx", NULL, MTX_DEF);
	TAILQ_INIT(&rule_head);
}

static void
ipacl_destroy(struct mac_policy_conf *conf)
{
	mtx_destroy(&rule_mtx);
	destroy_rules(&rule_head);
}

/*
 * Note: parsing routines are destructive on the passed string.
 */
static int
parse_rule_element(char *element, struct ip_rule *rule)
{
	char *tok, *p;
	int prefix;
#ifdef INET6
	int i;
#endif

	/* Should we support a jail wildcard? */
	tok = strsep(&element, ",");
	if (tok == NULL)
		return (EINVAL);
	rule->jid = strtol(tok, &p, 10);
	if (*p != '\0')
		return (EINVAL);
	tok = strsep(&element, ",");
	if (tok == NULL)
		return (EINVAL);
	rule->allow = strtol(tok, &p, 10);
	if (*p != '\0')
		return (EINVAL);
	tok = strsep(&element, ",");
	if (strlen(tok) + 1 > IFNAMSIZ)
		return (EINVAL);
	/* Empty interface name is wildcard to all interfaces. */
	strlcpy(rule->if_name, tok, strlen(tok) + 1);
	tok = strsep(&element, ",");
	if (tok == NULL)
		return (EINVAL);
	rule->af = (strcmp(tok, "AF_INET") == 0) ? AF_INET :
	    (strcmp(tok, "AF_INET6") == 0) ? AF_INET6 : -1;
	if (rule->af == -1)
		return (EINVAL);
	tok = strsep(&element, "/");
	if (tok == NULL)
		return (EINVAL);
	if (inet_pton(rule->af, tok, rule->addr.addr32) != 1)
		return (EINVAL);
	tok = element;
	if (tok == NULL)
		return (EINVAL);
	prefix = strtol(tok, &p, 10);
	if (*p != '\0')
		return (EINVAL);
	/* Value -1 for prefix make policy applicable to individual IP only. */
	if (prefix == -1)
		rule->subnet_apply = false;
	else {
		rule->subnet_apply = true;
		switch (rule->af) {
#ifdef INET
		case AF_INET:
			if (prefix < 0 || prefix > 32)
				return (EINVAL);

			if (prefix == 0)
				rule->mask.addr32[0] = htonl(0);
			else
				rule->mask.addr32[0] =
				    htonl(~((1 << (32 - prefix)) - 1));
			rule->addr.addr32[0] &= rule->mask.addr32[0];
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (prefix < 0 || prefix > 128)
				return (EINVAL);

			for (i = 0; prefix > 0; prefix -= 8, i++)
				rule->mask.addr8[i] = prefix >= 8 ? 0xFF :
				    (u_int8_t)((0xFFU << (8 - prefix)) & 0xFFU);
			for (i = 0; i < 16; i++)
				rule->addr.addr8[i] &= rule->mask.addr8[i];
			break;
#endif
		}
	}
	return (0);
}

/*
 * Format of Rule- jid,allow,interface_name,addr_family,ip_addr/subnet_mask
 * Example: sysctl security.mac.ipacl.rules=1,1,epair0b,AF_INET,192.0.2.2/24
 */
static int
parse_rules(char *string, struct rulehead *head)
{
	struct ip_rule *new;
	char *element;
	int error;

	error = 0;
	while ((element = strsep(&string, "@")) != NULL) {
		if (strlen(element) == 0)
			continue;

		new = malloc(sizeof(*new), M_IPACL, M_ZERO | M_WAITOK);
		error = parse_rule_element(element, new);
		if (error != 0) {
			free(new, M_IPACL);
			goto out;
		}
		TAILQ_INSERT_TAIL(head, new, r_entries);
	}
out:
	if (error != 0)
		destroy_rules(head);
	return (error);
}

static int
sysctl_rules(SYSCTL_HANDLER_ARGS)
{
	char *string, *copy_string, *new_string;
	struct rulehead head, save_head;
	int error;

	new_string = NULL;
	if (req->newptr != NULL) {
		new_string = malloc(MAC_RULE_STRING_LEN, M_IPACL,
		    M_WAITOK | M_ZERO);
		mtx_lock(&rule_mtx);
		strcpy(new_string, rule_string);
		mtx_unlock(&rule_mtx);
		string = new_string;
	} else
		string = rule_string;

	error = sysctl_handle_string(oidp, string, MAC_RULE_STRING_LEN, req);
	if (error)
		goto out;

	if (req->newptr != NULL) {
		copy_string = strdup(string, M_IPACL);
		TAILQ_INIT(&head);
		error = parse_rules(copy_string, &head);
		free(copy_string, M_IPACL);
		if (error)
			goto out;

		TAILQ_INIT(&save_head);
		mtx_lock(&rule_mtx);
		TAILQ_CONCAT(&save_head, &rule_head, r_entries);
		TAILQ_CONCAT(&rule_head, &head, r_entries);
		strcpy(rule_string, string);
		mtx_unlock(&rule_mtx);
		destroy_rules(&save_head);
	}
out:
	if (new_string != NULL)
		free(new_string, M_IPACL);
	return (error);
}
SYSCTL_PROC(_security_mac_ipacl, OID_AUTO, rules,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    0, sysctl_rules, "A", "IP ACL Rules");

static int
rules_check(struct ucred *cred,
    struct ipacl_addr *ip_addr, struct ifnet *ifp)
{
	struct ip_rule *rule;
	int error;
#ifdef INET6
	int i;
	bool same_subnet;
#endif

	error = EPERM;

	mtx_lock(&rule_mtx);

	/*
	 * In the case where multiple rules are applicable to an IP address or
	 * a set of IP addresses, the rule that is defined later in the list
	 * determines the outcome, disregarding any previous rule for that IP
	 * address.
	 * Walk the policy rules list in reverse order until rule applicable
	 * to the requested IP address is found.
	 */
	TAILQ_FOREACH_REVERSE(rule, &rule_head, rulehead, r_entries) {
		/* Skip if current rule applies to different jail. */
		if (cred->cr_prison->pr_id != rule->jid)
			continue;

		if (strcmp(rule->if_name, "\0") &&
		    strcmp(rule->if_name, ifp->if_xname))
			continue;

		switch (rule->af) {
#ifdef INET
		case AF_INET:
			if (rule->subnet_apply) {
				if (rule->addr.v4.s_addr !=
				    (ip_addr->v4.s_addr & rule->mask.v4.s_addr))
					continue;
			} else
				if (ip_addr->v4.s_addr != rule->addr.v4.s_addr)
					continue;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (rule->subnet_apply) {
				same_subnet = true;
				for (i = 0; i < 16; i++)
					if (rule->addr.v6.s6_addr[i] !=
					    (ip_addr->v6.s6_addr[i] &
					    rule->mask.v6.s6_addr[i])) {
						same_subnet = false;
						break;
					}
				if (!same_subnet)
					continue;
			} else
				if (bcmp(&rule->addr, ip_addr,
				    sizeof(*ip_addr)))
					continue;
			break;
#endif
		}

		if (rule->allow)
			error = 0;
		break;
	}

	mtx_unlock(&rule_mtx);

	return (error);
}

/*
 * Feature request: Can we make this sysctl policy apply to jails by default,
 * but also allow it to be changed to apply to the base system?
 */
#ifdef INET
static int
ipacl_ip4_check_jail(struct ucred *cred,
    const struct in_addr *ia, struct ifnet *ifp)
{
	struct ipacl_addr ip4_addr;

	ip4_addr.v4 = *ia;

	if (!jailed(cred))
		return (0);

	/* Checks with the policy only when it is enforced for ipv4. */
	if (ipacl_ipv4)
		return rules_check(cred, &ip4_addr, ifp);

	return (0);
}
#endif

#ifdef INET6
static int
ipacl_ip6_check_jail(struct ucred *cred,
    const struct in6_addr *ia6, struct ifnet *ifp)
{
	struct ipacl_addr ip6_addr;

	ip6_addr.v6 = *ia6; /* Make copy to not alter the original. */
	in6_clearscope(&ip6_addr.v6); /* Clear the scope id. */

	if (!jailed(cred))
		return (0);

	/* Checks with the policy when it is enforced for ipv6. */
	if (ipacl_ipv6)
		return rules_check(cred, &ip6_addr, ifp);

	return (0);
}
#endif

static struct mac_policy_ops ipacl_ops =
{
	.mpo_init = ipacl_init,
	.mpo_destroy = ipacl_destroy,
#ifdef INET
	.mpo_ip4_check_jail = ipacl_ip4_check_jail,
#endif
#ifdef INET6
	.mpo_ip6_check_jail = ipacl_ip6_check_jail,
#endif
};

MAC_POLICY_SET(&ipacl_ops, mac_ipacl, "TrustedBSD MAC/ipacl",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
