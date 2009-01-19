/* Binutils emulation layer.
   Copyright (C) 2002 Free Software Foundation, Inc.
   Written by Tom Rix, Redhat.

   This file is part of GNU Binutils.

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

#include "binemul.h"

struct bin_emulation_xfer_struct bin_vanilla_emulation =
{
  ar_emul_default_usage,
  ar_emul_default_append,
  ar_emul_default_replace,
  ar_emul_default_create,
  ar_emul_default_parse_arg,
};
