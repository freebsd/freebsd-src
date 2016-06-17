#ifndef _LINUX_SWAPCTL_H
#define _LINUX_SWAPCTL_H

typedef struct pager_daemon_v1
{
	unsigned int	tries_base;
	unsigned int	tries_min;
	unsigned int	swap_cluster;
} pager_daemon_v1;
typedef pager_daemon_v1 pager_daemon_t;
extern pager_daemon_t pager_daemon;

#endif /* _LINUX_SWAPCTL_H */
