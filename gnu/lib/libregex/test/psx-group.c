/* psx-group.c: test POSIX grouping, both basic and extended.  */

#include "test.h"


void
test_grouping ()
{
  printf ("\nStarting POSIX grouping tests.\n");

  test_should_match = true;

  test_fastmap (PARENS_TO_OPS ("(a)"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a)"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)"), "a", 0, 1, 0, 1, -1, -1);
  TEST_REGISTERS (PARENS_TO_OPS ("(a)"), "xax", 1, 2, 1, 2, -1, -1);

  test_match (PARENS_TO_OPS ("((a))"), "a");
  test_fastmap (PARENS_TO_OPS ("((a))"), "a", 0, 0);
  TEST_REGISTERS (PARENS_TO_OPS ("((a))"), "a", 0, 1, 0, 1, 0, 1);
  TEST_REGISTERS (PARENS_TO_OPS ("((a))"), "xax", 1, 2, 1, 2, 1, 2);

  test_fastmap (PARENS_TO_OPS ("(a)(b)"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a)(b)"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)(b)"), "ab", 0, 2, 0, 1, 1, 2);

  TEST_REGISTERS (PARENS_TO_OPS ("(a)(b)"), "xabx", 1, 3, 1, 2, 2, 3);

  test_all_registers (PARENS_TO_OPS ("((a)(b))"), "ab", "", 0, 2, 0, 2, 0, 1,
		   1, 2,  -1, -1,  -1, -1,  -1, -1,  -1, -1,  -1, -1,  -1, -1);


  /* Test that we simply ignore groups past the 255th.  */
  test_match (PARENS_TO_OPS ("((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((a))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))"), "a");


  /* Per POSIX D11.1, p. 125.  */

  test_fastmap (PARENS_TO_OPS ("(a)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*"), "", 0, 0, -1, -1, -1, -1);
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*"), "aa", 0, 2, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)"), "", 0, 0, 0, 0, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)"), "a", 0, 1, 0, 1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)b"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)b"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)b"), "b", 0, 1, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)b"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)b"), "ab", 0, 2, 0, 1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a*)b)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)b)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("((a*)b)*"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "ab", 0, 2, 0, 2, 0, 1);

  test_match (PARENS_TO_OPS ("((a*)b)*"), "abb");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "abb", 0, 3, 2, 3, 2, 2);

  test_match (PARENS_TO_OPS ("((a*)b)*"), "aabab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "aabab", 0, 5, 3, 5, 3, 4);

  test_match (PARENS_TO_OPS ("((a*)b)*"), "abbab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "abbab", 0, 5, 3, 5, 3, 4);

  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "xabbabx", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("((a*)b)*"), "abaabaaaab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b)*"), "abaabaaab", 0, 9, 5, 9, 5, 8);

  test_fastmap (PARENS_TO_OPS ("(ab)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(ab)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab)*"), "", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(ab)*"), "abab");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab)*"), "abab", 0, 4, 2, 4, -1, -1);

  /* We match the empty string here.  */
  TEST_REGISTERS (PARENS_TO_OPS ("(ab)*"), "xababx", 0, 0, -1, -1, -1, -1);

  /* Per David A. Willcox.  */
  TEST_REGISTERS (PARENS_TO_OPS ("a(b*)c"), "ac", 0, 2, 1, 1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a)*b"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a)*b"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*b"), "b", 0, 1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(a)*b"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*b"), "ab", 0, 2, 0, 1, -1, -1);

  test_match_2 (PARENS_TO_OPS ("(a)*b"), "a", "ab");
  TEST_REGISTERS_2 (PARENS_TO_OPS ("(a)*b"), "a", "ab", 0, 3, 1, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a)*b"), "aab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*b"), "aab", 0, 3, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a)*a"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a)*a"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*a"), "a", 0, 1, -1, -1, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*"), "", 0, 0, 0, 0, 0, 0);

  test_match (PARENS_TO_OPS ("((a*))*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*))*"), "", 0, 0, 0, 0, 0, 0);
  test_match (PARENS_TO_OPS ("((a*))*"), "aa");

  test_fastmap (PARENS_TO_OPS ("(a*)*b"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)*b"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b"), "b", 0, 1, 0, 0, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b"), "xbx", 1, 2, 1, 1, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)*b"), "ab"); 	/* Per rms.  */
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b"), "ab", 0, 2, 0, 1, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b"), "xabx", 1, 3, 1, 2, -1, -1);

  /* Test register restores.  */
  test_match (PARENS_TO_OPS ("(a*)*b"), "aab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b"), "aab", 0, 3, 0, 2, -1, -1);

  TEST_REGISTERS_2 (PARENS_TO_OPS ("(a*)*b"), "a", "ab", 0, 3, 0, 2, -1, -1);

  /* We are matching the empty string, with backtracking.  */
  test_fastmap (PARENS_TO_OPS ("(a*)a"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)a"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)a"), "a", 0, 1, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)a"), "aa");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)a"), "aa", 0, 2, 0, 1, -1, -1);

  /* We are matching the empty string, with backtracking.  */
/*fails  test_match (PARENS_TO_OPS ("(a*)*a"), "a"); */
  test_match (PARENS_TO_OPS ("(a*)*a"), "aa");
  /* Match the empty string.  */
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*a"), "a", 0, 1, 0, 0, -1, -1);
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*a"), "xax", 1, 2, 1, 1, -1, -1);
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*a"), "aa", 0, 2, 0, 1, -1, -1);
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*a"), "xaax", 1, 3, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a)*ab"), "a", 0 , 0);
  test_match (PARENS_TO_OPS ("(a)*ab"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*ab"), "ab", 0, 2, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(a)*ab"), "aab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*ab"), "aab", 0, 3, 0, 1, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS("(a)*ab"), "xaabx", 1, 4, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)ab"), "a", 0 , 0);
  test_match (PARENS_TO_OPS ("(a*)ab"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)ab"), "ab", 0, 2, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)ab"), "aab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)ab"), "aab", 0, 3, 0, 1, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS ("(a*)ab"), "xaabx", 1, 4, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)*ab"), "a", 0 , 0);
  test_match (PARENS_TO_OPS ("(a*)*ab"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*ab"), "ab", 0, 2, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)*ab"), "aab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*ab"), "aab", 0, 3, 0, 1, -1, -1);

  TEST_REGISTERS (PARENS_TO_OPS("(a*)*ab"), "xaabx", 1, 4, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)*b*c"), "abc", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)*b*c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*b*c"), "c", 0, 1, 0, 0, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a)*(ab)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a)*(ab)*"), "ab");
  /* Register 1 doesn't match at all (vs. matching the empty string)
     because of backtracking, hence -1's.  */
  TEST_REGISTERS (PARENS_TO_OPS ("(a)*(ab)*"), "ab", 0, 2, -1, -1, 0, 2);

  test_match (PARENS_TO_OPS ("(a*)*(ab)*"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*(ab)*"), "ab", 0, 2, 0, 0, 0, 2);

  test_fastmap (PARENS_TO_OPS ("(a*b)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a*b)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*"), "", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b)*"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*"), "b", 0, 1, 0, 1, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b)*"), "baab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*"), "baab", 0, 4, 1, 4, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*b*)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a*b*)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "", 0, 0, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "a", 0, 1, 0, 1, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "ba");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "ba", 0, 2, 1, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "ab", 0, 2, 0, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "aa");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "aa", 0, 2, 0, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "bb");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "bb", 0, 2, 0, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)*"), "aba");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "aba", 0, 3, 2, 3, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b*)b"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)b"), "b", 0, 1, 0, 0, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a*)*(b*)*)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)*(b*)*)*"), "");
  test_all_registers (PARENS_TO_OPS ("((a*)*(b*)*)*"), "", "", 0, 0, 0, 0,
		  0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("((a*)*(b*)*)*"), "aba");
  /* Perhaps register 3 should be 3/3 here?  Not sure if standard
     specifies this.  xx*/
  test_all_registers (PARENS_TO_OPS ("((a*)*(b*)*)*"), "aba", "", 0, 3, 2, 3,
		  2, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a*)(b*))*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)(b*))*"), "");

  test_all_registers (PARENS_TO_OPS ("((a*)(b*))*"), "", "", 0, 0, 0, 0,
		  0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(c(c(a)*(b)*)*)*"), "");

  test_match (PARENS_TO_OPS ("((a*)(b*))*"), "aba");
  test_all_registers (PARENS_TO_OPS ("((a*)(b*))*"), "aba", "", 0, 3, 2, 3,
		  2, 3, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a)*(b)*)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a)*(b)*)*"), "");
  test_all_registers (PARENS_TO_OPS ("((a)*(b)*)*"), "", "", 0, 0, 0, 0,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("((a)*(b)*)*"), "aba");

  test_all_registers (PARENS_TO_OPS ("((a)*(b)*)*"), "aba", "", 0, 3, 2, 3,
		  2, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(c(a)*(b)*)*"), "c", 0, 0);
  test_match (PARENS_TO_OPS ("(c(a)*(b)*)*"), "");
  test_all_registers (PARENS_TO_OPS ("(c(a)*(b)*)*"), "", "", 0, 0, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(c(a)*(b)*)*"), "c");
  test_all_registers (PARENS_TO_OPS ("(c(a)*(b)*)*"), "c", "", 0, 1, 0, 1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("c((a)*(b)*)*"), "c", 0, 0);
  test_match (PARENS_TO_OPS ("c((a)*(b)*)*"), "c");
  test_all_registers (PARENS_TO_OPS ("c((a)*(b)*)*"), "c", "", 0, 1, 1, 1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(((a)*(b)*)*)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(((a)*(b)*)*)*"), "");
  test_all_registers (PARENS_TO_OPS ("(((a)*(b)*)*)*"), "", "", 0, 0, 0, 0,
	0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(c(c(a)*(b)*)*)*"), "");
  test_fastmap (PARENS_TO_OPS ("(c(c(a)*(b)*)*)*"), "c", 0, 0);

  test_all_registers (PARENS_TO_OPS ("(c(c(a)*(b)*)*)*"), "", "", 0, 0, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a)*b)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a)*b)*"), "");

  test_match (PARENS_TO_OPS ("((a)*b)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b)*"), "", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("((a)*b)*"), "abb");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b)*"), "abb", 0, 3, 2, 3, 0, 1); /*zz*/

  test_match (PARENS_TO_OPS ("((a)*b)*"), "abbab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b)*"), "abbab", 0, 5, 3, 5, 3, 4);

  /* We match the empty string here.  */
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b)*"), "xabbabx", 0, 0, -1, -1, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(a*)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*"), "", 0, 0, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("(a*)*"), "aa");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*)*"), "aa", 0, 2, 0, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a*)*)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)*)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)*)*"), "", 0, 0, 0, 0, 0, 0);

  test_match (PARENS_TO_OPS ("((a*)*)*"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)*)*"), "a", 0, 1, 0, 1, 0, 1);

  test_fastmap (PARENS_TO_OPS ("(ab*)*"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("(ab*)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab*)*"), "", 0, 0, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(ab*)*"), "aa");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab*)*"), "aa", 0, 2, 1, 2, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(ab*)*c"), "ac", 0, 0);
  test_match (PARENS_TO_OPS ("(ab*)*c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab*)*c"), "c", 0, 1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(ab*)*c"), "abbac");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab*)*c"), "abbac", 0, 5, 3, 4, -1, -1);

  test_match (PARENS_TO_OPS ("(ab*)*c"), "abac");
  TEST_REGISTERS (PARENS_TO_OPS ("(ab*)*c"), "abac", 0, 4, 2, 3, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*b)*c"), "abc", 0, 0);
  test_match (PARENS_TO_OPS ("(a*b)*c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*c"), "c", 0, 1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b)*c"), "bbc");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*c"), "bbc", 0, 3, 1, 2, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b)*c"), "aababc");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*c"), "aababc", 0, 6, 3, 5, -1, -1);

  test_match (PARENS_TO_OPS ("(a*b)*c"), "aabaabc");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b)*c"), "aabaabc", 0, 7, 3, 6, -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a*)b*)"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)b*)"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b*)"), "", 0, 0, 0, 0, 0, 0);

  test_match (PARENS_TO_OPS ("((a*)b*)"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b*)"), "a", 0, 1, 0, 1, 0, 1);

  test_match (PARENS_TO_OPS ("((a*)b*)"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b*)"), "b", 0, 1, 0, 1, 0, 0);

  test_fastmap (PARENS_TO_OPS ("((a)*b*)"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("((a)*b*)"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b*)"), "", 0, 0, 0, 0, -1, -1);

  test_match (PARENS_TO_OPS ("((a)*b*)"), "a");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b*)"), "a", 0, 1, 0, 1, 0, 1);

  test_match (PARENS_TO_OPS ("((a)*b*)"), "b");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b*)"), "b", 0, 1, 0, 1, -1, -1);

  test_match (PARENS_TO_OPS ("((a)*b*)"), "ab");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b*)"), "ab", 0, 2, 0, 2, 0, 1);

  test_fastmap (PARENS_TO_OPS ("((a*)b*)c"), "abc", 0, 0);
  test_match (PARENS_TO_OPS ("((a*)b*)c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("((a*)b*)c"), "c", 0, 1, 0, 0, 0, 0);

  test_fastmap (PARENS_TO_OPS ("((a)*b*)c"), "abc", 0, 0);
  test_match (PARENS_TO_OPS ("((a)*b*)c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b*)c"), "c", 0, 1, 0, 0, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(a*b*)*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(a*b*)*"), "");
  TEST_REGISTERS (PARENS_TO_OPS ("(a*b*)*"), "", 0, 0, 0, 0, -1, -1);

  test_fastmap (PARENS_TO_OPS ("(((a*))((b*)))*"), "ab", 0, 0);
  test_match (PARENS_TO_OPS ("(((a*))((b*)))*"), "");
  test_all_registers (PARENS_TO_OPS ("(((a*))((b*)))*"), "", "", 0, 0,
    0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  -1, -1,  -1, -1,  -1, -1,  -1, -1);

  test_fastmap (PARENS_TO_OPS ("(c*((a*))d*((b*))e*)*"), "abcde", 0, 0);
  test_match (PARENS_TO_OPS ("(c*((a*))d*((b*))e*)*"), "");
  test_all_registers (PARENS_TO_OPS ("(c*((a*))d*((b*))e*)*"), "", "", 0, 0,
    0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  -1, -1,  -1, -1,  -1, -1,  -1, -1);

  test_fastmap (PARENS_TO_OPS ("((a)*b)*c"), "abc", 0, 0);
  test_match (PARENS_TO_OPS ("((a)*b)*c"), "c");
  TEST_REGISTERS (PARENS_TO_OPS ("((a)*b)*c"), "c", 0, 1, -1, -1, -1, -1);

  test_match (PARENS_TO_OPS ("(ab)*"), "");
  test_match (PARENS_TO_OPS ("((ab)*)"), "");
  test_match (PARENS_TO_OPS ("(((ab)*))"), "");
  test_match (PARENS_TO_OPS ("((((ab)*)))"), "");
  test_match (PARENS_TO_OPS ("(((((ab)*))))"), "");
  test_match (PARENS_TO_OPS ("((((((ab)*)))))"), "");
  test_match (PARENS_TO_OPS ("(((((((ab)*))))))"), "");
  test_match (PARENS_TO_OPS ("((((((((ab)*)))))))"), "");
  test_match (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "");


  test_fastmap (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "a", 0, 0);
  test_match (PARENS_TO_OPS ("((((((((((ab)*)))))))))"), "");
  test_match (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "");
  test_all_registers (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "", NULL,
        0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  0, 0,  -1, -1);

  test_match (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "abab");
  test_all_registers (PARENS_TO_OPS ("(((((((((ab)*))))))))"), "abab", NULL,
	  0, 4,  0, 4,  0, 4,  0, 4,  0, 4,  0, 4,  0, 4,  0, 4,  0, 4,  2, 4);


  test_should_match = false;

  invalid_pattern (REG_EPAREN, PARENS_TO_OPS ("(a"));

  test_match (PARENS_TO_OPS ("(a)"), "");
  test_match (PARENS_TO_OPS ("((a))"), "b");
  test_match (PARENS_TO_OPS ("(a)(b)"), "ac");
  test_match (PARENS_TO_OPS ("(ab)*"), "acab");
  test_match (PARENS_TO_OPS ("(a*)*b"), "c");
  test_match (PARENS_TO_OPS ("(a*b)*"), "baa");
  test_match (PARENS_TO_OPS ("(a*b)*"), "baabc");
  test_match (PARENS_TO_OPS ("(a*b*)*"), "c");
  test_match (PARENS_TO_OPS ("((a*)*(b*)*)*"), "c");
  test_match (PARENS_TO_OPS ("(a*)*"), "ab");
  test_match (PARENS_TO_OPS ("((a*)*)*"), "ab");
  test_match (PARENS_TO_OPS ("((a*)*)*"), "b");
  test_match (PARENS_TO_OPS ("(ab*)*"), "abc");
  test_match (PARENS_TO_OPS ("(ab*)*c"), "abbad");
  test_match (PARENS_TO_OPS ("(a*c)*b"), "aacaacd");
  test_match (PARENS_TO_OPS ("(a*)"), "b");
  test_match (PARENS_TO_OPS ("((a*)b*)"), "c");

                                                /* Expression anchoring.  */
  TEST_SEARCH (PARENS_TO_OPS ("(^b)"), "ab", 0, 2);
  TEST_SEARCH (PARENS_TO_OPS ("(a$)"), "ab", 0, 2);

  printf ("\nFinished POSIX grouping tests.\n");
}
