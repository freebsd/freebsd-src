/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.h,v 1.10 1999/03/07 18:13:44 brian Exp $
 */

struct cmdargs;

extern int alias_RedirectPort(struct cmdargs const *);
extern int alias_RedirectAddr(struct cmdargs const *);
extern int alias_ProxyRule(struct cmdargs const *);
extern int alias_Pptp(struct cmdargs const *);

extern struct layer aliaslayer;
