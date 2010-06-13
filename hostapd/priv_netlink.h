#ifndef PRIV_NETLINK_H
#define PRIV_NETLINK_H

/* Private copy of needed Linux netlink/rtnetlink definitions.
 *
 * This should be replaced with user space header once one is available with C
 * library, etc..
 */

#ifndef IFLA_IFNAME
#define IFLA_IFNAME 3
#endif
#ifndef IFLA_WIRELESS
#define IFLA_WIRELESS 11
#endif

#define NETLINK_ROUTE 0
#define RTMGRP_LINK 1
#define RTM_BASE 0x10
#define RTM_NEWLINK (RTM_BASE + 0)
#define RTM_DELLINK (RTM_BASE + 1)

#define NLMSG_ALIGNTO 4
#define NLMSG_ALIGN(len) (((len) + NLMSG_ALIGNTO - 1) & ~(NLMSG_ALIGNTO - 1))
#define NLMSG_LENGTH(len) ((len) + NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_DATA(nlh) ((void*) (((char*) nlh) + NLMSG_LENGTH(0)))

#define RTA_ALIGNTO 4
#define RTA_ALIGN(len) (((len) + RTA_ALIGNTO - 1) & ~(RTA_ALIGNTO - 1))
#define RTA_OK(rta,len) \
((len) > 0 && (rta)->rta_len >= sizeof(struct rtattr) && \
(rta)->rta_len <= (len))
#define RTA_NEXT(rta,attrlen) \
((attrlen) -= RTA_ALIGN((rta)->rta_len), \
(struct rtattr *) (((char *)(rta)) + RTA_ALIGN((rta)->rta_len)))


struct sockaddr_nl
{
	sa_family_t nl_family;
	unsigned short nl_pad;
	u32 nl_pid;
	u32 nl_groups;
};

struct nlmsghdr
{
	u32 nlmsg_len;
	u16 nlmsg_type;
	u16 nlmsg_flags;
	u32 nlmsg_seq;
	u32 nlmsg_pid;
};

struct ifinfomsg
{
	unsigned char ifi_family;
	unsigned char __ifi_pad;
	unsigned short ifi_type;
	int ifi_index;
	unsigned ifi_flags;
	unsigned ifi_change;
};

struct rtattr
{
	unsigned short rta_len;
	unsigned short rta_type;
};

#endif /* PRIV_NETLINK_H */
