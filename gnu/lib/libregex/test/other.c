/* other.c: test (not exhaustively) non-POSIX regular expressions.  */

#include "test.h"

void
test_others ()
{
  struct re_registers regs;
  
  printf ("\nStarting non-POSIX tests.\n");
  t = other_test;

  test_should_match = true;

  /* The big question: does the group participate in the match, or match
     the empty string?  */
  re_set_syntax (RE_NO_BK_PARENS);
  test_match ("(a*)*ab", "ab");
  TEST_REGISTERS ("(a*)*ab", "ab", 0, 2, 0, 0, -1, -1);
  test_match ("(a*)*", "");
  TEST_REGISTERS ("(a*)*ab", "ab", 0, 0, 0, 0, -1, -1);
  
  /* This tests finding the highest and lowest active registers.  */
  test_match ("(a(b)c(d(e)f)g)h(i(j)k(l(m)n)o)\\1\\2\\3\\4\\5\\6\\7\\8",
              "abcdefghijklmnoabcdefgbdefeijklmnojlmnm");

  /* Test that \< and \> match at the beginning and end of the string.  */
  test_match ("\\<abc\\>", "abc");
  
  /* May as well test \` and \' while we're at it.  */
  test_match ("\\`abc\\'", "abc");
  
#if 0
  /* Test backreferencing and the fastmap -- which doesn't work.  */
  test_fastmap ("(a)*\\1", "a", 0, 0);
#endif

  /* But at least we shouldn't search improperly.  */
  test_search_return (-1, "(a)\\1", "");
  
  re_set_syntax (RE_SYNTAX_EMACS);
  
  MATCH_SELF("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  MATCH_SELF ("a^");
  MATCH_SELF ("a^b");
  MATCH_SELF ("$a");
  MATCH_SELF ("a$b");

  re_set_syntax (RE_BACKSLASH_ESCAPE_IN_LISTS);
  test_match ("[\\^a]", "a");
  test_match ("[\\^a]", "^");

  /* These op characters should be ordinary if RE_CONTEXT_INVALID_OPS
     isn't set.  */
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_BRACES | RE_INTERVALS
                 | RE_NO_BK_PARENS);
  MATCH_SELF ("*");
  test_match ("a|*", "*");
  test_match ("(*)", "*");

  MATCH_SELF ("+");
  test_match ("a|+", "+");
  test_match ("(+)", "+");

  MATCH_SELF ("?");
  test_match ("a|?", "?");
  test_match ("(?)", "?");

  MATCH_SELF ("{1}");
  test_match ("a|{1}", "a");
  test_match ("a|{1}", "{1}");
  test_match ("({1})", "{1}");
  
  test_match ("\\{", "{");


  re_set_syntax (RE_LIMITED_OPS);
  MATCH_SELF ("|");
  MATCH_SELF ("a|");
  MATCH_SELF ("a|");
  MATCH_SELF ("a||");
  MATCH_SELF ("a||");
  MATCH_SELF ("(|)");

  re_set_syntax (RE_SYNTAX_EMACS);
  TEST_SEARCH ("^a", "b\na", 0, 3);
  TEST_SEARCH ("b$", "b\na", 0, 3);

#if 0
  /* Newline is no longer special for anchors (16 Sep 92).  --karl  */
  test_match_2 ("a\n^b", "a", "\nb");
  test_match_2 ("a$\nb", "a\n", "b");
#endif

  /* Test grouping.  */
  re_set_syntax (RE_NO_BK_PARENS);
  
  test_match ("()", "");
  test_fastmap ("()", "", 0, 0);
  TEST_REGISTERS ("()", "", 0, 0, 0, 0, -1, -1);

  test_match ("((((((((()))))))))", "");
  test_fastmap ("((((((((()))))))))", "", 0, 0);
  test_match ("a()b", "ab");
  TEST_REGISTERS ("a()b", "ab", 0, 2, 1, 1, -1, -1);

  test_match ("(((((((((())))))))))", "");
  test_fastmap ("(((((((((())))))))))", "", 0, 0);
  
  test_match ("()*", "");
  TEST_REGISTERS ("()*", "", 0, 0, 0, 0, -1, -1);  /* empty string */
  test_match ("(())*", "");

  re_set_syntax (RE_CONTEXT_INDEP_OPS);
  test_match ("*", "");
  
  re_set_syntax (RE_INTERVALS | RE_CONTEXT_INDEP_OPS | RE_NO_BK_BRACES);
  test_match ("{1}", "");		/* Should remain an interval.  */
  MATCH_SELF ("{1");			/* Not a valid interval.  */
  
  re_set_syntax (RE_NEWLINE_ALT);
  test_match ("a\nb", "a");
  test_match ("a\nb", "b");
  
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS);
  test_match ("^a", "a");
  test_match ("(^a)", "a");
  test_match ("(a|^b)", "b");
  test_match ("a$", "a");
  test_match ("(a$)", "a");
  test_match ("a$|b", "a");
  
  /* You should be able to have empty alternatives if RE_NO_EMPTY_ALTS
     isn't set.  */
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS);
  
  test_match ("|", "");
  test_match ("^|a", "");
  test_match ("^|a", "a");
  test_match ("a|", "");
  test_match ("a|", "a");
  test_match ("a|$", "");
  test_match ("a|$", "a");
  test_match ("a||b", "a");
  test_match ("a||b", "");
  test_match ("a||b", "b");
  test_match ("(|a)", "");
  test_match ("(|a)", "a");
  test_match ("(a|)", "");
  test_match ("(a|)", "a");

  TEST_SEARCH ("a|$", "xa", 0, 2);
  TEST_SEARCH ("a|$", "x", 0, 1);
  TEST_SEARCH ("$|b", "x", 0, 1);
  TEST_SEARCH ("$|b", "xb", 0, 2);
  TEST_SEARCH ("c(a|$)", "xca", 0, 3);
  TEST_SEARCH ("c(a|$)", "xc", 0, 2);
  TEST_SEARCH ("c($|b)", "xcb", 0, 3);
  TEST_SEARCH ("c($|b)", "xc", 0, 2);
  TEST_SEARCH ("c($|b$)", "xcb", 0, 3);
  TEST_SEARCH ("c($|b$)", "xc", 0, 2);
  TEST_SEARCH ("c(a$|$)", "xca", 0, 3);
  TEST_SEARCH ("c(a$|$)", "xc", 0, 2);
  TEST_SEARCH ("(a$|b$)|$", "x", 0, 1);
  TEST_SEARCH ("(a$|b$)|$", "xa", 0, 2);
  TEST_SEARCH ("(a$|b$)|$", "xb", 0, 2);
  TEST_SEARCH ("(a$|$)|c$", "x", 0, 1);
  TEST_SEARCH ("(a$|$)|c$", "xa", 0, 2);
  TEST_SEARCH ("(a$|$)|c$", "xc", 0, 2);
  TEST_SEARCH ("($|b$)|c$", "x", 0, 1);
  TEST_SEARCH ("($|b$)|c$", "xb", 0, 2);
  TEST_SEARCH ("($|b$)|c$", "xc", 0, 2);
  TEST_SEARCH ("c$|(a$|$)", "x", 0, 1);
  TEST_SEARCH ("c$|(a$|$)", "xa", 0, 2);
  TEST_SEARCH ("c$|(a$|$)", "xc", 0, 2);
  TEST_SEARCH ("c$|($|b$)", "x", 0, 1);
  TEST_SEARCH ("c$|($|b$)", "xb", 0, 2);
  TEST_SEARCH ("c$|($|b$)", "xc", 0, 2);
  TEST_SEARCH ("$|(a$|b$)", "x", 0, 1);
  TEST_SEARCH ("$|(a$|b$)", "xa", 0, 2);
  TEST_SEARCH ("$|(a$|b$)", "xb", 0, 2);
  TEST_SEARCH ("c(a$|b$)|$", "x", 0, 1);
  TEST_SEARCH ("c(a$|b$)|$", "xca", 0, 3);
  TEST_SEARCH ("c(a$|b$)|$", "xcb", 0, 3);
  TEST_SEARCH ("c(a$|$)|d$", "xc", 0, 2);
  TEST_SEARCH ("c(a$|$)|d$", "xca", 0, 3);
  TEST_SEARCH ("c(a$|$)|d$", "xd", 0, 2);
  TEST_SEARCH ("c($|b$)|d$", "xc", 0, 2);
  TEST_SEARCH ("c($|b$)|d$", "xcb", 0, 3);
  TEST_SEARCH ("c($|b$)|d$", "xd", 0, 2);
  TEST_SEARCH ("d(c$|e((a$|$)))", "xdc", 0, 3);
  TEST_SEARCH ("d(c$|e((a$|$)))", "xde", 0, 3);
  TEST_SEARCH ("d(c$|e((a$|$)))", "xdea", 0, 4);
  TEST_SEARCH ("d(c$|e(($|b$)))", "xdc", 0, 3);
  TEST_SEARCH ("d(c$|e(($|b$)))", "xde", 0, 3);
  TEST_SEARCH ("d(c$|e(($|b$)))", "xdeb", 0, 4);
  TEST_SEARCH ("d($|e((a$|b$)))", "xd", 0, 2);
  TEST_SEARCH ("d($|e((a$|b$)))", "xdea", 0, 4);
  TEST_SEARCH ("d($|e((a$|b$)))", "xdeb", 0, 4);
  TEST_SEARCH ("a(b$|c$)|$", "x", 0, 1);
  TEST_SEARCH ("a(b$|c$)|$", "xab", 0, 3);
  TEST_SEARCH ("a(b$|c$)|$", "xac", 0, 3);
  TEST_SEARCH ("a(b$|$)|d$", "xa", 0, 2);
  TEST_SEARCH ("a(b$|$)|d$", "xab", 0, 3);
  TEST_SEARCH ("a(b$|$)|d$", "xd", 0, 2);
  TEST_SEARCH ("a($|c$)|d$", "xa", 0, 2);
  TEST_SEARCH ("a($|c$)|d$", "xac", 0, 3);
  TEST_SEARCH ("a($|c$)|d$", "xd", 0, 2);
  TEST_SEARCH ("d$|a(b$|$)", "xd", 0, 2);
  TEST_SEARCH ("d$|a(b$|$)", "xa", 0, 2);
  TEST_SEARCH ("d$|a(b$|$)", "xab", 0, 3);
  TEST_SEARCH ("d$|a($|c$)", "xd", 0, 2);
  TEST_SEARCH ("d$|a($|c$)", "xa", 0, 2);
  TEST_SEARCH ("d$|a($|c$)", "xac", 0, 3);
  TEST_SEARCH ("$|a(b$|c$)", "x", 0, 1);
  TEST_SEARCH ("$|a(b$|c$)", "xab", 0, 3);
  TEST_SEARCH ("$|a(b$|c$)", "xac", 0, 3);
  TEST_SEARCH ("(a)(b$|c$)|d$", "xab", 0, 3);
  TEST_SEARCH ("(a)(b$|c$)|d$", "xac", 0, 3);
  TEST_SEARCH ("(a)(b$|c$)|d$", "xd", 0, 2);
  TEST_SEARCH ("(a)(b$|$)|d$", "xa", 0, 2);
  TEST_SEARCH ("(a)(b$|$)|d$", "xab", 0, 3);
  TEST_SEARCH ("(a)(b$|$)|d$", "xd", 0, 2);
  TEST_SEARCH ("(a)($|c$)|d$", "xa", 0, 2);
  TEST_SEARCH ("(a)($|c$)|d$", "xac", 0, 3);
  TEST_SEARCH ("(a)($|c$)|d$", "xd", 0, 2);
  TEST_SEARCH ("d$|(a)(b$|$)", "xd", 0, 2);
  TEST_SEARCH ("d$|(a)(b$|$)", "xa", 0, 2);
  TEST_SEARCH ("d$|(a)(b$|$)", "xab", 0, 3);
  TEST_SEARCH ("d$|(a)($|c$)", "xd", 0, 2);
  TEST_SEARCH ("d$|(a)($|c$)", "xa", 0, 2);
  TEST_SEARCH ("d$|(a)($|c$)", "xac", 0, 3);
  TEST_SEARCH ("$|(a)(b$|c$)", "x", 0, 1);
  TEST_SEARCH ("$|(a)(b$|c$)", "xab", 0, 3);
  TEST_SEARCH ("$|(a)(b$|c$)", "xac", 0, 3);
  TEST_SEARCH ("d$|(c$|(a$|$))", "x", 0, 1);
  TEST_SEARCH ("d$|(c$|(a$|$))", "xd", 0, 2);
  TEST_SEARCH ("d$|(c$|(a$|$))", "xc", 0, 2);
  TEST_SEARCH ("d$|(c$|(a$|$))", "xa", 0, 2);
  TEST_SEARCH ("d$|(c$|($|b$))", "x", 0, 1);
  TEST_SEARCH ("d$|(c$|($|b$))", "xd", 0, 2);
  TEST_SEARCH ("d$|(c$|($|b$))", "xc", 0, 2);
  TEST_SEARCH ("d$|(c$|($|b$))", "xb", 0, 2);
  TEST_SEARCH ("d$|($|(a$|b$))", "x", 0, 1);
  TEST_SEARCH ("d$|($|(a$|b$))", "xd", 0, 2);
  TEST_SEARCH ("d$|($|(a$|b$))", "xa", 0, 2);
  TEST_SEARCH ("d$|($|(a$|b$))", "xb", 0, 2);
  TEST_SEARCH ("$|(c$|(a$|b$))", "x", 0, 1);
  TEST_SEARCH ("$|(c$|(a$|b$))", "xc", 0, 2);
  TEST_SEARCH ("$|(c$|(a$|b$))", "xa", 0, 2);
  TEST_SEARCH ("$|(c$|(a$|b$))", "xb", 0, 2);
  TEST_SEARCH ("d$|c(a$|$)", "xd", 0, 2);
  TEST_SEARCH ("d$|c(a$|$)", "xc", 0, 2);
  TEST_SEARCH ("d$|c(a$|$)", "xca", 0, 3);
  TEST_SEARCH ("d$|c($|b$)", "xd", 0, 2);
  TEST_SEARCH ("d$|c($|b$)", "xc", 0, 2);
  TEST_SEARCH ("d$|c($|b$)", "xcb", 0, 3);
  TEST_SEARCH ("$|c(a$|b$)", "x", 0, 1);
  TEST_SEARCH ("$|c(a$|b$)", "xca", 0, 3);
  TEST_SEARCH ("$|c(a$|b$)", "xcb", 0, 3);
  TEST_SEARCH ("e(d$|c((a$|$)))", "xed", 0, 3);
  TEST_SEARCH ("e(d$|c((a$|$)))", "xec", 0, 3);
  TEST_SEARCH ("e(d$|c((a$|$)))", "xeca", 0, 3);
  TEST_SEARCH ("e(d$|c(($|b$)))", "xed", 0, 3);
  TEST_SEARCH ("e(d$|c(($|b$)))", "xec", 0, 3);
  TEST_SEARCH ("e(d$|c(($|b$)))", "xecb", 0, 4);
  TEST_SEARCH ("e($|c((a$|b$)))", "xe", 0, 2);
  TEST_SEARCH ("e($|c((a$|b$)))", "xeca", 0, 4);
  TEST_SEARCH ("e($|c((a$|b$)))", "xecb", 0, 4);
  TEST_SEARCH ("ed$|(c((a$|$)))", "xed", 0, 3);
  TEST_SEARCH ("ed$|(c((a$|$)))", "xc", 0, 2);
  TEST_SEARCH ("ed$|(c((a$|$)))", "xca", 0, 3);
  TEST_SEARCH ("ed$|(c(($|b$)))", "xed", 0, 3);
  TEST_SEARCH ("ed$|(c(($|b$)))", "xc", 0, 2);
  TEST_SEARCH ("ed$|(c(($|b$)))", "xcb", 0, 3);
  TEST_SEARCH ("$|(c((a$|b$)))", "x", 0, 1);
  TEST_SEARCH ("$|(c((a$|b$)))", "xca", 0, 3);
  TEST_SEARCH ("$|(c((a$|b$)))", "xcb", 0, 3);
  TEST_SEARCH ("d$|($|(a|b)$)", "x", 0, 1);
  TEST_SEARCH ("d$|($|(a|b)$)", "xa", 0, 2);
  TEST_SEARCH ("d$|($|(a|b)$)", "xb", 0, 2);
  TEST_SEARCH ("$|(c$|(a|b)$)", "x", 0, 1);
  TEST_SEARCH ("$|(c$|(a|b)$)", "xc", 0, 2);
  TEST_SEARCH ("$|(c$|(a|b)$)", "xa", 0, 2);
  TEST_SEARCH ("$|(c$|(a|b)$)", "xb", 0, 2);

  re_set_syntax (0);
  test_match ("[^\n]", "a");
  test_match ("[^a]", "\n");

  TEST_SEARCH ("^a", "b\na", 0, 3);
  TEST_SEARCH ("b$", "b\na", 0, 3);

  test_case_fold ("[!-`]", "A");
  test_case_fold ("[!-`]", "a");
  
  re_set_syntax (RE_CONTEXT_INDEP_OPS | RE_NO_BK_VBAR | RE_NO_BK_PARENS 
                 | RE_NO_BK_BRACES | RE_INTERVALS);
  valid_nonposix_pattern ("()^a");
  valid_nonposix_pattern ("()\\1^a");

  /* Per Cederqvist (cedar@lysator.liu.se) bug.  */

  re_set_syntax (RE_SYNTAX_EMACS);

  /* One `a' before the \n and 638 a's after it.  */
  test_search_return (0, "\\(.*\\)\n\\(\\(.\\|\n\\)*\\)$", "a\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  /* No a's before the \n and 639 a's after it.  */
  test_search_return (0, "\\(.*\\)\n\\(\\(.\\|\n\\)*\\)$", "\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  /* One `a' before the \n and 639 a's after it.  */
  test_search_return (0, "\\(.*\\)\n\\(\\(.\\|\n\\)*\\)$", "a\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  /* No a's before the \n and 640 a's after it.  */
  test_search_return (0, "\\(.*\\)\n\\(\\(.\\|\n\\)*\\)$", "\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS);
  TEST_SEARCH ("^(^a)", "ab", 0, 2);
  TEST_SEARCH ("(a$)$", "ba", 0, 2);
  test_match ("a|$b", "$b");
  
  /* Mike's curiosity item.  */
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS);
  test_all_registers ("(foo|foobar)(foo|bar)*\\1(foo|bar)*",
  		      "foobarfoobar", "",
    0, 12,  0, 3,  3, 6,  9, 12,  -1, -1,   -1, -1,   -1, -1,   -1, -1,  
    -1, -1,   -1, -1);

  /* Another one from Mike.  */
  test_match ("(foo|foobarfoo)(bar)*", "foobarfoo");
  
  /* And another.  */
  test_match("(foo|foobar)(bar|barfoo)?\\1", "foobarfoobar");
  
  re_set_syntax (RE_NO_BK_PARENS | RE_INTERVALS | RE_NO_BK_VBAR
                 | RE_NO_BK_BRACES);  /* xx get new ones from ext.*/
  test_match ("((a{0,}{0,0}()\\3\\b\\B\\<\\>\\`\\')|b)*", "bb");
  test_all_registers ("((a{0,}{0,0}()\\3\\b\\B\\<\\>\\`\\')|b)*", "", "bb", 
    0, 2, 1, 2, -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1);

  test_match ("((a+?*{0,}{0,0}()\\3\\b\\B\\<\\>\\`\\')|b)", "b");
  test_all_registers ("((a+?*{0,}{0,0}()\\3\\b\\B\\<\\>\\`\\')|b)", "", "b",
    0, 1, 0, 1, -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1);

						/* Valid anchoring.  */
          /* See generic_test.c and extended_test.c for more search
             tests. xx Not sure all these tests are represented in the
             search tests.  */
          
  re_set_syntax (RE_NO_BK_PARENS | RE_NO_BK_VBAR);
  valid_nonposix_pattern 
    ("(((((((((((((((((((((((((((((((((^a)))))))))))))))))))))))))))))))))");
  valid_nonposix_pattern 
    ("(((((((((((((((((((((((((((((((((a$)))))))))))))))))))))))))))))))))");
  valid_nonposix_pattern ("\\b\\B\\<\\>\\`\\'^a");
  valid_nonposix_pattern ("a$\\b\\B\\<\\>\\`\\'");
  valid_nonposix_pattern ("(^a)");
  valid_nonposix_pattern ("(a$)");
  valid_nonposix_pattern ("(^a)b");
  valid_nonposix_pattern ("b(a$)");
  valid_nonposix_pattern ("(^a|^b)c");
  valid_nonposix_pattern ("c(a$|b$)");
  valid_nonposix_pattern ("(^a|^b)|^c");
  valid_nonposix_pattern ("(a$|b$)|c$");
  valid_nonposix_pattern ("^c|(^a|^b)");
  valid_nonposix_pattern ("c$|(a$|b$)");
  valid_nonposix_pattern ("(^a|^b)c|^d");
  valid_nonposix_pattern ("c(a$|b$)|d$");
  valid_nonposix_pattern ("(((^a|^b))c|^d)e");
  valid_nonposix_pattern ("(c((a|b))|d)e$");
  valid_nonposix_pattern ("^d(c|e((a|b)))");
  valid_nonposix_pattern ("d(c$|e((a$|b$)))");
  valid_nonposix_pattern ("(((^a|^b))c)|^de");
  valid_nonposix_pattern ("(((a|b))c$)|de$");

  valid_nonposix_pattern ("((a$)$)$");
  valid_nonposix_pattern ("^(^(^a))");

  valid_nonposix_pattern ("^de|^(c((a|b)))");
  valid_nonposix_pattern ("^de|(^c((a|b)))");
  valid_nonposix_pattern ("de$|(c((a|b)$))");
  valid_nonposix_pattern ("de$|(c((a|b))$)");
  valid_nonposix_pattern ("de$|(c((a|b)))$");

  valid_nonposix_pattern ("^a(b|c)|^d");
  valid_nonposix_pattern ("a(b$|c$)|d$");
  valid_nonposix_pattern ("^d|^a(b|c)");
  valid_nonposix_pattern ("d$|a(b$|c$)");
  valid_nonposix_pattern ("^d|^(b|c)a");
  valid_nonposix_pattern ("d$|(b|c)a$");
  valid_nonposix_pattern ("^(a)(b|c)|^d");
  valid_nonposix_pattern ("(a)(b|c)$|d$");
  valid_nonposix_pattern ("(^a)(b|c)|^d");
  valid_nonposix_pattern ("(a)(b$|c$)|d$");
  valid_nonposix_pattern ("^d|^(b|c)(a)");
  valid_nonposix_pattern ("d$|(b|c)(a)$");
  valid_nonposix_pattern ("^d|(^b|^c)(a)");
  valid_nonposix_pattern ("d$|(b|c)(a$)");
  valid_nonposix_pattern ("^d|^(a)(b|c)");
  valid_nonposix_pattern ("^d|(^a)(b|c)");
  valid_nonposix_pattern ("d$|(a)(b$|c$)");
  valid_nonposix_pattern ("((^a|^b)|^c)|^d");
  valid_nonposix_pattern ("d$|(c$|(a$|b$))");


  /* Tests shouldn't match.  */
  test_should_match = false;

  /* Test that RE_CONTEXT_INVALID_OPS has precedence over
     RE_CONTEXT_INDEP_OPS.  */

  re_set_syntax (RE_CONTEXT_INDEP_OPS | RE_CONTEXT_INVALID_OPS
                 | RE_NO_BK_VBAR | RE_NO_BK_PARENS 
                 | RE_NO_BK_BRACES | RE_INTERVALS);
  INVALID_PATTERN ("*");
  INVALID_PATTERN ("^*");
  INVALID_PATTERN ("a|*");
  INVALID_PATTERN ("(*)");

  INVALID_PATTERN ("^+");
  INVALID_PATTERN ("+");
  INVALID_PATTERN ("a|+");
  INVALID_PATTERN ("(+)");

  INVALID_PATTERN ("^?");
  INVALID_PATTERN ("?");
  INVALID_PATTERN ("a|?");
  INVALID_PATTERN ("(?)");

  INVALID_PATTERN ("^{1}");
  INVALID_PATTERN ("{1}");
  INVALID_PATTERN ("a|{1}");
  INVALID_PATTERN ("({1})");

#if 0
  /* No longer have this syntax option -- POSIX says empty alternatives
     are undefined as of draft 11.2.  */

  /* You can't have empty alternatives if RE_NO_EMPTY_ALTS is set.  */
  
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS | RE_NO_EMPTY_ALTS);
  
  INVALID_PATTERN ("|");
  INVALID_PATTERN ("^|a");
  INVALID_PATTERN ("a|");
  INVALID_PATTERN ("a||");
  INVALID_PATTERN ("a||b");
  INVALID_PATTERN ("(|a)");
  INVALID_PATTERN ("(a|)");
  INVALID_PATTERN ("(a|)");


  /* Test above with `\(' and `\)'.  */
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_EMPTY_ALTS);
  INVALID_PATTERN ("\\(|a\\)");
  INVALID_PATTERN ("\\(a|\\)");

  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS | RE_NO_EMPTY_ALTS);
  INVALID_PATTERN ("(|)()$|d$");
#endif

  /* Test grouping.  */
  test_match ("()", "a");			

  /* Test backslashed intervals that are CONTEXTly invalid if have
     nothing on which to operate.  */

  re_set_syntax (RE_INTERVALS | RE_CONTEXT_INVALID_OPS);
  INVALID_PATTERN ("\\{1\\}");

  re_set_syntax (0);
  test_match ("z-a", "a");

  re_set_syntax (RE_BK_PLUS_QM);
  INVALID_PATTERN ("a*\\");
  
  re_set_syntax (0);
  INVALID_PATTERN ("a*\\");
  
  re_set_syntax (RE_BACKSLASH_ESCAPE_IN_LISTS);
  INVALID_PATTERN ("[\\");
  
#if 0
  /* Empty groups are always ok now.  (13 Sep 92)  */
  re_set_syntax (RE_NO_BK_VBAR | RE_NO_BK_PARENS | RE_NO_EMPTY_GROUPS);
  INVALID_PATTERN ("(|)()$|d$");
#endif

  printf ("\nFinished non-POSIX tests.\n");
}



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
