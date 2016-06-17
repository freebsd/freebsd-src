/*
 * Copyright (C) 2000, 2001 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <asm/bootinfo.h>

/*
 * The naming of this variable is a remnant of the initial mips port to
 * ARC-firmware based SGI consoles.  We don't really need to do anything for
 * the variable other than provide an instantiation.  Everything about
 * arcs_cmdline seems more than a little bit hackish...
 */
char arcs_cmdline[CL_SIZE];
