/* /home/ncvs/src/sys/netiso/xebec/debug.h,v 1.2 1995/05/30 08:11:55 rgrimes Exp */
/* /home/ncvs/src/sys/netiso/xebec/debug.h,v */

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

