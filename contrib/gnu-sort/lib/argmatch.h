/* argmatch.h -- definitions and prototypes for argmatch.c
   Copyright (C) 1990, 1998, 1999, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by David MacKenzie <djm@ai.mit.edu>
   Modified by Akim Demaille <demaille@inf.enst.fr> */

#ifndef ARGMATCH_H_
# define ARGMATCH_H_ 1

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <sys/types.h>

# ifndef PARAMS
#  if PROTOTYPES || (defined (__STDC__) && __STDC__)
#   define PARAMS(args) args
#  else
#   define PARAMS(args) ()
#  endif  /* GCC.  */
# endif  /* Not PARAMS.  */

/* Assert there are as many real arguments as there are values
   (argument list ends with a NULL guard).  There is no execution
   cost, since it will be statically evalauted to `assert (0)' or
   `assert (1)'.  Unfortunately there is no -Wassert-0. */

# undef ARRAY_CARDINALITY
# define ARRAY_CARDINALITY(Array) (sizeof ((Array)) / sizeof (*(Array)))

# define ARGMATCH_ASSERT(Arglist, Vallist)      \
  assert (ARRAY_CARDINALITY ((Arglist)) == ARRAY_CARDINALITY ((Vallist)) + 1)

/* Return the index of the element of ARGLIST (NULL terminated) that
   matches with ARG.  If VALLIST is not NULL, then use it to resolve
   false ambiguities (i.e., different matches of ARG but corresponding
   to the same values in VALLIST).  */

int argmatch
  PARAMS ((const char *arg, const char *const *arglist,
	   const char *vallist, size_t valsize));
int argcasematch
  PARAMS ((const char *arg, const char *const *arglist,
	   const char *vallist, size_t valsize));

# define ARGMATCH(Arg, Arglist, Vallist) \
  argmatch ((Arg), (Arglist), (const char *) (Vallist), sizeof (*(Vallist)))

# define ARGCASEMATCH(Arg, Arglist, Vallist) \
  argcasematch ((Arg), (Arglist), (const char *) (Vallist), sizeof (*(Vallist)))

/* xargmatch calls this function when it fails.  This function should not
   return.  By default, this is a function that calls ARGMATCH_DIE which
   in turn defaults to `exit (EXIT_FAILURE)'.  */
typedef void (*argmatch_exit_fn) PARAMS ((void));
extern argmatch_exit_fn argmatch_die;

/* Report on stderr why argmatch failed.  Report correct values. */

void argmatch_invalid
  PARAMS ((const char *context, const char *value, int problem));

/* Left for compatibility with the old name invalid_arg */

# define invalid_arg(Context, Value, Problem) \
  argmatch_invalid ((Context), (Value), (Problem))



/* Report on stderr the list of possible arguments.  */

void argmatch_valid
  PARAMS ((const char *const *arglist,
	   const char *vallist, size_t valsize));

# define ARGMATCH_VALID(Arglist, Vallist) \
  argmatch_valid (Arglist, (const char *) Vallist, sizeof (*(Vallist)))



/* Same as argmatch, but upon failure, reports a explanation on the
   failure, and exits using the function EXIT_FN. */

int __xargmatch_internal
  PARAMS ((const char *context,
	   const char *arg, const char *const *arglist,
	   const char *vallist, size_t valsize,
	   int case_sensitive, argmatch_exit_fn exit_fn));

/* Programmer friendly interface to __xargmatch_internal. */

# define XARGMATCH(Context, Arg, Arglist, Vallist)			\
  (Vallist [__xargmatch_internal ((Context), (Arg), (Arglist),	\
                                  (const char *) (Vallist),	\
				  sizeof (*(Vallist)),		\
				  1, argmatch_die)])

# define XARGCASEMATCH(Context, Arg, Arglist, Vallist)		\
  (Vallist [__xargmatch_internal ((Context), (Arg), (Arglist),	\
                                  (const char *) (Vallist),	\
				  sizeof (*(Vallist)),		\
				  0, argmatch_die)])

/* Convert a value into a corresponding argument. */

const char *argmatch_to_argument
  PARAMS ((char const *value, const char *const *arglist,
	   const char *vallist, size_t valsize));

# define ARGMATCH_TO_ARGUMENT(Value, Arglist, Vallist)			\
  argmatch_to_argument ((Value), (Arglist), 		\
		        (const char *) (Vallist), sizeof (*(Vallist)))

#endif /* ARGMATCH_H_ */
