/* te-netware.h -- NetWare target environment declarations.
   Copyright 2004 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define TE_NETWARE
#define LOCAL_LABELS_FB 1

#define LEX_AT  (LEX_NAME | LEX_BEGIN_NAME)  /* Can have @'s inside labels.  */
#define LEX_PCT (LEX_NAME | LEX_BEGIN_NAME)  /* Can have %'s inside labels.  */
#define LEX_QM  (LEX_NAME | LEX_BEGIN_NAME)  /* Can have ?'s inside labels.  */

#include "obj-format.h"
