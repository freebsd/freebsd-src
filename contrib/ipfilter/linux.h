/*
 * (C)opyright 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.  The author accepts no
 * responsibility and is not changed in any way.
 *
 * I hate legaleese, don't you ?
 * $Id: linux.h,v 2.0.2.3 1997/04/07 09:59:01 darrenr Exp $
 */

#include <linux/config.h>
#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif /* MODULE */

#include "ip_compat.h"
