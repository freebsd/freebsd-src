/*
 * Copyright (c) 2001 The FreeBSD Project, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY The FreeBSD Project, Inc. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL The FreeBSD Project, Inc. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* TTY subsystem   XXX rewrite! */
int redirect0;
int redirect1; 
int redirect2;
extern int kbd_fd;
extern const char *xfont;

int	KbdEmpty(void);
u_short	KbdPeek(void);
u_short	KbdRead(void);
void	KbdWrite(u_short);

void	console_init(void);
void	get_lines(void);
void	get_ximage(void);
void	init_window(void);
void	init_ximage(int, int);
void	int09(regcontext_t *);
void	kbd_bios_init(void);
void	kbd_init(void);
void	load_font(void);
void	resize_window(void);
int	tty_char(int, int);
int	tty_eread(REGISTERS, int, int);
int	tty_estate(void);
void	tty_flush(void);
void	tty_index(int);
void	tty_move(int, int);
int	tty_read(regcontext_t *, int);
void	tty_report(int *, int *);
void	tty_pause(void);
int	tty_peek(REGISTERS, int);
void	tty_rwrite(int, int, int);
int	tty_state(void);
void	tty_scroll(int, int, int, int, int, int);
void	tty_rscroll(int, int, int, int, int, int);
void	tty_write(int, int);
void	update_pixels(void);
void	video_blink(int);
void	video_setborder(int);
void	video_update(regcontext_t *);
