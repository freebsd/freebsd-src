/*
 * $Id: vjcomp.h,v 1.2 1997/10/26 12:42:13 brian Exp $
 */

extern void VjInit(int);
extern void SendPppFrame(struct mbuf *);
extern struct mbuf *VjCompInput(struct mbuf *, int);
extern const char *vj2asc(u_long);
