#ifndef hertz_h
#define hertz_h

#define	HZ_WRONG 0		/* impossible clock frequency */

/*
 * Discover the tick frequency of the machine if something goes wrong,
 * we return HZ_WRONG, an impossible sampling frequency.
 */

extern int hertz PARAMS ((void));

#endif /* hertz_h */
