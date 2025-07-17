/*	$Id: test-getsubopt.c,v 1.6 2018/08/15 14:37:41 schwarze Exp $	*/
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
 */

#include <stdlib.h>

/*
 * NetBSD declared this function in the wrong header before August 2018.
 * No harm is done by allowing that, too:
 * The only file using it, main.c, also includes unistd.h, anyway.
 */
#include <unistd.h>

int
main(void)
{
	char buf[] = "k=v";
	char *options = buf;
	char token0[] = "k";
	char *const tokens[] = { token0, NULL };
	char *value = NULL;
	return ! (getsubopt(&options, tokens, &value) == 0
	    && value == buf+2 && options == buf+3);
}
