/*
 *	from: debug.h,v 2.1 88/09/19 12:56:16 nhall Exp
 *	$Id: debug.h,v 1.2 1993/10/16 21:33:08 rgrimes Exp $
 */

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

