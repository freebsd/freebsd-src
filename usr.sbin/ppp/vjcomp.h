/*
 * $Id: vjcomp.h,v 1.1 1997/10/26 01:04:02 brian Exp $
 */

extern void VjInit(int);
extern void SendPppFrame(struct mbuf *);
extern struct mbuf *VjCompInput(struct mbuf *, int);
