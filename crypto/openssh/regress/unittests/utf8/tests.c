/*	$OpenBSD: tests.c,v 1.3 2016/12/19 04:55:18 djm Exp $ */
/*
 * Regress test for the utf8.h *mprintf() API
 *
 * Written by Ingo Schwarze <schwarze@openbsd.org> in 2016
 * and placed in the public domain.
 */

#include "includes.h"

#include <locale.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "utf8.h"

void	 badarg(void);
void	 one(const char *, const char *, int, int, int, const char *);

void
badarg(void)
{
	char	 buf[16];
	int	 len, width;

	width = 1;
	TEST_START("utf8_badarg");
	len = snmprintf(buf, sizeof(buf), &width, "\377");
	ASSERT_INT_EQ(len, -1);
	ASSERT_STRING_EQ(buf, "");
	ASSERT_INT_EQ(width, 0);
	TEST_DONE();
}

void
one(const char *name, const char *mbs, int width,
    int wantwidth, int wantlen, const char *wants)
{
	char	 buf[16];
	int	*wp;
	int	 len;

	if (wantlen == -2)
		wantlen = strlen(wants);
	(void)strlcpy(buf, "utf8_", sizeof(buf));
	(void)strlcat(buf, name, sizeof(buf));
	TEST_START(buf);
	wp = wantwidth == -2 ? NULL : &width;
	len = snmprintf(buf, sizeof(buf), wp, "%s", mbs);
	ASSERT_INT_EQ(len, wantlen);
	ASSERT_STRING_EQ(buf, wants);
	ASSERT_INT_EQ(width, wantwidth);
	TEST_DONE();
}

void
tests(void)
{
	char	*loc;

	TEST_START("utf8_setlocale");
	loc = setlocale(LC_CTYPE, "en_US.UTF-8");
	ASSERT_PTR_NE(loc, NULL);
	TEST_DONE();

	badarg();
	one("empty", "", 2, 0, 0, "");
	one("ascii", "x", -2, -2, -2, "x");
	one("newline", "a\nb", -2, -2, -2, "a\nb");
	one("cr", "a\rb", -2, -2, -2, "a\rb");
	one("tab", "a\tb", -2, -2, -2, "a\tb");
	one("esc", "\033x", -2, -2, -2, "\\033x");
	one("inv_badbyte", "\377x", -2, -2, -2, "\\377x");
	one("inv_nocont", "\341x", -2, -2, -2, "\\341x");
	one("inv_nolead", "a\200b", -2, -2, -2, "a\\200b");
	one("sz_ascii", "1234567890123456", -2, -2, 16, "123456789012345");
	one("sz_esc", "123456789012\033", -2, -2, 16, "123456789012");
	one("width_ascii", "123", 2, 2, -1, "12");
	one("width_double", "a\343\201\201", 2, 1, -1, "a");
	one("double_fit", "a\343\201\201", 3, 3, 4, "a\343\201\201");
	one("double_spc", "a\343\201\201", 4, 3, 4, "a\343\201\201");
}
