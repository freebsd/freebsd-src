/* psx-extend.c: Test POSIX extended regular expressions.  */

#include "test.h"


void
test_posix_extended ()
{
  /* Intervals can only match up to RE_DUP_MAX occurences of anything.  */
  char dup_max_plus_one[6];
  sprintf (dup_max_plus_one, "%d", RE_DUP_MAX + 1);


  printf ("\nStarting POSIX extended tests.\n");
  t = posix_extended_test;
  
  re_set_syntax (RE_SYNTAX_POSIX_MINIMAL_EXTENDED);  

  test_posix_generic ();

  printf ("\nContinuing POSIX extended tests.\n");

  /* Grouping tests that differ from basic's.  */
  
  test_should_match = true;
  MATCH_SELF ("a)");

                                /* Valid use of special characters.  */
  test_match ("\\(a", "(a");
  test_match ("a\\+", "a+");
  test_match ("a\\?", "a?");
  test_match ("\\{a", "{a");
  test_match ("\\|a", "|a");
  test_match ("a\\|b", "a|b");
  test_match ("a\\|?", "a");
  test_match ("a\\|?", "a|");
  test_match ("a\\|*", "a");
  test_match ("a\\|*", "a||");
  test_match ("\\(*\\)", ")");
  test_match ("\\(*\\)", "(()");
  test_match ("a\\|+", "a|");
  test_match ("a\\|+", "a||");
  test_match ("\\(+\\)", "()");
  test_match ("\\(+\\)", "(()");
  test_match ("a\\||b", "a|");
  test_match ("\\(?\\)", ")");
  test_match ("\\(?\\)", "()");

  test_match ("a+", "a");
  test_match ("a+", "aa");
  test_match ("a?", "");
  test_match ("a?", "a");

                                                /* Bracket expressions.  */
  test_match ("[(]", "(");
  test_match ("[+]", "+");
  test_match ("[?]", "?");
  test_match ("[{]", "{");
  test_match ("[|]", "|");
                                                /* Subexpressions.  */
  test_match ("(a+)*", "");
  test_match ("(a+)*", "aa");
  test_match ("(a?)*", "");
  test_match ("(a?)*", "aa");
                                                /* (No) back references.  */
  test_match ("(a)\\1", "a1");
                                                /* Invalid as intervals,
                                                   but are valid patterns.  */
  MATCH_SELF ("{");
  test_match ("^{", "{");
  test_match ("a|{", "{");
  test_match ("({)", "{");
  MATCH_SELF ("a{");
  MATCH_SELF ("a{}");
  MATCH_SELF ("a{-1");
  MATCH_SELF ("a{-1}");
  MATCH_SELF ("a{0");            
  MATCH_SELF ("a{0,"); 
  MATCH_SELF (concat ("a{", dup_max_plus_one));
  MATCH_SELF (concat (concat ("a{", dup_max_plus_one), ","));
  MATCH_SELF ("a{1,0");
  MATCH_SELF ("a{1,0}");
  MATCH_SELF ("a{0,1");
  test_match ("[a{0,1}]", "}");
  test_match ("a{1,3}{-1}", "aaa{-1}");
  test_match (concat ("a{1,3}{", dup_max_plus_one), 
	      concat ("aaa{", dup_max_plus_one));
  test_match ("a{1,3}{2,1}", "aaa{2,1}");
  test_match ("a{1,3}{1,2", "aaa{1,2");
					/* Valid consecutive repetitions.  */
  test_match ("a*+", "a");
  test_match ("a*?", "a");
  test_match ("a++", "a");
  test_match ("a+*", "a");
  test_match ("a+?", "a");
  test_match ("a??", "a");
  test_match ("a?*", "a");
  test_match ("a?+", "a");
  
  test_match ("a{2}?", "");
  test_match ("a{2}?", "aa");
  test_match ("a{2}+", "aa");
  test_match ("a{2}{2}", "aaaa");

  test_match ("a{1}?*", "");
  test_match ("a{1}?*", "aa");
  
  test_match ("(a?){0,3}b", "aaab"); 
  test_fastmap ("(a?){0,3}b", "ab", 0, 0); 
  test_match ("(a+){0,3}b", "b"); 
  test_fastmap ("(a+){0,3}b", "ab", 0, 0); 
  test_match ("(a+){0,3}b", "ab");
  test_fastmap ("(a+){0,3}b", "ab", 0, 0); 
  test_match ("(a+){1,3}b", "aaab"); 
  test_match ("(a?){1,3}b", "aaab");

  test_match ("\\\\{1}", "\\");				/* Extended only.  */

  test_match ("(a?)?", "a");
  test_match ("(a?b)?c", "abc");
  test_match ("(a+)*b", "b"); 
						/* Alternatives.  */
  test_match ("a|b", "a");
  test_match ("a|b", "b");
  test_fastmap ("a|b", "ab", 0, 0);

  TEST_SEARCH ("a|b", "cb", 0, 2);
  TEST_SEARCH ("a|b", "cb", 0, 2);

  test_match ("(a|b|c)", "a");
  test_match ("(a|b|c)", "b");
  test_match ("(a|b|c)", "c");

  test_match ("(a|b|c)*", "abccba");
  
  test_match ("(a(b*))|c", "a");	/* xx do registers.  */
  test_match ("(a(b*))|c", "ab");
  test_match ("(a(b*))|c", "c");

  test_fastmap ("(a+?*|b)", "ab", 0, 0);
  test_match ("(a+?*|b)", "b");
  TEST_REGISTERS ("(a+?*|b)", "b", 0, 1, 0, 1, -1, -1);
  
  test_fastmap ("(a+?*|b)*", "ab", 0, 0);
  test_match ("(a+?*|b)*", "bb");
  TEST_REGISTERS ("(a+?*|b)*", "bb", 0, 2, 1, 2, -1, -1);

  test_fastmap ("(a*|b)*", "ab", 0, 0);
  test_match ("(a*|b)*", "bb");
  TEST_REGISTERS ("(a*|b)*", "bb", 0, 2, 1, 2, -1, -1);

  test_fastmap ("((a*)|b)*", "ab", 0, 0);
  test_match ("((a*)|b)*", "bb");
  TEST_REGISTERS ("((a*)|b)*", "bb", 0, 2, 1, 2, 1, 1);

  test_fastmap ("(a{0,}|b)*", "ab", 0, 0);
  test_match ("(a{0,}|b)*", "bb");
  TEST_REGISTERS ("(a{0,}|b)*", "bb", 0, 2, 1, 2, -1, -1);

  test_fastmap ("((a{0,})|b)*", "ab", 0, 0);
  test_match ("((a{0,})|b)*", "bb");
  TEST_REGISTERS ("((a{0,})|b)*", "bb", 0, 2, 1, 2, 1, 1);

						/* With c's  */
  test_fastmap ("(a+?*|b)c", "abc", 0, 0);
  test_match ("(a+?*|b)c", "bc");
  TEST_REGISTERS ("(a+?*|b)c", "bc", 0, 2, 0, 1, -1, -1);
  
  test_fastmap ("(a+?*|b)*c", "abc", 0, 0);
  test_match ("(a+?*|b)*c", "bbc");
  TEST_REGISTERS ("(a+?*|b)*c", "bbc", 0, 3, 1, 2, -1, -1);

  test_fastmap ("(a*|b)*c", "abc", 0, 0);
  test_match ("(a*|b)*c", "bbc");
  TEST_REGISTERS ("(a*|b)*c", "bbc", 0, 3, 1, 2, -1, -1);

  test_fastmap ("((a*)|b)*c", "abc", 0, 0);
  test_match ("((a*)|b)*c", "bbc");
  TEST_REGISTERS ("((a*)|b)*c", "bbc", 0, 3, 1, 2, 1, 1);

  test_fastmap ("(a{0,}|b)*c", "abc", 0, 0);
  test_match ("(a{0,}|b)*c", "bbc");
  TEST_REGISTERS ("(a{0,}|b)*c", "bbc", 0, 3, 1, 2, -1, -1);

  test_fastmap ("((a{0,})|b)*c", "abc", 0, 0);
  test_match ("((a{0,})|b)*c", "bbc");
  TEST_REGISTERS ("((a{0,})|b)*c", "bbc", 0, 3, 1, 2, 1, 1);


  test_fastmap ("((a{0,}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a{0,}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a{0,}\\b\\<)|b)", "b", 
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a{0,}\\b\\<)|b)*", "ab", 0, 0);
  test_match ("((a{0,}\\b\\<)|b)*", "b");
  TEST_REGISTERS ("((a{0,}\\b\\<)|b)*", "b", 
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,1}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,1}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,1}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,2}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,2}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,2}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);


  test_fastmap ("((a+?*{0,4095}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,4095}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,4095}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,5119}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,5119}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,5119}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,6143}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,6143}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,6143}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,8191}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,8191}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,8191}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,16383}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,16383}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,16383}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);


  test_fastmap ("((a+?*{0,}\\b\\<)|b)", "ab", 0, 0);
  test_match ("((a+?*{0,}\\b\\<)|b)", "b");
  TEST_REGISTERS ("((a+?*{0,}\\b\\<)|b)", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,}\\b\\<)|b)*", "ab", 0, 0);
  test_match ("((a+?*{0,}\\b\\<)|b)*", "b");
  TEST_REGISTERS ("((a+?*{0,}\\b\\<)|b)*", "b",
    0, 1, 0, 1, 0, 0);

  test_fastmap ("((a+?*{0,}\\b\\<)|b)*", "ab", 0, 0);
  test_match ("((a+?*{0,}\\b\\<)|b)*", "bb");
  TEST_REGISTERS ("((a+?*{0,}\\b\\<)|b)*", "bb",
    0, 2, 1, 2, 0, 0);


				/* `*' after group.  */
  test_match ("(a*|b*)*c", "c");	
  TEST_REGISTERS ("(a*|b*)*c", "c", 0, 1, 0, 0, -1, -1);

  test_match ("(a*|b*)*c", "ac");
  TEST_REGISTERS ("(a*|b*)*c", "ac", 0, 2, 0, 1, -1, -1);

  test_match ("(a*|b*)*c", "aac");
  TEST_REGISTERS ("(a*|b*)*c", "aac", 0, 3, 0, 2, -1, -1);

  test_match ("(a*|b*)*c", "bbc");
  TEST_REGISTERS ("(a*|b*)*c", "bbc", 0, 3, 0, 2, -1, -1);

  test_match ("(a*|b*)*c", "abc");
  TEST_REGISTERS ("(a*|b*)*c", "abc", 0, 3, 1, 2, -1, -1);

				/* No `*' after group.  */
  test_match ("(a*|b*)c", "c");
  TEST_REGISTERS ("(a*|b*)c", "c", 0, 1, 0, 0, -1, -1);

  test_match ("(a*|b*)c", "ac");
  TEST_REGISTERS ("(a*|b*)c", "ac", 0, 2, 0, 1, -1, -1);

  test_match ("(a*|b*)c", "bc");
  TEST_REGISTERS ("(a*|b*)c", "bc", 0, 2, 0, 1, -1, -1);

  test_match ("(a*|b*)c", "aac");
  TEST_REGISTERS ("(a*|b*)c", "aac", 0, 3, 0, 2, -1, -1);

  /* Same as above, but with no `*'s in alternatives. 
  
  test_match ("(a|b)*c", "c");		/* `*' after group.  */
  TEST_REGISTERS ("(a|b)*c", "c", 0, 1, -1, -1, -1, -1);

  test_match ("(a|b)*c", "ac");
  TEST_REGISTERS ("(a|b)*c", "ac", 0, 2, 0, 1, -1, -1);

  test_match ("(a|b)*c", "bc");
  TEST_REGISTERS ("(a|b)*c", "bc", 0, 2, 0, 1, -1, -1);

  test_match ("(a|b)*c", "abc");
  TEST_REGISTERS ("(a|b)*c", "abc", 0, 3, 1, 2, -1, -1);


  test_match ("(a*|b*)c", "bbc");
  TEST_REGISTERS ("(a*|b*)c", "bbc", 0, 3, 0, 2, -1, -1);

  /* Complicated second alternative.  */
  
  test_match ("(a*|(b*)*)*c", "bc");
  TEST_REGISTERS ("(a*|(b*)*)*c", "bc", 0, 2, 0, 1, 0, 1);

  test_match ("(a*|(b*|c*)*)*d", "bd");
  TEST_REGISTERS ("(a*|(b*|c*)*)*d", "bd", 0, 2, 0, 1, 0, 1);

  test_match ("(a*|(b*|c*)*)*d", "bbd");
  TEST_REGISTERS ("(a*|(b*|c*)*)*d", "bbd", 0, 3, 0, 2, 0, 2);

  test_match ("(a*|(b*|c*)*)*d", "cd");
  TEST_REGISTERS ("(a*|(b*|c*)*)*d", "cd", 0, 2, 0, 1, 0, 1);

  test_match ("(a*|(b*|c*)*)*d", "ccd");
  TEST_REGISTERS ("(a*|(b*|c*)*)*d", "ccd", 0, 3, 0, 2, 0, 2);

  test_match ("(a*|b*|c*)*d", "aad");
  TEST_REGISTERS ("(a*|b*|c*)*d", "aad", 0, 3, 0, 2, 0, 2);

  test_match ("(a*|b*|c*)*d", "bbd");
  TEST_REGISTERS ("(a*|b*|c*)*d", "bbd", 0, 3, 0, 2, 0, 2);

  test_match ("(a*|b*|c*)*d", "ccd");
  TEST_REGISTERS ("(a*|b*|c*)*d", "ccd", 0, 3, 0, 2, 0, 2);

						/* Valid anchoring.  */
  valid_pattern ("a^");
  valid_pattern ("a^b");
  valid_pattern ("$a");
  valid_pattern ("a$b");
  valid_pattern ("foo^bar");
  valid_pattern ("foo$bar");
  valid_pattern ("(^)");
  valid_pattern ("($)");
  valid_pattern ("(^$)");

      /* These are the same (but valid) as those (invalid) in other_test.c.  */
  valid_pattern 
    ("(((((((((((((((((((((((((((((((((a^)))))))))))))))))))))))))))))))))");
  valid_pattern 
    ("((((((((((((((((((((((((((((((((($a)))))))))))))))))))))))))))))))))");
  valid_pattern ("\\(^a\\)");
  valid_pattern ("a\\|^b");
  valid_pattern ("\\w^a");
  valid_pattern ("\\W^a");
  valid_pattern ("(a^)");
  valid_pattern ("($a)");
  valid_pattern ("a(^b)");
  valid_pattern ("a$(b)");
  valid_pattern ("(a)^b");
  valid_pattern ("(a)$b");
  valid_pattern ("(a)(^b)");
  valid_pattern ("(a$)(b)");
  valid_pattern ("(a|b)^c");
  valid_pattern ("(a|b)$c");
  valid_pattern ("(a$|b)c");
  valid_pattern ("(a|b$)c");
  valid_pattern ("a(b|^c)");
  valid_pattern ("a(^b|c)");
  valid_pattern ("a$(b|c)");
  valid_pattern ("(a)(^b|c)");
  valid_pattern ("(a)(b|^c)");
  valid_pattern ("(b$|c)(a)");
  valid_pattern ("(b|c$)(a)");
  valid_pattern ("(a(^b|c))");
  valid_pattern ("(a(b|^c))");
  valid_pattern ("((b$|c)a)");
  valid_pattern ("((b|c$)a)");
  valid_pattern ("((^a|^b)^c)");
  valid_pattern ("(c$(a$|b$))");
  valid_pattern ("((^a|^b)^c)");
  valid_pattern ("((a$|b$)c)");
  valid_pattern ("(c$(a$|b$))");
  valid_pattern ("((^a|^b)|^c)^d");
  valid_pattern ("((a$|b$)|c$)d$");
  valid_pattern ("d$(c$|(a$|b$))");
  valid_pattern ("((^a|^b)|^c)(^d)");
  valid_pattern ("((a$|b$)|c$)(d$)");
  valid_pattern ("(d$)((a$|b$)|c$)");
  valid_pattern ("((^a|^b)|^c)((^d))");
  valid_pattern ("((a$|b$)|c$)((d$))");
  valid_pattern ("((d$))((a$|b$)|c$)");
  valid_pattern ("(((^a|^b))c|^d)^e");
  valid_pattern ("(((a$|b$))c|d$)$e$");
  valid_pattern ("e$(d$|c((a$|b$)))");
  valid_pattern ("(^a)((^b))");
  valid_pattern ("(a$)((b$))");
  valid_pattern ("((^a))(^b)");
  valid_pattern ("((a$))(b$)");
  valid_pattern ("((^a))((^b))");
  valid_pattern ("((a$))((b$))");
  valid_pattern ("((^a)^b)");
  valid_pattern ("((a$)b$)");
  valid_pattern ("(b$(a$))");
  valid_pattern ("(((^a)b)^c)");
  valid_pattern ("(((a$)b)c$)");
  valid_pattern ("(c$(b(a$)))");
  valid_pattern ("(((^a)b)c)^d");
  valid_pattern ("(((a$)b)c)d$");
  valid_pattern ("d$(c(b(a$)))");
  valid_pattern (".^a");
  valid_pattern ("a$.");
  valid_pattern ("[a]^b");
  valid_pattern ("b$[a]");
  valid_pattern ("\\(a$\\)");
  valid_pattern ("a$\\|b");
  valid_pattern ("(^a|^b)^c");
  valid_pattern ("c$(a$|b$)");
  valid_pattern ("(^a|^b)^|^c");
  valid_pattern ("(a$|b$)$|$c$");
  valid_pattern ("(a$|$b$)$|c$");
  valid_pattern ("($a$|b$)$|c$");
  valid_pattern ("$(a$|b$)$|c$");
  valid_pattern ("^c|d(^a|^b)");
  valid_pattern ("(^a|^b)|d^c");
  valid_pattern ("c$|(a$|b$)d");
  valid_pattern ("c$d|(a$|b$)");
  valid_pattern ("c(^a|^b)|^d");
  valid_pattern ("(a$|b$)c|d$");
  valid_pattern ("c(((^a|^b))|^d)e");
  valid_pattern ("(c((^a|^b))|^d)e");
  valid_pattern ("((c(^a|^b))|^d)e");
  valid_pattern ("(((^a|^b))|c^d)e");
  valid_pattern ("(((^a|^b))|^d)^e");
  valid_pattern ("(c$((a|b))|d)e$");
  valid_pattern ("(c((a$|b$))|d)e$");
  valid_pattern ("(c((a|b)$)|d)e$");
  valid_pattern ("(c((a|b))|d$)e$");
  valid_pattern ("^d(^c|e((a|b)))");
  valid_pattern ("^d(c|^e((a|b)))");
  valid_pattern ("^d(c|e(^(a|b)))");
  valid_pattern ("^d(c|e((^a|b)))");
  valid_pattern ("^d(c|e((a|^b)))");
  valid_pattern ("^d(c|e((a|b^)))");
  valid_pattern ("^d(c|e((a|b)^))");
  valid_pattern ("^d(c|e((a|b))^)");
  valid_pattern ("^d(c|e((a|b)))^");
  valid_pattern ("d$(c$|e((a$|b$)))");
  valid_pattern ("d(c$|e$((a$|b$)))");
  valid_pattern ("(((^a|^b))^c)|^de");
  valid_pattern ("(((^a|^b))c)|^d^e");
  valid_pattern ("(((a$|b))c$)|de$");
  valid_pattern ("(((a|b$))c$)|de$");
  valid_pattern ("(((a|b))c$)|d$e$");
  valid_pattern ("^d^e|^(c((a|b)))");
  valid_pattern ("^de|^(c^((a|b)))");
  valid_pattern ("^de|^(c(^(a|b)))");
  valid_pattern ("^de|^(c((^a|b)))");
  valid_pattern ("^de|^(c((a|^b)))");
  valid_pattern ("^de|(^c(^(a|b)))");
  valid_pattern ("^de|(^c((^a|b)))");
  valid_pattern ("^de|(^c((a|^b)))");
  valid_pattern ("de$|(c($(a|b)$))");
  valid_pattern ("de$|(c$((a|b)$))");
  valid_pattern ("de$|($c((a|b)$))");
  valid_pattern ("de$|$(c((a|b)$))");
  valid_pattern ("de$|(c($(a|b))$)");
  valid_pattern ("de$|(c$((a|b))$)");
  valid_pattern ("de$|$(c((a|b))$)");
  valid_pattern ("de$|(c($(a|b)))$");
  valid_pattern ("de$|(c$((a|b)))$");
  valid_pattern ("de$|($c((a|b)))$");
  valid_pattern ("de$|$(c((a|b)))$");
  valid_pattern ("^a(^b|c)|^d");
  valid_pattern ("^a(b|^c)|^d");
  valid_pattern ("^a(b|c^)|^d");
  valid_pattern ("^a(b|c)^|^d");
  valid_pattern ("a$(b$|c$)|d$");
  valid_pattern ("^d|^a(^b|c)");
  valid_pattern ("^d|^a(b|^c)");
  valid_pattern ("d$|a$(b$|c$)");
  valid_pattern ("^d|^(b|c)^a");
  valid_pattern ("d$|(b|c$)a$");
  valid_pattern ("d$|(b$|c)a$");
  valid_pattern ("^(a)^(b|c)|^d");
  valid_pattern ("^(a)(^b|c)|^d");
  valid_pattern ("^(a)(b|^c)|^d");
  valid_pattern ("(a)$(b|c)$|d$");
  valid_pattern ("(a$)(b|c)$|d$");
  valid_pattern ("(^a)(^b|c)|^d");
  valid_pattern ("(^a)(b|^c)|^d");
  valid_pattern ("(a)$(b$|c$)|d$");
  valid_pattern ("(a$)(b$|c$)|d$");
  valid_pattern ("^d|^(b|c)^(a)");
  valid_pattern ("^d|^(b|c)(^a)");
  valid_pattern ("d$|(b|c$)(a)$");
  valid_pattern ("d$|(b$|c)(a)$");
  valid_pattern ("^d|(^b|^c)^(a)");
  valid_pattern ("^d|(^b|^c)(^a)");
  valid_pattern ("d$|(b|c)$(a$)");
  valid_pattern ("d$|(b|c$)(a$)");
  valid_pattern ("d$|(b$|c)(a$)");
  valid_pattern ("^d|^(a)^(b|c)");
  valid_pattern ("^d|^(a)(^b|c)");
  valid_pattern ("^d|^(a)(b|^c)");
  valid_pattern ("^d|(^a)^(b|c)");
  valid_pattern ("^d|(^a)(^b|c)");
  valid_pattern ("^d|(^a)(b|^c)");
  valid_pattern ("d$|(a)$(b$|c$)");
  valid_pattern ("d$|(a$)(b$|c$)");
  valid_pattern ("((e^a|^b)|^c)|^d");
  valid_pattern ("((^a|e^b)|^c)|^d");
  valid_pattern ("((^a|^b)|e^c)|^d");
  valid_pattern ("((^a|^b)|^c)|e^d");
  valid_pattern ("d$e|(c$|(a$|b$))");
  valid_pattern ("d$|(c$e|(a$|b$))");
  valid_pattern ("d$|(c$|(a$e|b$))");
  valid_pattern ("d$|(c$|(a$|b$e))");
  valid_pattern ("d$|(c$|(a$|b$)e)");
  valid_pattern ("d$|(c$|(a$|b$))e");
  valid_pattern ("(a|b)^|c");
  valid_pattern ("(a|b)|c^");
  valid_pattern ("$(a|b)|c");
  valid_pattern ("(a|b)|$c");
  valid_pattern ("(a^|^b)|^c");
  valid_pattern ("(^a|b^)|^c");
  valid_pattern ("(^a|^b)|c^");
  valid_pattern ("($a|b$)|c$");
  valid_pattern ("(a$|$b)|c$");
  valid_pattern ("(a$|b$)|$c");
  valid_pattern ("c^|(^a|^b)");
  valid_pattern ("^c|(a^|^b)");
  valid_pattern ("^c|(^a|b^)");
  valid_pattern ("$c|(a$|b$)");
  valid_pattern ("c$|($a|b$)");
  valid_pattern ("c$|(a$|$b)");
  valid_pattern ("c^|^(a|b)");
  valid_pattern ("^c|(a|b)^");
  valid_pattern ("$c|(a|b)$");
  valid_pattern ("c$|$(a|b)");
  valid_pattern ("(a^|^b)c|^d");
  valid_pattern ("(^a|b^)c|^d");
  valid_pattern ("(^a|^b)c|d^");
  valid_pattern ("(^a|^b)^c|^d");
  valid_pattern ("(a|b)c$|$d");
  valid_pattern ("(a|b)$c$|d$");
  valid_pattern ("(a|b)$c$|d$");
  valid_pattern ("(a|b$)c$|d$");
  valid_pattern ("(a$|b)c$|d$");
  valid_pattern ("($a|b)c$|d$");
  valid_pattern ("$(a|b)c$|d$");
  valid_pattern ("^d|^c^(a|b)");
  valid_pattern ("^d|^c(^a|b)");
  valid_pattern ("^d|^c(a|^b)");
  valid_pattern ("^d|^c(a|b^)");
  valid_pattern ("^d|^c(a|b)^");
  valid_pattern ("$d|c(a$|b$)");
  valid_pattern ("d$|c($a$|b$)");
  valid_pattern ("d$|c$(a$|b$)");
  valid_pattern ("d$|$c(a$|b$)");

  valid_pattern ("(((a^|^b))c|^d)e");
  valid_pattern ("(((^a|b^))c|^d)e");
  valid_pattern ("(((^a|^b))^c|^d)e");
  valid_pattern ("((^(a|b))c|d^)e");
  valid_pattern ("(^((a|b))c|^d)^e");
  valid_pattern ("(^((a|b)^)c|^d)e");
  valid_pattern ("(^((a^|b))c|^d)e");
  valid_pattern ("(^((a|b^))c|^d)e");
  valid_pattern ("(^((a|b)^)c|^d)e");
  valid_pattern ("(^((a|b))^c|^d)e");
  valid_pattern ("(^((a|b))c^|^d)e");
  valid_pattern ("(^((a|b))c|^d^)e");
  valid_pattern ("(^((a|b))c|^d)^e");
  valid_pattern ("(((a|b))c|d)$e$");
  valid_pattern ("(((a|b))c|d$)e$");
  valid_pattern ("(((a|b))c|$d)e$");
  valid_pattern ("(((a|b))c$|d)e$");
  valid_pattern ("(((a|b))$c|d)e$");
  valid_pattern ("(((a|b)$)c|d)e$");
  valid_pattern ("(((a|b$))c|d)e$");
  valid_pattern ("(((a$|b))c|d)e$");
  valid_pattern ("((($a|b))c|d)e$");
  valid_pattern ("(($(a|b))c|d)e$");
  valid_pattern ("($((a|b))c|d)e$");
  valid_pattern ("$(((a|b))c|d)e$");
  valid_pattern ("(^((a|b)^)c|^d)e");
  valid_pattern ("(^((a|b))^c|^d)e");
  valid_pattern ("(^((a|b))c|^d^)e");
  valid_pattern ("(^((a|b))c|^d)^e");

  valid_pattern ("^e(^d|c((a|b)))");
  valid_pattern ("^e(d|^c((a|b)))");
  valid_pattern ("^e(d|c^((a|b)))");
  valid_pattern ("^e(d|c(^(a|b)))");
  valid_pattern ("^e(d|c((^a|b)))");
  valid_pattern ("^e(d|c((a|^b)))");
  valid_pattern ("^e(d|c((a|b^)))");
  valid_pattern ("^e(d|c((a|b)^))");
  valid_pattern ("^e(d|c((a|b))^)");
  valid_pattern ("^e(d|c((a|b)))^");
  valid_pattern ("e$(d$|c((a$|b$)))");
  valid_pattern ("e(d$|c$((a$|b$)))");
  valid_pattern ("e(d$|c($(a$|b$)))");
  valid_pattern ("e(d$|c(($a$|b$)))");
  valid_pattern ("e$(d$|c((a|b)$))");
  valid_pattern ("e($d$|c((a|b)$))");
  valid_pattern ("e(d$|$c((a|b)$))");
  valid_pattern ("e(d$|c$((a|b)$))");
  valid_pattern ("e(d$|c($(a|b)$))");
  valid_pattern ("e(d$|c(($a|b)$))");
  valid_pattern ("e(d$|c((a|$b)$))");
  valid_pattern ("e(d$|c((a$|$b$)))");

  valid_pattern ("e$(d$|c((a|b))$)");
  valid_pattern ("e($d$|c((a|b))$)");
  valid_pattern ("e(d$|$c((a|b))$)");
  valid_pattern ("e(d$|c$((a|b))$)");
  valid_pattern ("e(d$|c($(a|b))$)");
  valid_pattern ("e(d$|c(($a|b))$)");
  valid_pattern ("e(d$|c((a|$b))$)");
  valid_pattern ("e$(d$|c((a|b)))$");
  valid_pattern ("e($d$|c((a|b)))$");
  valid_pattern ("e(d$|$c((a|b)))$");
  valid_pattern ("e(d$|c$((a|b)))$");
  valid_pattern ("e(d$|c($(a|b)))$");
  valid_pattern ("e(d$|c(($a|b)))$");
  valid_pattern ("e(d$|c((a|$b)))$");
  valid_pattern ("(((^a|^b)^)c)|^de");
  valid_pattern ("(((^a|^b))^c)|^de");
  valid_pattern ("(((^a|^b))c)^|^de");
  valid_pattern ("$(((a|b))c$)|de$");
  valid_pattern ("($((a|b))c$)|de$");
  valid_pattern ("(($(a|b))c$)|de$");
  valid_pattern ("((($a|b))c$)|de$");
  valid_pattern ("(((a|$b))c$)|de$");
  valid_pattern ("(((a|b)$)c$)|de$");
  valid_pattern ("(((a|b))$c$)|de$");
  valid_pattern ("$(((a|b))c)$|de$");
  valid_pattern ("($((a|b))c)$|de$");
  valid_pattern ("(($(a|b))c)$|de$");
  valid_pattern ("((($a|b))c)$|de$");
  valid_pattern ("(((a|$b))c)$|de$");
  valid_pattern ("(((a|b)$)c)$|de$");
  valid_pattern ("(((a|b))$c)$|de$");
  valid_pattern ("^ed|^(c((a|b)))^");
  valid_pattern ("^ed|^(c((a|b))^)");
  valid_pattern ("^ed|^(c((a|b)^))");
  valid_pattern ("^ed|^(c((a|b^)))");
  valid_pattern ("^ed|^(c((a^|b)))");
  valid_pattern ("^ed|^(c((^a|b)))");
  valid_pattern ("^ed|^(c(^(a|b)))");
  valid_pattern ("^ed|^(c^((a|b)))");
  valid_pattern ("^ed|(^c((a|b)))^");
  valid_pattern ("^ed|(^c((a|b))^)");
  valid_pattern ("^ed|(^c((a|b)^))");
  valid_pattern ("^ed|(^c((a|b^)))");
  valid_pattern ("^ed|(^c((a|^b)))");
  valid_pattern ("^ed|(^c((a^|b)))");
  valid_pattern ("^ed|(^c((^a|b)))");
  valid_pattern ("^ed|(^c(^(a|b)))");
  valid_pattern ("^ed|(^c(^(a|b)))");
  valid_pattern ("^ed|(^c^((a|b)))");
  valid_pattern ("ed$|$(c((a|b)))$");
  valid_pattern ("ed$|($c((a|b)))$");
  valid_pattern ("ed$|(c$((a|b)))$");
  valid_pattern ("ed$|(c($(a|b)))$");
  valid_pattern ("ed$|(c(($a|b)))$");
  valid_pattern ("ed$|(c((a|$b)))$");
  valid_pattern ("ed$|$(c((a|b))$)");
  valid_pattern ("ed$|($c((a|b))$)");
  valid_pattern ("ed$|(c$((a|b))$)");
  valid_pattern ("ed$|(c($(a|b))$)");
  valid_pattern ("ed$|(c(($a|b))$)");
  valid_pattern ("ed$|(c((a|$b))$)");
  valid_pattern ("ed$|$(c((a|b)$))");
  valid_pattern ("ed$|($c((a|b)$))");
  valid_pattern ("ed$|(c$((a|b)$))");
  valid_pattern ("ed$|(c($(a|b)$))");
  valid_pattern ("ed$|(c(($a|b)$))");
  valid_pattern ("ed$|(c((a|$b)$))");
  valid_pattern ("ed$|$(c((a|b)$))");
  valid_pattern ("ed$|($c((a|b)$))");
  valid_pattern ("ed$|(c$((a|b)$))");
  valid_pattern ("ed$|(c($(a|b)$))");
  valid_pattern ("ed$|(c(($a|b)$))");
  valid_pattern ("ed$|(c((a|$b)$))");
  valid_pattern ("ed$|$(c((a|b)$))");
  valid_pattern ("ed$|($c((a|b)$))");
  valid_pattern ("ed$|(c$((a|b)$))");
  valid_pattern ("ed$|(c($(a|b)$))");
  valid_pattern ("ed$|(c(($a|b)$))");
  valid_pattern ("ed$|(c((a|$b)$))");
  valid_pattern ("ed$|$(c((a|b)$))");
  valid_pattern ("ed$|($c((a|b)$))");
  valid_pattern ("ed$|(c$((a|b)$))");
  valid_pattern ("ed$|(c($(a|b)$))");
  valid_pattern ("ed$|(c(($a|b)$))");
  valid_pattern ("ed$|(c((a|$b)$))");
  valid_pattern ("ed$|$(c((a|b)$))");
  valid_pattern ("ed$|($c((a|b)$))");
  valid_pattern ("ed$|(c$((a|b)$))");
  valid_pattern ("ed$|(c($(a|b)$))");
  valid_pattern ("ed$|(c(($a|b)$))");
  valid_pattern ("ed$|(c((a|$b)$))");
  valid_pattern ("ed$|$(c((a$|b$)))");
  valid_pattern ("ed$|($c((a$|b$)))");
  valid_pattern ("ed$|(c$((a$|b$)))");
  valid_pattern ("ed$|(c($(a$|b$)))");
  valid_pattern ("ed$|(c(($a$|b$)))");
  valid_pattern ("ed$|(c((a$|$b$)))");
  valid_pattern ("^a(b|c)^|^d");
  valid_pattern ("^a(b|c^)|^d");
  valid_pattern ("^a(b|^c)|^d");
  valid_pattern ("^a(b^|c)|^d");
  valid_pattern ("^a(^b|c)|^d");
  valid_pattern ("^a^(b|c)|^d");
  valid_pattern ("$a(b$|c$)|d$");
  valid_pattern ("a$(b$|c$)|d$");
  valid_pattern ("a($b$|c$)|d$");
  valid_pattern ("a(b$|$c$)|d$");
  valid_pattern ("a(b$|c$)|$d$");
  valid_pattern ("^(a^)(b|c)|^d");
  valid_pattern ("^(a)^(b|c)|^d");
  valid_pattern ("^(a)(^b|c)|^d");
  valid_pattern ("^(a)(b^|c)|^d");
  valid_pattern ("^(a)(b|^c)|^d");
  valid_pattern ("^(a)(b|c^)|^d");
  valid_pattern ("^(a)(b|c)^|^d");
  valid_pattern ("(^a^)(b|c)|^d");
  valid_pattern ("(^a)^(b|c)|^d");
  valid_pattern ("(^a)(^b|c)|^d");
  valid_pattern ("(^a)(b^|c)|^d");
  valid_pattern ("(^a)(b|^c)|^d");
  valid_pattern ("(^a)(b|c^)|^d");
  valid_pattern ("(^a)(b|c)^|^d");
  
  valid_pattern ("(a)(b$|c$)d$");
  valid_pattern ("(a)(b|$c)$|d$");
  valid_pattern ("(a)($b|c)$|d$");
  valid_pattern ("(a)$(b|c)$|d$");
  valid_pattern ("(a$)(b|c)$|d$");
  valid_pattern ("($a)(b|c)$|d$");
  valid_pattern ("$(a)(b|c)$|d$");
  valid_pattern ("(b|c)($a)$|d$");
  valid_pattern ("(b|c)$(a)$|d$");
  valid_pattern ("(b|c$)(a)$|d$");
  valid_pattern ("(b|$c)(a)$|d$");
  valid_pattern ("(b$|c)(a)$|d$");
  valid_pattern ("($b|c)(a)$|d$");
  valid_pattern ("$(b|c)(a)$|d$");
  valid_pattern ("(b|c)($a$)|d$");
  valid_pattern ("(b|c)$(a$)|d$");
  valid_pattern ("(b|c$)(a$)|d$");
  valid_pattern ("(b|$c)(a$)|d$");
  valid_pattern ("(b$|c)(a$)|d$");
  valid_pattern ("($b|c)(a$)|d$");
  valid_pattern ("$(b|c)(a$)|d$");
  valid_pattern ("(a)$(b$|c$)|d$");
  valid_pattern ("(a$)(b$|c$)|d$");
  valid_pattern ("($a)(b$|c$)|d$");
  valid_pattern ("$(a)(b$|c$)|d$");
  valid_pattern ("^d|^(b^|c)(a)");
  valid_pattern ("^d|^(b|c^)(a)");
  valid_pattern ("^d|^(b|c)^(a)");
  valid_pattern ("^d|^(b|c)(^a)");
  valid_pattern ("^d|^(b|c)(a^)");
  valid_pattern ("^d|^(b|c)(a)^");
  valid_pattern ("^d|(^b|^c^)(a)");
  valid_pattern ("^d|(^b|^c)^(a)");
  valid_pattern ("^d|(^b|^c)(^a)");
  valid_pattern ("^d|(^b|^c)(a^)");
  valid_pattern ("^d|(^b|^c)(a)^");
  valid_pattern ("d$|(b|c)($a$)");
  valid_pattern ("d$|(b|c)$(a$)");
  valid_pattern ("d$|(b|c$)(a$)");
  valid_pattern ("d$|(b$|c)(a$)");
  valid_pattern ("d$|($b|c)(a$)");
  valid_pattern ("d$|$(b|c)(a$)");
  valid_pattern ("d$|(b|c)($a)$");
  valid_pattern ("d$|(b|c)$(a)$");
  valid_pattern ("d$|(b|c$)(a)$");
  valid_pattern ("d$|(b$|c)(a)$");
  valid_pattern ("d$|($b|c)(a)$");
  valid_pattern ("d$|$(b|c)(a)$");
  valid_pattern ("^d|^(a^)(b|c)");
  valid_pattern ("^d|^(a)^(b|c)");
  valid_pattern ("^d|^(a)(^b|c)");
  valid_pattern ("^d|^(a)(b^|c)");
  valid_pattern ("^d|^(a)(b|^c)");
  valid_pattern ("^d|^(a)(b|c^)");
  valid_pattern ("^d|^(a)(b|c)^");
  valid_pattern ("^d|(^a^)(b|c)");
  valid_pattern ("^d|(^a)^(b|c)");
  valid_pattern ("^d|(^a)(^b|c)");
  valid_pattern ("^d|(^a)(b^|c)");
  valid_pattern ("^d|(^a)(b|^c)");
  valid_pattern ("^d|(^a)(b|c^)");
  valid_pattern ("^d|(^a)(b|c)^");
  valid_pattern ("d$|(a)$(b$|c$)");
  valid_pattern ("d$|(a$)(b$|c$)");
  valid_pattern ("d$|($a)(b$|c$)");
  valid_pattern ("d$|$(a)(b$|c$)");
  valid_pattern ("d$|(a)(b|$c)$");
  valid_pattern ("d$|(a)($b|c)$");
  valid_pattern ("d$|(a)$(b|c)$");
  valid_pattern ("d$|(a$)(b|c)$");
  valid_pattern ("d$|($a)(b|c)$");
  valid_pattern ("d$|$(a)(b|c)$");
  valid_pattern ("((^a|^b)|^c)|^d^");
  valid_pattern ("((^a|^b)|^c)^|^d");
  valid_pattern ("((^a|^b)|^c^)|^d");
  valid_pattern ("((^a|^b)^|^c)|^d");
  valid_pattern ("((^a|^b^)|^c)|^d");
  valid_pattern ("((^a^|^b)|^c)|^d");
  valid_pattern ("((a|b)|c)|$d$");
  valid_pattern ("((a|b)|$c)|d$");
  valid_pattern ("((a|$b)|c)|d$");
  valid_pattern ("(($a|b)|c)|d$");
  valid_pattern ("($(a|b)|c)|d$");
  valid_pattern ("$((a|b)|c)|d$");
  valid_pattern ("^d^|(c|(a|b))");
  valid_pattern ("^d|(c^|(a|b))");
  valid_pattern ("^d|(c|(a^|b))");
  valid_pattern ("^d|(c|(a|b^))");
  valid_pattern ("^d|(c|(a|b)^)");
  valid_pattern ("^d|(c|(a|b))^");
  valid_pattern ("d$|(c$|(a$|$b$))");
  valid_pattern ("d$|(c$|($a$|b$))");
  valid_pattern ("d$|($c$|(a$|b$))");
  valid_pattern ("d$|$(c$|(a$|b$))");
  valid_pattern ("$d$|(c$|(a$|b$))");
  valid_pattern ("d$|(c$|(a|$b)$)");
  valid_pattern ("d$|(c$|($a|b)$)");
  valid_pattern ("d$|($c$|(a|b)$)");
  valid_pattern ("d$|$(c$|(a|b)$)");
  valid_pattern ("$d$|(c$|(a|b)$)");
  valid_pattern ("d$|(c$|(a|$b))$");
  valid_pattern ("d$|(c$|($a|b))$");
  valid_pattern ("d$|($c$|(a|b))$");
  valid_pattern ("d$|$(c$|(a|b))$");
  valid_pattern ("$d$|(c$|(a|b))$");
  valid_pattern ("^c^|(^a|^b)");
  valid_pattern ("^c|(^a^|^b)");
  valid_pattern ("^c|(^a|^b^)");
  valid_pattern ("^c|(^a|^b)^");
  valid_pattern ("c$|(a$|$b$)");
  valid_pattern ("c$|($a$|b$)");
  valid_pattern ("c$|$(a$|b$)");
  valid_pattern ("$c$|(a$|b$)");
  valid_pattern ("^d^(c|e((a|b)))");
  valid_pattern ("^d(^c|e((a|b)))");
  valid_pattern ("^d(c^|e((a|b)))");
  valid_pattern ("^d(c|^e((a|b)))");
  valid_pattern ("^d(c|e^((a|b)))");
  valid_pattern ("^d(c|e(^(a|b)))");
  valid_pattern ("^d(c|e((^a|b)))");
  valid_pattern ("^d(c|e((a|^b)))");
  valid_pattern ("^d(c|e((a|b^)))");
  valid_pattern ("^d(c|e((a|b)^))");
  valid_pattern ("^d(c|e((a|b))^)");
  valid_pattern ("^d(c|e((a|b)))^");
  valid_pattern ("d(c$|e($(a$|b$)))");
  valid_pattern ("d(c$|e$((a$|b$)))");
  valid_pattern ("d(c$|$e((a$|b$)))");
  valid_pattern ("d($c$|e((a$|b$)))");
  valid_pattern ("d$(c$|e((a$|b$)))");
  valid_pattern ("$d(c$|e((a$|b$)))");
  valid_pattern ("^d|^a^(b|c)");
  valid_pattern ("^d|^a(^b|c)");
  valid_pattern ("^d|^a(b^|c)");
  valid_pattern ("^d|^a(b|^c)");
  valid_pattern ("^d|^a(b|c^)");
  valid_pattern ("^d|^a(b|c)^");
  valid_pattern ("d$|a($b$|c$)");
  valid_pattern ("d$|a$(b$|c$)");
  valid_pattern ("d$|$a(b$|c$)");
  valid_pattern ("$d$|a(b$|c$)");
  valid_pattern ("^d|^(b^|c)a");
  valid_pattern ("^d|^(b|c^)a");
  valid_pattern ("^d|^(b|c)^a");
  valid_pattern ("^d|^(b|c)a^");
  valid_pattern ("d$|(b|c)$a$");
  valid_pattern ("d$|(b|c$)a$");
  valid_pattern ("d$|(b|$c)a$");
  valid_pattern ("d$|(b$|c)a$");
  valid_pattern ("d$|($b|c)a$");
  valid_pattern ("d$|$(b|c)a$");
  valid_pattern ("$d$|(b|c)a$");

   /* xx Do these use all the valid_nonposix_pattern ones in other_test.c?  */

  TEST_SEARCH ("(^a|^b)c", "ac", 0, 2);
  TEST_SEARCH ("(^a|^b)c", "bc", 0, 2);
  TEST_SEARCH ("c(a$|b$)", "ca", 0, 2);
  TEST_SEARCH ("c(a$|b$)", "cb", 0, 2);
  TEST_SEARCH ("^(a|b)|^c", "ad", 0, 2);
  TEST_SEARCH ("^(a|b)|^c", "bd", 0, 2);
  TEST_SEARCH ("(a|b)$|c$", "da", 0, 2);
  TEST_SEARCH ("(a|b)$|c$", "db", 0, 2);
  TEST_SEARCH ("(a|b)$|c$", "dc", 0, 2);
  TEST_SEARCH ("(^a|^b)|^c", "ad", 0, 2);
  TEST_SEARCH ("(^a|^b)|^c", "bd", 0, 2);
  TEST_SEARCH ("(^a|^b)|^c", "cd", 0, 2);
  TEST_SEARCH ("(a$|b$)|c$", "da", 0, 2);
  TEST_SEARCH ("(a$|b$)|c$", "db", 0, 2);
  TEST_SEARCH ("(a$|b$)|c$", "dc", 0, 2);
  TEST_SEARCH ("^c|(^a|^b)", "ad", 0, 2);
  TEST_SEARCH ("^c|(^a|^b)", "bd", 0, 2);
  TEST_SEARCH ("^c|(^a|^b)", "cd", 0, 2);
  TEST_SEARCH ("c$|(a$|b$)", "da", 0, 2);
  TEST_SEARCH ("c$|(a$|b$)", "db", 0, 2);
  TEST_SEARCH ("c$|(a$|b$)", "dc", 0, 2);
  TEST_SEARCH ("^c|^(a|b)", "ad", 0, 2);
  TEST_SEARCH ("^c|^(a|b)", "bd", 0, 2);
  TEST_SEARCH ("^c|^(a|b)", "cd", 0, 2);
  TEST_SEARCH ("c$|(a|b)$", "da", 0, 2);
  TEST_SEARCH ("c$|(a|b)$", "db", 0, 2);
  TEST_SEARCH ("c$|(a|b)$", "dc", 0, 2);
  TEST_SEARCH ("(^a|^b)c|^d", "ace", 0, 3);
  TEST_SEARCH ("(^a|^b)c|^d", "bce", 0, 3);
  TEST_SEARCH ("(^a|^b)c|^d", "de", 0, 2);
  TEST_SEARCH ("(a|b)c$|d$", "eac", 0, 3);
  TEST_SEARCH ("(a|b)c$|d$", "ebc", 0, 3);
  TEST_SEARCH ("(a|b)c$|d$", "ed", 0, 3);
  TEST_SEARCH ("^d|^c(a|b)", "cae", 0, 3);
  TEST_SEARCH ("^d|^c(a|b)", "cbe", 0, 3);
  TEST_SEARCH ("^d|^c(a|b)", "de", 0, 3);
  TEST_SEARCH ("d$|c(a$|b$)", "eca", 0, 3);
  TEST_SEARCH ("d$|c(a$|b$)", "ecb", 0, 3);
  TEST_SEARCH ("d$|c(a$|b$)", "ed", 0, 3);

  TEST_SEARCH ("(((^a|^b))c|^d)e", "acef", 0, 4);
  TEST_SEARCH ("(((^a|^b))c|^d)e", "bcef", 0, 4);
  TEST_SEARCH ("(((^a|^b))c|^d)e", "def", 0, 3);

  TEST_SEARCH ("((^(a|b))c|^d)e", "acef", 0, 4);
  TEST_SEARCH ("((^(a|b))c|^d)e", "bcef", 0, 4);
  TEST_SEARCH ("((^(a|b))c|^d)e", "def", 0, 3);

  TEST_SEARCH ("(^((a|b))c|^d)e", "acef", 0, 4);
  TEST_SEARCH ("(^((a|b))c|^d)e", "bcef", 0, 4);
  TEST_SEARCH ("(^((a|b))c|^d)e", "def", 0, 3);

  TEST_SEARCH ("(((a|b))c|d)e$", "face", 0, 4);
  TEST_SEARCH ("(((a|b))c|d)e$", "fbce", 0, 4);
  TEST_SEARCH ("(((a|b))c|d)e$", "fde", 0, 3);

  TEST_SEARCH ("^e(d|c((a|b)))", "edf", 0, 3);
  TEST_SEARCH ("^e(d|c((a|b)))", "ecaf", 0, 4);
  TEST_SEARCH ("^e(d|c((a|b)))", "ecbf", 0, 4);

  TEST_SEARCH ("e(d$|c((a$|b$)))", "fed", 0, 3);
  TEST_SEARCH ("e(d$|c((a$|b$)))", "feca", 0, 4);
  TEST_SEARCH ("e(d$|c((a$|b$)))", "fecb", 0, 4);

  TEST_SEARCH ("e(d$|c((a|b)$))", "fed", 0, 3);
  TEST_SEARCH ("e(d$|c((a|b)$))", "feca", 0, 4);
  TEST_SEARCH ("e(d$|c((a|b)$))", "fecb", 0, 4);

  TEST_SEARCH ("e(d$|c((a|b))$)", "fed", 0, 3);
  TEST_SEARCH ("e(d$|c((a|b))$)", "feca", 0, 3);
  TEST_SEARCH ("e(d$|c((a|b))$)", "fecb", 0, 3);

  TEST_SEARCH ("e(d$|c((a|b)))$", "fed", 0, 3);
  TEST_SEARCH ("e(d$|c((a|b)))$", "feca", 0, 3);
  TEST_SEARCH ("e(d$|c((a|b)))$", "fecb", 0, 3);

  TEST_SEARCH ("(((^a|^b))c)|^de", "acf", 0, 3);
  TEST_SEARCH ("(((^a|^b))c)|^de", "bcf", 0, 3);
  TEST_SEARCH ("(((^a|^b))c)|^de", "def", 0, 3);

  TEST_SEARCH ("(((a|b))c$)|de$", "fac", 0, 3);
  TEST_SEARCH ("(((a|b))c$)|de$", "fbc", 0, 3);
  TEST_SEARCH ("(((a|b))c$)|de$", "fde", 0, 3);

  TEST_SEARCH ("(((a|b))c)$|de$", "fac", 0, 3);
  TEST_SEARCH ("(((a|b))c)$|de$", "fbc", 0, 3);
  TEST_SEARCH ("(((a|b))c)$|de$", "fde", 0, 3);

  TEST_SEARCH ("^ed|^(c((a|b)))", "edf", 0, 3);
  TEST_SEARCH ("^ed|^(c((a|b)))", "caf", 0, 3);
  TEST_SEARCH ("^ed|^(c((a|b)))", "cbf", 0, 3);

  TEST_SEARCH ("^ed|(^c((a|b)))", "edf", 0, 3);
  TEST_SEARCH ("^ed|(^c((a|b)))", "caf", 0, 3);
  TEST_SEARCH ("^ed|(^c((a|b)))", "cbf", 0, 3);

  TEST_SEARCH ("ed$|(c((a|b)))$", "fed", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b)))$", "fca", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b)))$", "fcb", 0, 3);

  TEST_SEARCH ("ed$|(c((a|b))$)", "fed", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b))$)", "fca", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b))$)", "fcb", 0, 3);

  TEST_SEARCH ("ed$|(c((a|b)$))", "fed", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b)$))", "fca", 0, 3);
  TEST_SEARCH ("ed$|(c((a|b)$))", "fcb", 0, 3);

  TEST_SEARCH ("ed$|(c((a$|b$)))", "fed", 0, 3);
  TEST_SEARCH ("ed$|(c((a$|b$)))", "fca", 0, 3);
  TEST_SEARCH ("ed$|(c((a$|b$)))", "fcb", 0, 3);

  TEST_SEARCH ("^a(b|c)|^d", "abe", 0, 3);
  TEST_SEARCH ("^a(b|c)|^d", "ace", 0, 3);
  TEST_SEARCH ("^a(b|c)|^d", "df", 0, 2);

  TEST_SEARCH ("a(b$|c$)|d$", "fab", 0, 3);
  TEST_SEARCH ("a(b$|c$)|d$", "fac", 0, 3);
  TEST_SEARCH ("a(b$|c$)|d$", "fd", 0, 2);

  TEST_SEARCH ("^(a)(b|c)|^d", "abe", 0, 3);
  TEST_SEARCH ("^(a)(b|c)|^d", "ace", 0, 3);
  TEST_SEARCH ("^(a)(b|c)|^d", "df", 0, 2);
  
  TEST_SEARCH ("(^a)(b|c)|^d", "abe", 0, 3);
  TEST_SEARCH ("(^a)(b|c)|^d", "ace", 0, 3);
  TEST_SEARCH ("(^a)(b|c)|^d", "df", 0, 2);
  
  TEST_SEARCH ("(a)(b|c)$|d$", "fab", 0, 3);
  TEST_SEARCH ("(a)(b|c)$|d$", "fac", 0, 3);
  TEST_SEARCH ("(a)(b|c)$|d$", "fd", 0, 2);

  TEST_SEARCH ("(b|c)(a)$|d$", "fba", 0, 3);
  TEST_SEARCH ("(b|c)(a)$|d$", "fca", 0, 3);
  TEST_SEARCH ("(b|c)(a)$|d$", "fd", 0, 2);

  TEST_SEARCH ("(b|c)(a$)|d$", "fba", 0, 3);
  TEST_SEARCH ("(b|c)(a$)|d$", "fca", 0, 3);
  TEST_SEARCH ("(b|c)(a$)|d$", "fd", 0, 2);

  TEST_SEARCH ("(a)(b$|c$)|d$", "fab", 0, 3);
  TEST_SEARCH ("(a)(b$|c$)|d$", "fac", 0, 3);
  TEST_SEARCH ("(a)(b$|c$)|d$", "fd", 0, 2);

  TEST_SEARCH ("^d|^(b|c)(a)", "df", 0, 2);
  TEST_SEARCH ("^d|^(b|c)(a)", "baf", 0, 3);
  TEST_SEARCH ("^d|^(b|c)(a)", "caf", 0, 3);
  
  TEST_SEARCH ("^d|(^b|^c)(a)", "df", 0, 2);
  TEST_SEARCH ("^d|(^b|^c)(a)", "baf", 0, 3);
  TEST_SEARCH ("^d|(^b|^c)(a)", "caf", 0, 3);
  
  TEST_SEARCH ("d$|(b|c)(a$)", "fd", 0, 2);
  TEST_SEARCH ("d$|(b|c)(a$)", "fba", 0, 3);
  TEST_SEARCH ("d$|(b|c)(a$)", "fca", 0, 3);

  TEST_SEARCH ("d$|(b|c)(a)$", "fd", 0, 2);
  TEST_SEARCH ("d$|(b|c)(a)$", "fba", 0, 3);
  TEST_SEARCH ("d$|(b|c)(a)$", "fca", 0, 3);

  TEST_SEARCH ("d$|(b|c)(a$)", "fd", 0, 2);
  TEST_SEARCH ("d$|(b|c)(a$)", "fba", 0, 3);
  TEST_SEARCH ("d$|(b|c)(a$)", "fca", 0, 3);

  TEST_SEARCH ("^d|^(a)(b|c)", "df", 0, 2);
  TEST_SEARCH ("^d|^(a)(b|c)", "abf", 0, 3);
  TEST_SEARCH ("^d|^(a)(b|c)", "acf", 0, 3);

  TEST_SEARCH ("^d|(^a)(b|c)", "df", 0, 2);
  TEST_SEARCH ("^d|(^a)(b|c)", "abf", 0, 3);
  TEST_SEARCH ("^d|(^a)(b|c)", "acf", 0, 3);

  TEST_SEARCH ("d$|(a)(b$|c$)", "fd", 0, 2);
  TEST_SEARCH ("d$|(a)(b$|c$)", "fab", 0, 3);
  TEST_SEARCH ("d$|(a)(b$|c$)", "fac", 0, 3);

  TEST_SEARCH ("d$|(a)(b|c)$", "fd", 0, 2);
  TEST_SEARCH ("d$|(a)(b|c)$", "fab", 0, 3);
  TEST_SEARCH ("d$|(a)(b|c)$", "fac", 0, 3);

  TEST_SEARCH ("((^a|^b)|^c)|^d", "ae", 0, 2);
  TEST_SEARCH ("((^a|^b)|^c)|^d", "be", 0, 2);
  TEST_SEARCH ("((^a|^b)|^c)|^d", "ce", 0, 2);
  TEST_SEARCH ("((^a|^b)|^c)|^d", "de", 0, 2);

  TEST_SEARCH ("((a|b)|c)|d$", "ed", 0, 2);
  TEST_SEARCH ("((a|b)|c)|d$", "ea", 0, 2);
  TEST_SEARCH ("((a|b)|c)|d$", "eb", 0, 2);
  TEST_SEARCH ("((a|b)|c)|d$", "ec", 0, 2);

  TEST_SEARCH ("^d|(c|(a|b))", "de", 0, 2);

  TEST_SEARCH ("d$|(c$|(a$|b$))", "ed", 0, 2);
  TEST_SEARCH ("d$|(c$|(a$|b$))", "ec", 0, 2);
  TEST_SEARCH ("d$|(c$|(a$|b$))", "ea", 0, 2);
  TEST_SEARCH ("d$|(c$|(a$|b$))", "eb", 0, 2);

  TEST_SEARCH ("d$|(c$|(a|b)$)", "ed", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b)$)", "ec", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b)$)", "ea", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b)$)", "eb", 0, 2);

  TEST_SEARCH ("d$|(c$|(a|b))$", "ed", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b))$", "ec", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b))$", "ea", 0, 2);
  TEST_SEARCH ("d$|(c$|(a|b))$", "eb", 0, 2);

  test_match ("a|^b", "b");
  test_match ("a|b$", "b");
  test_match ("^b|a", "b");
  test_match ("b$|a", "b");
  test_match ("(^a)", "a");
  test_match ("(a$)", "a");
  TEST_SEARCH ("c|^ab", "aba", 0, 3);
  TEST_SEARCH ("c|ba$", "aba", 0, 3);
  TEST_SEARCH ("^ab|c", "aba", 0, 3);
  TEST_SEARCH ("ba$|c", "aba", 0, 3);
  TEST_SEARCH ("(^a)", "ab", 0, 2);
  TEST_SEARCH ("(a$)", "ba", 0, 2);

  TEST_SEARCH ("(^a$)", "a", 0, 1);
  TEST_SEARCH ("(^a)", "ab", 0, 2);
  TEST_SEARCH ("(b$)", "ab", 0, 2);

                                                /* Backtracking.  */
  /* Per POSIX D11.1 p. 108, leftmost longest match.  */
  test_match ("(wee|week)(knights|night)", "weeknights");

  test_match ("(fooq|foo)qbar", "fooqbar");
  test_match ("(fooq|foo)(qbarx|bar)", "fooqbarx");

  /* Take first alternative that does the longest match.  */
  test_all_registers ("(fooq|(foo)|(fo))((qbarx)|(oqbarx)|bar)", "fooqbarx", 
    "", 0, 8,  0, 3,  0, 3,  -1, -1,  3, 8,  3, 8,  -1, -1,  -1, -1, -1, -1,  
    -1, -1);

  test_match ("(fooq|foo)*qbar", "fooqbar");
  test_match ("(fooq|foo)*(qbar)", "fooqbar");
  test_match ("(fooq|foo)*(qbar)*", "fooqbar"); 

  test_match ("(fooq|fo|o)*qbar", "fooqbar");
  test_match ("(fooq|fo|o)*(qbar)", "fooqbar");
  test_match ("(fooq|fo|o)*(qbar)*", "fooqbar");

  test_match ("(fooq|fo|o)*(qbar|q)*", "fooqbar");
  test_match ("(fooq|foo)*(qbarx|bar)", "fooqbarx");
  test_match ("(fooq|foo)*(qbarx|bar)*", "fooqbarx");

  test_match ("(fooq|fo|o)+(qbar|q)+", "fooqbar");
  test_match ("(fooq|foo)+(qbarx|bar)", "fooqbarx");
  test_match ("(fooq|foo)+(qbarx|bar)+", "fooqbarx");

  /* Per Mike Haertel.  */
  test_match ("(foo|foobarfoo)(bar)*", "foobarfoo");

  						/* Combination.  */
  test_match ("[ab]?c", "ac");
  test_match ("[ab]*c", "ac");
  test_match ("[ab]+c", "ac");
  test_match ("(a|b)?c", "ac");
  test_match ("(a|b)*c", "ac");
  test_match ("(a|b)+c", "ac");
  test_match ("(a*c)?b", "b");
  test_match ("(a*c)+b", "aacb");
						/* Registers.  */
  /* Per David A. Willcox.  */
  test_match ("a((b)|(c))d", "acd");
  test_all_registers ("a((b)|(c))d", "acd", "", 0, 3, 1, 2, -1, -1, 1, 2,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);


  /* Extended regular expressions, continued; these don't match their strings.  */
  test_should_match = false;

#if 0
                                /* Invalid use of special characters.  */
  /* These are not invalid anymore, since POSIX says the behavior is
     undefined, and we prefer context-independent to context-invalid.  */
  invalid_pattern (REG_BADRPT, "*");
  invalid_pattern (REG_BADRPT, "a|*");
  invalid_pattern (REG_BADRPT, "(*)");
  invalid_pattern (REG_BADRPT, "^*");
  invalid_pattern (REG_BADRPT, "+");
  invalid_pattern (REG_BADRPT, "a|+");
  invalid_pattern (REG_BADRPT, "(+)");
  invalid_pattern (REG_BADRPT, "^+");

  invalid_pattern (REG_BADRPT, "?");
  invalid_pattern (REG_BADRPT, "a|?");
  invalid_pattern (REG_BADRPT, "(?)");
  invalid_pattern (REG_BADRPT, "^?");

  invalid_pattern (REG_BADPAT, "|");
  invalid_pattern (REG_BADPAT, "a|");
  invalid_pattern (REG_BADPAT, "a||");
  invalid_pattern (REG_BADPAT, "(|a)");
  invalid_pattern (REG_BADPAT, "(a|)");

  invalid_pattern (REG_BADPAT, PARENS_TO_OPS ("(|)"));

  invalid_pattern (REG_BADRPT, "{1}");
  invalid_pattern (REG_BADRPT, "a|{1}");
  invalid_pattern (REG_BADRPT, "^{1}");
  invalid_pattern (REG_BADRPT, "({1})");

  invalid_pattern (REG_BADPAT, "|b");

  invalid_pattern (REG_BADRPT, "^{0,}*");
  invalid_pattern (REG_BADRPT, "$*");
  invalid_pattern (REG_BADRPT, "${0,}*");
#endif /* 0 */

  invalid_pattern (REG_EESCAPE, "\\");

  test_match ("a?b", "a");


  test_match ("a+", "");
  test_match ("a+b", "a");
  test_match ("a?", "b");

#if 0
  /* We make empty groups valid now, since they are undefined in POSIX.
    (13 Sep 92) */
                                                /* Subexpressions.  */
  invalid_pattern (REG_BADPAT, "()");
  invalid_pattern (REG_BADPAT, "a()");
  invalid_pattern (REG_BADPAT, "()b");
  invalid_pattern (REG_BADPAT, "a()b");
  invalid_pattern (REG_BADPAT, "()*");
  invalid_pattern (REG_BADPAT, "(()*");
#endif
                                                /* Invalid intervals.  */
  test_match ("a{2}*", "aaa");
  test_match ("a{2}?", "aaa");
  test_match ("a{2}+", "aaa");
  test_match ("a{2}{2}", "aaa");
  test_match ("a{1}{1}{2}", "aaa");
  test_match ("a{1}{1}{2}", "a");
						/* Invalid alternation.  */
  test_match ("a|b", "c");

  TEST_SEARCH ("c|^ba", "aba", 0, 3);
  TEST_SEARCH ("c|ab$", "aba", 0, 3);
  TEST_SEARCH ("^ba|c", "aba", 0, 3);
  TEST_SEARCH ("ab$|c", "aba", 0, 3);
						/* Invalid anchoring.  */
  TEST_SEARCH ("(^a)", "ba", 0, 2);
  TEST_SEARCH ("(b$)", "ba", 0, 2);

  printf ("\nFinished POSIX extended tests.\n");
}



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
