#include "file.h"

#include <string.h>

#define F 0
#define T 1

/*
 * List of characters that look "reasonable" in international
 * language texts.  That's almost all characters :), except a
 * few in the control range of ASCII (all the known international
 * charactersets share the bottom half with ASCII).
 */
static char maybe_internat[256] = {
	F, F, F, F, F, F, F, F, T, T, T, T, T, T, F, F,  /* 0x0X */
	F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x8X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x9X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xaX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xbX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xcX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xdX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0xeX */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T   /* 0xfX */
};

/* Maximal length of a line we consider "reasonable". */
#define MAXLINELEN 300

int
internatmagic(buf, nbytes)
	unsigned char *buf;
	int nbytes;
{
	int i;
	unsigned char *cp;

	nbytes--;

	/* First, look whether there are "unreasonable" characters. */
	for (i = 0, cp = buf; i < nbytes; i++, cp++)
		if (!maybe_internat[*cp])
			return 0;

	/*
	 * Now, look whether the file consists of lines of
	 * "reasonable" length.
	 */

	for (i = 0; i < nbytes;) {
		cp = memchr(buf, '\n', nbytes - i);
		if (cp == NULL) {
			/* Don't fail if we hit the end of buffer. */
			if (i + MAXLINELEN >= nbytes)
				break;
			else
				return 0;
		}
		if (cp - buf > MAXLINELEN)
			return 0;
		i += (cp - buf + 1);
		buf = cp + 1;
	}
	ckfputs("International language text", stdout);
	return 1;
}
