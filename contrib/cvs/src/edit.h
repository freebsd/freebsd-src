/* Interface to "cvs edit", "cvs watch on", and related features

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

extern int watch_on PROTO ((int argc, char **argv));
extern int watch_off PROTO ((int argc, char **argv));

#ifdef CLIENT_SUPPORT
/* Check to see if any notifications are sitting around in need of being
   sent.  These are the notifications stored in CVSADM_NOTIFY (edit,unedit);
   commit calls notify_do directly.  */
extern void notify_check PROTO ((char *repository, char *update_dir));
#endif /* CLIENT_SUPPORT */

/* Issue a notification for file FILENAME.  TYPE is 'E' for edit, 'U'
   for unedit, and 'C' for commit.  WHO is the user currently running.
   For TYPE 'E', VAL is the time+host+directory data which goes in
   _editors, and WATCHES is zero or more of E,U,C, in that order, to specify
   what kinds of temporary watches to set.  */
extern void notify_do PROTO ((int type, char *filename, char *who,
			      char *val, char *watches, char *repository));

/* Set attributes to reflect the fact that EDITOR is editing FILENAME.
   VAL is time+host+directory, or NULL if we are to say that EDITOR is
   *not* editing FILENAME.  */
extern void editor_set PROTO ((char *filename, char *editor, char *val));

/* Take note of the fact that FILE is up to date (this munges CVS/Base;
   processing of CVS/Entries is done separately).  */
extern void mark_up_to_date PROTO ((char *file));
