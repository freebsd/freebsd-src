/*
 * Copyright (c) 2001 Alexey Zelkin <phantom@FreeBSD.org>
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

#include <stddef.h>

#include "lmessages.h"
#include "ldpart.h"

#define LCMESSAGES_SIZE_FULL (sizeof(struct lc_messages_T) / sizeof(char *))
#define LCMESSAGES_SIZE_MIN \
		(offsetof(struct lc_messages_T, yesstr) / sizeof(char *))

static char empty[] = "";

static const struct lc_messages_T _C_messages_locale = {
	"^[yY]" ,	/* yesexpr */
	"^[nN]" ,	/* noexpr */
	"yes" , 	/* yesstr */
	"no"		/* nostr */
};

static struct lc_messages_T _messages_locale;
static int	_messages_using_locale;
static char	*_messages_locale_buf;

int
__messages_load_locale(const char *name) {

	/*
	 * Propose that we can have incomplete locale file (w/o "{yes,no}str").
	 * Initialize them before loading.  In case of complete locale, they'll
	 * be initialized to loaded value, otherwise they'll not be touched.
	 */
	_messages_locale.yesstr = empty;
	_messages_locale.nostr = empty;

	return __part_load_locale(name, &_messages_using_locale,
		_messages_locale_buf, "LC_MESSAGES",
		LCMESSAGES_SIZE_FULL, LCMESSAGES_SIZE_MIN,
		(const char **)&_messages_locale);
}

struct lc_messages_T *
__get_current_messages_locale(void) {

	return (_messages_using_locale
		? &_messages_locale
		: (struct lc_messages_T *)&_C_messages_locale);
}

#ifdef LOCALE_DEBUG
void
msgdebug() {
printf(	"yesexpr = %s\n"
	"noexpr = %s\n"
	"yesstr = %s\n"
	"nostr = %s\n",
	_messages_locale.yesexpr,
	_messages_locale.noexpr,
	_messages_locale.yesstr,
	_messages_locale.nostr
);
}
#endif /* LOCALE_DEBUG */
