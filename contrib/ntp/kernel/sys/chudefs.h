/*
 * Definitions for the CHU line discipline v2.0
 */

/*
 * The CHU time code consists of 10 BCD digits and is repeated
 * twice for a total of 10 characters.  A time is taken after
 * the arrival of each character.  The following structure is
 * used to return this stuff.
 */
#define	NCHUCHARS	(10)

struct chucode {
	u_char codechars[NCHUCHARS];	/* code characters */
	u_char ncodechars;		/* number of code characters */
	u_char chutype;			/* packet type */
	struct timeval codetimes[NCHUCHARS];	/* arrival times */
};

#define CHU_TIME 0		/* second half is equal to first half */
#define CHU_YEAR 1		/* second half is one's complement */

