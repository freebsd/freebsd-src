/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.h,v 1.7 1997/12/24 10:28:38 brian Exp $
 */

struct cmdargs;

extern int AliasRedirectPort(struct cmdargs const *);
extern int AliasRedirectAddr(struct cmdargs const *);
