/* psx-interf.c: test POSIX interface.  */

#include <string.h>
#include <assert.h>

#include "test.h"

#define ERROR_CODE_LENGTH 20
#define TEST_ERRBUF_SIZE 15


void test_compile ();


/* ANSWER should be at least ERROR_CODE_LENGTH long.  */

static char *
get_error_string (error_code, answer)
  int error_code;
  char answer[];
{
  switch (error_code)
    {
      case 0: strcpy (answer, "No error"); break;
      case REG_NOMATCH: strcpy (answer, "REG_NOMATCH"); break;
      case REG_BADPAT: strcpy (answer, "REG_BADPAT"); break;
      case REG_EPAREN: strcpy (answer, "REG_EPAREN"); break;
      case REG_ESPACE: strcpy (answer, "REG_ESPACE"); break;
      case REG_ECOLLATE: strcpy (answer, "REG_ECOLLATE"); break;
      case REG_ECTYPE: strcpy (answer, "REG_ECTYPE"); break;
      case REG_EESCAPE: strcpy (answer, "REG_EESCAPE"); break;
      case REG_ESUBREG: strcpy (answer, "REG_ESUBREG"); break;
      case REG_EBRACK: strcpy (answer, "REG_EBRACK"); break;
      case REG_EBRACE: strcpy (answer, "REG_EBRACE"); break;
      case REG_BADBR: strcpy (answer, "REG_BADBR"); break;
      case REG_ERANGE: strcpy (answer, "REG_ERANGE"); break;
      case REG_BADRPT: strcpy (answer, "REG_BADRPT"); break;
      case REG_EEND: strcpy (answer, "REG_EEND"); break;
      default: strcpy (answer, "Bad error code");
    }
  return answer;
}


/* I don't think we actually need to initialize all these things.
   --karl  */

void
init_pattern_buffer (pattern_buffer_ptr)
    regex_t *pattern_buffer_ptr;
{
  pattern_buffer_ptr->buffer = NULL;
  pattern_buffer_ptr->allocated = 0;
  pattern_buffer_ptr->used = 0;
  pattern_buffer_ptr->fastmap = NULL;
  pattern_buffer_ptr->fastmap_accurate = 0;
  pattern_buffer_ptr->translate = NULL;
  pattern_buffer_ptr->can_be_null = 0;
  pattern_buffer_ptr->re_nsub = 0;
  pattern_buffer_ptr->no_sub = 0;
  pattern_buffer_ptr->not_bol = 0;
  pattern_buffer_ptr->not_eol = 0;
}


void
test_compile (valid_pattern, error_code_expected, pattern,
	      pattern_buffer_ptr, cflags)
    unsigned valid_pattern;
    int error_code_expected;
    const char *pattern;
    regex_t *pattern_buffer_ptr;
    int cflags;
{
  int error_code_returned;
  boolean error = false;
  char errbuf[TEST_ERRBUF_SIZE];

  init_pattern_buffer (pattern_buffer_ptr);
  error_code_returned = regcomp (pattern_buffer_ptr, pattern, cflags);

  if (valid_pattern && error_code_returned)
    {
      printf ("\nShould have been a valid pattern but wasn't.\n");
      regerror (error_code_returned, pattern_buffer_ptr, errbuf,
	        TEST_ERRBUF_SIZE);
      printf ("%s", errbuf);
      error = true;
    }

  if (!valid_pattern && !error_code_returned)
    {
      printf ("\n\nInvalid pattern compiled as valid:\n");
      error = true;
    }

  if (error_code_returned != error_code_expected)
    {
      char expected_error_string[ERROR_CODE_LENGTH];
      char returned_error_string[ERROR_CODE_LENGTH];

      get_error_string (error_code_expected, expected_error_string),
      get_error_string (error_code_returned, returned_error_string);

      printf ("  Expected error code %s but got `%s'.\n",
              expected_error_string, returned_error_string);

      error = true;
    }

  if (error)
    print_pattern_info (pattern, pattern_buffer_ptr);
}


static void
test_nsub (sub_count, pattern, cflags)
  unsigned sub_count;
  char *pattern;
  int cflags;

{
  regex_t pattern_buffer;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);

  if (pattern_buffer.re_nsub != sub_count)
    {
      printf ("\nShould have counted %d subexpressions but counted %d \
instead.\n", sub_count, pattern_buffer.re_nsub);
    }

  regfree (&pattern_buffer);
}


static void
test_regcomp ()
{
  regex_t pattern_buffer;
  int cflags = 0;


  printf ("\nStarting regcomp tests.\n");

  cflags = 0;
  test_compile (0, REG_ESUBREG, "\\(a\\)\\2", &pattern_buffer, cflags);
  test_compile (0, REG_EBRACE, "a\\{", &pattern_buffer, cflags);
  test_compile (0, REG_BADBR, "a\\{-1\\}", &pattern_buffer, cflags);
  test_compile (0, REG_EBRACE, "a\\{", &pattern_buffer, cflags);
  test_compile (0, REG_EBRACE, "a\\{1", &pattern_buffer, cflags);

  cflags = REG_EXTENDED;
  test_compile (0, REG_ECTYPE, "[[:alpo:]]", &pattern_buffer, cflags);
  test_compile (0, REG_EESCAPE, "\\", &pattern_buffer, cflags);
  test_compile (0, REG_EBRACK, "[a", &pattern_buffer, cflags);
  test_compile (0, REG_EPAREN, "(", &pattern_buffer, cflags);
  test_compile (0, REG_ERANGE, "[z-a]", &pattern_buffer, cflags);

  test_nsub (1, "(a)", cflags);
  test_nsub (2, "((a))", cflags);
  test_nsub (2, "(a)(b)", cflags);

  cflags = REG_EXTENDED | REG_NOSUB;
  test_nsub (1, "(a)", cflags);

  regfree (&pattern_buffer);

  printf ("\nFinished regcomp tests.\n");
}


static void
fill_pmatch (pmatch, start0, end0, start1, end1, start2, end2)
  regmatch_t pmatch[];
  regoff_t start0, end0, start1, end1, start2, end2;
{
  pmatch[0].rm_so = start0;
  pmatch[0].rm_eo = end0;
  pmatch[1].rm_so = start1;
  pmatch[1].rm_eo = end1;
  pmatch[2].rm_so = start2;
  pmatch[2].rm_eo = end2;
}


static void
test_pmatch (pattern, string, nmatch, pmatch, correct_pmatch, cflags)
  char *pattern;
  char *string;
  unsigned nmatch;
  regmatch_t pmatch[];
  regmatch_t correct_pmatch[];
  int cflags;
{
  regex_t pattern_buffer;
  unsigned this_match;
  int error_code_returned;
  boolean found_nonmatch = false;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);
  error_code_returned = regexec (&pattern_buffer, string, nmatch, pmatch, 0);

  if (error_code_returned == REG_NOMATCH)
    printf ("Matching failed in test_pmatch.\n");
  else
    {
      for (this_match = 0; this_match < nmatch; this_match++)
        {
          if (pmatch[this_match].rm_so != correct_pmatch[this_match].rm_so)
            {
              if (found_nonmatch == false)
                printf ("\n");

             printf ("Pmatch start %d wrong: was %d when should have \
been %d.\n", this_match, pmatch[this_match].rm_so,
		      correct_pmatch[this_match].rm_so);
              found_nonmatch = true;
            }
          if (pmatch[this_match].rm_eo != correct_pmatch[this_match].rm_eo)
            {
              if (found_nonmatch == false)
                printf ("\n");

              printf ("Pmatch end   %d wrong: was %d when should have been \
%d.\n", this_match, pmatch[this_match].rm_eo,
                       correct_pmatch[this_match].rm_eo);
              found_nonmatch = true;
            }
        }

      if (found_nonmatch)
        {
          printf ("  The number of pmatches requested was: %d.\n", nmatch);
          printf ("  The string to match was:  `%s'.\n", string);
          print_pattern_info (pattern, &pattern_buffer);
        }
    }  /* error_code_returned == REG_NOMATCH  */

  regfree (&pattern_buffer);
}


static void
test_eflags (must_match_bol, must_match_eol, pattern, string, cflags, eflags)
  boolean must_match_bol;
  boolean must_match_eol;
  char *pattern;
  char *string;
  int cflags;
  int eflags;
{
  regex_t pattern_buffer;
  int error_code_returned;
  boolean was_error = false;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);
  error_code_returned = regexec (&pattern_buffer, string, 0, 0, eflags);

  if (error_code_returned == REG_NOMATCH)
    {
      /* If wasn't true that both 1) the anchored part of the pattern
         had to match this string and 2) this string was a proper
	 substring...  */

      if (!( (must_match_bol && (eflags & REG_NOTBOL))
	     || (must_match_eol && (eflags & REG_NOTEOL)) ))
        {
          printf ("\nEflags test failed:  didn't match when should have.\n");
          was_error = true;
	}
    }
  else  /* We got a match.  */
    {
      /* If wasn't true that either 1) the anchored part of the pattern
         didn't have to match this string or 2) this string wasn't a
         proper substring...  */

      if ((must_match_bol == (eflags & REG_NOTBOL))
          || (must_match_eol == (eflags & REG_NOTEOL)))
        {
          printf ("\nEflags test failed:  matched when shouldn't have.\n");
	  was_error = true;
        }
    }

  if (was_error)
    {
      printf ("  The string to match was:  `%s'.\n", string);
      print_pattern_info (pattern, &pattern_buffer);

      if (eflags & REG_NOTBOL)
        printf ("  The eflag REG_BOL was set.\n");
      if (eflags & REG_NOTEOL)
        printf ("  The eflag REG_EOL was set.\n");
    }

  regfree (&pattern_buffer);
}


static void
test_ignore_case (should_match, pattern, string, cflags)
  boolean should_match;
  char *pattern;
  char *string;
  int cflags;
{
  regex_t pattern_buffer;
  int error_code_returned;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);
  error_code_returned = regexec (&pattern_buffer, string, 0, 0, 0);

  if (should_match && error_code_returned == REG_NOMATCH)
    {
      printf ("\nIgnore-case test failed:\n");
      printf ("  The string to match was:  `%s'.\n", string);
      print_pattern_info (pattern, &pattern_buffer);

      if (cflags & REG_ICASE)
        printf ("  The cflag REG_ICASE was set.\n");
    }

  regfree (&pattern_buffer);
}


static void
test_newline (should_match, pattern, string, cflags)
  boolean should_match;
  char *pattern;
  char *string;
  int cflags;
{
  regex_t pattern_buffer;
  int error_code_returned;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);
  error_code_returned = regexec (&pattern_buffer, string, 0, 0, 0);

  if (should_match && error_code_returned == REG_NOMATCH)
    {
      printf ("\nNewline test failed:\n");
      printf ("  The string to match was:  `%s'.\n", string);
      print_pattern_info (pattern, &pattern_buffer);

      if (cflags & REG_NEWLINE)
        printf ("  The cflag REG_NEWLINE was set.\n");
      else
        printf ("  The cflag REG_NEWLINE wasn't set.\n");
    }

  regfree (&pattern_buffer);
}


static void
test_posix_match (should_match, pattern, string, cflags)
    boolean should_match;
    char *pattern;
    char *string;
    int cflags;
{
  regex_t pattern_buffer;
  int error_code_returned;
  boolean was_error = false;

  test_compile (1, 0, pattern, &pattern_buffer, cflags);
  error_code_returned = regexec (&pattern_buffer, string, 0, 0, 0);

  if (should_match && error_code_returned == REG_NOMATCH)
    {
      printf ("\nShould have matched but didn't:\n");
      was_error = true;
    }
  else if (!should_match && error_code_returned != REG_NOMATCH)
    {
      printf ("\nShould not have matched but did:\n");
      was_error = true;
    }

  if (was_error)
    {
      printf ("  The string to match was:  `%s'.\n", string);
      print_pattern_info (pattern, &pattern_buffer);
    }

  regfree (&pattern_buffer);
}


static void
test_regexec ()
{
  regmatch_t pmatch[3];
  regmatch_t correct_pmatch[3];
  int cflags = 0;
  int eflags = 0;

  printf ("\nStarting regexec tests.\n");

  cflags = REG_NOSUB;    /* shouldn't look at any of pmatch.  */
  test_pmatch ("a", "a", 0, pmatch, correct_pmatch, cflags);

  /* Ask for less `pmatch'es than there are pattern subexpressions.
     (Shouldn't look at pmatch[2].  */
  cflags = REG_EXTENDED;
  fill_pmatch (correct_pmatch, 0, 1, 0, 1, 100, 101);
  test_pmatch ("((a))", "a", 2, pmatch, correct_pmatch, cflags);

  /* Ask for same number of `pmatch'es as there are pattern subexpressions.  */
  cflags = REG_EXTENDED;
  fill_pmatch(correct_pmatch, 0, 1, 0, 1, -1, -1);
  test_pmatch ("(a)", "a", 2, pmatch, correct_pmatch, cflags);

  /* Ask for more `pmatch'es than there are pattern subexpressions.  */
  cflags = REG_EXTENDED;
  fill_pmatch (correct_pmatch, 0, 1, -1, -1, -1, -1);
  test_pmatch ("a", "a", 2, pmatch, correct_pmatch, cflags);

  eflags = REG_NOTBOL;
  test_eflags (true, false, "^a", "a", cflags, eflags);
  test_eflags (true, false, "(^a)", "a", cflags, eflags);
  test_eflags (true, false, "a|^b", "b", cflags, eflags);
  test_eflags (true, false, "^b|a", "b", cflags, eflags);

  eflags = REG_NOTEOL;
  test_eflags (false, true, "a$", "a", cflags, eflags);
  test_eflags (false, true, "(a$)", "a", cflags, eflags);
  test_eflags (false, true, "a|b$", "b", cflags, eflags);
  test_eflags (false, true, "b$|a", "b", cflags, eflags);

  eflags = REG_NOTBOL | REG_NOTEOL;
  test_eflags (true, true, "^a$", "a", cflags, eflags);
  test_eflags (true, true, "(^a$)", "a", cflags, eflags);
  test_eflags (true, true, "a|(^b$)", "b", cflags, eflags);
  test_eflags (true, true, "(^b$)|a", "b", cflags, eflags);

  cflags = REG_ICASE;
  test_ignore_case (true, "a", "a", cflags);
  test_ignore_case (true, "A", "A", cflags);
  test_ignore_case (true, "A", "a", cflags);
  test_ignore_case (true, "a", "A", cflags);

  test_ignore_case (true, "@", "@", cflags);
  test_ignore_case (true, "\\[", "[", cflags);
  test_ignore_case (true, "`", "`", cflags);
  test_ignore_case (true, "{", "{", cflags);

  test_ignore_case (true, "[!-`]", "A", cflags);
  test_ignore_case (true, "[!-`]", "a", cflags);

  cflags = 0;
  test_ignore_case (false, "a", "a", cflags);
  test_ignore_case (false, "A", "A", cflags);
  test_ignore_case (false, "A", "a", cflags);
  test_ignore_case (false, "a", "A", cflags);

  test_ignore_case (true, "@", "@", cflags);
  test_ignore_case (true, "\\[", "[", cflags);
  test_ignore_case (true, "`", "`", cflags);
  test_ignore_case (true, "{", "{", cflags);

  test_ignore_case (true, "[!-`]", "A", cflags);
  test_ignore_case (false, "[!-`]", "a", cflags);


  /* Test newline stuff.  */
  cflags = REG_EXTENDED | REG_NEWLINE;
  test_newline (true, "\n", "\n", cflags);
  test_newline (true, "a\n", "a\n", cflags);
  test_newline (true, "\nb", "\nb", cflags);
  test_newline (true, "a\nb", "a\nb", cflags);

  test_newline (false, ".", "\n", cflags);
  test_newline (false, "[^a]", "\n", cflags);

  test_newline (true, "\n^a", "\na", cflags);
  test_newline (true, "\n(^a|b)", "\na", cflags);
  test_newline (true, "a$\n", "a\n", cflags);
  test_newline (true, "(a$|b)\n", "a\n", cflags);
  test_newline (true, "(a$|b|c)\n", "a\n", cflags);
  test_newline (true, "((a$|b|c)$)\n", "a\n", cflags);
  test_newline (true, "((a$|b|c)$)\n", "b\n", cflags);
  test_newline (true, "(a$|b)\n|a\n", "a\n", cflags);

  test_newline (true, "^a", "\na", cflags);
  test_newline (true, "a$", "a\n", cflags);

  /* Now test normal behavior.  */
  cflags = REG_EXTENDED;
  test_newline (true, "\n", "\n", cflags);
  test_newline (true, "a\n", "a\n", cflags);
  test_newline (true, "\nb", "\nb", cflags);
  test_newline (true, "a\nb", "a\nb", cflags);

  test_newline (true, ".", "\n", cflags);
  test_newline (true, "[^a]", "\n", cflags);

  test_newline (false, "\n^a", "\na", cflags);
  test_newline (false, "a$\n", "a\n", cflags);

  test_newline (false, "^a", "\na", cflags);
  test_newline (false, "a$", "a\n", cflags);


  /* Test that matches whole string only.  */
  cflags = 0;
  test_posix_match (true, "a", "a", cflags);

  /* Tests that match substrings.  */
  test_posix_match (true, "a", "ab", cflags);
  test_posix_match (true, "b", "ab", cflags);

  /* Test that doesn't match.  */
  test_posix_match (false, "a", "b", cflags);

  printf ("\nFinished regexec tests.\n");
}


static void
test_error_code_message (error_code, expected_error_message)
  int error_code;
  char *expected_error_message;
{
  char returned_error_message[TEST_ERRBUF_SIZE];
  char error_code_string[ERROR_CODE_LENGTH];
  size_t expected_error_message_length = strlen (expected_error_message) + 1;
  size_t returned_error_message_length = regerror (error_code, 0,
					             returned_error_message,
	                                             TEST_ERRBUF_SIZE);

  if (returned_error_message_length != expected_error_message_length)
    {
      printf ("\n\n  Testing returned error codes, with expected error \
message  `%s':\n", expected_error_message);

      printf ("\n\n  and returned error message `%s':\n",
       	      returned_error_message);
      printf ("  should have returned a length of %d but returned %d.\n",
	      expected_error_message_length, returned_error_message_length);
    }

  if (strncmp (expected_error_message, returned_error_message,
	       TEST_ERRBUF_SIZE - 1) != 0)
    {

      get_error_string (error_code, error_code_string),
      printf ("\n\n  With error code %s (%d), expected error message:\n",
      	      error_code_string, error_code);

      printf ("    `%s'\n", expected_error_message);
      printf ("  but got:\n");
      printf ("    `%s'\n", returned_error_message);
    }
}


static void
test_error_code_allocation (error_code, expected_error_message)
  int error_code;
  char *expected_error_message;
{
  char *returned_error_message = NULL;
  char error_code_string[ERROR_CODE_LENGTH];
  size_t returned_error_message_length = regerror (error_code, 0,
					             returned_error_message,
	                                             (size_t)0);

  returned_error_message = xmalloc (returned_error_message_length + 1);

  regerror (error_code, 0, returned_error_message,
	    returned_error_message_length);

  if (strcmp (expected_error_message, returned_error_message) != 0)
    {
      get_error_string (error_code, error_code_string),

      printf ("\n\n  Testing error code allocation,\n");
      printf ("with error code %s (%d), expected error message:\n",
	       error_code_string, error_code);
      printf ("    `%s'\n", expected_error_message);
      printf ("  but got:\n");
      printf ("    `%s'\n", returned_error_message);
    }
}


static void
test_regerror ()
{
  test_error_code_message (REG_NOMATCH, "No match");
  test_error_code_message (REG_BADPAT, "Invalid regular expression");
  test_error_code_message (REG_ECOLLATE, "Invalid collation character");
  test_error_code_message (REG_ECTYPE, "Invalid character class name");
  test_error_code_message (REG_EESCAPE, "Trailing backslash");
  test_error_code_message (REG_ESUBREG, "Invalid back reference");
  test_error_code_message (REG_EBRACK, "Unmatched [ or [^");
  test_error_code_message (REG_EPAREN, "Unmatched ( or \\(");
  test_error_code_message (REG_EBRACE, "Unmatched \\{");
  test_error_code_message (REG_BADBR, "Invalid content of \\{\\}");
  test_error_code_message (REG_ERANGE, "Invalid range end");
  test_error_code_message (REG_ESPACE, "Memory exhausted");
  test_error_code_message (REG_BADRPT, "Invalid preceding regular expression");
  test_error_code_message (REG_EEND, "Premature end of regular expression");
  test_error_code_message (REG_ESIZE, "Regular expression too big");
  test_error_code_allocation (REG_ERPAREN, "Unmatched ) or \\)");
}


void
test_posix_interface ()
{
  printf ("\nStarting POSIX interface tests.\n");
  t = posix_interface_test;

  test_regcomp ();
  test_regexec ();
  test_regerror ();

  printf ("\nFinished POSIX interface tests.\n");
}
