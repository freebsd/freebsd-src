/*
 * procfs ioctl definitions.
 *
 * $Id: pioctl.h,v 1.4 1997/12/13 03:13:36 sef Exp $
 */

#ifndef _SYS_PIOCTL_H
# define _SYS_PIOCTL_H

# include <sys/ioccom.h>

struct procfs_status {
	int	state;	/* Running, stopped, something else? */
	int	flags;	/* Any flags */
	unsigned long	events;	/* Events to stop on */
	int	why;	/* What event, if any, proc stopped on */
	unsigned long	val;	/* Any extra data */
};

# define	PIOCBIS	_IOC(IOC_IN, 'p', 1, 0)	/* Set event flag */
# define	PIOCBIC	_IOC(IOC_IN, 'p', 2, 0)	/* Clear event flag */
# define	PIOCSFL	_IOC(IOC_IN, 'p', 3, 0)	/* Set flags */
			/* wait for proc to stop */
# define	PIOCWAIT	_IOR('p', 4, struct procfs_status)
# define	PIOCCONT	_IOC(IOC_IN, 'p', 5, 0)	/* Continue a process */
			/* Get proc status */
# define	PIOCSTATUS	_IOR('p', 6, struct procfs_status)
# define	PIOCGFL	_IOR('p', 7, unsigned int)	/* Get flags */

# define S_EXEC	0x00000001	/* stop-on-exec */
# define	S_SIG	0x00000002	/* stop-on-signal */
# define	S_SCE	0x00000004	/* stop on syscall entry */
# define	S_SCX	0x00000008	/* stop on syscall exit */
# define	S_CORE	0x00000010	/* stop on coredump */
# define	S_EXIT	0x00000020	/* stop on exit */

/*
 * If PF_LINGER is set in procp->p_pfsflags, then the last close
 * of a /proc/<pid>/mem file will nto clear out the stops and continue
 * the process.
 */

# define PF_LINGER	0x01	/* Keep stops around after last close */

#endif 
