#include <regexp.h>
#include <stdio.h>

void
regerror(s)
const char *s;
{
#ifdef ERRAVAIL
	error("regexp: %s", s);
#else
/*
	fprintf(stderr, "regexp(3): %s\n", s);
	exit(1);
*/
	return;	  /* let std. egrep handle errors */
#endif
	/* NOTREACHED */
}
