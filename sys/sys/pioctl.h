/*
 * procfs ioctl definitions.
 *
 * $Id: pioctl.h,v 1.1 1997/12/06 04:11:14 sef Exp $
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

# define	PIOCBIS	_IOW('p', 1, unsigned int)	/* Set event flag */
# define	PIOCBIC	_IOW('p', 2, unsigned int)	/* Clear event flag */
# define	PIOCSFL	_IOR('p', 3, unsigned int)	/* Set flags */
			/* wait for proc to stop */
# define	PIOCWAIT	_IOR('p', 4, struct procfs_status)
# define	PIOCCONT	_IOW('p', 5, int)	/* Continue a process */
			/* Get proc status */
# define	PIOCSTATUS	_IOW('p', 6, struct procfs_status)

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
