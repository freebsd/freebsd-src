/*
 * $FreeBSD$
 */
/*
 *  Top - a top users display for Berkeley Unix
 *
 *  General (global) definitions
 */

#ifndef TOP_H
#define TOP_H

#define Default_DELAY 2

/* Number of lines of header information on the standard screen */
extern int Header_lines;	/* 7 */

/* Maximum number of columns allowed for display */
#define MAX_COLS	512

/* Special atoi routine returns either a non-negative number or one of: */
#define Infinity	-1
#define Invalid		-2

/* maximum number we can have */
#define Largest		0x7fffffff

/*
 * The entire display is based on these next numbers being defined as is.
 */

/* Exit code for system errors */
#define TOP_EX_SYS_ERROR	23

enum displaymodes { DISP_CPU = 0, DISP_IO, DISP_MAX };

/*
 * Format modifiers
 */
#define FMT_SHOWARGS 0x00000001

extern enum displaymodes displaymode;

extern int pcpu_stats;
extern int  overstrike;

extern const char * myname;

extern int (*compares[])(const void*, const void*);

char* kill_procs(char *);
char* renice_procs(char *);

extern char copyright[];
/* internal routines */
void quit(int);


/*
 *  The space command forces an immediate update.  Sometimes, on loaded
 *  systems, this update will take a significant period of time (because all
 *  the output is buffered).  So, if the short-term load average is above
 *  "LoadMax", then top will put the cursor home immediately after the space
 *  is pressed before the next update is attempted.  This serves as a visual
 *  acknowledgement of the command.
 */
#define LoadMax  5.0

/*
 *  "Nominal_TOPN" is used as the default TOPN when 
 *  the output is a dumb terminal.  If we didn't do this, then
 *  we will get every
 *  process in the system when running top on a dumb terminal (or redirected
 *  to a file).  Note that Nominal_TOPN is a default:  it can still be
 *  overridden on the command line, even with the value "infinity".
 */
#define Nominal_TOPN	18

/*
 *  If the local system's getpwnam interface uses random access to retrieve
 *  a record (i.e.: 4.3 systems, Sun "yellow pages"), then defining
 *  RANDOM_PW will take advantage of that fact.  
 */

#define RANDOM_PW	1

#endif /* TOP_H */
