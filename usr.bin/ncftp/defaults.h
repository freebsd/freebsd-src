/* Defaults.h: default values for ftp's common variables */

/* These are all surrounded by #ifndef blocks so you can just use
 * the -D flag with your compiler (i.e. -DZCAT=\"/usr/local/bin/zcat\").
 */

#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

/*  $RCSfile: defaults.h,v $
 *  $Revision: 14020.13 $
 *  $Date: 93/07/09 10:58:27 $
 */

#ifndef NEWMAILMESSAGE			/* For english speakers, "You have new mail." */
#define NEWMAILMESSAGE "You have new mail."
#endif

#ifndef ZCAT					/* Usually "zcat," but use the full pathname */
								/* if possible. */
#	ifdef GZCAT					/* If you said you had gnu's zcat, use it
								 * since it can do .Z files too.
								 */

#		define ZCAT GZCAT
#	else /* !GZCAT */
#		define ZCAT "zcat"
#	endif	/* ifdef GZCAT */
#endif	/* ifndef ZCAT */

#ifndef MAX_XFER_BUFSIZE
#define MAX_XFER_BUFSIZE 32768
#endif

#ifndef dANONOPEN				/* 1 or 0, usually 1 */
#define dANONOPEN	1
#endif

#ifndef dDEBUG					/* 1 or 0, usually 0 */
#define dDEBUG 0
#endif

#ifndef dMPROMPT				/* Usually 1, I prefer 0... */
#define dMPROMPT 0
#endif

/* If passive FTP can be used, this specifies whether it is turned on
 * by default.  If not, we have passive mode available, but are using
 * Port ftp by default.
 */
#ifndef	dPASSIVE
#define	dPASSIVE 0				/* Use PORT for more portability... */
#endif

#ifndef	dRESTRICT
#define	dRESTRICT 1				/* should be safe to be 1 */
#endif

#ifndef dVERBOSE				/* V_QUIET, V_ERRS, V_TERSE, V_VERBOSE */
#define dVERBOSE V_TERSE
#endif

#ifndef dPROMPT					/* short: "@Bftp@P>" */
								/* long: "@B@E @UNcFTP@P @B@M@D@P ->" */
#define dPROMPT "@B@c@Mncftp@P>" /* new two line prompt */
#endif

#ifndef dPAGER					/* if set to empty string, act like 'cat' */
#define dPAGER "more"
#endif

#ifndef dLOGNAME				/* usu. put in the user's home directory. */
#define dLOGNAME "~/.ftplog"
#endif

#ifndef dRECENTF				/* usu. put in the user's home directory. */
#define dRECENTF "~/.ncrecent"
#endif

#ifndef dMAXRECENTS				/* limit to how many recent sites to save. */
#define dMAXRECENTS 50
#endif

#ifndef dRECENT_ON				/* Do you want the recent log on? */
								/* usually 1. */
#define dRECENT_ON 1
#endif

								/* Do you want logging on by default? */
#ifndef dLOGGING				/* usually 0 */
#define dLOGGING 0
#endif

#ifndef dTYPE					/* usually TYPE_A */
#define dTYPE TYPE_A
#endif

#ifndef dTYPESTR				/* usually "ascii" */
#define dTYPESTR "ascii"
#endif

#ifndef dREDIALDELAY			/* usu. 60 (seconds). */
#define dREDIALDELAY 60
#endif

#ifndef CMDLINELEN
#define CMDLINELEN 256
#endif

#ifndef RECEIVEDLINELEN
#define RECEIVEDLINELEN 256
#endif

#ifndef MAXMACROS
#define MAXMACROS 16
#endif

#ifndef MACBUFLEN				/* usually 4096. */
#define MACBUFLEN 4096
#endif

/* Do you want binary transfers by default? */
#ifndef dAUTOBINARY				/* usually 1 */
#define dAUTOBINARY 1
#endif

#ifndef dPROGRESS
#define dPROGRESS pr_philbar	/* can be: pr_none, pr_percent, pr_philbar,
								 * or pr_kbytes
								 */
#endif

/* Default login name at gateway */
#ifdef GATEWAY
#	ifndef dGATEWAY_LOGIN
#		define dGATEWAY_LOGIN "ftp"
#	endif
#endif

#endif	/* _DEFAULTS_H_ */

/* eof */
