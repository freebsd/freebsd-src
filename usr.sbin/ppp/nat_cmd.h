/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 * $FreeBSD: src/usr.sbin/ppp/nat_cmd.h,v 1.13.4.2 2000/06/27 16:35:12 ru Exp $
 */

struct cmdargs;

extern int nat_RedirectPort(struct cmdargs const *);
extern int nat_RedirectAddr(struct cmdargs const *);
extern int nat_ProxyRule(struct cmdargs const *);
extern int nat_SetTarget(struct cmdargs const *);

extern struct layer natlayer;
