/* Interface to "cvs watch add", "cvs watchers", and related features

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

extern const char *const watch_usage[];

/* Flags to pass between the various functions making up the
   add/remove code.  All in a single structure in case there is some
   need to make the code reentrant some day.  */

struct addremove_args {
    /* A flag for each watcher type.  */
    int edit;
    int unedit;
    int commit;

    /* Are we adding or removing (non-temporary) edit,unedit,and/or commit
       watches?  */
    int adding;

    /* Should we add a temporary edit watch?  */
    int add_tedit;
    /* Should we add a temporary unedit watch?  */
    int add_tunedit;
    /* Should we add a temporary commit watch?  */
    int add_tcommit;

    /* Should we remove all temporary watches?  */
    int remove_temp;

    /* Should we set the default?  This is here for passing among various
       routines in watch.c (a good place for it if there is ever any reason
       to make the stuff reentrant), not for watch_modify_watchers.  */
    int setting_default;
};

/* Modify the watchers for FILE.  *WHAT tells what to do to them.
   If FILE is NULL, modify default args (WHAT->SETTING_DEFAULT is
   not used).  */
extern void watch_modify_watchers PROTO ((const char *file,
					  struct addremove_args *what));

extern int watch_add PROTO ((int argc, char **argv));
extern int watch_remove PROTO ((int argc, char **argv));
