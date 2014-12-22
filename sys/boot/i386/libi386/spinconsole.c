/*-
 * spinconsole.c
 *
 * Author: Maksym Sobolyev <sobomax@sippysoft.com>
 * Copyright (c) 2009 Sippy Software, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <bootstrap.h>

extern void get_pos(int *x, int *y);
extern void curs_move(int *_x, int *_y, int x, int y);
extern void vidc_biosputchar(int c);

static void	spinc_probe(struct console *cp);
static int	spinc_init(int arg);
static void	spinc_putchar(int c);
static int	spinc_getchar(void);
static int	spinc_ischar(void);

struct console spinconsole = {
	"spinconsole",
	"spin port",
	0,
	spinc_probe,
	spinc_init,
	spinc_putchar,
	spinc_getchar,
	spinc_ischar
};

static void
spinc_probe(struct console *cp)
{
	cp->c_flags |= (C_PRESENTIN | C_PRESENTOUT);
}

static int
spinc_init(int arg)
{
	return(0);
}

static void
spinc_putchar(int c)
{
	static int curx, cury;
	static unsigned tw_chars = 0x5C2D2F7C;    /* "\-/|" */
	static time_t lasttime;
	time_t now;

	now = time(NULL);
	if (now < (lasttime + 1))
		return;
	lasttime = now;
#ifdef TERM_EMU
	get_pos(&curx, &cury);
	if (curx > 0)
		curs_move(&curx, &cury, curx - 1, cury);
#endif
	vidc_biosputchar((char)tw_chars);
	tw_chars = (tw_chars >> 8) | ((tw_chars & (unsigned long)0xFF) << 24);
}

static int
spinc_getchar(void)
{
	return(-1);
}

static int
spinc_ischar(void)
{
	return(0);
}
