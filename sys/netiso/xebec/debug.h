/* $Header: /home/ncvs/src/sys/netiso/xebec/Attic/debug.h,v 1.2.4.1 1996/06/05 02:56:02 jkh Exp $ */
/* $Source: /home/ncvs/src/sys/netiso/xebec/Attic/debug.h,v $ */

#define OUT stdout

extern int	debug[128];

#ifdef DEBUG
extern int column;

#define IFDEBUG(letter) \
	if(debug['letter']) {
#define ENDDEBUG  ; (void) fflush(stdout);}

#else

#define STAR *
#define IFDEBUG(letter)	 //*beginning of comment*/STAR
#define ENDDEBUG	 STAR/*end of comment*//

#endif DEBUG

