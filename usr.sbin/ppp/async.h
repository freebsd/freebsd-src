/*
 * $Id: $
 */

extern void AsyncInit(void);
extern void SetLinkParams(struct lcpstate *);
extern void AsyncOutput(int, struct mbuf *, int);
extern void AsyncInput(u_char *, int);
