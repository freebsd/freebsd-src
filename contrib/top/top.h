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

/* Current major version number */
#define VERSION		3

/* Number of lines of header information on the standard screen */
extern int Header_lines;	/* 7 */

/* Maximum number of columns allowed for display */
#define MAX_COLS	512

/* Log base 2 of 1024 is 10 (2^10 == 1024) */
#define LOG1024		10

char *itoa();
char *itoa7();

char *version_string();

/* Special atoi routine returns either a non-negative number or one of: */
#define Infinity	-1
#define Invalid		-2

/* maximum number we can have */
#define Largest		0x7fffffff

/*
 * The entire display is based on these next numbers being defined as is.
 */

#define NUM_AVERAGES    3

enum displaymodes { DISP_CPU = 0, DISP_IO, DISP_MAX };

/*
 * Format modifiers
 */
#define FMT_SHOWARGS 0x00000001

extern enum displaymodes displaymode;

extern int pcpu_stats;

#endif /* TOP_H */
