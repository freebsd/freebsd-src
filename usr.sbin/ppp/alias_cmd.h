/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 * $FreeBSD: src/usr.sbin/ppp/alias_cmd.h,v 1.8.2.5 1999/08/29 15:45:40 peter Exp $
 */

struct cmdargs;

extern int nat_RedirectPort(struct cmdargs const *);
extern int nat_RedirectAddr(struct cmdargs const *);
extern int nat_ProxyRule(struct cmdargs const *);
extern int nat_Pptp(struct cmdargs const *);

extern struct layer natlayer;
