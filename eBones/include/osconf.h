/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Athena configuration.
 *
 *	from: osconf.h,v 4.4 89/12/19 13:26:27 jtkohl Exp $
 *	$FreeBSD$
 */

#ifdef tahoe
#include "conf-bsdtahoe.h"
#else /* !tahoe */
#ifdef vax
#include "conf-bsdvax.h"
#else /* !vax */
#if defined(mips) && defined(ultrix)
#include "conf-ultmips2.h"
#else /* !Ultrix MIPS-2 */
#ifdef ibm032
#include "conf-bsdibm032.h"
#else /* !ibm032 */
#ifdef apollo
#include "conf-bsdapollo.h"
#else /* !apollo */
#ifdef sun
#ifdef sparc
#include "conf-bsdsparc.h"
#else /* sun but not sparc */
#ifdef i386
#include "conf-bsd386i.h"
#else /* sun but not (sparc or 386i) */
#include "conf-bsdm68k.h"
#endif /* i386 */
#endif /* sparc */
#else /* !sun */
#ifdef pyr
#include "conf-pyr.h"
#endif /* pyr */
#endif /* sun */
#endif /* apollo */
#endif /* ibm032 */
#endif /* mips */
#endif /* vax */
#endif /* tahoe */

#if defined(__FreeBSD__) && defined(i386)
#include "conf-bsd386i.h"
#endif

