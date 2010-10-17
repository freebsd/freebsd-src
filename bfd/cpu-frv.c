/* BFD support for the FRV processor.
   Copyright 2002, 2003 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#define FRV_ARCH(MACHINE, NAME, DEFAULT, NEXT)				\
{									\
  32,	                        /* 32 bits in a word */			\
  32,	                        /* 32 bits in an address */		\
  8,	                        /* 8 bits in a byte */			\
  bfd_arch_frv,			/* architecture */			\
  MACHINE,			/* which machine */			\
  "frv",			/* architecture name */			\
  NAME,				/* machine name */			\
  4,				/* default alignment */			\
  DEFAULT,			/* is this the default? */		\
  bfd_default_compatible,	/* architecture comparison fn */	\
  bfd_default_scan,		/* string to architecture convert fn */	\
  NEXT				/* next in list */			\
}

static const bfd_arch_info_type arch_info_300
  = FRV_ARCH (bfd_mach_fr300,   "fr300",   FALSE, (bfd_arch_info_type *)0);

static const bfd_arch_info_type arch_info_400
  = FRV_ARCH (bfd_mach_fr400, "fr400", FALSE, &arch_info_300);

static const bfd_arch_info_type arch_info_500
  = FRV_ARCH (bfd_mach_fr500, "fr500", FALSE, &arch_info_400);

static const bfd_arch_info_type arch_info_550
  = FRV_ARCH (bfd_mach_fr550, "fr550", FALSE, &arch_info_500);

static const bfd_arch_info_type arch_info_simple
  = FRV_ARCH (bfd_mach_frvsimple, "simple", FALSE, &arch_info_550);

static const bfd_arch_info_type arch_info_tomcat
  = FRV_ARCH (bfd_mach_frvtomcat, "tomcat", FALSE, &arch_info_simple);

const bfd_arch_info_type bfd_frv_arch
  = FRV_ARCH (bfd_mach_frv, "frv", TRUE, &arch_info_tomcat);

