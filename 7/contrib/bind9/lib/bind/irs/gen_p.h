/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * $Id: gen_p.h,v 1.2.18.1 2005-04-27 05:00:56 sra Exp $
 */

/*! \file
 *  Notes:
 *	We hope to create a complete set of thread-safe entry points someday,
 *	which will mean a set of getXbyY() functions that take as an argument
 *	a pointer to the map class, which will have a pointer to the private
 *	data, which will be used preferentially to the static variables that
 *	are necessary to support the "classic" interface.  This "classic"
 *	interface will then be reimplemented as stubs on top of the thread
 *	safe modules, and will keep the map class pointers as their only
 *	static data.  HOWEVER, we are not there yet.  So while we will call
 *	the just-barely-converted map class methods with map class pointers,
 *	right now they probably all still use statics.  We're not fooling
 *	anybody, and we're not trying to (yet).
 */

#ifndef _GEN_P_H_INCLUDED
#define _GEN_P_H_INCLUDED

/*%
 * These are the access methods.
 */
enum irs_acc_id {
	irs_lcl,	/*%< Local. */
	irs_dns,	/*%< DNS or Hesiod. */
	irs_nis,	/*%< Sun NIS ("YP"). */
	irs_irp,	/*%< IR protocol. */
	irs_nacc
};

/*%
 * These are the map types.
 */
enum irs_map_id {
	irs_gr,		/*%< "group" */
	irs_pw,		/*%< "passwd" */
	irs_sv,		/*%< "services" */
	irs_pr,		/*%< "protocols" */
	irs_ho,		/*%< "hosts" */
	irs_nw,		/*%< "networks" */
	irs_ng,		/*%< "netgroup" */
	irs_nmap
};

/*%
 * This is an accessor instance.
 */
struct irs_inst {
	struct irs_acc *acc;
	struct irs_gr *	gr;
	struct irs_pw *	pw;
	struct irs_sv *	sv;
	struct irs_pr *	pr;
	struct irs_ho *	ho;
	struct irs_nw *	nw;
	struct irs_ng *	ng;
};

/*%
 * This is a search rule for some map type.
 */
struct irs_rule {
	struct irs_rule *	next;
	struct irs_inst *	inst;
	int			flags;
};
#define IRS_MERGE		0x0001	/*%< Don't stop if acc. has data? */
#define	IRS_CONTINUE		0x0002	/*%< Don't stop if acc. has no data? */
/*
 * This is the private data for a search access class.
 */
struct gen_p {
	char *			options;
	struct irs_rule *	map_rules[(int)irs_nmap];
	struct irs_inst		accessors[(int)irs_nacc];
	struct __res_state *	res;
	void			(*free_res) __P((void *));
};

/*
 * Externs.
 */

extern struct irs_acc *	irs_gen_acc __P((const char *, const char *conf_file));
extern struct irs_gr *	irs_gen_gr __P((struct irs_acc *));
extern struct irs_pw *	irs_gen_pw __P((struct irs_acc *));
extern struct irs_sv *	irs_gen_sv __P((struct irs_acc *));
extern struct irs_pr *	irs_gen_pr __P((struct irs_acc *));
extern struct irs_ho *	irs_gen_ho __P((struct irs_acc *));
extern struct irs_nw *	irs_gen_nw __P((struct irs_acc *));
extern struct irs_ng *	irs_gen_ng __P((struct irs_acc *));

#endif /*_IRS_P_H_INCLUDED*/
