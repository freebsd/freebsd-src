/* psx-generic.c: test POSIX re's independent of us using basic or
   extended syntax.  */

#include "test.h"


void
test_posix_generic ()
{
  int omit_generic_tests = 0; /* reset in debugger to skip */

  if (omit_generic_tests)
    return;
                                /* Tests somewhat in the order of P1003.2.  */

  /* Both posix basic and extended; should match.  */

  printf ("\nStarting generic POSIX tests.\n");
  test_grouping ();
  test_intervals ();

  test_should_match = true;
						  /* Ordinary characters.  */
  printf ("\nContinuing generic POSIX tests.\n");

  MATCH_SELF ("");
  test_fastmap ("", "", 0, 0);
  test_fastmap_search ("", "", "", 0, 0, 2, 0, 0);
  TEST_REGISTERS ("", "", 0, 0, -1, -1, -1, -1);
  TEST_SEARCH ("", "", 0, 0);
  TEST_SEARCH_2 ("", "", "", 0, 1, 0);

  MATCH_SELF ("abc");
  test_fastmap ("abc", "a", 0, 0);
  TEST_REGISTERS ("abc", "abc", 0, 3, -1, -1, -1, -1);
  TEST_REGISTERS ("abc", "xabcx", 1, 4, -1, -1, -1, -1);

  test_match ("\\a","a");
  test_match ("\\0", "0");

  TEST_SEARCH ("a", "ab", 0, 2);
  TEST_SEARCH ("b", "ab", 0, 2);
  TEST_SEARCH ("a", "ab", 1, -2);
  TEST_SEARCH_2 ("a", "a", "b", 0, 2, 2);
  TEST_SEARCH_2 ("b", "a", "b", 0, 2, 2);
  TEST_SEARCH_2 ("a", "a", "b", 1, -2, 2);

  test_match ("\n", "\n");
  test_match ("a\n", "a\n");
  test_match ("\nb", "\nb");
  test_match ("a\nb", "a\nb");

  TEST_SEARCH ("b", "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 236, -237);
					/* Valid use of special characters. */
  test_match ("a*", "aa");
  test_fastmap ("a*", "a", 0, 0);
  TEST_REGISTERS ("a*", "aa", 0, 2, -1, -1, -1, -1);

  test_match ("a*b", "aab");
  test_fastmap ("a*b", "ab", 0, 0);

  test_match ("a*ab", "aab");
  TEST_REGISTERS ("a*a", "aa", 0, 2, -1, -1, -1, -1);
  TEST_REGISTERS ("a*a", "xaax", 1, 3, -1, -1, -1, -1);

  test_match ("\\{", "{");
  test_match ("\\^", "^");
  test_match ("\\.", ".");
  test_match ("\\*", "*");
  test_match ("\\[", "[");
  test_match ("\\$", "$");
  test_match ("\\\\", "\\");

  test_match ("ab*", "a");
  test_match ("ab*", "abb");

  /* Valid consecutive repetitions.  */
  test_match ("a**", "a");
                                                /* Valid period.  */
  test_match (".", "a");
  TEST_REGISTERS (".", "a", 0, 1, -1, -1, -1, -1);
  test_match (".", "\004");
  test_match (".", "\n");
                                              /* Valid bracket expressions.  */
  test_match ("[ab]", "a");
  test_match ("[ab]", "b");
  test_fastmap ("[ab]", "ab", 0, 0);
  TEST_REGISTERS ("[ab]", "a", 0, 1, -1, -1, -1, -1);
  TEST_REGISTERS ("[ab]", "xax", 1, 2, -1, -1, -1, -1);

  test_fastmap ("[^ab]", "ab", 1, 1);
  test_match ("[^ab]", "c");
  test_match ("[^a]", "\n");

  test_match ("[a]*a", "aa");

  test_match ("[[]", "[");
  test_match ("[]]", "]");
  test_match ("[.]", ".");
  test_match ("[*]", "*");
  test_match ("[\\]", "\\");
  test_match ("[\\(]", "(");
  test_match ("[\\)]", ")");
  test_match ("[^]]", "a");
  test_match ("[a^]", "^");
  test_match ("[a$]", "$");
  test_match ("[]a]", "]");
  test_match ("[a][]]", "a]");
  test_match ("[\n]", "\n");
  test_match ("[^a]", "\n");
  test_match ("[a-]", "a");

  TEST_REGISTERS ("\\`[ \t\n]*", " karl (Karl Berry)", 0, 1, -1, -1, -1, -1);
  TEST_REGISTERS ("[ \t\n]*\\'", " karl (Karl Berry)", 18, 18, -1, -1, -1, -1);

                                                /* Collating, noncollating,
                                             	   equivalence classes aren't
                                           	   implemented yet.  */


                                                /* Character classes.  */
  test_match ("[:alpha:]", "p");
  test_match ("[[:alpha:]]", "a");
  test_match ("[[:alpha:]]", "z");
  test_match ("[[:alpha:]]", "A");
  test_match ("[[:alpha:]]", "Z");
  test_match ("[[:upper:]]", "A");
  test_match ("[[:upper:]]", "Z");
  test_match ("[[:lower:]]", "a");
  test_match ("[[:lower:]]", "z");

  test_match ("[[:digit:]]", "0");
  test_match ("[[:digit:]]", "9");
  test_fastmap ("[[:digit:]]", "0123456789", 0, 0);

  test_match ("[[:alnum:]]", "0");
  test_match ("[[:alnum:]]", "9");
  test_match ("[[:alnum:]]", "a");
  test_match ("[[:alnum:]]", "z");
  test_match ("[[:alnum:]]", "A");
  test_match ("[[:alnum:]]", "Z");
  test_match ("[[:xdigit:]]", "0");
  test_match ("[[:xdigit:]]", "9");
  test_match ("[[:xdigit:]]", "A");
  test_match ("[[:xdigit:]]", "F");
  test_match ("[[:xdigit:]]", "a");
  test_match ("[[:xdigit:]]", "f");
  test_match ("[[:space:]]", " ");
  test_match ("[[:print:]]", " ");
  test_match ("[[:print:]]", "~");
  test_match ("[[:punct:]]", ",");
  test_match ("[[:graph:]]", "!");
  test_match ("[[:graph:]]", "~");
  test_match ("[[:cntrl:]]", "\177");
  test_match ("[[:digit:]a]", "a");
  test_match ("[[:digit:]a]", "2");
  test_match ("[a[:digit:]]", "a");
  test_match ("[a[:digit:]]", "2");
  test_match ("[[:]", "[");
  test_match ("[:]", ":");
  test_match ("[[:a]", "[");
  test_match ("[[:alpha:a]", "[");
  						/* Valid ranges.  */
  test_match ("[a-a]", "a");
  test_fastmap ("[a-a]", "a", 0, 0);
  TEST_REGISTERS ("[a-a]", "xax", 1, 2, -1, -1, -1, -1);

  test_match ("[a-z]", "z");
  test_fastmap ("[a-z]", "abcdefghijklmnopqrstuvwxyz", 0, 0);
  test_match ("[-a]", "-");		/* First  */
  test_match ("[-a]", "a");
  test_match ("[a-]", "-");		/* Last  */
  test_match ("[a-]", "a");
  test_match ("[--@]", "@");		/* First and starting point.  */

  test_match ("[%--a]", "%");		/* Ending point.  */
  test_match ("[%--a]", "-");		/* Ditto.  */

  test_match ("[a%--]", "%");		/* Both ending point and last.  */
  test_match ("[a%--]", "-");
  test_match ("[%--a]", "a");		/* Ending point only.  */
  test_match ("[a-c-f]", "e");		/* Piggyback.  */

  test_match ("[)-+--/]", "*");
  test_match ("[)-+--/]", ",");
  test_match ("[)-+--/]", "/");
  test_match ("[[:digit:]-]", "-");
                                        /* Concatenation  ????*/
  test_match ("[ab][cd]", "ac");
  test_fastmap ("[ab][cd]", "ab", 0, 0);
  TEST_REGISTERS ("[ab][cd]", "ad", 0, 2, -1, -1, -1, -1);
  TEST_REGISTERS ("[ab][cd]", "xadx", 1, 3, -1, -1, -1, -1);

                                             /* Valid expression anchoring.  */
  test_match ("^a", "a");
  test_fastmap ("^a", "a", 0, 0);
  TEST_REGISTERS ("^a", "ax", 0, 1, -1, -1, -1, -1);

  test_match ("^", "");
  TEST_REGISTERS ("^", "", 0, 0, -1, -1, -1, -1);
  test_match ("$", "");
  TEST_REGISTERS ("$", "", 0, 0, -1, -1, -1, -1);

  test_match ("a$", "a");
  test_fastmap ("a$", "a", 0, 0);
  TEST_REGISTERS ("a$", "xa", 1, 2, -1, -1, -1, -1);

  test_match ("^ab$", "ab");
  test_fastmap ("^ab$", "a", 0, 0);
  TEST_REGISTERS ("^a$", "a", 0, 1, -1, -1, -1, -1);

  test_fastmap ("^$", "", 0, 0);
  test_match ("^$", "");
  TEST_REGISTERS ("^$", "", 0, 0, -1, -1, -1, -1);

  TEST_SEARCH (PARENS_TO_OPS ("(^a)"), "ab", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("(a$)"), "ba", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("^(^a)"), "ab", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("(a$)$"), "ba", 0, 2);

                                                /* Two strings.  */
  test_match_2 ("ab", "a", "b");
  TEST_REGISTERS_2 ("ab", "a", "b", 0, 2, -1, -1, -1, -1);

  test_match_2 ("a", "", "a");
  test_match_2 ("a", "a", "");
  test_match_2 ("ab", "a", "b");
						/* (start)pos.  */
  TEST_POSITIONED_MATCH ("b", "ab", 1);
  						/* mstop.  */
  TEST_TRUNCATED_MATCH ("a", "ab", 1);


  /* Both basic and extended, continued; should not match.  */

  test_should_match = false;
                                          /* Ordinary characters.  */
  test_match ("abc", "ab");

  TEST_SEARCH ("c", "ab", 0, 2);
  TEST_SEARCH ("c", "ab", 0, 2);
  TEST_SEARCH ("c", "ab", 1, -2);
  TEST_SEARCH ("c", "ab", 0, 10);
  TEST_SEARCH ("c", "ab", 1, -10);
  TEST_SEARCH_2 ("c", "a", "b", 0, 2, 2);
  TEST_SEARCH_2 ("c", "a", "b", 0, 2, 2);
  TEST_SEARCH_2 ("c", "a", "b", 0, 2, 2);
  TEST_SEARCH_2 ("c", "a", "b", 1, -2, 2);
  TEST_SEARCH_2 ("c", "a", "b", 1, -2, 2);

  TEST_SEARCH ("c", "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 236, -237);

				/* Invalid use of special characters.  */
  invalid_pattern (REG_EESCAPE, "\\");
  invalid_pattern (REG_EESCAPE, "a\\");
  invalid_pattern (REG_EESCAPE, "a*\\");
						  /* Invalid period.  */
  test_match (".", "");
					  /* Invalid bracket expressions.  */
  test_match ("[ab]", "c");
  test_match ("[^b]", "b");
  test_match ("[^]]", "]");

  invalid_pattern (REG_EBRACK, "[");
  invalid_pattern (REG_EBRACK, "[^");
  invalid_pattern (REG_EBRACK, "[a");
  invalid_pattern (REG_EBRACK, "[]");
  invalid_pattern (REG_EBRACK, "[]a");
  invalid_pattern (REG_EBRACK, "a[]a");


  test_match ("[:alpha:]", "q");	       /* Character classes.  */
  test_match ("[[:alpha:]]", "2");
  test_match ("[[:upper:]]", "a");
  test_match ("[[:lower:]]", "A");
  test_match ("[[:digit:]]", "a");
  test_match ("[[:alnum:]]", ":");
  test_match ("[[:xdigit:]]", "g");
  test_match ("[[:space:]]", "a");
  test_match ("[[:print:]]", "\177");
  test_match ("[[:punct:]]", "a");
  test_match ("[[:graph:]]", " ");
  test_match ("[[:cntrl:]]", "a");
  invalid_pattern (REG_EBRACK, "[[:");
  invalid_pattern (REG_EBRACK, "[[:alpha:");
  invalid_pattern (REG_EBRACK, "[[:alpha:]");
  invalid_pattern (REG_ECTYPE, "[[::]]");
  invalid_pattern (REG_ECTYPE, "[[:a:]]");
  invalid_pattern (REG_ECTYPE, "[[:alpo:]]");
  invalid_pattern (REG_ECTYPE, "[[:a:]");

  test_match ("[a-z]", "2");			/* Invalid ranges.  */
  test_match ("[^-a]", "-");
  test_match ("[^a-]", "-");
  test_match ("[)-+--/]", ".");
  invalid_pattern (REG_ERANGE, "[z-a]");		/* Empty  */
  invalid_pattern (REG_ERANGE, "[a--]");		/* Empty  */
  invalid_pattern (REG_ERANGE, "[[:digit:]-9]");
  invalid_pattern (REG_ERANGE, "[a-[:alpha:]]");
  invalid_pattern (REG_ERANGE, "[a-");
  invalid_pattern (REG_EBRACK, "[a-z");

  test_match ("[ab][cd]", "ae");		/* Concatenation.  */
  test_match ("b*c", "b");			/* Star.  */

	                                        /* Invalid anchoring.  */
  test_match ("^", "a");
  test_match ("^a", "ba");
  test_match ("$", "b");
  test_match ("a$", "ab");
  test_match ("^$", "a");
  test_match ("^ab$", "a");

  TEST_SEARCH ("^a", "b\na", 0, 3);
  TEST_SEARCH ("b$", "b\na", 0, 3);

  test_match_2 ("^a", "\n", "a");
  test_match_2 ("a$", "a", "\n");

  TEST_SEARCH (PARENS_TO_OPS ("(^a)"), "ba", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("(a$)"), "ab", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("^(^a)"), "ba", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("(a$)$"), "ab", 0, 2);

  printf ("\nFinished generic POSIX tests.\n");
}



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
