/* Declarations for update.c.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

int do_update PROTO((int argc, char *argv[], char *xoptions, char *xtag,
	       char *xdate, int xforce, int local, int xbuild,
	       int xaflag, int xprune, int xpipeout, int which,
	       char *xjoin_rev1, char *xjoin_rev2, char *preload_update_dir));
int joining PROTO((void));
