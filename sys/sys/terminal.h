/*-
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ed Schouten under sponsorship from the
 * FreeBSD Foundation.
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

#ifndef _SYS_TERMINAL_H_
#define	_SYS_TERMINAL_H_

#include <sys/param.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/cons.h>
#include <sys/linker_set.h>
#include <sys/ttycom.h>

#include <teken/teken.h>

struct terminal;
struct thread;
struct tty;

/*
 * The terminal layer is an abstraction on top of the TTY layer and the
 * console interface.  It can be used by system console drivers to
 * easily interact with the kernel console and TTYs.
 *
 * Terminals contain terminal emulators, which means console drivers
 * don't need to implement their own terminal emulator. The terminal
 * emulator deals with UTF-8 exclusively. This means that term_char_t,
 * the data type used to store input/output characters will always
 * contain Unicode codepoints.
 *
 * To save memory usage, the top bits of term_char_t will contain other
 * attributes, like colors. Right now term_char_t is composed as
 * follows:
 *
 *  Bits  Meaning
 *  0-20: Character value
 * 21-25: Bold, underline, blink, reverse, right part of CJK fullwidth character
 * 26-28: Foreground color
 * 29-31: Background color
 */

typedef uint32_t term_char_t;
#define	TCHAR_CHARACTER(c)	((c) & 0x1fffff)
#define	TCHAR_FORMAT(c)		(((c) >> 21) & 0x1f)
#define	TCHAR_FGCOLOR(c)	(((c) >> 26) & 0x7)
#define	TCHAR_BGCOLOR(c)	((c) >> 29)

typedef teken_color_t term_color_t;
#define	TCOLOR_LIGHT(c)	((c) | 0x8)
#define	TCOLOR_DARK(c)	((c) & ~0x8)
typedef teken_pos_t term_pos_t;
typedef teken_rect_t term_rect_t;

typedef void tc_cursor_t(struct terminal *tm, const term_pos_t *p);
typedef void tc_putchar_t(struct terminal *tm, const term_pos_t *p,
    term_char_t c);
typedef void tc_fill_t(struct terminal *tm, const term_rect_t *r,
    term_char_t c);
typedef void tc_copy_t(struct terminal *tm, const term_rect_t *r,
    const term_pos_t *p);
typedef void tc_param_t(struct terminal *tm, int cmd, unsigned int arg);
typedef void tc_done_t(struct terminal *tm);

typedef void tc_cnprobe_t(struct terminal *tm, struct consdev *cd);
typedef int tc_cngetc_t(struct terminal *tm);

typedef void tc_opened_t(struct terminal *tm, int opened);
typedef int tc_ioctl_t(struct terminal *tm, u_long cmd, caddr_t data,
    struct thread *td);
typedef void tc_bell_t(struct terminal *tm);

struct terminal_class {
	/* Terminal emulator. */
	tc_cursor_t	*tc_cursor;
	tc_putchar_t	*tc_putchar;
	tc_fill_t	*tc_fill;
	tc_copy_t	*tc_copy;
	tc_param_t	*tc_param;
	tc_done_t	*tc_done;

	/* Low-level console interface. */
	tc_cnprobe_t	*tc_cnprobe;
	tc_cngetc_t	*tc_cngetc;
	
	/* Misc. */
	tc_opened_t	*tc_opened;
	tc_ioctl_t	*tc_ioctl;
	tc_bell_t	*tc_bell;
};

struct terminal {
	const struct terminal_class *tm_class;
	void		*tm_softc;
	struct mtx	 tm_mtx;
	struct tty	*tm_tty;
	teken_t		 tm_emulator;
	struct winsize	 tm_winsize;
	unsigned int	 tm_flags;
#define	TF_MUTE		0x1	/* Drop incoming data. */
#define	TF_BELL		0x2	/* Bell needs to be sent. */
#define	TF_CONS		0x4	/* Console device (needs spinlock). */
	struct consdev	*consdev;
};

#ifdef _KERNEL

struct terminal *terminal_alloc(const struct terminal_class *tc, void *softc);
void	terminal_maketty(struct terminal *tm, const char *fmt, ...);
void	terminal_set_winsize_blank(struct terminal *tm,
    const struct winsize *size, int blank);
void	terminal_set_winsize(struct terminal *tm, const struct winsize *size);
void	terminal_mute(struct terminal *tm, int yes);
void	terminal_input_char(struct terminal *tm, term_char_t c);
void	terminal_input_raw(struct terminal *tm, char c);
void	terminal_input_special(struct terminal *tm, unsigned int k);

void	termcn_cnregister(struct terminal *tm);

/* Kernel console helper interface. */
extern const struct consdev_ops termcn_cnops;

#define	TERMINAL_DECLARE_EARLY(name, class, softc)			\
	static struct terminal name = {					\
		.tm_class = &class,					\
		.tm_softc = softc,					\
		.tm_flags = TF_CONS,					\
	};								\
	CONSOLE_DEVICE(name ## _consdev, termcn_cnops, &name)

#endif /* _KERNEL */

#endif /* !_SYS_TERMINAL_H_ */
