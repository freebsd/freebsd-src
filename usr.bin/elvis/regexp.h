/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define NSUBEXP  10

typedef struct regexp {
	char	*startp[NSUBEXP];
	char	*endp[NSUBEXP];
	int	minlen;		/* length of shortest possible match */
	char	first;		/* first character, if known; else \0 */
	char	bol;		/* boolean: must start at beginning of line? */
	char	program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

extern regexp *regcomp();
extern int regexec();
extern void regsub();
extern void regerr();
