/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed under sponsorship from Snow
 * B.V., the Netherlands.
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
 */

#ifndef _SYS_TTYDISC_H_
#define	_SYS_TTYDISC_H_

#ifndef _SYS_TTY_H_
#error "can only be included through <sys/tty.h>"
#endif /* !_SYS_TTY_H_ */

struct cv;
struct thread;
struct tty;
struct uio;

/* Top half routines. */
void	ttydisc_open(struct tty *);
void	ttydisc_close(struct tty *);
int	ttydisc_read(struct tty *, struct uio *, int);
int	ttydisc_write(struct tty *, struct uio *, int);
void	ttydisc_optimize(struct tty *);

/* Bottom half routines. */
void	ttydisc_modem(struct tty *, int);
#define ttydisc_can_bypass(tp) ((tp)->t_flags & TF_BYPASS)
int	ttydisc_rint(struct tty *, char, int);
size_t	ttydisc_rint_bypass(struct tty *, char *, size_t);
void	ttydisc_rint_done(struct tty *);
size_t	ttydisc_getc(struct tty *, void *buf, size_t);
int	ttydisc_getc_uio(struct tty *, struct uio *);

/* Error codes for ttydisc_rint(). */
#define	TRE_FRAMING	0x01
#define	TRE_PARITY	0x02
#define	TRE_OVERRUN	0x04
#define	TRE_BREAK	0x08

static __inline size_t
ttydisc_read_poll(struct tty *tp)
{

	tty_lock_assert(tp, MA_OWNED);

	return ttyinq_bytescanonicalized(&tp->t_inq);
}

static __inline size_t
ttydisc_write_poll(struct tty *tp)
{

	tty_lock_assert(tp, MA_OWNED);

	return ttyoutq_bytesleft(&tp->t_outq);
}

static __inline size_t
ttydisc_rint_poll(struct tty *tp)
{

	tty_lock_assert(tp, MA_OWNED);

	return ttyinq_bytesleft(&tp->t_inq);
}

static __inline size_t
ttydisc_getc_poll(struct tty *tp)
{

	tty_lock_assert(tp, MA_OWNED);

	if (tp->t_flags & TF_STOPPED)
		return (0);

	return ttyoutq_bytesused(&tp->t_outq);
}

#endif /* !_SYS_TTYDISC_H_ */
