/* Declarations for update.c.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

int do_update PROTO((int argc, char *argv[], char *xoptions, char *xtag,
	       char *xdate, int xforce, int local, int xbuild,
	       int xaflag, int xprune, int xpipeout, int which,
	       char *xjoin_rev1, char *xjoin_rev2, char *preload_update_dir));
int joining PROTO((void));
extern int isemptydir PROTO ((char *dir, int might_not_exist));
