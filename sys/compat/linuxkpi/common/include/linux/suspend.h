/* Public domain. */

#ifndef _LINUXKPI_LINUX_SUSPEND_H_
#define _LINUXKPI_LINUX_SUSPEND_H_

typedef int suspend_state_t;

extern suspend_state_t pm_suspend_target_state;

#define	PM_SUSPEND_ON		0
#define	PM_SUSPEND_TO_IDLE	1

static inline int
pm_suspend_via_firmware()
{
	return 0;
}

#endif	/* _LINUXKPI_LINUX_SUSPEND_H_ */
