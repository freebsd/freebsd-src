#include <stdio.h>
#include <stdarg.h>
#include <isc/ctl.h>
#include <isc/eventlib.h>

struct ctl_verb verbs[];

static void
logger(enum ctl_severity sev, const char *fmt, ...) {
	va_list ap ;

	va_start (ap, fmt) ;
	fprintf (stderr, "logger: ");
	vfprintf (stderr, fmt, ap) ;
	va_end (ap) ;
}
	

main (int argc, char **argv) {
	evContext ctx ;
	struct sockaddr addr ;
	struct ctl_sctx *sctx ;


	sctx = ctl_server(ctx, &addr, verbs, 666, "Go away peon!", 222,
			  333, 10, 5, 6, logger);
	
}
