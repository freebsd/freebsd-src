/*
 * Include file for a midi timer.
 * 
 * Copyright by Seigo Tanimura 2002.
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
 *
 */

#ifndef _TIMER_H_
#define _TIMER_H_

typedef struct _timerdev_info timerdev_info;

typedef int (tmr_open_t)(timerdev_info *tmd, int oflags, int devtype, struct thread *td);
typedef int (tmr_close_t)(timerdev_info *tmd, int fflag, int devtype, struct thread *td);
typedef int (tmr_event_t)(timerdev_info *tmd, u_char *ev);
typedef int (tmr_gettime_t)(timerdev_info *tmd, u_long *t);
typedef int (tmr_ioctl_t)(timerdev_info *tmd, u_long cmd, caddr_t data, int fflag, struct thread *td);
typedef int (tmr_armtimer_t)(timerdev_info *tmd, u_long t);

struct _timerdev_info {
	/*
	 * the first part of the descriptor is filled up from a
	 * template.
	 */
	char		name[32];

	int		caps;
	int		prio;

	tmr_open_t	*open;
	tmr_close_t	*close;
	tmr_event_t	*event;
	tmr_gettime_t	*gettime;
	tmr_ioctl_t	*ioctl;
	tmr_armtimer_t	*armtimer;


	int		unit;
	void		*softc;
	void		*seq;

	/* The tailq entry of the next timer device. */
	TAILQ_ENTRY(_timerdev_info)	tmd_link;

	int		opened;

	struct mtx	mtx;
};

#ifdef _KERNEL
int		timerdev_install(void);
timerdev_info	*create_timerdev_info_unit(timerdev_info *tmdinf);
timerdev_info	*get_timerdev_info_unit(int unit);
timerdev_info	*get_timerdev_info(void);
#endif /* _KERNEL */

#endif /* _TIMER_H_ */
