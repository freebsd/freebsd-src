/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.h,v 1.7.2.2 1998/05/01 19:23:44 brian Exp $
 */

struct cmdargs;

extern int alias_RedirectPort(struct cmdargs const *);
extern int alias_RedirectAddr(struct cmdargs const *);
