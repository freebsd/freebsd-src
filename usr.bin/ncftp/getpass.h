/* Getpass.h */

#ifndef _getpass_h_
#define _getpass_h_

/*  $RCSfile: getpass.h,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/05/21 05:45:36 $
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
