/* modechange.c -- file mode manipulation
   Copyright (C) 1989, 1990, 1997, 1998, 1999, 2001 Free Software Foundation, Inc.

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

/* Written by David MacKenzie <djm@ai.mit.edu> */

/* The ASCII mode string is compiled into a linked list of `struct
   modechange', which can then be applied to each file to be changed.
   We do this instead of re-parsing the ASCII string for each file
   because the compiled form requires less computation to use; when
   changing the mode of many files, this probably results in a
   performance gain. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "modechange.h"
#include <sys/stat.h>
#include "xstrtol.h"

#if STDC_HEADERS
# include <stdlib.h>
#else
char *malloc ();
#endif

#ifndef NULL
# define NULL 0
#endif

#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/* The traditional octal values corresponding to each mode bit.  */
#define SUID 04000
#define SGID 02000
#define SVTX 01000
#define RUSR 00400
#define WUSR 00200
#define XUSR 00100
#define RGRP 00040
#define WGRP 00020
#define XGRP 00010
#define ROTH 00004
#define WOTH 00002
#define XOTH 00001
#define ALLM 07777 /* all octal mode bits */

#ifndef S_ISUID
# define S_ISUID SUID
#endif
#ifndef S_ISGID
# define S_ISGID SGID
#endif
#ifndef S_ISVTX
# define S_ISVTX SVTX
#endif
#ifndef S_IRUSR
# define S_IRUSR RUSR
#endif
#ifndef S_IWUSR
# define S_IWUSR WUSR
#endif
#ifndef S_IXUSR
# define S_IXUSR XUSR
#endif
#ifndef S_IRGRP
# define S_IRGRP RGRP
#endif
#ifndef S_IWGRP
# define S_IWGRP WGRP
#endif
#ifndef S_IXGRP
# define S_IXGRP XGRP
#endif
#ifndef S_IROTH
# define S_IROTH ROTH
#endif
#ifndef S_IWOTH
# define S_IWOTH WOTH
#endif
#ifndef S_IXOTH
# define S_IXOTH XOTH
#endif
#ifndef S_IRWXU
# define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#ifndef S_IRWXG
# define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#endif
#ifndef S_IRWXO
# define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#endif

/* All the mode bits that can be affected by chmod.  */
#define CHMOD_MODE_BITS \
  (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

/* Return newly allocated memory to hold one element of type TYPE. */
#define talloc(type) ((type *) malloc (sizeof (type)))

/* Create a mode_change entry with the specified `=ddd'-style
   mode change operation, where NEW_MODE is `ddd'.  Return the
   new entry, or NULL upon failure.  */

static struct mode_change *
make_node_op_equals (mode_t new_mode)
{
  struct mode_change *p;
  p = talloc (struct mode_change);
  if (p == NULL)
    return p;
  p->next = NULL;
  p->op = '=';
  p->flags = 0;
  p->value = new_mode;
  p->affected = CHMOD_MODE_BITS;	/* Affect all permissions. */
  return p;
}

/* Append entry E to the end of the link list with the specified
   HEAD and TAIL.  */

static void
mode_append_entry (struct mode_change **head,
		   struct mode_change **tail,
		   struct mode_change *e)
{
  if (*head == NULL)
    *head = *tail = e;
  else
    {
      (*tail)->next = e;
      *tail = e;
    }
}

/* Return a linked list of file mode change operations created from
   MODE_STRING, an ASCII string that contains either an octal number
   specifying an absolute mode, or symbolic mode change operations with
   the form:
   [ugoa...][[+-=][rwxXstugo...]...][,...]
   MASKED_OPS is a bitmask indicating which symbolic mode operators (=+-)
   should not affect bits set in the umask when no users are given.
   Operators not selected in MASKED_OPS ignore the umask.

   Return MODE_INVALID if `mode_string' does not contain a valid
   representation of file mode change operations;
   return MODE_MEMORY_EXHAUSTED if there is insufficient memory. */

struct mode_change *
mode_compile (const char *mode_string, unsigned int masked_ops)
{
  struct mode_change *head;	/* First element of the linked list. */
  struct mode_change *tail;	/* An element of the linked list. */
  unsigned long octal_value;	/* The mode value, if octal.  */
  mode_t umask_value;		/* The umask value (surprise). */

  head = NULL;
#ifdef lint
  tail = NULL;
#endif

  if (xstrtoul (mode_string, NULL, 8, &octal_value, "") == LONGINT_OK)
    {
      struct mode_change *p;
      mode_t mode;
      if (octal_value != (octal_value & ALLM))
	return MODE_INVALID;

      /* Help the compiler optimize the usual case where mode_t uses
	 the traditional octal representation.  */
      mode = ((S_ISUID == SUID && S_ISGID == SGID && S_ISVTX == SVTX
	       && S_IRUSR == RUSR && S_IWUSR == WUSR && S_IXUSR == XUSR
	       && S_IRGRP == RGRP && S_IWGRP == WGRP && S_IXGRP == XGRP
	       && S_IROTH == ROTH && S_IWOTH == WOTH && S_IXOTH == XOTH)
	      ? octal_value
	      : ((octal_value & SUID ? S_ISUID : 0)
		 | (octal_value & SGID ? S_ISGID : 0)
		 | (octal_value & SVTX ? S_ISVTX : 0)
		 | (octal_value & RUSR ? S_IRUSR : 0)
		 | (octal_value & WUSR ? S_IWUSR : 0)
		 | (octal_value & XUSR ? S_IXUSR : 0)
		 | (octal_value & RGRP ? S_IRGRP : 0)
		 | (octal_value & WGRP ? S_IWGRP : 0)
		 | (octal_value & XGRP ? S_IXGRP : 0)
		 | (octal_value & ROTH ? S_IROTH : 0)
		 | (octal_value & WOTH ? S_IWOTH : 0)
		 | (octal_value & XOTH ? S_IXOTH : 0)));

      p = make_node_op_equals (mode);
      if (p == NULL)
	return MODE_MEMORY_EXHAUSTED;
      mode_append_entry (&head, &tail, p);
      return head;
    }

  umask_value = umask (0);
  umask (umask_value);		/* Restore the old value. */
  --mode_string;

  /* One loop iteration for each "ugoa...=+-rwxXstugo...[=+-rwxXstugo...]". */
  do
    {
      /* Which bits in the mode are operated on. */
      mode_t affected_bits = 0;
      /* `affected_bits' modified by umask. */
      mode_t affected_masked;
      /* Operators to actually use umask on. */
      unsigned ops_to_mask = 0;

      int who_specified_p;

      affected_bits = 0;
      ops_to_mask = 0;
      /* Turn on all the bits in `affected_bits' for each group given. */
      for (++mode_string;; ++mode_string)
	switch (*mode_string)
	  {
	  case 'u':
	    affected_bits |= S_ISUID | S_IRWXU;
	    break;
	  case 'g':
	    affected_bits |= S_ISGID | S_IRWXG;
	    break;
	  case 'o':
	    affected_bits |= S_ISVTX | S_IRWXO;
	    break;
	  case 'a':
	    affected_bits |= CHMOD_MODE_BITS;
	    break;
	  default:
	    goto no_more_affected;
	  }

    no_more_affected:
      /* If none specified, affect all bits, except perhaps those
	 set in the umask. */
      if (affected_bits)
	who_specified_p = 1;
      else
	{
	  who_specified_p = 0;
	  affected_bits = CHMOD_MODE_BITS;
	  ops_to_mask = masked_ops;
	}

      while (*mode_string == '=' || *mode_string == '+' || *mode_string == '-')
	{
	  struct mode_change *change = talloc (struct mode_change);
	  if (change == NULL)
	    {
	      mode_free (head);
	      return MODE_MEMORY_EXHAUSTED;
	    }

	  change->next = NULL;
	  change->op = *mode_string;	/* One of "=+-". */
	  affected_masked = affected_bits;

	  /* Per the Single Unix Spec, if `who' is not specified and the
	     `=' operator is used, then clear all the bits first.  */
	  if (!who_specified_p &&
	      ops_to_mask & (*mode_string == '=' ? MODE_MASK_EQUALS : 0))
	    {
	      struct mode_change *p = make_node_op_equals (0);
	      if (p == NULL)
		return MODE_MEMORY_EXHAUSTED;
	      mode_append_entry (&head, &tail, p);
	    }

	  if (ops_to_mask & (*mode_string == '=' ? MODE_MASK_EQUALS
			     : *mode_string == '+' ? MODE_MASK_PLUS
			     : MODE_MASK_MINUS))
	    affected_masked &= ~umask_value;
	  change->affected = affected_masked;
	  change->value = 0;
	  change->flags = 0;

	  /* Add the element to the tail of the list, so the operations
	     are performed in the correct order. */
	  mode_append_entry (&head, &tail, change);

	  /* Set `value' according to the bits set in `affected_masked'. */
	  for (++mode_string;; ++mode_string)
	    switch (*mode_string)
	      {
	      case 'r':
		change->value |= ((S_IRUSR | S_IRGRP | S_IROTH)
				  & affected_masked);
		break;
	      case 'w':
		change->value |= ((S_IWUSR | S_IWGRP | S_IWOTH)
				  & affected_masked);
		break;
	      case 'X':
		change->flags |= MODE_X_IF_ANY_X;
		/* Fall through. */
	      case 'x':
		change->value |= ((S_IXUSR | S_IXGRP | S_IXOTH)
				  & affected_masked);
		break;
	      case 's':
		/* Set the setuid/gid bits if `u' or `g' is selected. */
		change->value |= (S_ISUID | S_ISGID) & affected_masked;
		break;
	      case 't':
		/* Set the "save text image" bit if `o' is selected. */
		change->value |= S_ISVTX & affected_masked;
		break;
	      case 'u':
		/* Set the affected bits to the value of the `u' bits
		   on the same file.  */
		if (change->value)
		  goto invalid;
		change->value = S_IRWXU;
		change->flags |= MODE_COPY_EXISTING;
		break;
	      case 'g':
		/* Set the affected bits to the value of the `g' bits
		   on the same file.  */
		if (change->value)
		  goto invalid;
		change->value = S_IRWXG;
		change->flags |= MODE_COPY_EXISTING;
		break;
	      case 'o':
		/* Set the affected bits to the value of the `o' bits
		   on the same file.  */
		if (change->value)
		  goto invalid;
		change->value = S_IRWXO;
		change->flags |= MODE_COPY_EXISTING;
		break;
	      default:
		goto no_more_values;
	      }
	no_more_values:;
	}
  } while (*mode_string == ',');
  if (*mode_string == 0)
    return head;
invalid:
  mode_free (head);
  return MODE_INVALID;
}

/* Return a file mode change operation that sets permissions to match those
   of REF_FILE.  Return MODE_BAD_REFERENCE if REF_FILE can't be accessed.  */

struct mode_change *
mode_create_from_ref (const char *ref_file)
{
  struct mode_change *change;	/* the only change element */
  struct stat ref_stats;

  if (stat (ref_file, &ref_stats))
    return MODE_BAD_REFERENCE;

  change = talloc (struct mode_change);

  if (change == NULL)
    return MODE_MEMORY_EXHAUSTED;

  change->op = '=';
  change->flags = 0;
  change->affected = CHMOD_MODE_BITS;
  change->value = ref_stats.st_mode;
  change->next = NULL;

  return change;
}

/* Return file mode OLDMODE, adjusted as indicated by the list of change
   operations CHANGES.  If OLDMODE is a directory, the type `X'
   change affects it even if no execute bits were set in OLDMODE.
   The returned value has the S_IFMT bits cleared. */

mode_t
mode_adjust (mode_t oldmode, const struct mode_change *changes)
{
  mode_t newmode;	/* The adjusted mode and one operand. */
  mode_t value;		/* The other operand. */

  newmode = oldmode & CHMOD_MODE_BITS;

  for (; changes; changes = changes->next)
    {
      if (changes->flags & MODE_COPY_EXISTING)
	{
	  /* Isolate in `value' the bits in `newmode' to copy, given in
	     the mask `changes->value'. */
	  value = newmode & changes->value;

	  if (changes->value & S_IRWXU)
	    /* Copy `u' permissions onto `g' and `o'. */
	    value |= ((value & S_IRUSR ? S_IRGRP | S_IROTH : 0)
		      | (value & S_IWUSR ? S_IWGRP | S_IROTH : 0)
		      | (value & S_IXUSR ? S_IXGRP | S_IXOTH : 0));
	  else if (changes->value & S_IRWXG)
	    /* Copy `g' permissions onto `u' and `o'. */
	    value |= ((value & S_IRGRP ? S_IRUSR | S_IROTH : 0)
		      | (value & S_IWGRP ? S_IWUSR | S_IROTH : 0)
		      | (value & S_IXGRP ? S_IXUSR | S_IXOTH : 0));
	  else
	    /* Copy `o' permissions onto `u' and `g'. */
	    value |= ((value & S_IROTH ? S_IRUSR | S_IRGRP : 0)
		      | (value & S_IWOTH ? S_IWUSR | S_IRGRP : 0)
		      | (value & S_IXOTH ? S_IXUSR | S_IXGRP : 0));

	  /* In order to change only `u', `g', or `o' permissions,
	     or some combination thereof, clear unselected bits.
	     This cannot be done in mode_compile because the value
	     to which the `changes->affected' mask is applied depends
	     on the old mode of each file. */
	  value &= changes->affected;
	}
      else
	{
	  value = changes->value;
	  /* If `X', do not affect the execute bits if the file is not a
	     directory and no execute bits are already set. */
	  if ((changes->flags & MODE_X_IF_ANY_X)
	      && !S_ISDIR (oldmode)
	      && (newmode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
	    /* Clear the execute bits. */
	    value &= ~ (S_IXUSR | S_IXGRP | S_IXOTH);
	}

      switch (changes->op)
	{
	case '=':
	  /* Preserve the previous values in `newmode' of bits that are
	     not affected by this change operation. */
	  newmode = (newmode & ~changes->affected) | value;
	  break;
	case '+':
	  newmode |= value;
	  break;
	case '-':
	  newmode &= ~value;
	  break;
	}
    }
  return newmode;
}

/* Free the memory used by the list of file mode change operations
   CHANGES. */

void
mode_free (register struct mode_change *changes)
{
  register struct mode_change *next;

  while (changes)
    {
      next = changes->next;
      free (changes);
      changes = next;
    }
}
