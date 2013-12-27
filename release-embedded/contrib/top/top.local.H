/*
 *  Top - a top users display for Berkeley Unix
 *
 *  Definitions for things that might vary between installations.
 */

/*
 *  The space command forces an immediate update.  Sometimes, on loaded
 *  systems, this update will take a significant period of time (because all
 *  the output is buffered).  So, if the short-term load average is above
 *  "LoadMax", then top will put the cursor home immediately after the space
 *  is pressed before the next update is attempted.  This serves as a visual
 *  acknowledgement of the command.  On Suns, "LoadMax" will get multiplied by
 *  "FSCALE" before being compared to avenrun[0].  Therefore, "LoadMax"
 *  should always be specified as a floating point number.
 */
#ifndef LoadMax
#define LoadMax  %LoadMax%
#endif

/*
 *  "Table_size" defines the size of the hash tables used to map uid to
 *  username.  The number of users in /etc/passwd CANNOT be greater than
 *  this number.  If the error message "table overflow: too many users"
 *  is printed by top, then "Table_size" needs to be increased.  Things will
 *  work best if the number is a prime number that is about twice the number
 *  of lines in /etc/passwd.
 */
#ifndef Table_size
#define Table_size	%TableSize%
#endif

/*
 *  "Nominal_TOPN" is used as the default TOPN when Default_TOPN is Infinity
 *  and the output is a dumb terminal.  If we didn't do this, then
 *  installations who use a default TOPN of Infinity will get every
 *  process in the system when running top on a dumb terminal (or redirected
 *  to a file).  Note that Nominal_TOPN is a default:  it can still be
 *  overridden on the command line, even with the value "infinity".
 */
#ifndef Nominal_TOPN
#define Nominal_TOPN	%NominalTopn%
#endif

#ifndef Default_TOPN
#define Default_TOPN	%topn%
#endif

#ifndef Default_DELAY
#define Default_DELAY	%delay%
#endif

/*
 *  If the local system's getpwnam interface uses random access to retrieve
 *  a record (i.e.: 4.3 systems, Sun "yellow pages"), then defining
 *  RANDOM_PW will take advantage of that fact.  If RANDOM_PW is defined,
 *  then getpwnam is used and the result is cached.  If not, then getpwent
 *  is used to read and cache the password entries sequentially until the
 *  desired one is found.
 *
 *  We initially set RANDOM_PW to something which is controllable by the
 *  Configure script.  Then if its value is 0, we undef it.
 */

#define RANDOM_PW	%random%
#if RANDOM_PW == 0
#undef RANDOM_PW
#endif
