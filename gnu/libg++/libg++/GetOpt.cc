/*
Getopt for GNU. 
Copyright (C) 1987, 1989 Free Software Foundation, Inc.

(Modified by Douglas C. Schmidt for use with GNU G++.)
This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
/* AIX requires the alloca decl to be the first thing in the file. */
#ifdef __GNUC__
#define alloca __builtin_alloca
#elif defined(sparc)
#include <alloca.h>
extern "C" void *__builtin_alloca(...);
#elif defined(_AIX)
#pragma alloca
#else
char *alloca ();
#endif
#include <GetOpt.h>

char* GetOpt::nextchar = 0;
int GetOpt::first_nonopt = 0;
int GetOpt::last_nonopt = 0;

GetOpt::GetOpt (int argc, char **argv, const char *optstring)
 :opterr (1), nargc (argc), nargv (argv), noptstring (optstring)
{
  /* Initialize the internal data when the first call is made.
     Start processing options with ARGV-element 1 (since ARGV-element 0
     is the program name); the sequence of previously skipped
     non-option ARGV-elements is empty.  */

  first_nonopt = last_nonopt = optind = 1;
  optarg = nextchar = 0;

  /* Determine how to handle the ordering of options and nonoptions.  */

  if (optstring[0] == '-')
    ordering = RETURN_IN_ORDER;
  else if (getenv ("_POSIX_OPTION_ORDER") != 0)
    ordering = REQUIRE_ORDER;
  else
    ordering = PERMUTE;
}

void
GetOpt::exchange (char **argv)
{
  int nonopts_size
    = (last_nonopt - first_nonopt) * sizeof (char *);
  char **temp = (char **) alloca (nonopts_size);

  /* Interchange the two blocks of data in argv.  */

  memcpy (temp, &argv[first_nonopt], nonopts_size);
  memcpy (&argv[first_nonopt], &argv[last_nonopt],
         (optind - last_nonopt) * sizeof (char *));
  memcpy (&argv[first_nonopt + optind - last_nonopt], temp,
         nonopts_size);

  /* Update records for the slots the non-options now occupy.  */

  first_nonopt += (optind - last_nonopt);
  last_nonopt = optind;
}

/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `getopt'
   is called repeatedly, it returns successively each of theoption characters
   from each of the option elements.

   If `getopt' finds another option character, it returns that character,
   updating `optind' and `nextchar' so that the next call to `getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `getopt' returns `EOF'.
   Then `optind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   A colon in OPTSTRING means that the previous character is an option
   that wants an argument.  The argument is taken from the rest of the
   current ARGV-element, or from the following ARGV-element,
   and returned in `optarg'.

   If an option character is seen that is not listed in OPTSTRING,
   return '?' after printing an error message.  If you set `opterr' to
   zero, the error message is suppressed but we still return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `optarg.  Two colons mean an option that
   wants an optional arg; if there is text in the current ARGV-element,
   it is returned in `optarg'.

   If OPTSTRING starts with `-', it requests a different method of handling the
   non-option ARGV-elements.  See the comments about RETURN_IN_ORDER, above.  */

int
GetOpt::operator () (void)
{
  if (nextchar == 0 || *nextchar == 0)
    {
      if (ordering == PERMUTE)
        {
          /* If we have just processed some options following some non-options,
             exchange them so that the options come first.  */

          if (first_nonopt != last_nonopt && last_nonopt != optind)
            exchange (nargv);
          else if (last_nonopt != optind)
            first_nonopt = optind;

          /* Now skip any additional non-options
             and extend the range of non-options previously skipped.  */

          while (optind < nargc
                 && (nargv[optind][0] != '-'
                     || nargv[optind][1] == 0))
            optind++;
          last_nonopt = optind;
        }

      /* Special ARGV-element `--' means premature end of options.
         Skip it like a null option,
         then exchange with previous non-options as if it were an option,
         then skip everything else like a non-option.  */

      if (optind != nargc && !strcmp (nargv[optind], "--"))
        {
          optind++;

          if (first_nonopt != last_nonopt && last_nonopt != optind)
            exchange (nargv);
          else if (first_nonopt == last_nonopt)
            first_nonopt = optind;
          last_nonopt = nargc;

          optind = nargc;
        }

      /* If we have done all the ARGV-elements, stop the scan
         and back over any non-options that we skipped and permuted.  */

      if (optind == nargc)
        {
          /* Set the next-arg-index to point at the non-options
             that we previously skipped, so the caller will digest them.  */
          if (first_nonopt != last_nonopt)
            optind = first_nonopt;
          return EOF;
        }
	 
      /* If we have come to a non-option and did not permute it,
         either stop the scan or describe it to the caller and pass it by.  */

      if (nargv[optind][0] != '-' || nargv[optind][1] == 0)
        {
          if (ordering == REQUIRE_ORDER)
            return EOF;
          optarg = nargv[optind++];
          return 0;
        }

      /* We have found another option-ARGV-element.
         Start decoding its characters.  */

      nextchar = nargv[optind] + 1;
    }

  /* Look at and handle the next option-character.  */

  {
    char c = *nextchar++;
    char *temp = (char *) strchr (noptstring, c);

    /* Increment `optind' when we start to process its last character.  */
    if (*nextchar == 0)
      optind++;

    if (temp == 0 || c == ':')
      {
        if (opterr != 0)
          {
            if (c < 040 || c >= 0177)
              fprintf (stderr, "%s: unrecognized option, character code 0%o\n",
                       nargv[0], c);
            else
              fprintf (stderr, "%s: unrecognized option `-%c'\n",
                       nargv[0], c);
          }
        return '?';
      }
    if (temp[1] == ':')
      {
        if (temp[2] == ':')
          {
            /* This is an option that accepts an argument optionally.  */
            if (*nextchar != 0)
              {
                optarg = nextchar;
                optind++;
              }
            else
              optarg = 0;
            nextchar = 0;
          }
        else
          {
            /* This is an option that requires an argument.  */
            if (*nextchar != 0)
              {
                optarg = nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                optind++;
              }
            else if (optind == nargc)
              {
                if (opterr != 0)
                  fprintf (stderr, "%s: no argument for `-%c' option\n",
                           nargv[0], c);
                c = '?';
              }
            else
              /* We already incremented `optind' once;
                 increment it again when taking next ARGV-elt as argument.  */
              optarg = nargv[optind++];
            nextchar = 0;
          }
      }
    return c;
  }
}
