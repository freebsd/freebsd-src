/* flexdef - definitions file for flex */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson.
 * 
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* @(#) $Header: flexdef.h,v 1.2 94/01/04 14:33:14 vern Exp $ (LBL) */

#include <stdio.h>
#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if __STDC__
#include <stdlib.h>
#endif

/* Always be prepared to generate an 8-bit scanner. */
#define CSIZE 256
#define Char unsigned char

/* Size of input alphabet - should be size of ASCII set. */
#ifndef DEFAULT_CSIZE
#define DEFAULT_CSIZE 128
#endif

#ifndef PROTO
#ifdef __STDC__
#define PROTO(proto) proto
#else
#define PROTO(proto) ()
#endif
#endif

#ifdef VMS
#define unlink delete
#define SHORT_FILE_NAMES
#endif

#ifdef MS_DOS
#define SHORT_FILE_NAMES
#endif


/* Maximum line length we'll have to deal with. */
#define MAXLINE 2048

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif


/* ANSI C does not guarantee that isascii() is defined */
#ifndef isascii
#define isascii(c) ((c) <= 0177)
#endif


#define true 1
#define false 0


/* Special chk[] values marking the slots taking by end-of-buffer and action
 * numbers.
 */
#define EOB_POSITION -1
#define ACTION_POSITION -2

/* Number of data items per line for -f output. */
#define NUMDATAITEMS 10

/* Number of lines of data in -f output before inserting a blank line for
 * readability.
 */
#define NUMDATALINES 10

/* Transition_struct_out() definitions. */
#define TRANS_STRUCT_PRINT_LENGTH 15

/* Returns true if an nfa state has an epsilon out-transition slot
 * that can be used.  This definition is currently not used.
 */
#define FREE_EPSILON(state) \
	(transchar[state] == SYM_EPSILON && \
	 trans2[state] == NO_TRANSITION && \
	 finalst[state] != state)

/* Returns true if an nfa state has an epsilon out-transition character
 * and both slots are free
 */
#define SUPER_FREE_EPSILON(state) \
	(transchar[state] == SYM_EPSILON && \
	 trans1[state] == NO_TRANSITION) \

/* Maximum number of NFA states that can comprise a DFA state.  It's real
 * big because if there's a lot of rules, the initial state will have a
 * huge epsilon closure.
 */
#define INITIAL_MAX_DFA_SIZE 750
#define MAX_DFA_SIZE_INCREMENT 750


/* A note on the following masks.  They are used to mark accepting numbers
 * as being special.  As such, they implicitly limit the number of accepting
 * numbers (i.e., rules) because if there are too many rules the rule numbers
 * will overload the mask bits.  Fortunately, this limit is \large/ (0x2000 ==
 * 8192) so unlikely to actually cause any problems.  A check is made in
 * new_rule() to ensure that this limit is not reached.
 */

/* Mask to mark a trailing context accepting number. */
#define YY_TRAILING_MASK 0x2000

/* Mask to mark the accepting number of the "head" of a trailing context
 * rule.
 */
#define YY_TRAILING_HEAD_MASK 0x4000

/* Maximum number of rules, as outlined in the above note. */
#define MAX_RULE (YY_TRAILING_MASK - 1)


/* NIL must be 0.  If not, its special meaning when making equivalence classes
 * (it marks the representative of a given e.c.) will be unidentifiable.
 */
#define NIL 0

#define JAM -1	/* to mark a missing DFA transition */
#define NO_TRANSITION NIL
#define UNIQUE -1	/* marks a symbol as an e.c. representative */
#define INFINITY -1	/* for x{5,} constructions */

#define INITIAL_MAX_CCLS 100	/* max number of unique character classes */
#define MAX_CCLS_INCREMENT 100

/* Size of table holding members of character classes. */
#define INITIAL_MAX_CCL_TBL_SIZE 500
#define MAX_CCL_TBL_SIZE_INCREMENT 250

#define INITIAL_MAX_RULES 100	/* default maximum number of rules */
#define MAX_RULES_INCREMENT 100

#define INITIAL_MNS 2000	/* default maximum number of nfa states */
#define MNS_INCREMENT 1000	/* amount to bump above by if it's not enough */

#define INITIAL_MAX_DFAS 1000	/* default maximum number of dfa states */
#define MAX_DFAS_INCREMENT 1000

#define JAMSTATE -32766	/* marks a reference to the state that always jams */

/* Enough so that if it's subtracted from an NFA state number, the result
 * is guaranteed to be negative.
 */
#define MARKER_DIFFERENCE 32000
#define MAXIMUM_MNS 31999

/* Maximum number of nxt/chk pairs for non-templates. */
#define INITIAL_MAX_XPAIRS 2000
#define MAX_XPAIRS_INCREMENT 2000

/* Maximum number of nxt/chk pairs needed for templates. */
#define INITIAL_MAX_TEMPLATE_XPAIRS 2500
#define MAX_TEMPLATE_XPAIRS_INCREMENT 2500

#define SYM_EPSILON (CSIZE + 1)	/* to mark transitions on the symbol epsilon */

#define INITIAL_MAX_SCS 40	/* maximum number of start conditions */
#define MAX_SCS_INCREMENT 40	/* amount to bump by if it's not enough */

#define ONE_STACK_SIZE 500	/* stack of states with only one out-transition */
#define SAME_TRANS -1	/* transition is the same as "default" entry for state */

/* The following percentages are used to tune table compression:

 * The percentage the number of out-transitions a state must be of the
 * number of equivalence classes in order to be considered for table
 * compaction by using protos.
 */
#define PROTO_SIZE_PERCENTAGE 15

/* The percentage the number of homogeneous out-transitions of a state
 * must be of the number of total out-transitions of the state in order
 * that the state's transition table is first compared with a potential 
 * template of the most common out-transition instead of with the first
 * proto in the proto queue.
 */
#define CHECK_COM_PERCENTAGE 50

/* The percentage the number of differences between a state's transition
 * table and the proto it was first compared with must be of the total
 * number of out-transitions of the state in order to keep the first
 * proto as a good match and not search any further.
 */
#define FIRST_MATCH_DIFF_PERCENTAGE 10

/* The percentage the number of differences between a state's transition
 * table and the most similar proto must be of the state's total number
 * of out-transitions to use the proto as an acceptable close match.
 */
#define ACCEPTABLE_DIFF_PERCENTAGE 50

/* The percentage the number of homogeneous out-transitions of a state
 * must be of the number of total out-transitions of the state in order
 * to consider making a template from the state.
 */
#define TEMPLATE_SAME_PERCENTAGE 60

/* The percentage the number of differences between a state's transition
 * table and the most similar proto must be of the state's total number
 * of out-transitions to create a new proto from the state.
 */
#define NEW_PROTO_DIFF_PERCENTAGE 20

/* The percentage the total number of out-transitions of a state must be
 * of the number of equivalence classes in order to consider trying to
 * fit the transition table into "holes" inside the nxt/chk table.
 */
#define INTERIOR_FIT_PERCENTAGE 15

/* Size of region set aside to cache the complete transition table of
 * protos on the proto queue to enable quick comparisons.
 */
#define PROT_SAVE_SIZE 2000

#define MSP 50	/* maximum number of saved protos (protos on the proto queue) */

/* Maximum number of out-transitions a state can have that we'll rummage
 * around through the interior of the internal fast table looking for a
 * spot for it.
 */
#define MAX_XTIONS_FULL_INTERIOR_FIT 4

/* Maximum number of rules which will be reported as being associated
 * with a DFA state.
 */
#define MAX_ASSOC_RULES 100

/* Number that, if used to subscript an array, has a good chance of producing
 * an error; should be small enough to fit into a short.
 */
#define BAD_SUBSCRIPT -32767

/* Absolute value of largest number that can be stored in a short, with a
 * bit of slop thrown in for general paranoia.
 */
#define MAX_SHORT 32700


/* Declarations for global variables. */

/* Variables for symbol tables:
 * sctbl - start-condition symbol table
 * ndtbl - name-definition symbol table
 * ccltab - character class text symbol table
 */

struct hash_entry
	{
	struct hash_entry *prev, *next;
	char *name;
	char *str_val;
	int int_val;
	} ;

typedef struct hash_entry **hash_table;

#define NAME_TABLE_HASH_SIZE 101
#define START_COND_HASH_SIZE 101
#define CCL_HASH_SIZE 101

extern struct hash_entry *ndtbl[NAME_TABLE_HASH_SIZE]; 
extern struct hash_entry *sctbl[START_COND_HASH_SIZE];
extern struct hash_entry *ccltab[CCL_HASH_SIZE];


/* Variables for flags:
 * printstats - if true (-v), dump statistics
 * syntaxerror - true if a syntax error has been found
 * eofseen - true if we've seen an eof in the input file
 * ddebug - if true (-d), make a "debug" scanner
 * trace - if true (-T), trace processing
 * nowarn - if true (-w), do not generate warnings
 * spprdflt - if true (-s), suppress the default rule
 * interactive - if true (-I), generate an interactive scanner
 * caseins - if true (-i), generate a case-insensitive scanner
 * lex_compat - if true (-l), maximize compatibility with AT&T lex
 * useecs - if true (-Ce flag), use equivalence classes
 * fulltbl - if true (-Cf flag), don't compress the DFA state table
 * usemecs - if true (-Cm flag), use meta-equivalence classes
 * fullspd - if true (-F flag), use Jacobson method of table representation
 * gen_line_dirs - if true (i.e., no -L flag), generate #line directives
 * performance_report - if > 0 (i.e., -p flag), generate a report relating
 *   to scanner performance; if > 1 (-p -p), report on minor performance
 *   problems, too
 * backing_up_report - if true (i.e., -b flag), generate "lex.backup" file
 *   listing backing-up states
 * C_plus_plus - if true (i.e., -+ flag), generate a C++ scanner class;
 *   otherwise, a standard C scanner
 * long_align - if true (-Ca flag), favor long-word alignment.
 * use_read - if true (-f, -F, or -Cr) then use read() for scanner input;
 *   otherwise, use fread().
 * yytext_is_array - if true (i.e., %array directive), then declare
 *   yytext as a array instead of a character pointer.  Nice and inefficient.
 * csize - size of character set for the scanner we're generating;
 *   128 for 7-bit chars and 256 for 8-bit
 * yymore_used - if true, yymore() is used in input rules
 * reject - if true, generate back-up tables for REJECT macro
 * real_reject - if true, scanner really uses REJECT (as opposed to just
 *   having "reject" set for variable trailing context)
 * continued_action - true if this rule's action is to "fall through" to
 *   the next rule's action (i.e., the '|' action)
 * yymore_really_used - has a REALLY_xxx value indicating whether a
 *   %used or %notused was used with yymore()
 * reject_really_used - same for REJECT
 */

extern int printstats, syntaxerror, eofseen, ddebug, trace, nowarn, spprdflt;
extern int interactive, caseins, lex_compat, useecs, fulltbl, usemecs;
extern int fullspd, gen_line_dirs, performance_report, backing_up_report;
extern int C_plus_plus, long_align, use_read, yytext_is_array, csize;
extern int yymore_used, reject, real_reject, continued_action;

#define REALLY_NOT_DETERMINED 0
#define REALLY_USED 1
#define REALLY_NOT_USED 2
extern int yymore_really_used, reject_really_used;


/* Variables used in the flex input routines:
 * datapos - characters on current output line
 * dataline - number of contiguous lines of data in current data
 * 	statement.  Used to generate readable -f output
 * linenum - current input line number
 * skelfile - the skeleton file
 * skel - compiled-in skeleton array
 * skel_ind - index into "skel" array, if skelfile is nil
 * yyin - input file
 * backing_up_file - file to summarize backing-up states to
 * infilename - name of input file
 * input_files - array holding names of input files
 * num_input_files - size of input_files array
 * program_name - name with which program was invoked 
 *
 * action_array - array to hold the rule actions
 * action_size - size of action_array
 * defs1_offset - index where the user's section 1 definitions start
 *	in action_array
 * prolog_offset - index where the prolog starts in action_array
 * action_offset - index where the non-prolog starts in action_array
 * action_index - index where the next action should go, with respect
 * 	to "action_array"
 */

extern int datapos, dataline, linenum;
extern FILE *skelfile, *yyin, *backing_up_file;
extern char *skel[];
extern int skel_ind;
extern char *infilename;
extern char **input_files;
extern int num_input_files;
extern char *program_name;

extern char *action_array;
extern int action_size;
extern int defs1_offset, prolog_offset, action_offset, action_index;


/* Variables for stack of states having only one out-transition:
 * onestate - state number
 * onesym - transition symbol
 * onenext - target state
 * onedef - default base entry
 * onesp - stack pointer
 */

extern int onestate[ONE_STACK_SIZE], onesym[ONE_STACK_SIZE];
extern int onenext[ONE_STACK_SIZE], onedef[ONE_STACK_SIZE], onesp;


/* Variables for nfa machine data:
 * current_mns - current maximum on number of NFA states
 * num_rules - number of the last accepting state; also is number of
 * 	rules created so far
 * num_eof_rules - number of <<EOF>> rules
 * default_rule - number of the default rule
 * current_max_rules - current maximum number of rules
 * lastnfa - last nfa state number created
 * firstst - physically the first state of a fragment
 * lastst - last physical state of fragment
 * finalst - last logical state of fragment
 * transchar - transition character
 * trans1 - transition state
 * trans2 - 2nd transition state for epsilons
 * accptnum - accepting number
 * assoc_rule - rule associated with this NFA state (or 0 if none)
 * state_type - a STATE_xxx type identifying whether the state is part
 * 	of a normal rule, the leading state in a trailing context
 * 	rule (i.e., the state which marks the transition from
 * 	recognizing the text-to-be-matched to the beginning of
 * 	the trailing context), or a subsequent state in a trailing
 * 	context rule
 * rule_type - a RULE_xxx type identifying whether this a ho-hum
 * 	normal rule or one which has variable head & trailing
 * 	context
 * rule_linenum - line number associated with rule
 * rule_useful - true if we've determined that the rule can be matched
 */

extern int current_mns, num_rules, num_eof_rules, default_rule;
extern int current_max_rules, lastnfa;
extern int *firstst, *lastst, *finalst, *transchar, *trans1, *trans2;
extern int *accptnum, *assoc_rule, *state_type;
extern int *rule_type, *rule_linenum, *rule_useful;

/* Different types of states; values are useful as masks, as well, for
 * routines like check_trailing_context().
 */
#define STATE_NORMAL 0x1
#define STATE_TRAILING_CONTEXT 0x2

/* Global holding current type of state we're making. */

extern int current_state_type;

/* Different types of rules. */
#define RULE_NORMAL 0
#define RULE_VARIABLE 1

/* True if the input rules include a rule with both variable-length head
 * and trailing context, false otherwise.
 */
extern int variable_trailing_context_rules;


/* Variables for protos:
 * numtemps - number of templates created
 * numprots - number of protos created
 * protprev - backlink to a more-recently used proto
 * protnext - forward link to a less-recently used proto
 * prottbl - base/def table entry for proto
 * protcomst - common state of proto
 * firstprot - number of the most recently used proto
 * lastprot - number of the least recently used proto
 * protsave contains the entire state array for protos
 */

extern int numtemps, numprots, protprev[MSP], protnext[MSP], prottbl[MSP];
extern int protcomst[MSP], firstprot, lastprot, protsave[PROT_SAVE_SIZE];


/* Variables for managing equivalence classes:
 * numecs - number of equivalence classes
 * nextecm - forward link of Equivalence Class members
 * ecgroup - class number or backward link of EC members
 * nummecs - number of meta-equivalence classes (used to compress
 *   templates)
 * tecfwd - forward link of meta-equivalence classes members
 * tecbck - backward link of MEC's
 */

/* Reserve enough room in the equivalence class arrays so that we
 * can use the CSIZE'th element to hold equivalence class information
 * for the NUL character.  Later we'll move this information into
 * the 0th element.
 */
extern int numecs, nextecm[CSIZE + 1], ecgroup[CSIZE + 1], nummecs;

/* Meta-equivalence classes are indexed starting at 1, so it's possible
 * that they will require positions from 1 .. CSIZE, i.e., CSIZE + 1
 * slots total (since the arrays are 0-based).  nextecm[] and ecgroup[]
 * don't require the extra position since they're indexed from 1 .. CSIZE - 1.
 */
extern int tecfwd[CSIZE + 1], tecbck[CSIZE + 1];


/* Variables for start conditions:
 * lastsc - last start condition created
 * current_max_scs - current limit on number of start conditions
 * scset - set of rules active in start condition
 * scbol - set of rules active only at the beginning of line in a s.c.
 * scxclu - true if start condition is exclusive
 * sceof - true if start condition has EOF rule
 * scname - start condition name
 * actvsc - stack of active start conditions for the current rule;
 * 	a negative entry means that the start condition is *not*
 *	active for the current rule.  Start conditions may appear
 *	multiple times on the stack; the entry for it closest
 *	to the top of the stack (i.e., actvsc[actvp]) is the
 *	one to use.  Others are present from "<sc>{" scoping
 *	constructs.
 */

extern int lastsc, current_max_scs, *scset, *scbol, *scxclu, *sceof, *actvsc;
extern char **scname;


/* Variables for dfa machine data:
 * current_max_dfa_size - current maximum number of NFA states in DFA
 * current_max_xpairs - current maximum number of non-template xtion pairs
 * current_max_template_xpairs - current maximum number of template pairs
 * current_max_dfas - current maximum number DFA states
 * lastdfa - last dfa state number created
 * nxt - state to enter upon reading character
 * chk - check value to see if "nxt" applies
 * tnxt - internal nxt table for templates
 * base - offset into "nxt" for given state
 * def - where to go if "chk" disallows "nxt" entry
 * nultrans - NUL transition for each state
 * NUL_ec - equivalence class of the NUL character
 * tblend - last "nxt/chk" table entry being used
 * firstfree - first empty entry in "nxt/chk" table
 * dss - nfa state set for each dfa
 * dfasiz - size of nfa state set for each dfa
 * dfaacc - accepting set for each dfa state (if using REJECT), or accepting
 *	number, if not
 * accsiz - size of accepting set for each dfa state
 * dhash - dfa state hash value
 * numas - number of DFA accepting states created; note that this
 *	is not necessarily the same value as num_rules, which is the analogous
 *	value for the NFA
 * numsnpairs - number of state/nextstate transition pairs
 * jambase - position in base/def where the default jam table starts
 * jamstate - state number corresponding to "jam" state
 * end_of_buffer_state - end-of-buffer dfa state number
 */

extern int current_max_dfa_size, current_max_xpairs;
extern int current_max_template_xpairs, current_max_dfas;
extern int lastdfa, *nxt, *chk, *tnxt;
extern int *base, *def, *nultrans, NUL_ec, tblend, firstfree, **dss, *dfasiz;
extern union dfaacc_union
	{
	int *dfaacc_set;
	int dfaacc_state;
	} *dfaacc;
extern int *accsiz, *dhash, numas;
extern int numsnpairs, jambase, jamstate;
extern int end_of_buffer_state;

/* Variables for ccl information:
 * lastccl - ccl index of the last created ccl
 * current_maxccls - current limit on the maximum number of unique ccl's
 * cclmap - maps a ccl index to its set pointer
 * ccllen - gives the length of a ccl
 * cclng - true for a given ccl if the ccl is negated
 * cclreuse - counts how many times a ccl is re-used
 * current_max_ccl_tbl_size - current limit on number of characters needed
 *	to represent the unique ccl's
 * ccltbl - holds the characters in each ccl - indexed by cclmap
 */

extern int lastccl, current_maxccls, *cclmap, *ccllen, *cclng, cclreuse;
extern int current_max_ccl_tbl_size;
extern Char *ccltbl;


/* Variables for miscellaneous information:
 * nmstr - last NAME scanned by the scanner
 * sectnum - section number currently being parsed
 * nummt - number of empty nxt/chk table entries
 * hshcol - number of hash collisions detected by snstods
 * dfaeql - number of times a newly created dfa was equal to an old one
 * numeps - number of epsilon NFA states created
 * eps2 - number of epsilon states which have 2 out-transitions
 * num_reallocs - number of times it was necessary to realloc() a group
 *	  of arrays
 * tmpuses - number of DFA states that chain to templates
 * totnst - total number of NFA states used to make DFA states
 * peakpairs - peak number of transition pairs we had to store internally
 * numuniq - number of unique transitions
 * numdup - number of duplicate transitions
 * hshsave - number of hash collisions saved by checking number of states
 * num_backing_up - number of DFA states requiring backing up
 * bol_needed - whether scanner needs beginning-of-line recognition
 */

extern char nmstr[MAXLINE];
extern int sectnum, nummt, hshcol, dfaeql, numeps, eps2, num_reallocs;
extern int tmpuses, totnst, peakpairs, numuniq, numdup, hshsave;
extern int num_backing_up, bol_needed;

void *allocate_array PROTO((int, int));
void *reallocate_array PROTO((void*, int, int));

void *flex_alloc PROTO((unsigned int));
void *flex_realloc PROTO((void*, unsigned int));
void flex_free PROTO((void*));

#define allocate_integer_array(size) \
	(int *) allocate_array( size, sizeof( int ) )

#define reallocate_integer_array(array,size) \
	(int *) reallocate_array( (void *) array, size, sizeof( int ) )

#define allocate_int_ptr_array(size) \
	(int **) allocate_array( size, sizeof( int * ) )

#define allocate_char_ptr_array(size) \
	(char **) allocate_array( size, sizeof( char * ) )

#define allocate_dfaacc_union(size) \
	(union dfaacc_union *) \
		allocate_array( size, sizeof( union dfaacc_union ) )

#define reallocate_int_ptr_array(array,size) \
	(int **) reallocate_array( (void *) array, size, sizeof( int * ) )

#define reallocate_char_ptr_array(array,size) \
	(char **) reallocate_array( (void *) array, size, sizeof( char * ) )

#define reallocate_dfaacc_union(array, size) \
	(union dfaacc_union *) \
	reallocate_array( (void *) array, size, sizeof( union dfaacc_union ) )

#define allocate_character_array(size) \
	(char *) allocate_array( size, sizeof( char ) )

#define reallocate_character_array(array,size) \
	(char *) reallocate_array( (void *) array, size, sizeof( char ) )

#define allocate_Character_array(size) \
	(Char *) allocate_array( size, sizeof( Char ) )

#define reallocate_Character_array(array,size) \
	(Char *) reallocate_array( (void *) array, size, sizeof( Char ) )


/* Used to communicate between scanner and parser.  The type should really
 * be YYSTYPE, but we can't easily get our hands on it.
 */
extern int yylval;


/* External functions that are cross-referenced among the flex source files. */


/* from file ccl.c */

extern void ccladd PROTO((int, int));	/* add a single character to a ccl */
extern int cclinit PROTO((void));	/* make an empty ccl */
extern void cclnegate PROTO((int));	/* negate a ccl */

/* List the members of a set of characters in CCL form. */
extern void list_character_set PROTO((FILE*, int[]));


/* from file dfa.c */

/* Increase the maximum number of dfas. */
extern void increase_max_dfas PROTO((void));

extern void ntod PROTO((void));	/* convert a ndfa to a dfa */


/* from file ecs.c */

/* Convert character classes to set of equivalence classes. */
extern void ccl2ecl PROTO((void));

/* Associate equivalence class numbers with class members. */
extern int cre8ecs PROTO((int[], int[], int));

/* Update equivalence classes based on character class transitions. */
extern void mkeccl PROTO((Char[], int, int[], int[], int, int));

/* Create equivalence class for single character. */
extern void mkechar PROTO((int, int[], int[]));


/* from file gen.c */

extern void make_tables PROTO((void));	/* generate transition tables */


/* from file main.c */

extern void flexend PROTO((int));
extern void usage PROTO((void));


/* from file misc.c */

/* Add the given text to the stored actions. */
extern void add_action PROTO(( char *new_text ));

/* True if a string is all lower case. */
extern int all_lower PROTO((register char *));

/* True if a string is all upper case. */
extern int all_upper PROTO((register char *));

/* Bubble sort an integer array. */
extern void bubble PROTO((int [], int));

/* Check a character to make sure it's in the expected range. */
extern void check_char PROTO((int c));

/* Shell sort a character array. */
extern void cshell PROTO((Char [], int, int));

/* Finish up a block of data declarations. */
extern void dataend PROTO((void));

/* Report an error message and terminate. */
extern void flexerror PROTO((char[]));

/* Report a fatal error message and terminate. */
extern void flexfatal PROTO((char[]));

/* Report an error message formatted with one integer argument. */
extern void lerrif PROTO((char[], int));

/* Report an error message formatted with one string argument. */
extern void lerrsf PROTO((char[], char[]));

/* Spit out a "# line" statement. */
extern void line_directive_out PROTO((FILE*));

/* Mark the current position in the action array as the end of the section 1
 * user defs.
 */
extern void mark_defs1 PROTO((void));

/* Mark the current position in the action array as the end of the prolog. */
extern void mark_prolog PROTO((void));

/* Generate a data statment for a two-dimensional array. */
extern void mk2data PROTO((int));

extern void mkdata PROTO((int));	/* generate a data statement */

/* Return the integer represented by a string of digits. */
extern int myctoi PROTO((char []));

/* Return a printable version of the given character, which might be
 * 8-bit.
 */
extern char *readable_form PROTO((int));

/* Write out one section of the skeleton file. */
extern void skelout PROTO((void));

/* Output a yy_trans_info structure. */
extern void transition_struct_out PROTO((int, int));

/* Only needed when using certain broken versions of bison to build parse.c. */
extern void *yy_flex_xmalloc PROTO(( int ));

/* Set a region of memory to 0. */
extern void zero_out PROTO((char *, int));


/* from file nfa.c */

/* Add an accepting state to a machine. */
extern void add_accept PROTO((int, int));

/* Make a given number of copies of a singleton machine. */
extern int copysingl PROTO((int, int));

/* Debugging routine to write out an nfa. */
extern void dumpnfa PROTO((int));

/* Finish up the processing for a rule. */
extern void finish_rule PROTO((int, int, int, int));

/* Connect two machines together. */
extern int link_machines PROTO((int, int));

/* Mark each "beginning" state in a machine as being a "normal" (i.e.,
 * not trailing context associated) state.
 */
extern void mark_beginning_as_normal PROTO((register int));

/* Make a machine that branches to two machines. */
extern int mkbranch PROTO((int, int));

extern int mkclos PROTO((int));	/* convert a machine into a closure */
extern int mkopt PROTO((int));	/* make a machine optional */

/* Make a machine that matches either one of two machines. */
extern int mkor PROTO((int, int));

/* Convert a machine into a positive closure. */
extern int mkposcl PROTO((int));

extern int mkrep PROTO((int, int, int));	/* make a replicated machine */

/* Create a state with a transition on a given symbol. */
extern int mkstate PROTO((int));

extern void new_rule PROTO((void));	/* initialize for a new rule */


/* from file parse.y */

/* Write out a message formatted with one string, pinpointing its location. */
extern void format_pinpoint_message PROTO((char[], char[]));

/* Write out a message, pinpointing its location. */
extern void pinpoint_message PROTO((char[]));

/* Write out a warning, pinpointing it at the given line. */
void line_warning PROTO(( char[], int ));

/* Write out a message, pinpointing it at the given line. */
void line_pinpoint PROTO(( char[], int ));

/* Report a formatted syntax error. */
extern void format_synerr PROTO((char [], char[]));
extern void synerr PROTO((char []));	/* report a syntax error */
extern void warn PROTO((char []));	/* report a warning */
extern int yyparse PROTO((void));	/* the YACC parser */


/* from file scan.l */

/* The Flex-generated scanner for flex. */
extern int flexscan PROTO((void));

/* Open the given file (if NULL, stdin) for scanning. */
extern void set_input_file PROTO((char*));

/* Wrapup a file in the lexical analyzer. */
extern int yywrap PROTO((void));


/* from file sym.c */

/* Save the text of a character class. */
extern void cclinstal PROTO ((Char [], int));

/* Lookup the number associated with character class. */
extern int ccllookup PROTO((Char []));

extern void ndinstal PROTO((char[], Char[]));	/* install a name definition */
/* Increase maximum number of SC's. */
extern void scextend PROTO((void));
extern void scinstal PROTO((char[], int));	/* make a start condition */

/* Lookup the number associated with a start condition. */
extern int sclookup PROTO((char[]));


/* from file tblcmp.c */

/* Build table entries for dfa state. */
extern void bldtbl PROTO((int[], int, int, int, int));

extern void cmptmps PROTO((void));	/* compress template table entries */
extern void expand_nxt_chk PROTO((void));	/* increase nxt/chk arrays */
extern void inittbl PROTO((void));	/* initialize transition tables */
/* Make the default, "jam" table entries. */
extern void mkdeftbl PROTO((void));

/* Create table entries for a state (or state fragment) which has
 * only one out-transition.
 */
extern void mk1tbl PROTO((int, int, int, int));

/* Place a state into full speed transition table. */
extern void place_state PROTO((int*, int, int));

/* Save states with only one out-transition to be processed later. */
extern void stack1 PROTO((int, int, int, int));


/* from file yylex.c */

extern int yylex PROTO((void));
