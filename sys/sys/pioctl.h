/*
 * Copryight 1997 Sean Eric Fagan
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

/*
 * procfs ioctl definitions.
 *
 * $FreeBSD$
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

/*
 * If PF_LINGER is set in procp->p_pfsflags, then the last close
 * of a /proc/<pid>/mem file will nto clear out the stops and continue
 * the process.
 */

# define PF_LINGER	0x01	/* Keep stops around after last close */
# define PF_ISUGID	0x02	/* Ignore UID/GID changes */
#endif 
