/* test.c: testing routines for regex.c.  */

#include <assert.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
char *malloc ();
char *realloc ();
#endif

/* Just to be complete, we make both the system V/ANSI and the BSD
   versions of the string functions available.  */
#if USG || STDC_HEADERS
#include <string.h>
#define index strchr
#define rindex strrchr
#define bcmp(s1, s2, len) memcmp ((s1), (s2), (len))
#define bcopy(from, to, len) memcpy ((to), (from), (len))
#define bzero(s, len) memset ((s), 0, (len))
#else
#include <strings.h>
#define strchr index
#define strrchr rindex
#ifndef NEED_MEMORY_H
#define memcmp(s1, s2, n) bcmp ((s1), (s2), (n))
#define memcpy(to, from, len) bcopy ((from), (to), (len))
#endif
extern char *strtok ();
extern char *strstr ();
#endif /* not USG or STDC_HEADERS */

/* SunOS 4.1 declares memchr in <memory.h>, not <string.h>.  I don't
   understand why.  */
#if NEED_MEMORY_H
#include <memory.h>
#endif

#include "test.h"

#define BYTEWIDTH 8

extern void print_partial_compiled_pattern ();
extern void print_compiled_pattern ();
extern void print_double_string ();

/* If nonzero, the results of every test are displayed.  */
boolean verbose = false;

/* If nonzero, don't do register testing.  */
boolean omit_register_tests = true;

/* Says whether the current test should match or fail to match.  */
boolean test_should_match;


static void
set_all_registers (start0, end0, start1, end1, 
	           start2, end2, start3, end3,
                   start4, end4, start5, end5, 
                   start6, end6, start7, end7,
                   start8, end8, start9, end9, regs)

  int start0; int end0; int start1; int end1; 
  int start2; int end2; int start3; int end3;
  int start4; int end4; int start5; int end5; 
  int start6; int end6; int start7; int end7;
  int start8; int end8; int start9; int end9; 
  struct re_registers *regs;

  {
    unsigned r;
    
    regs->start[0] = start0;   regs->end[0] = end0;      
    regs->start[1] = start1;   regs->end[1] = end1;	
    regs->start[2] = start2;   regs->end[2] = end2;	
    regs->start[3] = start3;   regs->end[3] = end3;	
    regs->start[4] = start4;   regs->end[4] = end4;	
    regs->start[5] = start5;   regs->end[5] = end5;	
    regs->start[6] = start6;   regs->end[6] = end6;	
    regs->start[7] = start7;   regs->end[7] = end7;	
    regs->start[8] = start8;   regs->end[8] = end8;	
    regs->start[9] = start9;   regs->end[9] = end9;	
    for (r = 10; r < regs->num_regs; r++)
      {
        regs->start[r] = -1;
        regs->end[r] = -1;
      }
  }



/* Return the concatenation of S1 and S2.  This would be a prime place
   to use varargs.  */

char *
concat (s1, s2)
    char *s1;
    char *s2;
{
  char *answer = xmalloc (strlen (s1) + strlen (s2) + 1);

  strcpy (answer, s1);
  strcat (answer, s2);

  return answer;
}


#define OK_TO_SEARCH  (nonconst_buf.fastmap_accurate && (str1 || str2))

/* We ignore the `can_be_null' argument.  Should just be removed.  */

void 
general_test (pattern_should_be_valid, match_whole_string,
	      pat, str1, str2, start, range, end, correct_fastmap,
              correct_regs, can_be_null)
    unsigned pattern_should_be_valid; 
    unsigned match_whole_string;
    const char *pat;
    char *str1, *str2;
    int start, range, end; 
    char *correct_fastmap; 
    struct re_registers *correct_regs;
    int can_be_null;
{
  struct re_pattern_buffer nonconst_buf;
  struct re_pattern_buffer old_buf;
  struct re_registers regs;
  const char *r;
  char fastmap[1 << BYTEWIDTH];
  unsigned *regs_correct = NULL;
  unsigned all_regs_correct = 1;
  boolean fastmap_internal_error = false;
  unsigned match = 0;
  unsigned match_1 = 0;
  unsigned match_2 = 0;
  unsigned invalid_pattern = 0;
  boolean internal_error_1 = false;
  boolean internal_error_2 = false;
  
  
  nonconst_buf.allocated = 8;
  nonconst_buf.buffer = xmalloc (nonconst_buf.allocated);
  nonconst_buf.fastmap = fastmap;
  nonconst_buf.translate = 0;

  assert (pat != NULL);
  r = re_compile_pattern (pat, strlen (pat), &nonconst_buf);
  
  /* Kludge: if we are doing POSIX testing, we really should have
     called regcomp, not re_compile_pattern.  As it happens, the only
     way in which it matters is that re_compile_pattern sets the
     newline/anchor field for matching (part of what happens when
     REG_NEWLINE is given to regcomp).  We have to undo that for POSIX
     matching.  */
  if (t == posix_basic_test || t == posix_extended_test)
    nonconst_buf.newline_anchor = 0;
    
  invalid_pattern = r != NULL;

  if (!r)
    {
      int r;

      if (!pattern_should_be_valid)
        printf ("\nShould have been an invalid pattern but wasn't:\n");
      else
        {
          fastmap_internal_error = (re_compile_fastmap (&nonconst_buf) == -2);
          
          if (correct_fastmap)
            nonconst_buf.fastmap_accurate = 
              memcmp (nonconst_buf.fastmap, correct_fastmap, 1 << BYTEWIDTH)
              == 0;
	    
	  if (OK_TO_SEARCH)
	   {
              old_buf = nonconst_buf;
              old_buf.buffer = (unsigned char *) xmalloc (nonconst_buf.used);
              memcpy (old_buf.buffer, nonconst_buf.buffer, nonconst_buf.used);
	      
              /* If only one string is null, call re_match or re_search,
                which is what the user would probably do.  */ 
              if (str1 == NULL && str2 != NULL
                  || str2 == NULL && str1 != NULL)
                {
                  char *the_str = str1 == NULL ? str2 : str1;
                  
                  match_1
                    = match_whole_string 
	              ? (r = re_match (&nonconst_buf, the_str,
                                       strlen (the_str), start, &regs))
			== strlen (the_str)
                      : (r = re_search (&nonconst_buf,
                                        the_str, strlen (the_str),
                                        start, range, &regs))
			>= 0;

                  if (r == -2)
	           internal_error_1 = true;
                 }
	      else  
                match_1 = 1;
                
              /* Also call with re_match_2 or re_search_2, as they might
                 do this.  (Also can check calling with either string1
                 or string2 or both null.)  */
              if (match_whole_string)
                {
                  r = re_match_2 (&nonconst_buf,
                                  str1, SAFE_STRLEN (str1), 
	                          str2, SAFE_STRLEN (str2),
                                  start, &regs, end);
                  match_2 = r == SAFE_STRLEN (str1) + SAFE_STRLEN (str2);
                }
              else
                {
                  r = re_search_2 (&nonconst_buf,
                                   str1, SAFE_STRLEN (str1), 
				   str2, SAFE_STRLEN (str2),
                                   start, range, &regs, end);
                  match_2 = r >= 0;
                }

              if (r == -2)
	       internal_error_2 = true;
                
              match = match_1 & match_2;
              
              if (correct_regs)
                {
		  unsigned reg;
		  if (regs_correct != NULL)
                    free (regs_correct);
                    
                  regs_correct 
                    = (unsigned *) xmalloc (regs.num_regs * sizeof (unsigned));

                  for (reg = 0;
                       reg < regs.num_regs && reg < correct_regs->num_regs;
                       reg++)
                    {
                      regs_correct[reg]
                        = (regs.start[reg] == correct_regs->start[reg]
		           && regs.end[reg] == correct_regs->end[reg])
#ifdef EMPTY_REGS_CONFUSED
                          /* There is confusion in the standard about
                             the registers in some patterns which can
                             match either the empty string or not match.
                             For example, in `((a*))*' against the empty
                             string, the two registers can either match
                             the empty string (be 0/0), or not match
                             (because of the outer *) (be -1/-1).  (Or
                             one can do one and one can do the other.)  */
                          || (regs.start[reg] == -1 && regs.end[reg] == -1
                              && correct_regs->start[reg]
                                 == correct_regs->end[reg])
#endif 
                          ;
                                                  
                      all_regs_correct &= regs_correct[reg];
                    }
    	        }
           } /* OK_TO_SEARCH  */
        }
    }

  if (fastmap_internal_error)
    printf ("\n\nInternal error in re_compile_fastmap:");
    
  if (internal_error_1)
    {
      if (!fastmap_internal_error)
        printf ("\n");
        
      printf ("\nInternal error in re_match or re_search:");
    }
  
  if (internal_error_2)
    {
      if (!internal_error_1)
        printf ("\n");

      printf ("\nInternal error in re_match_2 or re_search_2:");
    }

  if ((OK_TO_SEARCH && ((match && !test_should_match) 
		       || (!match && test_should_match))
                       || (correct_regs && !all_regs_correct))
      || !nonconst_buf.fastmap_accurate
      || invalid_pattern 
      || !pattern_should_be_valid
      || internal_error_1 || internal_error_2
      || verbose)
    {
      if (OK_TO_SEARCH && match && !test_should_match)
        {
          printf ("\n\nMatched but shouldn't have:\n");
	  if (match_1)
	    printf ("The single match/search succeeded.\n");

	  if (match_2)
	    printf ("The double match/search succeeded.\n");
        }
      else if (OK_TO_SEARCH && !match && test_should_match)
        {
          printf ("\n\nDidn't match but should have:\n");
	  if (!match_1)
	    printf ("The single match/search failed.\n");

	  if (!match_2)
	    printf ("The double match/search failed.\n");
        }
      else if (invalid_pattern && pattern_should_be_valid)
        printf ("\n\nInvalid pattern (%s):\n", r);
      else if (!nonconst_buf.fastmap_accurate && pattern_should_be_valid)
	printf ("\n\nIncorrect fastmap:\n");
      else if (OK_TO_SEARCH && correct_regs && !all_regs_correct)
        printf ("\n\nNot all registers were correct:\n");
      else if (verbose)
        printf ("\n\nTest was OK:\n");


      if ((!(invalid_pattern && !pattern_should_be_valid)) || verbose)
        printf ("  Pattern:  `%s'.\n", pat);
             
      if (pattern_should_be_valid || verbose 
          || internal_error_1 || internal_error_2)
        {
          printf("  Strings: ");
          printf ("`%s' and ", str1 == NULL ? "NULL" : str1);
          printf ("`%s'.\n", str2  == NULL ? "NULL" : str2);

          if ((OK_TO_SEARCH || verbose || internal_error_1 || internal_error_2)
              && !invalid_pattern)
	    {
              if (memcmp (old_buf.buffer, nonconst_buf.buffer, 
	                  nonconst_buf.used) != 0
                  && !invalid_pattern)
                {
                  printf("  (%s)\n", r ? r : "Valid regular expression");
	          printf ("\n  Compiled pattern before matching: ");
	          print_compiled_pattern (&old_buf);
                  printf ("\n  Compiled pattern after matching:  ");
		}
	      else
		printf ("\n  Compiled pattern:   ");
                
              print_compiled_pattern (&nonconst_buf);
            }

          if (correct_fastmap && (!nonconst_buf.fastmap_accurate || verbose))
	    {
	      printf ("\n  The fastmap should have been: "); 
              print_fastmap (correct_fastmap);

              printf ("\n  Fastmap: "); 
	      print_fastmap (fastmap);

              printf ("\n  Compiled pattern before matching: ");
              print_compiled_pattern (&nonconst_buf);
            }

          if ((!all_regs_correct || verbose) && correct_regs)
            {
              unsigned this_reg;
	      printf ("\n  Incorrect registers:");
              
	      for (this_reg = 0; this_reg < regs.num_regs; this_reg++)
                {
                  if (!regs_correct[this_reg])
		    {
              	      printf ("\n    Register %d's start was %2d.  ", this_reg,
				                regs.start[this_reg]);
		      printf ("\tIt should have been %d.\n", 
						correct_regs->start[this_reg]);
              	      printf ("    Register %d's end was   %2d.  ", this_reg,
				                regs.end[this_reg]);
		      printf ("\tIt should have been %d.\n", 
						correct_regs->end[this_reg]);
                    }
                }
            }
        }
    }
    
  if (nonconst_buf.buffer != NULL)
    free (nonconst_buf.buffer);

  if (OK_TO_SEARCH)
    {
      free (old_buf.buffer);

      if (correct_regs)
        free (regs_correct);

    }
    
  nonconst_buf.buffer = old_buf.buffer = NULL;
  regs_correct = NULL;
  regs.start = regs.end = NULL;

} /* general_test */


void
test_search_return (match_start_wanted, pattern, string)
    int match_start_wanted;
    const char *pattern;
    char *string;
{
  struct re_pattern_buffer buf;
  char fastmap[1 << BYTEWIDTH];
  const char *compile_return;
  int match_start;
  static num_times_called = 0;

  num_times_called++;
  buf.allocated = 1;
  buf.buffer = xmalloc (buf.allocated);
    
  assert (pattern != NULL);
  buf.translate = 0;
  compile_return = re_compile_pattern (pattern, strlen (pattern), &buf);
  
  if (compile_return)
    {
      printf ("\n\nInvalid pattern in test_match_start:\n");
      printf ("%s\n", compile_return);
    }
  else 
    {
      buf.fastmap = fastmap;
      match_start = re_search (&buf, string, strlen (string),
                               0, strlen (string), 0);

      if (match_start != match_start_wanted)
	printf ("\nWanted search to start at %d but started at %d.\n",
	         match_start, match_start_wanted);
    }
  free (buf.buffer);
  buf.buffer = NULL;
}


#define SET_FASTMAP()							\
  {									\
    unsigned this_char;							\
 									\
    memset (correct_fastmap, invert, (1 << BYTEWIDTH));			\
 									\
    for (this_char = 0; this_char < strlen (fastmap_string); this_char++)\
      correct_fastmap[fastmap_string[this_char]] = !invert;		\
    correct_fastmap['\n'] = match_newline;				\
  }					
  

void
test_fastmap (pat, fastmap_string, invert, match_newline)
    const char *pat;
    char *fastmap_string;
    unsigned invert;
    unsigned match_newline;
{
  char correct_fastmap[(1 << BYTEWIDTH)];				

  SET_FASTMAP ();
  general_test (1, 0, pat, NULL, NULL, -1, 0, -1, correct_fastmap, 0, -1);
}


void
test_fastmap_search (pat, str, fastmap_string, invert, match_newline, 
                     can_be_null, start0, end0)
    const char *pat; 
    char *str; 
    char *fastmap_string;
    unsigned invert; 
    unsigned match_newline; 
    int can_be_null; 
    int start0; 
    int end0;
{
  char correct_fastmap[(1 << BYTEWIDTH)];				
  struct re_registers correct_regs;

  correct_regs.num_regs = RE_NREGS;
  correct_regs.start = (int *) xmalloc (RE_NREGS * sizeof (int));
  correct_regs.end = (int *) xmalloc (RE_NREGS * sizeof (int));
  
  set_all_registers (start0, end0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
                     -1, -1, -1, -1, -1, -1, -1, -1, &correct_regs);
  SET_FASTMAP ();
  general_test (1, 0, pat, str, NULL, 0, SAFE_STRLEN (str), SAFE_STRLEN (str),
		correct_fastmap, &correct_regs, can_be_null);

  free (correct_regs.start);
  free (correct_regs.end);
}




void
test_all_registers (pat, str1, str2, 
		    start0, end0, start1, end1, 
                    start2, end2, start3, end3, 
                    start4, end4, start5, end5, 
                    start6, end6, start7, end7, 
                    start8, end8, start9, end9)
  char *pat; char *str1; char *str2; 
  int start0; int end0; int start1; int end1; 
  int start2; int end2; int start3; int end3; 
  int start4; int end4; int start5; int end5; 
  int start6; int end6; int start7; int end7; 
  int start8; int end8; int start9; int end9;
{
  struct re_registers correct_regs;
  
  if (omit_register_tests) return;
  
  correct_regs.num_regs = RE_NREGS;
  correct_regs.start = (int *) xmalloc (RE_NREGS * sizeof (int));
  correct_regs.end = (int *) xmalloc (RE_NREGS * sizeof (int));

  set_all_registers (start0, end0, start1, end1, start2, end2, start3, end3, 
                     start4, end4, start5, end5, start6, end6, start7, end7, 
                     start8, end8, start9, end9, &correct_regs);

  general_test (1, 0, pat, str1, str2, 0, 
	        SAFE_STRLEN (str1) + SAFE_STRLEN (str2), 
		SAFE_STRLEN (str1) + SAFE_STRLEN (str2), 
                NULL, &correct_regs, -1);

  free (correct_regs.start);
  free (correct_regs.end);
}


void
invalid_pattern (error_code_expected, pattern)
  int error_code_expected;
  char *pattern;
{
  regex_t pattern_buffer;
  int cflags
    = re_syntax_options == RE_SYNTAX_POSIX_EXTENDED
      || re_syntax_options == RE_SYNTAX_POSIX_MINIMAL_EXTENDED
      ? REG_EXTENDED : 0; 
  
  test_compile (0, error_code_expected, pattern, &pattern_buffer, cflags);
}


void
valid_pattern (pattern)
  char *pattern;
{
  regex_t pattern_buffer;
  int cflags
    = re_syntax_options == RE_SYNTAX_POSIX_EXTENDED
      || re_syntax_options == RE_SYNTAX_POSIX_MINIMAL_EXTENDED
      ? REG_EXTENDED : 0; 
  
  test_compile (1, 0, pattern, &pattern_buffer, cflags);
}


char *
delimiters_to_ops (source, left_delimiter, right_delimiter)
  char *source;
  char left_delimiter;
  char right_delimiter;
{
  static char *answer = NULL;
  char *tmp = NULL;
  boolean double_size = false;
  unsigned source_char;
  unsigned answer_char = 0;
  
  assert (source != NULL);

  switch (left_delimiter)
    {
      case '(': if (!(re_syntax_options & RE_NO_BK_PARENS))
	          double_size = true;
       	        break;
      case '{': if (!(re_syntax_options & RE_NO_BK_BRACES))
	          double_size = true;
       	        break;
      default: printf ("Found strange delimiter %c in delimiter_to_ops.\n",
		        left_delimiter);
	       printf ("The source was `%s'\n", source);
	       exit (0);
    }

  if (answer == source)
    {
      tmp = (char *) xmalloc (strlen (source) + 1);
      strcpy (tmp, source);
      source = tmp;
    }

  if (answer)
    {
      free (answer);
      answer = NULL;
    }

  answer = (char *) xmalloc ((double_size 
		             ? strlen (source) << 1
                             : strlen (source))
                            + 1);
  if (!double_size)
    strcpy (answer, source);
  else
    {
      for (source_char = 0; source_char < strlen (source); source_char++)
        {
          if (source[source_char] == left_delimiter 
	      || source[source_char] == right_delimiter)
            answer[answer_char++] = '\\';

          answer[answer_char++] = source[source_char];
        }
      answer[answer_char] = 0;
    }
    
  return answer;
}


void
print_pattern_info (pattern, pattern_buffer_ptr)
  const char *pattern; 
  regex_t *pattern_buffer_ptr;
{
  printf ("  Pattern:  `%s'.\n", pattern);
  printf ("  Compiled pattern:  ");
  print_compiled_pattern (pattern_buffer_ptr);
}


void
valid_nonposix_pattern (pattern)
  char *pattern;
{
  struct re_pattern_buffer nonconst_buf;
  
  nonconst_buf.allocated = 0;
  nonconst_buf.buffer = NULL;
  nonconst_buf.translate = NULL;
  
  assert (pattern != NULL);
  
  if (re_compile_pattern (pattern, strlen (pattern), &nonconst_buf))
    {
      printf ("Couldn't compile the pattern.\n");
      print_pattern_info (pattern, &nonconst_buf);
    }
}


void
compile_and_print_pattern (pattern)
  char *pattern;
{
  struct re_pattern_buffer nonconst_buf;
  
  nonconst_buf.allocated = 0;
  nonconst_buf.buffer = NULL;
  
  if (re_compile_pattern (pattern, strlen (pattern), &nonconst_buf))
    printf ("Couldn't compile the pattern.\n");
    
  print_pattern_info (pattern, &nonconst_buf);
}


void
test_case_fold (pattern, string)
  const char *pattern;
  char* string;
{
  struct re_pattern_buffer nonconst_buf;
  const char *ret;

  init_pattern_buffer (&nonconst_buf);
  nonconst_buf.translate = upcase;

  assert (pattern != NULL);
  ret = re_compile_pattern (pattern, strlen (pattern), &nonconst_buf);
  
  if (ret)
    {
      printf ("\nShould have been a valid pattern but wasn't.\n");
      print_pattern_info (pattern, &nonconst_buf);
    }
  else
    {
      if (test_should_match 
          && re_match (&nonconst_buf, string, strlen (string), 0, 0) 
             != strlen (string))
       {
         printf ("Match failed for case fold.\n");
         printf ("  Pattern:  `%s'.\n", pattern);
         printf ("  String: `%s'.\n", string == NULL ? "NULL" : string);
       }
    }
}


void
test_match_n_times (n, pattern, string)
  unsigned n;
  char* pattern;
  char* string;
{
  struct re_pattern_buffer buf;
  const char *r;
  unsigned match = 0;
  unsigned this_match;

  buf.allocated = 0;
  buf.buffer = NULL;
  buf.translate = 0;

  assert (pattern != NULL);
  
  r = re_compile_pattern (pattern, strlen (pattern), &buf);
  if (r)
    {
      printf ("Didn't compile.\n");
      printf ("  Pattern: %s.\n", pattern);
    }
  else
    {
      for (this_match = 1; this_match <= n; this_match++)
        match = (re_match (&buf, string, strlen (string),
                           0, 0) 
	         == strlen (string));

      if (match && !test_should_match)
          printf ("\n\nMatched but shouldn't have:\n");
      else if (!match && test_should_match)
          printf ("\n\nDidn't match but should have:\n");
      
      if ((match && !test_should_match) || (!match && test_should_match))
        {
	  printf("  The string to match was:  ");
          if (string)
            printf ("`%s' and ", string);
          else
            printf ("`'");
    
          printf ("  Pattern: %s.\n", pattern);
          printf ("  Compiled pattern: %s.\n", pattern);
	  print_compiled_pattern (&buf);
        }
    }
}


void 
test_match_2 (pat, str1, str2)
  const char *pat; 
  char *str1; 
  char *str2;
{
  general_test (1, 1, pat, str1, str2, 0, 1,
		  SAFE_STRLEN (str1) + SAFE_STRLEN (str2), NULL, 0, -1);
}

void 
test_match (pat, str)
  const char *pat;
  char *str;
{
  test_match_2 (pat, str, NULL);
  test_match_2 (pat, NULL, str);
}
