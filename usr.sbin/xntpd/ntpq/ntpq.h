/* ntpq.h,v 3.1 1993/07/06 01:09:30 jbj Exp
 * ntpq.h - definitions of interest to ntpq
 */
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_control.h"
#include "ntp_string.h"
#include "ntp_malloc.h"

/*
 * Maximum number of arguments
 */
#define	MAXARGS	4

/*
 * Flags for forming descriptors.
 */
#define	OPT	0x80		/* this argument is optional, or'd with type */

#define	NO	0x0
#define	STR	0x1		/* string argument */
#define	UINT	0x2		/* unsigned integer */
#define	INT	0x3		/* signed integer */
#define	ADD	0x4		/* IP network address */

/*
 * Arguments are returned in a union
 */
typedef union {
	char *string;
	LONG ival;
	U_LONG uval;
	U_LONG netnum;
} arg_v;

/*
 * Structure for passing parsed command line
 */
struct parse {
	char *keyword;
	arg_v argval[MAXARGS];
	int nargs;
};

/*
 * xntpdc includes a command parser which could charitably be called
 * crude.  The following structure is used to define the command
 * syntax.
 */
struct xcmd {
	char *keyword;		/* command key word */
	void (*handler)	P((struct parse *, FILE *));	/* command handler */
	u_char arg[MAXARGS];	/* descriptors for arguments */
	char *desc[MAXARGS];	/* descriptions for arguments */
	char *comment;
};

/*
 * Types of things we may deal with
 */
#define	TYPE_SYS	1
#define	TYPE_PEER	2
#define	TYPE_CLOCK	3


/*
 * Structure to hold association data
 */
struct association {
	u_short assid;
	u_short status;
};

#define	MAXASSOC	1024

/*
 * Structure for translation tables between text format
 * variable indices and text format.
 */
struct ctl_var {
	u_short code;
	u_short fmt;
	char *text;
};

extern	void	asciize		P((int, char *, FILE *));
extern	int	getnetnum	P((char *, U_LONG *, char *));
extern	void	sortassoc	P((void));
extern	int	doquery		P((int, int, int, int, char *, u_short *, int *, char **));
extern	char *	nntohost	P((U_LONG));
extern	int	decodets	P((char *, l_fp *));
extern	int	decodeuint	P((char *, U_LONG *));
extern	int	nextvar		P((int *, char **, char **, char **));
extern	int	decodetime	P((char *, l_fp *));
extern	void	printvars	P((int, char *, int, int, FILE *));
extern	int	decodeint	P((char *, LONG *));
extern	int	findvar		P((char *, struct ctl_var *));
