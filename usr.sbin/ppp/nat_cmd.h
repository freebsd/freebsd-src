/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 * $FreeBSD: src/usr.sbin/ppp/nat_cmd.h,v 1.13.2.2 2000/06/01 00:48:54 brian Exp $
 */

struct cmdargs;

extern int nat_RedirectPort(struct cmdargs const *);
extern int nat_RedirectAddr(struct cmdargs const *);
extern int nat_ProxyRule(struct cmdargs const *);
extern int nat_Pptp(struct cmdargs const *);
extern int nat_SetTarget(struct cmdargs const *);

extern struct layer natlayer;
