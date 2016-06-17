/* Minor modifications to fit on compatibility framework:
   Rusty.Russell@rustcorp.com.au
*/

#ifndef __LINUX_FIREWALL_H
#define __LINUX_FIREWALL_H

/*
 *	Definitions for loadable firewall modules
 */

#define FW_QUEUE	0
#define FW_BLOCK	1
#define FW_ACCEPT	2
#define FW_REJECT	(-1)
#define FW_REDIRECT	3
#define FW_MASQUERADE	4
#define FW_SKIP		5

struct firewall_ops
{
	struct firewall_ops *next;
	int (*fw_forward)(struct firewall_ops *this, int pf,
			  struct net_device *dev, void *phdr, void *arg,
			  struct sk_buff **pskb);
	int (*fw_input)(struct firewall_ops *this, int pf,
			struct net_device *dev, void *phdr, void *arg,
			struct sk_buff **pskb);
	int (*fw_output)(struct firewall_ops *this, int pf,
			 struct net_device *dev, void *phdr, void *arg,
			 struct sk_buff **pskb);
	/* These may be NULL. */
	int (*fw_acct_in)(struct firewall_ops *this, int pf,
			  struct net_device *dev, void *phdr, void *arg,
			  struct sk_buff **pskb);
	int (*fw_acct_out)(struct firewall_ops *this, int pf,
			   struct net_device *dev, void *phdr, void *arg,
			   struct sk_buff **pskb);
};

extern int register_firewall(int pf, struct firewall_ops *fw);
extern int unregister_firewall(int pf, struct firewall_ops *fw);

extern int ip_fw_masq_timeouts(void *user, int len);
#endif /* __LINUX_FIREWALL_H */
