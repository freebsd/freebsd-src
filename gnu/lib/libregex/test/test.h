/* test.h: for Regex testing.  */

#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <assert.h>

#include <sys/types.h>
#include "regex.h"


/* A strlen that works even on a null pointer.  */
#define SAFE_STRLEN(s)  (s == NULL ? 0 : strlen (s))

typedef enum { false = 0, true = 1 } boolean;

extern boolean test_should_match;
extern boolean omit_register_tests;
extern void *xmalloc ();

/* Defined in upcase.c.  */
extern char upcase[];

typedef enum
{
  all_test,
  other_test,
  posix_basic_test,
  posix_extended_test,
  posix_interface_test,
  regress_test
} test_type;

extern test_type t;


#if __STDC__

extern char *concat (char *, char *);

extern void general_test (unsigned pattern_should_be_valid,
		          unsigned match_whole_string,
			  const char *pat, char *str1, char *str2,
		          int start, int range, int end,
              		  char *correct_fastmap,
                          struct re_registers *correct_regs, int can_be_null);


extern void init_pattern_buffer (regex_t *pattern_buffer_ptr);

extern void test_compile (unsigned valid_pattern, int error_code_expected,
			  const char *pattern, regex_t *pattern_buffer_ptr,
			  int cflags);

extern char *delimiter_to_ops (char *source, char left_delimiter,
			       char right_delimiter);


extern void test_search_return (int, const char *, char *);

extern void test_berk_search (const char *pattern, char *string);

extern void test_fastmap (const char *pat, char *fastmap_string, unsigned invert,
	    		  unsigned match_newline);

extern void test_fastmap_search (const char *pat, char *str, char *fastmap_string,
				 unsigned invert, unsigned match_newline,
		                 int can_be_null, int start0, int end0);

extern void test_all_registers (char *pat, char *str1, char *str2,
			       int start0, int end0, int start1, int end1,
		               int start2, int end2, int start3, int end3,
                	       int start4, int end4, int start5, int end5,
	                       int start6, int end6, int start7, int end7,
           		       int start8, int end8, int start9, int end9);

extern void print_pattern_info (const char *pattern, regex_t *pattern_buffer_ptr);
extern void compile_and_print_pattern (char *pattern);

extern void test_case_fold (const char *pattern, char* string);

extern void test_posix_generic ();

extern void test_grouping ();

extern void invalid_pattern (int error_code_expected, char *pattern);
extern void valid_nonposix_pattern (char *pattern);
extern void valid_pattern (char *pattern);

extern void test_match_2 (const char *pat, char *str1, char *str2);
extern void test_match (const char *pat, char *str);

#endif  /* __STDC__  */


#define TEST_REGISTERS_2(pat, str1, str2, start0, end0, start1, end1, start2, end2)\
  if (!omit_register_tests)						\
    test_all_registers (pat, str1, str2, start0, end0, start1, end1,	\
	  	        start2, end2, -1, -1,  -1, -1,  -1, -1,  -1, -1,\
                        -1, -1, -1, -1,  -1, -1)			\


#define TEST_REGISTERS(pat, str, start0, end0, start1, end1, start2, end2) \
   TEST_REGISTERS_2 (pat, str, NULL, start0, end0, start1, end1, start2, end2)\

#define BRACES_TO_OPS(string)  ((char *) delimiters_to_ops (string, '{', '}'))
#define PARENS_TO_OPS(string)  ((char *) delimiters_to_ops (string, '(', ')'))

#define INVALID_PATTERN(pat)						\
  general_test (0, 0, pat, NULL, NULL, -1, 0, -1, NULL, 0, -1)


#define MATCH_SELF(p) test_match (p, p)

#define TEST_POSITIONED_MATCH(pat, str, start) 				\
  general_test (1, 0, pat, str, NULL, start, 1, SAFE_STRLEN (str),	\
		NULL, 0, -1)

#define TEST_TRUNCATED_MATCH(pat, str, end)				\
  general_test (1, 0, pat, str, NULL, 0, 1, end, NULL, 0, -1)

#define TEST_SEARCH_2(pat, str1, str2, start, range, one_past_end)	\
  general_test (1, 0, pat, str1, str2, start, range, one_past_end, 	\
  NULL, 0, -1)

#define TEST_SEARCH(pat, str, start, range)				\
  {									\
    TEST_SEARCH_2 (pat, str, NULL, start, range, SAFE_STRLEN (str));	\
    TEST_SEARCH_2 (pat, NULL, str, start, range, SAFE_STRLEN (str));	\
  }

#endif /* TEST_H  */

/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
