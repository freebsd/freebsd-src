/*
 * modetoa - return an asciized mode
 */
#include <stdio.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

const char *
modetoa(
	int mode
	)
{
	char *bp;
	static const char *modestrings[] = {
		"unspec",
		"sym_active",
		"sym_passive",
		"client",
		"server",
		"broadcast",
		"control",
		"private",
		"bclient",
	};

	if (mode < 0 || mode >= (sizeof modestrings)/sizeof(char *)) {
		LIB_GETBUF(bp);
		(void)sprintf(bp, "mode#%d", mode);
		return bp;
	}

	return modestrings[mode];
}
