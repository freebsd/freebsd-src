/* psx-interv.c: test POSIX intervals, both basic and extended.  */

#include "test.h"

void
test_intervals ()
{
  printf ("\nStarting POSIX interval tests.\n");

  test_should_match = true;
                                                /* Valid intervals.  */
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,2}b)*")), "abaab");
  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,2}b)*")), "a", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,2}b)*")),
                  "abaab", 0, 5, 2, 5, -1, -1);

  test_match (BRACES_TO_OPS ("a{0}"), "");
  test_fastmap (BRACES_TO_OPS ("a{0}"), "", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0}"), "", 0, 0, -1, -1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0}"), "x", 0, 0, -1, -1, -1, -1);

  test_match (BRACES_TO_OPS ("a{0,}"), "");
  test_match (BRACES_TO_OPS ("a{0,}"), "a");
  test_fastmap (BRACES_TO_OPS ("a{0,}"), "a", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0,}"), "a", 0, 1, -1, -1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0,}"), "xax", 0, 0, -1, -1, -1, -1);

  test_match (BRACES_TO_OPS ("a{1}"), "a");
  test_match (BRACES_TO_OPS ("a{1,}"), "a");
  test_match (BRACES_TO_OPS ("a{1,}"), "aa"); 
  test_match (BRACES_TO_OPS ("a{0,0}"), "");
  test_match (BRACES_TO_OPS ("a{0,1}"), "");
  test_match (BRACES_TO_OPS ("a{0,1}"), "a"); 
  test_match (BRACES_TO_OPS ("a{1,3}"), "a");
  test_match (BRACES_TO_OPS ("a{1,3}"), "aa");
  test_match (BRACES_TO_OPS ("a{1,3}"), "aaa");
  TEST_REGISTERS (BRACES_TO_OPS ("a{1,3}"), "aaa", 0, 3, -1, -1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS ("a{1,3}"), "xaaax", 1, 4, -1, -1, -1, -1);

  test_match (BRACES_TO_OPS ("a{0,3}b"), "b");
  test_match (BRACES_TO_OPS ("a{0,3}b"), "aaab");
  test_fastmap (BRACES_TO_OPS ("a{0,3}b"), "ab", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0,3}b"), "b", 0, 1, -1, -1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS ("a{0,3}b"), "xbx", 1, 2, -1, -1, -1, -1);

  test_match (BRACES_TO_OPS ("a{1,3}b"), "ab");
  test_match (BRACES_TO_OPS ("a{1,3}b"), "aaab");
  test_match (BRACES_TO_OPS ("ab{1,3}c"), "abbbc");

  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a){0,3}b")), "b");
  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a){0,3}b")), "ab", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a){0,3}b")), "b", 0, 1, -1, -1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a){0,3}b")), "ab", 0, 2, 0, 1, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a){0,3}b")), "xabx", 1, 3, 1, 2, -1, -1);

  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a){1,3}b")), "ab");
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a){1,3}b")), "aaab");
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a){1,3}b")), "aaab", 0, 4, 2, 3, -1, -1);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a){1,3}b")), "xaaabx", 1, 5, 3, 4, -1, -1);

  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){0,3}b")), "aaaab");
  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){0,3}b")), "ab", 0, 0);
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){0,3}b")), "aaaab", 0, 5, 4, 4, -1, -1);

  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,3}b")), "b");
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,3}b")), "aaab");
  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,3}b")), "ab", 0, 0);

  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,1}ab")), "aaaab");
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,1}ab")), "aaaab", 0, 5, 0, 3, -1, -1);
  
  test_match (BRACES_TO_OPS (".{0,3}b"), "b");
  test_match (BRACES_TO_OPS (".{0,3}b"), "ab");

  test_match (BRACES_TO_OPS ("[a]{0,3}b"), "b");
  test_match (BRACES_TO_OPS ("[a]{0,3}b"), "aaab");
  test_fastmap (BRACES_TO_OPS ("[a]{0,3}b"), "ab", 0, 0);
  test_match (BRACES_TO_OPS ("[^a]{0,3}b"), "bcdb");
  test_match (BRACES_TO_OPS ("ab{0,3}c"), "abbbc");
  test_match (BRACES_TO_OPS ("[[:digit:]]{0,3}d"), "123d");
  test_fastmap (BRACES_TO_OPS ("[[:digit:]]{0,3}d"), "0123456789d", 0, 0);

  test_match (BRACES_TO_OPS ("\\*{0,3}a"), "***a");
  test_match (BRACES_TO_OPS (".{0,3}b"), "aaab");
  test_match (BRACES_TO_OPS ("a{0,3}a"), "aaa");
						/* Backtracking.  */
  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,})*a")), "a", 0, 0);
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,})*a")), "a");
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a{1,})*a")), "a", 0, 1, -1, -1, -1, -1);

  test_fastmap (BRACES_TO_OPS (PARENS_TO_OPS ("(a{2,})*aa")), "aa", 0, 0);
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a{2,})*aa")), "aa");
  TEST_REGISTERS (BRACES_TO_OPS (PARENS_TO_OPS ("(a{2,})*aa")), "aa", 0, 2, -1, -1, -1, -1);

  test_match (BRACES_TO_OPS ("a{2}*"), "");
  test_match (BRACES_TO_OPS ("a{2}*"), "aa");

  test_match (BRACES_TO_OPS ("a{1}*"), "");
  test_match (BRACES_TO_OPS ("a{1}*"), "a");
  test_match (BRACES_TO_OPS ("a{1}*"), "aa");
  
  test_match (BRACES_TO_OPS ("a{1}{1}"), "a");

  test_match (BRACES_TO_OPS ("a{1}{1}{1}"), "a");
  test_match (BRACES_TO_OPS ("a{1}{1}{2}"), "aa");

  test_match (BRACES_TO_OPS ("a{1}{1}*"), "");
  test_match (BRACES_TO_OPS ("a{1}{1}*"), "a");
  test_match (BRACES_TO_OPS ("a{1}{1}*"), "aa");
  test_match (BRACES_TO_OPS ("a{1}{1}*"), "aaa");
  
  test_match (BRACES_TO_OPS ("a{1}{2}"), "aa");
  test_match (BRACES_TO_OPS ("a{2}{1}"), "aa");


  test_should_match = false;
  
  test_match (BRACES_TO_OPS ("a{0}"), "a");
  test_match (BRACES_TO_OPS ("a{0,}"), "b");
  test_match (BRACES_TO_OPS ("a{1}"), ""); 
  test_match (BRACES_TO_OPS ("a{1}"), "aa");
  test_match (BRACES_TO_OPS ("a{1,}"), "");
  test_match (BRACES_TO_OPS ("a{1,}"), "b");
  test_match (BRACES_TO_OPS ("a{0,0}"), "a"); 
  test_match (BRACES_TO_OPS ("a{0,1}"), "aa");
  test_match (BRACES_TO_OPS ("a{0,1}"), "b");
  test_match (BRACES_TO_OPS ("a{1,3}"), ""); 
  test_match (BRACES_TO_OPS ("a{1,3}"), "aaaa");
  test_match (BRACES_TO_OPS ("a{1,3}"), "b"); 
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a){1,3}b")), "aaaab");
  test_match (BRACES_TO_OPS (PARENS_TO_OPS ("(a*){1,3}b")), "bb");
  test_match (BRACES_TO_OPS ("[a]{0,3}"), "aaaa");
  test_match (BRACES_TO_OPS ("[^a]{0,3}b"), "ab");
  test_match (BRACES_TO_OPS ("ab{0,3}c"), "abababc");
  test_match (BRACES_TO_OPS ("[:alpha:]{0,3}d"), "123d");
  test_match (BRACES_TO_OPS ("\\^{1,3}a"), "a");
  test_match (BRACES_TO_OPS (".{0,3}b"), "aaaab");

  printf ("\nFinished POSIX interval tests.\n");
}
