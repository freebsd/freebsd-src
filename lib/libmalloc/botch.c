#include "defs.h"
#include "globals.h"

int
__nothing()
{
	return 0;
}

/*
 * Simple botch routine - writes directly to stderr.  CAREFUL -- do not use
 * printf because of the vile hack we use to redefine fputs with write for
 * normal systems (i.e not super-pure ANSI)!
 */
int
__m_botch(s, filename, linenumber)
const char *s;
const char *filename;
int linenumber;
{
	static char linebuf[32];	/* Enough for a BIG linenumber! */
	static int notagain = 0;

	if (notagain == 0) {
		/* Try to flush the trace file and unbuffer stderr */
		(void) fflush(_malloc_statsfile);
		(void) setvbuf(stderr, (char *) 0, _IONBF, 0);
		(void) sprintf(linebuf, "%d: ", linenumber);
		(void) fputs("memory corruption error detected, file ",
			     stderr);
		(void) fputs(filename, stderr);
		(void) fputs(", line ", stderr);
		(void) fputs(linebuf, stderr);
		(void) fputs(s, stderr);
		(void) fputs("\n", stderr);
		/*
		 * In case stderr is buffered and was written to before we
		 * tried to unbuffer it
		 */
		(void) fflush(stderr);
		notagain++;	/* just in case abort() tries to cleanup */
		abort();
	}
	return 0;	/* SHOULDNTHAPPEN */
}
