/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this notice are
 * duplicated in all such forms.
 *
 * [additional clause stricken -- see below]
 *
 * The name of the University may not be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY 
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE.
 * 
 * This notice previously contained the additional clause:
 *
 *   and that any documentation, advertising materials, and other 
 *   materials related to such distribution and use acknowledge that 
 *   the software was developed by the University of California, 
 *   Berkeley.
 *
 * Pursuant to the licensing change made by the Office of Technology
 * Licensing of the University of California, Berkeley on July 22, 
 * 1999 and documented in:
 *
 *   ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 *
 * this clause has been stricken and no longer is applicable to this
 * software.
 *
 *    @(#)dbm.h    5.2 (Berkeley) 5/24/89
 */

#ifndef NULL
/*
 * this is lunacy, we no longer use it (and never should have
 * unconditionally defined it), but, this whole file is for
 * backwards compatability - someone may rely on this.
 */
#define    NULL    ((char *) 0)
#endif

#ifdef I_NDBM
# include <ndbm.h>
#endif

datum    fetch();
datum    firstkey();
datum    nextkey();
