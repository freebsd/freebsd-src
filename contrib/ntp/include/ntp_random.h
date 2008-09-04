
#include <ntp_types.h>

long ntp_random P((void));
void ntp_srandom P((unsigned long));
void ntp_srandomdev P((void));
char * ntp_initstate P((unsigned long, 	/* seed for R.N.G. */
			char *,		/* pointer to state array */
			long		/* # bytes of state info */
			));
char * ntp_setstate P((char *));	/* pointer to state array */



