/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.h,v 1.8.2.3 1999/05/02 08:59:33 brian Exp $
 */

struct cmdargs;

extern int alias_RedirectPort(struct cmdargs const *);
extern int alias_RedirectAddr(struct cmdargs const *);
extern int alias_ProxyRule(struct cmdargs const *);
extern int alias_Pptp(struct cmdargs const *);
