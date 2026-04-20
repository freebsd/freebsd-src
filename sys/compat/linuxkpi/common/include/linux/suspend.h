/* Public domain. */

#ifndef _LINUXKPI_LINUX_SUSPEND_H_
#define _LINUXKPI_LINUX_SUSPEND_H_

#include <linux/swap.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/mm.h>

typedef int suspend_state_t;

extern suspend_state_t pm_suspend_target_state;

#define	PM_SUSPEND_ON		0
#define	PM_SUSPEND_TO_IDLE	1
#define	PM_SUSPEND_STANDBY	2
#define	PM_SUSPEND_MEM		3
#define	PM_SUSPEND_MIN		PM_SUSPEND_TO_IDLE
#define	PM_SUSPEND_MAX		4

#define	PM_HIBERNATION_PREPARE	0x0001
#define	PM_POST_HIBERNATION	0x0002
#define	PM_SUSPEND_PREPARE	0x0003
#define	PM_POST_SUSPEND		0x0004
#define	PM_RESTORE_PREPARE	0x0005
#define	PM_POST_RESTORE		0x0006

static inline int
pm_suspend_via_firmware(void)
{
	return (0);
}

static inline int
register_pm_notifier(struct notifier_block *nb)
{
	return (0);
}

static inline int
unregister_pm_notifier(struct notifier_block *nb)
{
	return (0);
}

#endif	/* _LINUXKPI_LINUX_SUSPEND_H_ */
