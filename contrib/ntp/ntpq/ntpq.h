/*
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
	long ival;
	u_long uval;
	u_int32 netnum;
} arg_v;

/*
 * Structure for passing parsed command line
 */
struct parse {
	const char *keyword;
	arg_v argval[MAXARGS];
	int nargs;
};

/*
 * ntpdc includes a command parser which could charitably be called
 * crude.  The following structure is used to define the command
 * syntax.
 */
struct xcmd {
  const char *keyword;		/* command key word */
	void (*handler)	P((struct parse *, FILE *));	/* command handler */
	u_char arg[MAXARGS];	/* descriptors for arguments */
  const char *desc[MAXARGS];	/* descriptions for arguments */
  const char *comment;
};

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
	const char *text;
};

extern	void	asciize		P((int, char *, FILE *));
extern	int	getnetnum	P((const char *, u_int32 *, char *));
extern	void	sortassoc	P((void));
extern	int	doquery		P((int, int, int, int, char *, u_short *, int *, char **));
extern	char *	nntohost	P((u_int32));
extern	int	decodets	P((char *, l_fp *));
extern	int	decodeuint	P((char *, u_long *));
extern	int	nextvar		P((int *, char **, char **, char **));
extern	int	decodetime	P((char *, l_fp *));
extern	void	printvars	P((int, char *, int, int, FILE *));
extern	int	decodeint	P((char *, long *));
extern	int	findvar		P((char *, struct ctl_var *));
