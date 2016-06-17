#ifndef __BEN_VLAN_PROC_INC__
#define __BEN_VLAN_PROC_INC__

int vlan_proc_init(void);

int vlan_proc_rem_dev(struct net_device *vlandev);
int vlan_proc_add_dev (struct net_device *vlandev);
void vlan_proc_cleanup (void);

#define	VLAN_PROC_BUFSZ	(4096)	/* buffer size for printing proc info */

#endif /* !(__BEN_VLAN_PROC_INC__) */
