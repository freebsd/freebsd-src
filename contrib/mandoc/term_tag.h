/* $Id: term_tag.h,v 1.4 2021/03/30 17:16:55 schwarze Exp $ */
/*
 * Copyright (c) 2015, 2018, 2019, 2020 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Internal interfaces to write a ctags(1) file.
 * For use by the mandoc(1) ASCII and UTF-8 formatters only.
 */

struct	tag_files {
	char	 ofn[80];	/* Output file name. */
	char	 tfn[80];	/* Tag file name. */
	FILE	*tfs;		/* Tag file object. */
	int	 ofd;		/* Original output file descriptor. */
	pid_t	 tcpgid;	/* Process group controlling the terminal. */
	pid_t	 pager_pid;	/* Process ID of the pager. */
};


struct tag_files	*term_tag_init(const char *, const char *, const char *);
void			 term_tag_write(struct roff_node *, size_t);
int			 term_tag_close(void);
void			 term_tag_unlink(void);
