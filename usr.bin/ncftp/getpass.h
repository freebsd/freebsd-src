/* Getpass.h */

#ifndef _getpass_h_
#define _getpass_h_

/*  getpass.h,v
 *  1.1.1.1
 *  1994/09/22 23:45:35
 */

#define kMaxPassLen 127

#ifdef GETPASS
extern char *getpass();	/* Use the system supplied getpass. */
#else
char *Getpass(char *prompt);
#endif

void Echo(FILE *fp, int on);

#endif	/* _getpass_h_ */

/* eof Getpass.h */
