/* psx-basic.c: Test POSIX basic regular expressions.  */

#include "test.h"


void
test_posix_basic ()
{
  /* Intervals can only match up to RE_DUP_MAX occurences of anything.  */
  char dup_max_plus_one[6];
  sprintf (dup_max_plus_one, "%d", RE_DUP_MAX + 1);
  
  printf ("\nStarting POSIX basic tests.\n");
  t = posix_basic_test;

  re_set_syntax (RE_SYNTAX_POSIX_MINIMAL_BASIC);  
  
  test_posix_generic ();

  printf ("\nContinuing POSIX basic tests.\n");

/* Grouping tests that are not the same.  */
  
  test_should_match = false;
  invalid_pattern (REG_EPAREN, PARENS_TO_OPS ("a)"));

  test_should_match = true;
						/* Special characters.  */
  MATCH_SELF ("*");
  test_match ("\\(*\\)", "*");
  test_match ("\\(^*\\)", "*");
  test_match ("**", "***");
  test_match ("***", "****");
  
  MATCH_SELF ("{");					/* of extended...  */
  MATCH_SELF ("()");					/* also non-Posix.  */
  MATCH_SELF ("a+");
  MATCH_SELF ("a?");
  MATCH_SELF ("a|b");
  MATCH_SELF ("a|");					/* No alternations, */
  MATCH_SELF ("|a");					/* so OK if empty.  */
  MATCH_SELF ("a||");
  test_match ("\\(|a\\)", "|a");
  test_match ("\\(a|\\)", "a|");
  test_match ("a\\+", "a+");
  test_match ("a\\?", "a?");
  test_match ("a\\|b", "a|b");
  test_match ("^*", "*");
  test_match ("^+", "+");
  test_match ("^?", "?");
  test_match ("^{", "{");
                                              /* Valid subexpressions
                                                 (empty) in basic only.  */
  test_match ("\\(\\)", "");

  test_match ("a\\(\\)", "a");
  test_match ("\\(\\)b", "b");
  test_match ("a\\(\\)b", "ab");
  TEST_REGISTERS ("a\\(\\)b", "ab", 0, 2, 1, 1, -1, -1);

  test_match ("\\(\\)*", "");
  test_match ("\\(\\(\\)\\)*", "");
                                                /* Valid back references.  */

  /* N.B.: back references to subexpressions that include a * are
     undefined in the spec.  The tests are in here to see if we handle
     the situation consistently, but if it fails any of them, it doesn't
     matter.  */

  test_match ("\\(\\)\\1", "");
  TEST_REGISTERS ("\\(\\)\\1", "", 0, 0, 0, 0, -1, -1);

  test_match ("\\(\\(\\)\\)\\(\\)\\2", "");

  test_match ("\\(a\\)\\1", "aa");
  TEST_REGISTERS ("\\(a\\)\\1", "aa", 0, 2, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a\\)\\1", "xaax", 1, 3, 1, 2, -1, -1);

  test_match ("\\(\\(a\\)\\)\\1", "aa");
  test_match ("\\(a\\)\\(b\\)\\2\\1", "abba");

  test_match ("\\(a\\)*\\1", "aa");
  TEST_REGISTERS ("\\(a\\)*\\1", "aa", 0, 2, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a\\)*\\1", "xaax", 0, 0, -1, -1, -1, -1);
  
  test_match ("\\(\\(a\\)\\2b\\)*", "aab");
  TEST_REGISTERS ("\\(\\(a\\)\\2b\\)*", "aab", 0, 3, 0, 3, 0, 1);
  TEST_REGISTERS ("\\(\\(a\\)\\2b\\)*", "xaabx", 0, 0, -1, -1, -1, -1);
  
  test_match ("\\(a*\\)*\\1", "");
  test_match ("\\(a*\\)*\\1", "aa");
  TEST_REGISTERS ("\\(a*\\)*\\1", "aa", 0, 2, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a*\\)*\\1", "xaax", 0, 0, 0, 0, -1, -1);
  
  test_match ("\\(a*\\)*\\1", ""); 
  test_match ("\\(a*\\)*\\1", "aa"); 
  test_match ("\\(\\(a*\\)*\\)*\\1", "aa"); 
  test_match ("\\(ab*\\)*\\1", "abab");
  TEST_REGISTERS ("\\(ab*\\)*\\1", "abab", 0, 4, 0, 2, -1, -1);
  TEST_REGISTERS ("\\(ab*\\)*\\1", "xababx", 0, 0, -1, -1, -1, -1);

  test_match ("\\(a*\\)ab\\1", "aaba"); 
  TEST_REGISTERS ("\\(a*\\)ab\\1", "aaba", 0, 4, 0, 1, -1, -1); 
  TEST_REGISTERS ("\\(a*\\)ab\\1", "xaabax", 1, 5, 1, 2, -1, -1); 

  test_match ("\\(a*\\)*ab\\1", "aaba"); 
  TEST_REGISTERS ("\\(a*\\)*ab\\1", "aaba", 0, 4, 0, 1, -1, -1); 
  TEST_REGISTERS ("\\(a*\\)*ab\\1", "xaabax", 1, 5, 1, 2, -1, -1); 

  test_match ("\\(\\(a*\\)b\\)*\\2", "abb"); 
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2", "abb", 0, 3, 2, 3, 2, 2);
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2", "xabbx", 0, 0, -1, -1, -1, -1); 

  /* Different from above.  */
  test_match ("\\(\\(a*\\)b*\\)*\\2", "aa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "aa", 0, 2, 0, 1, 0, 1);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "xaax", 0, 0, 0, 0, 0, 0); 

  test_match ("\\(\\(a*\\)b*\\)*\\2", "aba"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "aba", 0, 3, 0, 2, 0, 1);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "xabax", 0, 0, 0, 0, 0, 0); 

  test_match ("\\(\\(a*\\)b\\)*\\2", "aababa");
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2", "aababa", 0, 6, 3, 5, 3, 4); 
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2", "xaababax", 0, 0, -1, -1, -1, -1); 

  test_match ("\\(\\(a*\\)b*\\)*\\2", "aabaa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "aabaa", 0, 5, 0, 3, 0, 2);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "xaabaax", 0, 0, 0, 0, 0, 0); 

  test_match ("\\(\\(a*\\)b*\\)*\\2", "aabbaa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "aabbaa", 0, 6, 0, 4, 0, 2);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "xaabbaax", 0, 0, 0, 0, 0, 0); 

  test_match ("\\(\\(a*\\)b*\\)*\\2", "abaabaa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "abaabaa", 0, 7, 2, 5, 2, 4);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2", "xaababaax", 0, 0, 0, 0, 0, 0); 

  test_match ("\\(\\(a*\\)b*\\)*a\\2", "aabaaa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*a\\)*\\2", "aabaaa", 0, 6, 0, 3, 0, 2);
  TEST_REGISTERS ("\\(\\(a*\\)b*a\\)*\\2", "xaabaax", 0, 0, -1, -1, -1, -1); 

  test_match ("\\(\\(a*\\)b*\\)*\\2a", "aabaaa"); 
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2a", "aabaaa", 0, 6, 0, 3, 0, 2);
  TEST_REGISTERS ("\\(\\(a*\\)b*\\)*\\2a", "xaabaaax", 1, 7, 1, 4, 1, 3); 

  test_match ("\\(\\(a*\\)b\\)*\\2\\1", "abaabaaaab");
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2\\1", "abaabaaaab", 0, 10, 2, 5, 2, 4);
  /* We are matching the empty string here.  */
  TEST_REGISTERS ("\\(\\(a*\\)b\\)*\\2\\1", "xabaabaaaabx", 0, 0, -1, -1, -1, -1);

  test_match ("\\(a*b\\)\\1", "abab");
  test_match ("\\(a\\)\\1\\1", "aaa");
  test_match ("\\(a\\(c\\)d\\)\\1\\2", "acdacdc");
  
  test_match ("\\(a\\)\\1*", "aaa");
  TEST_REGISTERS ("\\(a\\)\\1*", "aaa", 0, 3, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a\\)\\1*", "xaaax", 1, 4, 1, 2, -1, -1);

  test_match ("\\(a\\)\\{1,3\\}b\\1", "aba");
  TEST_REGISTERS ("\\(a\\)\\{1,3\\}b\\1", "aba", 0, 3, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a\\)\\{1,3\\}b\\1", "xabax", 1, 4, 1, 2, -1, -1);

  test_match ("\\(\\(a\\)\\2\\)*", "aaaa"); /* rms? */
  TEST_REGISTERS ("\\(\\(a*b\\)\\2\\)*", "bbabab", 0, 6, 2, 6, 2, 4); /* rms? */

  test_match ("\\(\\(a\\)\\1\\)*", "a1a1");

  test_match ("\\(\\(a\\)\\2\\)\\1", "aaaa");

  test_match ("\\(\\(a*\\)\\2\\)\\1", "aaaa");
  TEST_REGISTERS ("\\(\\(a*\\)\\2\\)\\1", "aaaa", 0, 4, 0, 2, 0, 1);
  TEST_REGISTERS ("\\(\\(a*\\)\\2\\)\\1", "xaaaax", 0, 0, 0, 0, 0, 0);

  test_match ("\\{1\\}", "{1}");
  test_match ("^\\{1\\}", "{1}");

  test_match ("\\(a\\)\\1\\{1,2\\}", "aaa");
  TEST_REGISTERS ("\\(a\\)\\1\\{1,2\\}", "aaa", 0, 3, 0, 1, -1, -1);
  TEST_REGISTERS ("\\(a\\)\\1\\{1,2\\}", "xaaax", 1, 4, 1, 2, -1, -1);


  /* Per POSIX D11.1 p. 109, leftmost longest match.  */

  test_match (PARENS_TO_OPS ("(.*).*\\1"), "abcabc");


  /* Per POSIX D11.1, p. 125, leftmost longest match.  */
  
  test_match (PARENS_TO_OPS ("(ac*)c*d[ac]*\\1"), "acdacaaa");
  TEST_REGISTERS (PARENS_TO_OPS ("(ac*)c*d[ac]*\\1"), "acdacaaa", 	
    0, 8, 0, 1, -1, -1);

  /* Anchors become ordinary, sometimes.  */
  MATCH_SELF ("a^");
  MATCH_SELF ("$a");		
  MATCH_SELF ("$^");		
  test_fastmap ("$a^", "$", 0, 0);
  test_match ("$^*", "$^^");
  test_match ("\\($^\\)", "$^");
  test_match ("$*", "$$");
  /* xx -- known bug, solution pending test_match ("^^$", "^"); */
  test_match ("$\\{0,\\}", "$$");
  TEST_SEARCH ("^$*", "$$", 0, 2);
  TEST_SEARCH ("^$\\{0,\\}", "$$", 0, 2);
  MATCH_SELF ("2^10");
  MATCH_SELF ("$HOME");
  MATCH_SELF ("$1.35");


  /* Basic regular expressions, continued; these don't match their strings.  */
  test_should_match = false;

  invalid_pattern (REG_EESCAPE, "\\(a\\");
                                                /* Invalid back references.  */
  test_match ("\\(a\\)\\1", "ab");
  test_match ("\\(a\\)\\1\\1", "aab");
  test_match ("\\(a\\)\\(b\\)\\2\\1", "abab");
  test_match ("\\(a\\(c\\)d\\)\\1\\2", "acdc");
  test_match ("\\(a*b\\)\\1", "abaab");
  test_match ("\\(a\\)\\1*", "aaaaaaaaaab");
  test_match ("\\(\\(a\\)\\1\\)*", "aaa");
  invalid_pattern (REG_ESUBREG, "\\1");
  invalid_pattern (REG_ESUBREG, "\\(a\\)\\2");
  test_match ("\\(\\(a\\)\\2\\)*", "abaa");
  test_match ("\\(\\(a\\)\\1\\)*", "a");
  test_match ("\\(\\(a\\)\\2\\)\\1", "abaa");
  test_match ("\\(\\(a*\\)\\2\\)\\1", "abaa");
  						/* Invalid intervals.  */
  invalid_pattern (REG_EBRACE, "a\\{");

  invalid_pattern (REG_BADBR, "a\\{-1");
  invalid_pattern (REG_BADBR, concat ("a\\{", (char *)dup_max_plus_one));
  invalid_pattern (REG_BADBR, concat (concat ("a\\{", (char *)dup_max_plus_one), ","));
  invalid_pattern (REG_BADBR, "a\\{1,0");

  invalid_pattern (REG_EBRACE, "a\\{1");
  invalid_pattern (REG_EBRACE, "a\\{0,");
  invalid_pattern (REG_EBRACE, "a\\{0,1");
  invalid_pattern (REG_EBRACE, "a\\{0,1}");

  printf ("\nFinished POSIX basic tests.\n");
}



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
