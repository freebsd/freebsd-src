/* tregress.c: reported bugs.  The `t' just makes the filename not have
   a common prefix with `regex.c', so completion works better.  */

#include "test.h"


boolean pause_at_error = true;

char *
itoa (i)
    int i;
{
  char *a = xmalloc (21); /* sign + 19 digits (enough for 64 bits) + null */

  sprintf (a, "%d", i);
  return a;
}


static void
simple_fail (routine, pat, buf, str, ret)
    const char *routine;
    const char *pat;
    struct re_pattern_buffer *buf;
    const char *str;
    char *ret;
{
  fprintf (stderr, "Failed %s (return = %s).\n", routine, ret);
  if (str && *str) fprintf (stderr, "   String = %s\n", str);
  fprintf (stderr, "  Pattern = %s\n", pat);
  print_compiled_pattern (buf);

  if (pause_at_error)
    {
      fprintf (stderr, "RET to continue: ");
      (void) getchar ();
    }
}


/* Abbreviate the most common calls.  */

static void
simple_compile (pat, buf)
    const char *pat;
    struct re_pattern_buffer *buf;
{
  const char *ret = re_compile_pattern (pat, strlen (pat), buf);

  if (ret != NULL) simple_fail ("compile", pat, buf, NULL, ret);
}


static void
simple_fastmap (pat)
    const char *pat;
{
  struct re_pattern_buffer buf;
  char fastmap[256];
  int ret;

  buf.allocated = 0;
  buf.buffer = buf.translate = NULL;
  buf.fastmap = fastmap;

  simple_compile (pat, &buf);

  ret = re_compile_fastmap (&buf);

  if (ret != 0) simple_fail ("fastmap compile", pat, &buf, NULL, itoa (ret));
}


#define SIMPLE_MATCH(pat, str) do_match (pat, str, strlen (str))
#define SIMPLE_NONMATCH(pat, str) do_match (pat, str, -1)

static void
do_match (pat, str, expected)
    const char *pat, *str;
    int expected;
{
  int ret;
  unsigned len;
  struct re_pattern_buffer buf;

  buf.allocated = 0;
  buf.buffer = buf.translate = buf.fastmap = NULL;

  simple_compile (pat, &buf);

  len = strlen (str);

  ret = re_match_2 (&buf, NULL, 0, str, len, 0, NULL, len);

  if (ret != expected) simple_fail ("match", pat, &buf, str, itoa (ret));
}


static void
simple_search (pat, str, correct_startpos)
    const char *pat, *str;
    int correct_startpos;
{
  int ret;
  unsigned len;
  struct re_pattern_buffer buf;

  buf.allocated = 0;
  buf.buffer = buf.translate = buf.fastmap = NULL;

  simple_compile (pat, &buf);

  len = strlen (str);

  ret = re_search_2 (&buf, NULL, 0, str, len, 0, len, NULL, len);

  if (ret != correct_startpos)
    simple_fail ("match", pat, &buf, str, itoa (ret));
}

/* Past bugs people have reported.  */

void
test_regress ()
{
  extern char upcase[];
  struct re_pattern_buffer buf;
  unsigned len;
  struct re_registers regs;
  int ret;
  char *fastmap = xmalloc (256);

  buf.translate = NULL;
  buf.fastmap = NULL;
  buf.allocated = 0;
  buf.buffer = NULL;

  printf ("\nStarting regression tests.\n");
  t = regress_test;

  test_should_match = true;
  re_set_syntax (RE_SYNTAX_EMACS);

  /* enami@sys.ptg.sony.co.jp  10 Nov 92 15:19:02 JST  */
  buf.translate = upcase;
  SIMPLE_MATCH ("[A-[]", "A");
  buf.translate = NULL;

  /* meyering@cs.utexas.edu  Nov  6 22:34:41 1992  */
  simple_search ("\\w+", "a", 0);

  /* jimb@occs.cs.oberlin.edu  10 Sep 92 00:42:33  */
  buf.translate = upcase;
  SIMPLE_MATCH ("[\001-\377]", "\001");
  SIMPLE_MATCH ("[\001-\377]", "a");
  SIMPLE_MATCH ("[\001-\377]", "\377");
  buf.translate = NULL;

  /* mike@skinner.cs.uoregon.edu  1 Sep 92 01:45:22  */
  SIMPLE_MATCH ("^^$", "^");

  /* pclink@qld.tne.oz.au  Sep  7 22:42:36 1992  */
  re_set_syntax (RE_INTERVALS);
  SIMPLE_MATCH ("^a\\{3\\}$", "aaa");
  SIMPLE_NONMATCH ("^a\\{3\\}$", "aa");
  re_set_syntax (RE_SYNTAX_EMACS);

  /* pclink@qld.tne.oz.au, 31 Aug 92.  (conjecture) */
  re_set_syntax (RE_INTERVALS);
  simple_search ("a\\{1,3\\}b", "aaab", 0);
  simple_search ("a\\{1,3\\}b", "aaaab", 1);
  re_set_syntax (RE_SYNTAX_EMACS);

  /* trq@dionysos.thphys.ox.ac.uk, 31 Aug 92.  (simplified) */
  simple_fastmap ("^.*\n[  ]*");

  /* wind!greg@plains.NoDak.edu, 25 Aug 92.  (simplified) */
  re_set_syntax (RE_INTERVALS);
  SIMPLE_MATCH ("[a-zA-Z]*.\\{5\\}", "xN0000");
  SIMPLE_MATCH ("[a-zA-Z]*.\\{5\\}$", "systemxN0000");
  SIMPLE_MATCH ("\\([a-zA-Z]*\\).\\{5\\}$", "systemxN0000");
  re_set_syntax (RE_SYNTAX_EMACS);

  /* jimb, 18 Aug 92.  Don't use \000, so `strlen' (in our testing
     routines) will work.  (This still tickles the bug jimb reported.)  */
  SIMPLE_MATCH ("[\001-\377]", "\001");
  SIMPLE_MATCH ("[\001-\377]", "a");
  SIMPLE_MATCH ("[\001-\377]", "\377");

  /* jimb, 13 Aug 92.  */
  SIMPLE_MATCH ("[\001-\177]", "\177");

  /* Tests based on bwoelfel's below.  */
  SIMPLE_MATCH ("\\(a\\|ab\\)*", "aab");
  SIMPLE_MATCH ("\\(a\\|ab\\)+", "aab");
  SIMPLE_MATCH ("\\(a*\\|ab\\)+", "aab");
  SIMPLE_MATCH ("\\(a+\\|ab\\)+", "aab");
  SIMPLE_MATCH ("\\(a?\\|ab\\)+", "aab");

  /* bwoelfel@widget.seas.upenn.edu, 25 Jul 92.  */
  SIMPLE_MATCH ("^\\([ab]+\\|bc\\)+", "abc");

  /* jla, 3 Jul 92.  Core dump in re_search_2.  */
  buf.fastmap = fastmap;
  buf.translate = upcase;
#define DATEDUMP_PATTERN " *[0-9]*:"
  if (re_compile_pattern (DATEDUMP_PATTERN, strlen (DATEDUMP_PATTERN), &buf)
      != NULL)
    printf ("date dump compile failed.\n");
  regs.num_regs = 0;
  regs.start = regs.end = NULL;
  if (re_search_2 (&buf, NULL, 0, "Thu Jul  2 18:34:18 1992",
                   24, 3, 21, &regs, 24) != 10)
    printf ("date dump search failed.\n");
  buf.fastmap = 0;
  buf.translate = 0;


  /* rms, 4 Jul 1992.  Pattern is much slower in Emacs 19.  Fastmap
     should be only a backslash.  */
#define BEGINEND_PATTERN "\\(\\\\begin\\s *{\\)\\|\\(\\\\end\\s *{\\)"
  test_fastmap (BEGINEND_PATTERN, "\\", false, 0);


  /* kaoru@is.s.u-tokyo.ac.jp, 27 Jun 1992.  Code for [a-z] (in regex.c)
     should translate the whole set.  */
  buf.translate = upcase;
#define CASE_SET_PATTERN "[ -`]"
  if (re_compile_pattern (CASE_SET_PATTERN, strlen (CASE_SET_PATTERN), &buf)
      != NULL)
    printf ("case set compile failed.\n");
  if (re_match_2 (&buf, "K", 1, "", 0, 0, NULL, 1) != 1)
    printf ("case set match failed.\n");

#define CASE_SET_PATTERN2 "[`-|]"
  if (re_compile_pattern (CASE_SET_PATTERN2, strlen (CASE_SET_PATTERN2), &buf)
      != NULL)
    printf ("case set2 compile failed.\n");
  if (re_match_2 (&buf, "K", 1, "", 0, 0, NULL, 1) != 1)
    printf ("case set2 match failed.\n");

  buf.translate = NULL;


  /* jimb, 27 Jun 92.  Problems with gaps in the string.  */
#define GAP_PATTERN "x.*y.*z"
  if (re_compile_pattern (GAP_PATTERN, strlen (GAP_PATTERN), &buf) != NULL)
    printf ("gap didn't compile.\n");
  if (re_match_2 (&buf, "x-", 2, "y-z-", 4, 0, NULL, 6) != 5)
    printf ("gap match failed.\n");


  /* jimb, 19 Jun 92.  Since `beginning of word' matches at the
     beginning of the string, then searching ought to find it there.
     If `re_compile_fastmap' is not called, then it works ok.  */
  buf.fastmap = fastmap;
#define BOW_BEG_PATTERN "\\<"
  if (re_compile_pattern (BOW_BEG_PATTERN, strlen (BOW_BEG_PATTERN), &buf)
      != NULL)
    printf ("begword-begstring didn't compile.\n");
  if (re_search (&buf, "foo", 3, 0, 3, NULL) != 0)
    printf ("begword-begstring search failed.\n");

  /* Same bug report, different null-matching pattern.  */
#define EMPTY_ANCHOR_PATTERN "^$"
  if (re_compile_pattern (EMPTY_ANCHOR_PATTERN, strlen (EMPTY_ANCHOR_PATTERN),
      &buf) != NULL)
    printf ("empty anchor didn't compile.\n");
  if (re_search (&buf, "foo\n\nbar", 8, 0, 8, NULL) != 4)
    printf ("empty anchor search failed.\n");

  /* jimb@occs.cs.oberlin.edu, 21 Apr 92.  After we first allocate
     registers for a particular re_pattern_buffer, we might have to
     reallocate more registers on subsequent calls -- and we should be
     reusing the same memory.  */
#define ALLOC_REG_PATTERN "\\(abc\\)"
  free (buf.fastmap);
  buf.fastmap = 0;
  if (re_compile_pattern (ALLOC_REG_PATTERN, strlen (ALLOC_REG_PATTERN), &buf)
      != NULL)
    printf ("register allocation didn't compile.\n");
  if (re_match (&buf, "abc", 3, 0, &regs) != 3)
    printf ("register allocation didn't match.\n");
  if (regs.start[1] != 0 || regs.end[1] != 3)
    printf ("register allocation reg #1 wrong.\n");

  {
    int *old_regstart = regs.start;
    int *old_regend = regs.end;

    if (re_match (&buf, "abc", 3, 0, &regs) != 3)
      printf ("register reallocation didn't match.\n");
    if (regs.start[1] != 0 || regs.end[1] != 3
        || old_regstart[1] != 0 || old_regend[1] != 3
        || regs.start != old_regstart || regs.end != old_regend)
      printf ("register reallocation registers wrong.\n");
  }

  /* jskudlarek@std.MENTORG.COM, 21 Apr 92 (string-match).  */
#define JSKUD_PATTERN "[^/]+\\(/[^/.]+\\)?/[0-9]+$"
  if (re_compile_pattern (JSKUD_PATTERN, strlen (JSKUD_PATTERN), &buf) != NULL)
    printf ("jskud test didn't compile.\n");
  if (re_search (&buf, "a/1", 3, 0, 3, &regs) != 0)
    printf ("jskud test didn't match.\n");
  if (regs.start[1] != -1 || regs.end[1] != -1)
    printf ("jskud test, reg #1 wrong.\n");

  /* jla's bug (with string-match), 5 Feb 92.  */
  TEST_SEARCH ("\\`[ \t\n]*", "jla@challenger (Joseph Arceneaux)", 0, 100);

  /* jwz@lucid.com, 8 March 1992 (re-search-forward).  (His is the
     second.)  These are not supposed to match.  */
#if 0
  /* This one fails quickly, because we can change the maybe_pop_jump
     from the + to a pop_failure_pop, because of the c's.  */
  TEST_SEARCH ("^\\(To\\|CC\\):\\([^c]*\\)+co",
"To: hbs%titanic@lucid.com (Harlan Sexton)\n\
Cc: eb@thalidomide, jlm@thalidomide\n\
Subject: Re: so is this really as horrible an idea as it seems to me?\n\
In-Reply-To: Harlan Sexton's message of Sun 8-Mar-92 11:00:06 PST <9203081900.AA24794@titanic.lucid>\n\
References: <9203080736.AA05869@thalidomide.lucid>\n\
	<9203081900.AA24794@titanic.lucid>", 0, 5000);

  /* This one takes a long, long time to complete, because we have to
     keep the failure points around because we might backtrack.  */
  TEST_SEARCH ("^\\(To\\|CC\\):\\(.*\n.*\\)+co",
               /* "X-Windows: The joke that kills.\n\
FCC:  /u/jwz/VM/inbox\n\
From: Jamie Zawinski <jwz@lucid.com>\n\ */
"To: hbs%titanic@lucid.com (Harlan Sexton)\n\
Cc: eb@thalidomide, jlm@thalidomide\n\
Subject: Re: so is this really as horrible an idea as it seems to me?\n\
In-Reply-To: Harlan Sexton's message of Sun 8-Mar-92 11:00:06 PST <9203081900.AA24794@titanic.lucid>\n\
References: <9203080736.AA05869@thalidomide.lucid>\n\
	<9203081900.AA24794@titanic.lucid>", 0, 5000);
#endif /* 0 [failed searches] */


  /* macrakis' bugs.  */
  buf.translate = upcase; /* message of 24 Jan 91 */
  if (re_compile_pattern ("[!-`]", 5, &buf) != NULL)
    printf ("Range test didn't compile.\n");
  if (re_match (&buf, "A", 1, 0, NULL) != 1)
    printf ("Range test #1 didn't match.\n");
  if (re_match (&buf, "a", 1, 0, NULL) != 1)
    printf ("Range test #2 didn't match.\n");

  buf.translate = 0;
#define FAO_PATTERN "\\(f\\(.\\)o\\)+"
  if (re_compile_pattern (FAO_PATTERN, strlen (FAO_PATTERN), &buf) != NULL)
    printf ("faofdx test didn't compile.\n");
  if (re_search (&buf, "faofdx", 6, 0, 6, &regs) != 0)
    printf ("faofdx test didn't match.\n");
  if (regs.start[1] != 0 || regs.end[1] != 3)
    printf ("faofdx test, reg #1 wrong.\n");
  if (regs.start[2] != 1 || regs.end[2] != 2)
    printf ("faofdx test, reg #2 wrong.\n");

  TEST_REGISTERS ("\\(a\\)*a", "aaa", 0, 3, 1, 2, -1, -1);
  test_fastmap ("^\\([^ \n]+:\n\\)+\\([^ \n]+:\\)", " \n", 1, 0);

  /* 40 lines, 48 a's in each line.  */
  test_match ("^\\([^ \n]+:\n\\)+\\([^ \n]+:\\)",
	      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n\
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:");

   /* 640 a's followed by one b, twice.  */
   test_match ("\\(.*\\)\\1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab");

   /* 640 a's followed by two b's, twice.  */
   test_match ("\\(.*\\)\\1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabb");


  /* Dave G. bug: Reference to a subexpression which didn't match.
     Should fail. */
  re_set_syntax (RE_NO_BK_PARENS | RE_NO_BK_VBAR);
  test_match ("(ooooooooooone())-annnnnnnnnnnd-(twooooooooooo\\2)",
               "ooooooooooone-annnnnnnnnnnd-twooooooooooo");
  test_match ("(o|t)", "o");
  test_match ("(o()|t)", "o");
  test_match ("(o|t)", "o");
  test_match ("(ooooooooooooooo|tttttttttttttttt())", "ooooooooooooooo");
  test_match ("(o|t())", "o");
  test_match ("(o()|t())", "o");
  test_match ("(ooooooooooooooooooooooooone()|twooooooooooooooooooooooooo())", "ooooooooooooooooooooooooone");
  test_match ("(o()|t())-a-(t\\2|f\\3)", "o-a-t");
  test_match ("(o()|t())-a-(t\\2|f\\3)", "t-a-f");

  test_should_match = 0;
  test_match ("(foo(bar)|second)\\2", "second");
  test_match ("(o()|t())-a-(t\\2|f\\3)", "t-a-t");
  test_match ("(o()|t())-a-(t\\2|f\\3)", "o-a-f");

  re_set_syntax (RE_SYNTAX_EMACS);
  test_match ("\\(foo\\(bar\\)\\|second\\)\\2", "secondbar");
  test_match ("\\(one\\(\\)\\|two\\(\\)\\)-and-\\(three\\2\\|four\\3\\)",
	      "one-and-four");
  test_match ("\\(one\\(\\)\\|two\\(\\)\\)-and-\\(three\\2\\|four\\3\\)",
	      "two-and-three");

  test_should_match = 1;
  re_set_syntax (RE_SYNTAX_EMACS);
  test_match ("\\(one\\(\\)\\|two\\(\\)\\)-and-\\(three\\2\\|four\\3\\)",
	      "one-and-three");
  test_match ("\\(one\\(\\)\\|two\\(\\)\\)-and-\\(three\\2\\|four\\3\\)",
	      "two-and-four");

  TEST_REGISTERS (":\\(.*\\)", ":/", 0, 2, 1, 2, -1, -1);

  /* Bug with `upcase' translation table, from Nico Josuttis
     <nico@bredex.de> */
  test_should_match = 1;
  test_case_fold ("[a-a]", "a");

  printf ("\nFinished regression tests.\n");
}



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
