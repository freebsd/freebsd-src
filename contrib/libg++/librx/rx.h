#if !defined(RXH) || defined(RX_WANT_SE_DEFS)
#define RXH

/*	Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of the librx library.

Librx is free software; you can redistribute it and/or modify it under
the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Librx is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU Library General Public
License along with this software; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, 675 Mass Ave, Cambridge, MA
02139, USA.  */
/*  t. lord	Wed Sep 23 18:20:57 1992	*/








#ifndef RX_WANT_SE_DEFS

/* This page: Bitsets */

#ifndef RX_subset
typedef unsigned int RX_subset;
#define RX_subset_bits	(32)
#define RX_subset_mask	(RX_subset_bits - 1)
#endif

typedef RX_subset * rx_Bitset;

#ifdef __STDC__
typedef void (*rx_bitset_iterator) (void *, int member_index);
#else
typedef void (*rx_bitset_iterator) ();
#endif

#define rx_bitset_subset(N)  ((N) / RX_subset_bits)
#define rx_bitset_subset_val(B,N)  ((B)[rx_bitset_subset(N)])
#define RX_bitset_access(B,N,OP) \
  ((B)[rx_bitset_subset(N)] OP rx_subset_singletons[(N) & RX_subset_mask])
#define RX_bitset_member(B,N)   RX_bitset_access(B, N, &)
#define RX_bitset_enjoin(B,N)   RX_bitset_access(B, N, |=)
#define RX_bitset_remove(B,N)   RX_bitset_access(B, N, &= ~)
#define RX_bitset_toggle(B,N)   RX_bitset_access(B, N, ^= )
#define rx_bitset_numb_subsets(N) (((N) + RX_subset_bits - 1) / RX_subset_bits)
#define rx_sizeof_bitset(N)	(rx_bitset_numb_subsets(N) * sizeof(RX_subset))



/* This page: Splay trees. */

#ifdef __STDC__
typedef int (*rx_sp_comparer) (void * a, void * b);
#else
typedef int (*rx_sp_comparer) ();
#endif

struct rx_sp_node 
{
  void * key;
  void * data;
  struct rx_sp_node * kids[2];
};

#ifdef __STDC__
typedef void (*rx_sp_key_data_freer) (struct rx_sp_node *);
#else
typedef void (*rx_sp_key_data_freer) ();
#endif


/* giant inflatable hash trees */

struct rx_hash_item
{
  struct rx_hash_item * next_same_hash;
  struct rx_hash * table;
  unsigned long hash;
  void * data;
  void * binding;
};

struct rx_hash
{
  struct rx_hash * parent;
  int refs;
  struct rx_hash * children[13];
  struct rx_hash_item * buckets [13];
  int bucket_size [13];
};

struct rx_hash_rules;

#ifdef __STDC__
/* should return like == */
typedef int (*rx_hash_eq)(void *, void *);
typedef struct rx_hash * (*rx_alloc_hash)(struct rx_hash_rules *);
typedef void (*rx_free_hash)(struct rx_hash *,
			    struct rx_hash_rules *);
typedef struct rx_hash_item * (*rx_alloc_hash_item)(struct rx_hash_rules *,
						    void *);
typedef void (*rx_free_hash_item)(struct rx_hash_item *,
				 struct rx_hash_rules *);
#else
typedef int (*rx_hash_eq)();
typedef struct rx_hash * (*rx_alloc_hash)();
typedef void (*rx_free_hash)();
typedef struct rx_hash_item * (*rx_alloc_hash_item)();
typedef void (*rx_free_hash_item)();
#endif

struct rx_hash_rules
{
  rx_hash_eq eq;
  rx_alloc_hash hash_alloc;
  rx_free_hash free_hash;
  rx_alloc_hash_item hash_item_alloc;
  rx_free_hash_item free_hash_item;
};


/* Forward declarations */

struct rx_cache;
struct rx_superset;
struct rx;
struct rx_se_list;



/* 
 * GLOSSARY
 *
 * regexp
 * regular expression
 * expression
 * pattern - a `regular' expression.  The expression
 *       need not be formally regular -- it can contain
 *       constructs that don't correspond to purely regular
 *       expressions.
 *
 * buffer
 * string - the string (or strings) being searched or matched.
 *
 * pattern buffer - a structure of type `struct re_pattern_buffer'
 *       This in turn contains a `struct rx', which holds the
 *       NFA compiled from a pattern, as well as some of the state
 *       of a matcher using the pattern.
 *
 * NFA - nondeterministic finite automata.  Some people
 *       use this term to a member of the class of 
 *       regular automata (those corresponding to a regular
 *       language).  However, in this code, the meaning is
 *       more general.  The automata used by Rx are comperable
 *       in power to what are usually called `push down automata'.
 *
 *       Two NFA are built by rx for every pattern.  One is built
 *       by the compiler.  The other is built from the first, on
 *       the fly, by the matcher.  The latter is called the `superstate
 *       NFA' because its states correspond to sets of states from
 *       the first NFA.  (Joe Keane gets credit for the name
 *       `superstate NFA').
 *
 * NFA edges
 * epsilon edges
 * side-effect edges - The NFA compiled from a pattern can have three
 *       kinds of edges.  Epsilon edges can be taken freely anytime
 *       their source state is reached.  Character set edges can be
 *       taken when their source state is reached and when the next 
 *       character in the buffer is a member of the set.  Side effect
 *       edges imply a transition that can only be taken after the
 *       indicated side effect has been successfully accomplished.
 *       Some examples of side effects are:
 *
 *		Storing the current match position to record the
 *              location of a parentesized subexpression.
 *
 *              Advancing the matcher over N characters if they
 *              match the N characters previously matched by a 
 *              parentesized subexpression.
 *
 *       Both of those kinds of edges occur in the NFA generated
 *       by the pattern:  \(.\)\1
 *
 *       Epsilon and side effect edges are similar.  Unfortunately,
 *       some of the code uses the name `epsilon edge' to mean
 *       both epsilon and side effect edges.  For example,  the
 *       function has_non_idempotent_epsilon_path computes the existance
 *       of a non-trivial path containing only a mix of epsilon and
 *       side effect edges.  In that case `nonidempotent epsilon' is being
 *       used to mean `side effect'.
 */





/* LOW LEVEL PATTERN BUFFERS */

/* Suppose that from some NFA state, more than one path through
 * side-effect edges is possible.  In what order should the paths
 * be tried?  A function of type rx_se_list_order answers that
 * question.  It compares two lists of side effects, and says
 * which list comes first.
 */
 
#ifdef __STDC__
typedef int (*rx_se_list_order) (struct rx *,
				 struct rx_se_list *, 
				 struct rx_se_list *);
#else
typedef int (*rx_se_list_order) ();
#endif



/* Struct RX holds a compiled regular expression - that is, an nfa
 * ready to be converted on demand to a more efficient superstate nfa.
 * This is for the low level interface.  The high-level interfaces enclose
 * this in a `struct re_pattern_buffer'.  
 */
struct rx
{
  /* The compiler assigns a unique id to every pattern.
   * Like sequence numbers in X, there is a subtle bug here
   * if you use Rx in a system that runs for a long time.
   * But, because of the way the caches work out, it is almost
   * impossible to trigger the Rx version of this bug.
   *
   * The id is used to validate superstates found in a cache
   * of superstates.  It isn't sufficient to let a superstate
   * point back to the rx for which it was compiled -- the caller
   * may be re-using a `struct rx' in which case the superstate
   * is not really valid.  So instead, superstates are validated
   * by checking the sequence number of the pattern for which
   * they were built.
   */
  int rx_id;

  /* This is memory mgt. state for superstates.  This may be 
   * shared by more than one struct rx.
   */
  struct rx_cache * cache;

  /* Every regex defines the size of its own character set. 
   * A superstate has an array of this size, with each element
   * a `struct rx_inx'.  So, don't make this number too large.
   * In particular, don't make it 2^16.
   */
  int local_cset_size;

  /* After the NFA is built, it is copied into a contiguous region
   * of memory (mostly for compatability with GNU regex).
   * Here is that region, and it's size:
   */
  void * buffer;
  unsigned long allocated;

  /* Clients of RX can ask for some extra storage in the space pointed
   * to by BUFFER.  The field RESERVED is an input parameter to the
   * compiler.  After compilation, this much space will be available 
   * at (buffer + allocated - reserved)
   */
  unsigned long reserved;

  /* --------- The remaining fields are for internal use only. --------- */
  /* --------- But! they must be initialized to 0.	       --------- */

  /* NODEC is the number of nodes in the NFA with non-epsilon
   * transitions. 
   */
  int nodec;

  /* EPSNODEC is the number of nodes with only epsilon transitions. */
  int epsnodec;

  /* The sum (NODEC + EPSNODEC) is the total number of states in the
   * compiled NFA.
   */

  /* Lists of side effects as stored in the NFA are `hash consed'..meaning
   * that lists with the same elements are ==.  During compilation, 
   * this table facilitates hash-consing.
   */
  struct rx_hash se_list_memo;

  /* Lists of NFA states are also hashed. 
   */
  struct rx_hash set_list_memo;




  /* The compiler and matcher must build a number of instruction frames.
   * The format of these frames is fixed (c.f. struct rx_inx).  The values
   * of the instructions is not fixed.
   *
   * An enumerated type (enum rx_opcode) defines the set of instructions
   * that the compiler or matcher might generate.  When filling an instruction
   * frame, the INX field is found by indexing this instruction table
   * with an opcode:
   */
  void ** instruction_table;

  /* The list of all states in an NFA.
   * During compilation, the NEXT field of NFA states links this list.
   * After compilation, all the states are compacted into an array,
   * ordered by state id numbers.  At that time, this points to the base 
   * of that array.
   */
  struct rx_nfa_state *nfa_states;

  /* Every nfa begins with one distinguished starting state:
   */
  struct rx_nfa_state *start;

  /* This orders the search through super-nfa paths.
   * See the comment near the typedef of rx_se_list_order.
   */
  rx_se_list_order se_list_cmp;

  struct rx_superset * start_set;
};




/* SYNTAX TREES */

/* Compilation is in stages.  
 *
 * In the first stage, a pattern specified by a string is 
 * translated into a syntax tree.  Later stages will convert
 * the syntax tree into an NFA optimized for conversion to a
 * superstate-NFA.
 *
 * This page is about syntax trees.
 */

enum rexp_node_type
{
  r_cset,			/* Match from a character set. `a' or `[a-z]'*/
  r_concat,			/* Concat two subexpressions.   `ab' */
  r_alternate,			/* Choose one of two subexpressions. `a\|b' */
  r_opt,			/* Optional subexpression. `a?' */
  r_star,			/* Repeated subexpression. `a*' */


  /* A 2phase-star is a variation on a repeated subexpression.
   * In this case, there are two subexpressions.  The first, if matched,
   * begins a repitition (otherwise, the whole expression is matches the
   * empth string).  
   * 
   * After matching the first subexpression, a 2phase star either finishes,
   * or matches the second subexpression.  If the second subexpression is
   * matched, then the whole construct repeats.
   *
   * 2phase stars are used in two circumstances.  First, they
   * are used as part of the implementation of POSIX intervals (counted
   * repititions).  Second, they are used to implement proper star
   * semantics when the repeated subexpression contains paths of
   * only side effects.  See rx_compile for more information.
   */
  r_2phase_star,


  /* c.f. "typedef void * rx_side_effect" */
  r_side_effect,

  /* This is an extension type:  It is for transient use in source->source
   * transformations (implemented over syntax trees).
   */
  r_data
};

/* A side effect is a matcher-specific action associated with
 * transitions in the NFA.  The details of side effects are up
 * to the matcher.  To the compiler and superstate constructors
 * side effects are opaque:
 */

typedef void * rx_side_effect;

/* Nodes in a syntax tree are of this type:
 */
struct rexp_node
{
  enum rexp_node_type type;
  union
  {
    rx_Bitset cset;
    rx_side_effect side_effect;
    struct
      {
	struct rexp_node *left;
	struct rexp_node *right;
      } pair;
    void * data;
  } params;
};



/* NFA
 *
 * A syntax tree is compiled into an NFA.  This page defines the structure
 * of that NFA.
 */

struct rx_nfa_state
{
  /* These are kept in a list as the NFA is being built. */
  struct rx_nfa_state *next;

  /* After the NFA is built, states are given integer id's.
   * States whose outgoing transitions are all either epsilon or 
   * side effect edges are given ids less than 0.  Other states
   * are given successive non-negative ids starting from 0.
   */
  int id;

  /* The list of NFA edges that go from this state to some other. */
  struct rx_nfa_edge *edges;

  /* If you land in this state, then you implicitly land
   * in all other states reachable by only epsilon translations.
   * Call the set of maximal paths to such states the epsilon closure
   * of this state.
   *
   * There may be other states that are reachable by a mixture of
   * epsilon and side effect edges.  Consider the set of maximal paths
   * of that sort from this state.  Call it the epsilon-side-effect
   * closure of the state.
   * 
   * The epsilon closure of the state is a subset of the epsilon-side-
   * effect closure.  It consists of all the paths that contain 
   * no side effects -- only epsilon edges.
   * 
   * The paths in the epsilon-side-effect closure  can be partitioned
   * into equivalance sets. Two paths are equivalant if they have the
   * same set of side effects, in the same order.  The epsilon-closure
   * is one of these equivalance sets.  Let's call these equivalance
   * sets: observably equivalant path sets.  That name is chosen
   * because equivalance of two paths means they cause the same side
   * effects -- so they lead to the same subsequent observations other
   * than that they may wind up in different target states.
   *
   * The superstate nfa, which is derived from this nfa, is based on
   * the observation that all of the paths in an observably equivalant
   * path set can be explored at the same time, provided that the
   * matcher keeps track not of a single nfa state, but of a set of
   * states.   In particular, after following all the paths in an
   * observably equivalant set, you wind up at a set of target states.
   * That set of target states corresponds to one state in the
   * superstate NFA.
   *
   * Staticly, before matching begins, it is convenient to analyze the
   * nfa.  Each state is labeled with a list of the observably
   * equivalant path sets who's union covers all the
   * epsilon-side-effect paths beginning in this state.  This list is
   * called the possible futures of the state.
   *
   * A trivial example is this NFA:
   *             s1
   *         A  --->  B
   *
   *             s2  
   *            --->  C
   *
   *             epsilon           s1
   *            --------->  D   ------> E
   * 
   * 
   * In this example, A has two possible futures.
   * One invokes the side effect `s1' and contains two paths,
   * one ending in state B, the other in state E.
   * The other invokes the side effect `s2' and contains only
   * one path, landing in state C.
   */
  struct rx_possible_future *futures;


  /* There are exactly two distinguished states in every NFA: */
  unsigned int is_final:1;
  unsigned int is_start:1;

  /* These are used during NFA construction... */
  unsigned int eclosure_needed:1;
  unsigned int mark:1;
};


/* An edge in an NFA is typed: */
enum rx_nfa_etype
{
  /* A cset edge is labled with a set of characters one of which
   * must be matched for the edge to be taken.
   */
  ne_cset,

  /* An epsilon edge is taken whenever its starting state is
   * reached. 
   */
  ne_epsilon,

  /* A side effect edge is taken whenever its starting state is
   * reached.  Side effects may cause the match to fail or the
   * position of the matcher to advance.
   */
  ne_side_effect		/* A special kind of epsilon. */
};

struct rx_nfa_edge
{
  struct rx_nfa_edge *next;
  enum rx_nfa_etype type;
  struct rx_nfa_state *dest;
  union
  {
    rx_Bitset cset;
    rx_side_effect side_effect;
  } params;
};



/* A possible future consists of a list of side effects
 * and a set of destination states.  Below are their
 * representations.  These structures are hash-consed which
 * means that lists with the same elements share a representation
 * (their addresses are ==).
 */

struct rx_nfa_state_set
{
  struct rx_nfa_state * car;
  struct rx_nfa_state_set * cdr;
};

struct rx_se_list
{
  rx_side_effect car;
  struct rx_se_list * cdr;
};

struct rx_possible_future
{
  struct rx_possible_future *next;
  struct rx_se_list * effects;
  struct rx_nfa_state_set * destset;
};



/* This begins the description of the superstate NFA.
 *
 * The superstate NFA corresponds to the NFA in these ways:
 *
 * Every superstate NFA states SUPER correspond to sets of NFA states,
 * nfa_states(SUPER).
 *
 * Superstate edges correspond to NFA paths.
 *
 * The superstate has no epsilon transitions;
 * every edge has a character label, and a (possibly empty) side
 * effect label.   The side effect label corresponds to a list of
 * side effects that occur in the NFA.  These parts are referred
 * to as:   superedge_character(EDGE) and superedge_sides(EDGE).
 *
 * For a superstate edge EDGE starting in some superstate SUPER,
 * the following is true (in pseudo-notation :-):
 *
 *       exists DEST in nfa_states s.t. 
 *         exists nfaEDGE in nfa_edges s.t.
 *                 origin (nfaEDGE) == DEST
 *              && origin (nfaEDGE) is a member of nfa_states(SUPER)
 *              && exists PF in possible_futures(dest(nfaEDGE)) s.t.
 * 	                sides_of_possible_future (PF) == superedge_sides (EDGE)
 *
 * also:
 *
 *      let SUPER2 := superedge_destination(EDGE)
 *          nfa_states(SUPER2)
 *           == union of all nfa state sets S s.t.
 *                          exists PF in possible_futures(dest(nfaEDGE)) s.t.
 * 	                       sides_of_possible_future (PF) == superedge_sides (EDGE)
 *                          && S == dests_of_possible_future (PF) }
 *
 * Or in english, every superstate is a set of nfa states.  A given
 * character and a superstate implies many transitions in the NFA --
 * those that begin with an edge labeled with that character from a
 * state in the set corresponding to the superstate.
 * 
 * The destinations of those transitions each have a set of possible
 * futures.  A possible future is a list of side effects and a set of
 * destination NFA states.  Two sets of possible futures can be
 * `merged' by combining all pairs of possible futures that have the
 * same side effects.  A pair is combined by creating a new future
 * with the same side effect but the union of the two destination sets.
 * In this way, all the possible futures suggested by a superstate
 * and a character can be merged into a set of possible futures where
 * no two elements of the set have the same set of side effects.
 *
 * The destination of a possible future, being a set of NFA states, 
 * corresponds to a supernfa state.  So, the merged set of possible
 * futures we just created can serve as a set of edges in the
 * supernfa.
 *
 * The representation of the superstate nfa and the nfa is critical.
 * The nfa has to be compact, but has to facilitate the rapid
 * computation of missing superstates.  The superstate nfa has to 
 * be fast to interpret, lazilly constructed, and bounded in space.
 *
 * To facilitate interpretation, the superstate data structures are 
 * peppered with `instruction frames'.  There is an instruction set
 * defined below which matchers using the supernfa must be able to
 * interpret.
 *
 * We'd like to make it possible but not mandatory to use code
 * addresses to represent instructions (c.f. gcc's computed goto).
 * Therefore, we define an enumerated type of opcodes, and when
 * writing one of these instructions into a data structure, use
 * the opcode as an index into a table of instruction values.
 * 
 * Here are the opcodes that occur in the superstate nfa:
 */
 

/* Every superstate contains a table of instruction frames indexed 
 * by characters.  A normal `move' in a matcher is to fetch the next
 * character and use it as an index into a superstates transition
 * table.
 *
 * In the fasted case, only one edge follows from that character.
 * In other cases there is more work to do.
 * 
 * The descriptions of the opcodes refer to data structures that are
 * described further below. 
 */

enum rx_opcode
{
  /* 
   * BACKTRACK_POINT is invoked when a character transition in 
   * a superstate leads to more than one edge.  In that case,
   * the edges have to be explored independently using a backtracking
   * strategy.
   *
   * A BACKTRACK_POINT instruction is stored in a superstate's 
   * transition table for some character when it is known that that
   * character crosses more than one edge.  On encountering this
   * instruction, the matcher saves enough state to backtrack to this
   * point in the match later.
   */
  rx_backtrack_point = 0,	/* data is (struct transition_class *) */

  /* 
   * RX_DO_SIDE_EFFECTS evaluates the side effects of an epsilon path.
   * There is one occurence of this instruction per rx_distinct_future.
   * This instruction is skipped if a rx_distinct_future has no side effects.
   */
  rx_do_side_effects = rx_backtrack_point + 1,

  /* data is (struct rx_distinct_future *) */

  /* 
   * RX_CACHE_MISS instructions are stored in rx_distinct_futures whose
   * destination superstate has been reclaimed (or was never built).
   * It recomputes the destination superstate.
   * RX_CACHE_MISS is also stored in a superstate transition table before
   * any of its edges have been built.
   */
  rx_cache_miss = rx_do_side_effects + 1,
  /* data is (struct rx_distinct_future *) */

  /* 
   * RX_NEXT_CHAR is called to consume the next character and take the
   * corresponding transition.  This is the only instruction that uses 
   * the DATA field of the instruction frame instead of DATA_2.
   * (see EXPLORE_FUTURE in regex.c).
   */
  rx_next_char = rx_cache_miss + 1, /* data is (struct superstate *) */

  /* RX_BACKTRACK indicates that a transition fails.
   */
  rx_backtrack = rx_next_char + 1, /* no data */

  /* 
   * RX_ERROR_INX is stored only in places that should never be executed.
   */
  rx_error_inx = rx_backtrack + 1, /* Not supposed to occur. */

  rx_num_instructions = rx_error_inx + 1
};

/* An id_instruction_table holds the values stored in instruction
 * frames.  The table is indexed by the enums declared above.
 */
extern void * rx_id_instruction_table[rx_num_instructions];

/* The heart of the matcher is a `word-code-interpreter' 
 * (like a byte-code interpreter, except that instructions
 * are a full word wide).
 *
 * Instructions are not stored in a vector of code, instead,
 * they are scattered throughout the data structures built
 * by the regexp compiler and the matcher.  One word-code instruction,
 * together with the arguments to that instruction, constitute
 * an instruction frame (struct rx_inx).
 *
 * This structure type is padded by hand to a power of 2 because
 * in one of the dominant cases, we dispatch by indexing a table
 * of instruction frames.  If that indexing can be accomplished
 * by just a shift of the index, we're happy.
 *
 * Instructions take at most one argument, but there are two
 * slots in an instruction frame that might hold that argument.
 * These are called data and data_2.  The data slot is only
 * used for one instruction (RX_NEXT_CHAR).  For all other 
 * instructions, data should be set to 0.
 *
 * RX_NEXT_CHAR is the most important instruction by far.
 * By reserving the data field for its exclusive use, 
 * instruction dispatch is sped up in that case.  There is
 * no need to fetch both the instruction and the data,
 * only the data is needed.  In other words, a `cycle' begins
 * by fetching the field data.  If that is non-0, then it must
 * be the destination state of a next_char transition, so
 * make that value the current state, advance the match position
 * by one character, and start a new cycle.  On the other hand,
 * if data is 0, fetch the instruction and do a more complicated
 * dispatch on that.
 */

struct rx_inx 
{
  void * data;
  void * data_2;
  void * inx;
  void * fnord;
};

#ifndef RX_TAIL_ARRAY
#define RX_TAIL_ARRAY  1
#endif

/* A superstate corresponds to a set of nfa states.  Those sets are
 * represented by STRUCT RX_SUPERSET.  The constructors
 * guarantee that only one (shared) structure is created for a given set.
 */
struct rx_superset
{
  int refs;			/* This is a reference counted structure. */

  /* We keep these sets in a cache because (in an unpredictable way),
   * the same set is often created again and again.  But that is also
   * problematic -- compatibility with POSIX and GNU regex requires
   * that we not be able to tell when a program discards a particular
   * NFA (thus invalidating the supersets created from it).
   *
   * But when a cache hit appears to occur, we will have in hand the
   * nfa for which it may have happened.  That is why every nfa is given
   * its own sequence number.  On a cache hit, the cache is validated
   * by comparing the nfa sequence number to this field:
   */
  int id;

  struct rx_nfa_state * car;	/* May or may not be a valid addr. */
  struct rx_superset * cdr;

  /* If the corresponding superstate exists: */
  struct rx_superstate * superstate;


  /* There is another bookkeeping problem.  It is expensive to 
   * compute the starting nfa state set for an nfa.  So, once computed,
   * it is cached in the `struct rx'.
   *
   * But, the state set can be flushed from the superstate cache.
   * When that happens, we can't know if the corresponding `struct rx'
   * is still alive or if it has been freed or re-used by the program.
   * So, the cached pointer to this set in a struct rx might be invalid
   * and we need a way to validate it.
   *
   * Fortunately, even if this set is flushed from the cache, it is
   * not freed.  It just goes on the free-list of supersets.
   * So we can still examine it.  
   *
   * So to validate a starting set memo, check to see if the
   * starts_for field still points back to the struct rx in question,
   * and if the ID matches the rx sequence number.
   */
  struct rx * starts_for;

  /* This is used to link into a hash bucket so these objects can
   * be `hash-consed'.
   */
  struct rx_hash_item hash_item;
};

#define rx_protect_superset(RX,CON) (++(CON)->refs)

/* The terminology may be confusing (rename this structure?).
 * Every character occurs in at most one rx_super_edge per super-state.
 * But, that structure might have more than one option, indicating a point
 * of non-determinism. 
 *
 * In other words, this structure holds a list of superstate edges
 * sharing a common starting state and character label.  The edges
 * are in the field OPTIONS.  All superstate edges sharing the same
 * starting state and character are in this list.
 */
struct rx_super_edge
{
  struct rx_super_edge *next;
  struct rx_inx rx_backtrack_frame;
  int cset_size;
  rx_Bitset cset;
  struct rx_distinct_future *options;
};

/* A superstate is a set of nfa states (RX_SUPERSET) along
 * with a transition table.  Superstates are built on demand and reclaimed
 * without warning.  To protect a superstate from this ghastly fate,
 * use LOCK_SUPERSTATE. 
 */
struct rx_superstate
{
  int rx_id;			/* c.f. the id field of rx_superset */
  int locks;			/* protection from reclamation */

  /* Within a superstate cache, all the superstates are kept in a big
   * queue.  The tail of the queue is the state most likely to be
   * reclaimed.  The *recyclable fields hold the queue position of 
   * this state.
   */
  struct rx_superstate * next_recyclable;
  struct rx_superstate * prev_recyclable;

  /* The supernfa edges that exist in the cache and that have
   * this state as their destination are kept in this list:
   */
  struct rx_distinct_future * transition_refs;

  /* The list of nfa states corresponding to this superstate: */
  struct rx_superset * contents;

  /* The list of edges in the cache beginning from this state. */
  struct rx_super_edge * edges;

  /* A tail of the recyclable queue is marked as semifree.  A semifree
   * state has no incoming next_char transitions -- any transition
   * into a semifree state causes a complex dispatch with the side
   * effect of rescuing the state from its semifree state.
   *
   * An alternative to this might be to make next_char more expensive,
   * and to move a state to the head of the recyclable queue whenever
   * it is entered.  That way, popular states would never be recycled.
   *
   * But unilaterally making next_char more expensive actually loses.
   * So, incoming transitions are only made expensive for states near
   * the tail of the recyclable queue.  The more cache contention
   * there is, the more frequently a state will have to prove itself
   * and be moved back to the front of the queue.  If there is less 
   * contention, then popular states just aggregate in the front of 
   * the queue and stay there.
   */
  int is_semifree;


  /* This keeps track of the size of the transition table for this
   * state.  There is a half-hearted attempt to support variable sized
   * superstates.
   */
  int trans_size;

  /* Indexed by characters... */
  struct rx_inx transitions[RX_TAIL_ARRAY];
};


/* A list of distinct futures define the edges that leave from a 
 * given superstate on a given character.  c.f. rx_super_edge.
 */

struct rx_distinct_future
{
  struct rx_distinct_future * next_same_super_edge[2];
  struct rx_distinct_future * next_same_dest;
  struct rx_distinct_future * prev_same_dest;
  struct rx_superstate * present;	/* source state */
  struct rx_superstate * future;	/* destination state */
  struct rx_super_edge * edge;


  /* The future_frame holds the instruction that should be executed
   * after all the side effects are done, when it is time to complete
   * the transition to the next state.
   *
   * Normally this is a next_char instruction, but it may be a
   * cache_miss instruction as well, depending on whether or not
   * the superstate is in the cache and semifree.
   * 
   * If this is the only future for a given superstate/char, and
   * if there are no side effects to be performed, this frame is
   * not used (directly) at all.  Instead, its contents are copied
   * into the transition table of the starting state of this dist. future.
   */
  struct rx_inx future_frame;

  struct rx_inx side_effects_frame;
  struct rx_se_list * effects;
};

#define rx_lock_superstate(R,S)  ((S)->locks++)
#define rx_unlock_superstate(R,S) (--(S)->locks)


/* This page destined for rx.h */

struct rx_blocklist
{
  struct rx_blocklist * next;
  int bytes;
};

struct rx_freelist
{
  struct rx_freelist * next;
};

struct rx_cache;

#ifdef __STDC__
typedef void (*rx_morecore_fn)(struct rx_cache *);
#else
typedef void (*rx_morecore_fn)();
#endif

/* You use this to control the allocation of superstate data 
 * during matching.  Most of it should be initialized to 0.
 *
 * A MORECORE function is necessary.  It should allocate
 * a new block of memory or return 0.
 * A default that uses malloc is called `rx_morecore'.
 *
 * The number of SUPERSTATES_ALLOWED indirectly limits how much memory
 * the system will try to allocate.  The default is 128.  Batch style
 * applications that are very regexp intensive should use as high a number
 * as possible without thrashing.
 * 
 * The LOCAL_CSET_SIZE is the number of characters in a character set.
 * It is therefore the number of entries in a superstate transition table.
 * Generally, it should be 256.  If your character set has 16 bits, 
 * it is better to translate your regexps into equivalent 8 bit patterns.
 */

struct rx_cache
{
  struct rx_hash_rules superset_hash_rules;

  /* Objects are allocated by incrementing a pointer that 
   * scans across rx_blocklists.
   */
  struct rx_blocklist * memory;
  struct rx_blocklist * memory_pos;
  int bytes_left;
  char * memory_addr;
  rx_morecore_fn morecore;

  /* Freelists. */
  struct rx_freelist * free_superstates;
  struct rx_freelist * free_transition_classes;
  struct rx_freelist * free_discernable_futures;
  struct rx_freelist * free_supersets;
  struct rx_freelist * free_hash;

  /* Two sets of superstates -- those that are semifreed, and those
   * that are being used.
   */
  struct rx_superstate * lru_superstate;
  struct rx_superstate * semifree_superstate;

  struct rx_superset * empty_superset;

  int superstates;
  int semifree_superstates;
  int hits;
  int misses;
  int superstates_allowed;

  int local_cset_size;
  void ** instruction_table;

  struct rx_hash superset_table;
};



/* The lowest-level search function supports arbitrarily fragmented
 * strings and (optionally) suspendable/resumable searches.
 *
 * Callers have to provide a few hooks.
 */

#ifndef __GNUC__
#ifdef __STDC__
#define __const__ const
#else
#define __const__
#endif
#endif

/* This holds a matcher position */
struct rx_string_position
{
  __const__ unsigned char * pos;	/* The current pos. */
  __const__ unsigned char * string; /* The current string burst. */
  __const__ unsigned char * end;	/* First invalid position >= POS. */
  int offset;			/* Integer address of the current burst. */
  int size;			/* Current string's size. */
  int search_direction;		/* 1 or -1 */
  int search_end;		/* First position to not try. */
};


enum rx_get_burst_return
{
  rx_get_burst_continuation,
  rx_get_burst_error,
  rx_get_burst_ok,
  rx_get_burst_no_more
};


/* A call to get burst should make POS valid.  It might be invalid
 * if the STRING field doesn't point to a burst that actually
 * contains POS.
 *
 * GET_BURST should take a clue from SEARCH_DIRECTION (1 or -1) as to
 * whether or not to pad to the left.  Padding to the right is always
 * appropriate, but need not go past the point indicated by STOP.
 *
 * If a continuation is returned, then the reentering call to
 * a search function will retry the get_burst.
 */

#ifdef __STDC__
typedef enum rx_get_burst_return
  (*rx_get_burst_fn) (struct rx_string_position * pos,
		      void * app_closure,
		      int stop);
					       
#else
typedef enum rx_get_burst_return (*rx_get_burst_fn) ();
#endif


enum rx_back_check_return
{
  rx_back_check_continuation,
  rx_back_check_error,
  rx_back_check_pass,
  rx_back_check_fail
};

/* Back_check should advance the position it is passed 
 * over rparen - lparen characters and return pass iff
 * the characters starting at POS match those indexed
 * by [LPAREN..RPAREN].
 *
 * If a continuation is returned, then the reentering call to
 * a search function will retry the back_check.
 */

#ifdef __STDC__
typedef enum rx_back_check_return
  (*rx_back_check_fn) (struct rx_string_position * pos,
		       int lparen,
		       int rparen,
		       unsigned char * translate,
		       void * app_closure,
		       int stop);
					       
#else
typedef enum rx_back_check_return (*rx_back_check_fn) ();
#endif




/* A call to fetch_char should return the character at POS or POS + 1.
 * Returning continuations here isn't supported.  OFFSET is either 0 or 1
 * and indicates which characters is desired.
 */

#ifdef __STDC__
typedef int (*rx_fetch_char_fn) (struct rx_string_position * pos,
				 int offset,
				 void * app_closure,
				 int stop);
#else
typedef int (*rx_fetch_char_fn) ();
#endif


enum rx_search_return
{
  rx_search_continuation = -4,
  rx_search_error = -3,
  rx_search_soft_fail = -2,	/* failed by running out of string */
  rx_search_fail = -1		/* failed only by reaching failure states */
  /* return values >= 0 indicate the position of a successful match */
};






/* regex.h
 * 
 * The remaining declarations replace regex.h.
 */

/* This is an array of error messages corresponding to the error codes.
 */
extern __const__ char *re_error_msg[];

/* If any error codes are removed, changed, or added, update the
   `re_error_msg' table in regex.c.  */
typedef enum
{
  REG_NOERROR = 0,	/* Success.  */
  REG_NOMATCH,		/* Didn't find a match (for regexec).  */

  /* POSIX regcomp return error codes.  (In the order listed in the
     standard.)  */
  REG_BADPAT,		/* Invalid pattern.  */
  REG_ECOLLATE,		/* Not implemented.  */
  REG_ECTYPE,		/* Invalid character class name.  */
  REG_EESCAPE,		/* Trailing backslash.  */
  REG_ESUBREG,		/* Invalid back reference.  */
  REG_EBRACK,		/* Unmatched left bracket.  */
  REG_EPAREN,		/* Parenthesis imbalance.  */ 
  REG_EBRACE,		/* Unmatched \{.  */
  REG_BADBR,		/* Invalid contents of \{\}.  */
  REG_ERANGE,		/* Invalid range end.  */
  REG_ESPACE,		/* Ran out of memory.  */
  REG_BADRPT,		/* No preceding re for repetition op.  */

  /* Error codes we've added.  */
  REG_EEND,		/* Premature end.  */
  REG_ESIZE,		/* Compiled pattern bigger than 2^16 bytes.  */
  REG_ERPAREN		/* Unmatched ) or \); not returned from regcomp.  */
} reg_errcode_t;

/* The regex.c support, as a client of rx, defines a set of possible
 * side effects that can be added to the edge lables of nfa edges.
 * Here is the list of sidef effects in use.
 */

enum re_side_effects
{
#define RX_WANT_SE_DEFS 1
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#define RX_DEF_SE(IDEM, NAME, VALUE)	      NAME VALUE,
#define RX_DEF_CPLX_SE(IDEM, NAME, VALUE)     NAME VALUE,
#include "rx.h"
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#undef RX_WANT_SE_DEFS
   re_floogle_flap = 65533
};

/* These hold paramaters for the kinds of side effects that are possible
 * in the supported pattern languages.  These include things like the 
 * numeric bounds of {} operators and the index of paren registers for 
 * subexpression measurement or backreferencing.
 */
struct re_se_params
{
  enum re_side_effects se;
  int op1;
  int op2;
};

typedef unsigned reg_syntax_t;

struct re_pattern_buffer
{
  struct rx rx;
  reg_syntax_t syntax;		/* See below for syntax bit definitions. */

  unsigned int no_sub:1;	/* If set, don't  return register offsets. */
  unsigned int not_bol:1;	/* If set, the anchors ('^' and '$') don't */
  unsigned int not_eol:1;	/*     match at the ends of the string.  */  
  unsigned int newline_anchor:1;/* If true, an anchor at a newline matches.*/
  unsigned int least_subs:1;	/* If set, and returning registers, return
				 * as few values as possible.  Only 
				 * backreferenced groups and group 0 (the whole
				 * match) will be returned.
				 */

  /* If true, this says that the matcher should keep registers on its
   * backtracking stack.  For many patterns, we can easily determine that
   * this isn't necessary.
   */
  unsigned int match_regs_on_stack:1;
  unsigned int search_regs_on_stack:1;

  /* is_anchored and begbuf_only are filled in by rx_compile. */
  unsigned int is_anchored:1;	/* Anchorded by ^? */
  unsigned int begbuf_only:1;	/* Anchored to char position 0? */

  
  /* If REGS_UNALLOCATED, allocate space in the `regs' structure
   * for `max (RE_NREGS, re_nsub + 1)' groups.
   * If REGS_REALLOCATE, reallocate space if necessary.
   * If REGS_FIXED, use what's there.  
   */
#define REGS_UNALLOCATED 0
#define REGS_REALLOCATE 1
#define REGS_FIXED 2
  unsigned int regs_allocated:2;

  
  /* Either a translate table to apply to all characters before
   * comparing them, or zero for no translation.  The translation
   * is applied to a pattern when it is compiled and to a string
   * when it is matched.
   */
  unsigned char * translate;

  /* If this is a valid pointer, it tells rx not to store the extents of 
   * certain subexpressions (those corresponding to non-zero entries).
   * Passing 0x1 is the same as passing an array of all ones.  Passing 0x0
   * is the same as passing an array of all zeros.
   * The array should contain as many entries as their are subexps in the 
   * regexp.
   *
   * For POSIX compatability, when using regcomp and regexec this field
   * is zeroed and ignored.
   */
  char * syntax_parens;

	/* Number of subexpressions found by the compiler.  */
  size_t re_nsub;

  void * buffer;		/* Malloced memory for the nfa. */
  unsigned long allocated;	/* Size of that memory. */

  /* Pointer to a fastmap, if any, otherwise zero.  re_search uses
   * the fastmap, if there is one, to skip over impossible
   * starting points for matches.  */
  char *fastmap;

  unsigned int fastmap_accurate:1; /* These three are internal. */
  unsigned int can_match_empty:1;  
  struct rx_nfa_state * start;	/* The nfa starting state. */

  /* This is the list of iterator bounds for {lo,hi} constructs.
   * The memory pointed to is part of the rx->buffer.
   */
  struct re_se_params *se_params;

  /* This is a bitset representation of the fastmap.
   * This is a true fastmap that already takes the translate
   * table into account.
   */
  rx_Bitset fastset;
};

/* Type for byte offsets within the string.  POSIX mandates this.  */
typedef int regoff_t;

/* This is the structure we store register match data in.  See
   regex.texinfo for a full description of what registers match.  */
struct re_registers
{
  unsigned num_regs;
  regoff_t *start;
  regoff_t *end;
};

typedef struct re_pattern_buffer regex_t;

/* POSIX specification for registers.  Aside from the different names than
   `re_registers', POSIX uses an array of structures, instead of a
   structure of arrays.  */
typedef struct
{
  regoff_t rm_so;  /* Byte offset from string's start to substring's start.  */
  regoff_t rm_eo;  /* Byte offset from string's start to substring's end.  */
} regmatch_t;


/* The following bits are used to determine the regexp syntax we
   recognize.  The set/not-set meanings are chosen so that Emacs syntax
   remains the value 0.  The bits are given in alphabetical order, and
   the definitions shifted by one from the previous bit; thus, when we
   add or remove a bit, only one other definition need change.  */

/* If this bit is not set, then \ inside a bracket expression is literal.
   If set, then such a \ quotes the following character.  */
#define RE_BACKSLASH_ESCAPE_IN_LISTS (1)

/* If this bit is not set, then + and ? are operators, and \+ and \? are
     literals. 
   If set, then \+ and \? are operators and + and ? are literals.  */
#define RE_BK_PLUS_QM (RE_BACKSLASH_ESCAPE_IN_LISTS << 1)

/* If this bit is set, then character classes are supported.  They are:
     [:alpha:], [:upper:], [:lower:],  [:digit:], [:alnum:], [:xdigit:],
     [:space:], [:print:], [:punct:], [:graph:], and [:cntrl:].
   If not set, then character classes are not supported.  */
#define RE_CHAR_CLASSES (RE_BK_PLUS_QM << 1)

/* If this bit is set, then ^ and $ are always anchors (outside bracket
     expressions, of course).
   If this bit is not set, then it depends:
        ^  is an anchor if it is at the beginning of a regular
           expression or after an open-group or an alternation operator;
        $  is an anchor if it is at the end of a regular expression, or
           before a close-group or an alternation operator.  

   This bit could be (re)combined with RE_CONTEXT_INDEP_OPS, because
   POSIX draft 11.2 says that * etc. in leading positions is undefined.
   We already implemented a previous draft which made those constructs
   invalid, though, so we haven't changed the code back.  */
#define RE_CONTEXT_INDEP_ANCHORS (RE_CHAR_CLASSES << 1)

/* If this bit is set, then special characters are always special
     regardless of where they are in the pattern.
   If this bit is not set, then special characters are special only in
     some contexts; otherwise they are ordinary.  Specifically, 
     * + ? and intervals are only special when not after the beginning,
     open-group, or alternation operator.  */
#define RE_CONTEXT_INDEP_OPS (RE_CONTEXT_INDEP_ANCHORS << 1)

/* If this bit is set, then *, +, ?, and { cannot be first in an re or
     immediately after an alternation or begin-group operator.  */
#define RE_CONTEXT_INVALID_OPS (RE_CONTEXT_INDEP_OPS << 1)

/* If this bit is set, then . matches newline.
   If not set, then it doesn't.  */
#define RE_DOT_NEWLINE (RE_CONTEXT_INVALID_OPS << 1)

/* If this bit is set, then . doesn't match NUL.
   If not set, then it does.  */
#define RE_DOT_NOT_NULL (RE_DOT_NEWLINE << 1)

/* If this bit is set, nonmatching lists [^...] do not match newline.
   If not set, they do.  */
#define RE_HAT_LISTS_NOT_NEWLINE (RE_DOT_NOT_NULL << 1)

/* If this bit is set, either \{...\} or {...} defines an
     interval, depending on RE_NO_BK_BRACES. 
   If not set, \{, \}, {, and } are literals.  */
#define RE_INTERVALS (RE_HAT_LISTS_NOT_NEWLINE << 1)

/* If this bit is set, +, ? and | aren't recognized as operators.
   If not set, they are.  */
#define RE_LIMITED_OPS (RE_INTERVALS << 1)

/* If this bit is set, newline is an alternation operator.
   If not set, newline is literal.  */
#define RE_NEWLINE_ALT (RE_LIMITED_OPS << 1)

/* If this bit is set, then `{...}' defines an interval, and \{ and \}
     are literals.
  If not set, then `\{...\}' defines an interval.  */
#define RE_NO_BK_BRACES (RE_NEWLINE_ALT << 1)

/* If this bit is set, (...) defines a group, and \( and \) are literals.
   If not set, \(...\) defines a group, and ( and ) are literals.  */
#define RE_NO_BK_PARENS (RE_NO_BK_BRACES << 1)

/* If this bit is set, then \<digit> matches <digit>.
   If not set, then \<digit> is a back-reference.  */
#define RE_NO_BK_REFS (RE_NO_BK_PARENS << 1)

/* If this bit is set, then | is an alternation operator, and \| is literal. 
   If not set, then \| is an alternation operator, and | is literal.  */
#define RE_NO_BK_VBAR (RE_NO_BK_REFS << 1)

/* If this bit is set, then an ending range point collating higher
     than the starting range point, as in [z-a], is invalid.
   If not set, then when ending range point collates higher than the
     starting range point, the range is ignored.  */
#define RE_NO_EMPTY_RANGES (RE_NO_BK_VBAR << 1)

/* If this bit is set, then an unmatched ) is ordinary.
   If not set, then an unmatched ) is invalid.  */
#define RE_UNMATCHED_RIGHT_PAREN_ORD (RE_NO_EMPTY_RANGES << 1)

/* This global variable defines the particular regexp syntax to use (for
   some interfaces).  When a regexp is compiled, the syntax used is
   stored in the pattern buffer, so changing this does not affect
   already-compiled regexps.  */
extern reg_syntax_t re_syntax_options;

/* Define combinations of the above bits for the standard possibilities.
   (The [[[ comments delimit what gets put into the Texinfo file, so
   don't delete them!)  */ 
/* [[[begin syntaxes]]] */
#define RE_SYNTAX_EMACS 0

#define RE_SYNTAX_AWK							\
  (RE_BACKSLASH_ESCAPE_IN_LISTS | RE_DOT_NOT_NULL			\
   | RE_NO_BK_PARENS            | RE_NO_BK_REFS				\
   | RE_NO_BK_VBAR               | RE_NO_EMPTY_RANGES			\
   | RE_UNMATCHED_RIGHT_PAREN_ORD)

#define RE_SYNTAX_POSIX_AWK 						\
  (RE_SYNTAX_POSIX_EXTENDED | RE_BACKSLASH_ESCAPE_IN_LISTS)

#define RE_SYNTAX_GREP							\
  (RE_BK_PLUS_QM              | RE_CHAR_CLASSES				\
   | RE_HAT_LISTS_NOT_NEWLINE | RE_INTERVALS				\
   | RE_NEWLINE_ALT)

#define RE_SYNTAX_EGREP							\
  (RE_CHAR_CLASSES        | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INDEP_OPS | RE_HAT_LISTS_NOT_NEWLINE			\
   | RE_NEWLINE_ALT       | RE_NO_BK_PARENS				\
   | RE_NO_BK_VBAR)

#define RE_SYNTAX_POSIX_EGREP						\
  (RE_SYNTAX_EGREP | RE_INTERVALS | RE_NO_BK_BRACES)

#define RE_SYNTAX_SED RE_SYNTAX_POSIX_BASIC

/* Syntax bits common to both basic and extended POSIX regex syntax.  */
#define _RE_SYNTAX_POSIX_COMMON						\
  (RE_CHAR_CLASSES | RE_DOT_NEWLINE      | RE_DOT_NOT_NULL		\
   | RE_INTERVALS  | RE_NO_EMPTY_RANGES)

#define RE_SYNTAX_POSIX_BASIC						\
  (_RE_SYNTAX_POSIX_COMMON | RE_BK_PLUS_QM)

/* Differs from ..._POSIX_BASIC only in that RE_BK_PLUS_QM becomes
   RE_LIMITED_OPS, i.e., \? \+ \| are not recognized.  Actually, this
   isn't minimal, since other operators, such as \`, aren't disabled.  */
#define RE_SYNTAX_POSIX_MINIMAL_BASIC					\
  (_RE_SYNTAX_POSIX_COMMON | RE_LIMITED_OPS)

#define RE_SYNTAX_POSIX_EXTENDED					\
  (_RE_SYNTAX_POSIX_COMMON | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INDEP_OPS  | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS       | RE_NO_BK_VBAR				\
   | RE_UNMATCHED_RIGHT_PAREN_ORD)

/* Differs from ..._POSIX_EXTENDED in that RE_CONTEXT_INVALID_OPS
   replaces RE_CONTEXT_INDEP_OPS and RE_NO_BK_REFS is added.  */
#define RE_SYNTAX_POSIX_MINIMAL_EXTENDED				\
  (_RE_SYNTAX_POSIX_COMMON  | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INVALID_OPS | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS        | RE_NO_BK_REFS				\
   | RE_NO_BK_VBAR	    | RE_UNMATCHED_RIGHT_PAREN_ORD)
/* [[[end syntaxes]]] */

/* Maximum number of duplicates an interval can allow.  Some systems
   (erroneously) define this in other header files, but we want our
   value, so remove any previous define.  */
#ifdef RE_DUP_MAX
#undef RE_DUP_MAX
#endif
#define RE_DUP_MAX ((1 << 15) - 1) 



/* POSIX `cflags' bits (i.e., information for `regcomp').  */

/* If this bit is set, then use extended regular expression syntax.
   If not set, then use basic regular expression syntax.  */
#define REG_EXTENDED 1

/* If this bit is set, then ignore case when matching.
   If not set, then case is significant.  */
#define REG_ICASE (REG_EXTENDED << 1)
 
/* If this bit is set, then anchors do not match at newline
     characters in the string.
   If not set, then anchors do match at newlines.  */
#define REG_NEWLINE (REG_ICASE << 1)

/* If this bit is set, then report only success or fail in regexec.
   If not set, then returns differ between not matching and errors.  */
#define REG_NOSUB (REG_NEWLINE << 1)


/* POSIX `eflags' bits (i.e., information for regexec).  */

/* If this bit is set, then the beginning-of-line operator doesn't match
     the beginning of the string (presumably because it's not the
     beginning of a line).
   If not set, then the beginning-of-line operator does match the
     beginning of the string.  */
#define REG_NOTBOL 1

/* Like REG_NOTBOL, except for the end-of-line.  */
#define REG_NOTEOL (1 << 1)

/* If `regs_allocated' is REGS_UNALLOCATED in the pattern buffer,
 * `re_match_2' returns information about at least this many registers
 * the first time a `regs' structure is passed. 
 *
 * Also, this is the greatest number of backreferenced subexpressions
 * allowed in a pattern being matched without caller-supplied registers.
 */
#ifndef RE_NREGS
#define RE_NREGS 30
#endif

extern int rx_cache_bound;
extern char rx_version_string[];



#ifdef RX_WANT_RX_DEFS

/* This is decls to the interesting subsystems and lower layers
 * of rx.  Everything which doesn't have a public counterpart in 
 * regex.c is declared here.
 */


#ifdef __STDC__
typedef void (*rx_hash_freefn) (struct rx_hash_item * it);
#else /* ndef __STDC__ */
typedef void (*rx_hash_freefn) ();
#endif /* ndef __STDC__ */




#ifdef __STDC__
RX_DECL int rx_bitset_is_equal (int size, rx_Bitset a, rx_Bitset b);
RX_DECL int rx_bitset_is_subset (int size, rx_Bitset a, rx_Bitset b);
RX_DECL int rx_bitset_empty (int size, rx_Bitset set);
RX_DECL void rx_bitset_null (int size, rx_Bitset b);
RX_DECL void rx_bitset_universe (int size, rx_Bitset b);
RX_DECL void rx_bitset_complement (int size, rx_Bitset b);
RX_DECL void rx_bitset_assign (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_union (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_intersection (int size,
				     rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_difference (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_revdifference (int size,
				      rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_xor (int size, rx_Bitset a, rx_Bitset b);
RX_DECL unsigned long rx_bitset_hash (int size, rx_Bitset b);
RX_DECL struct rx_hash_item * rx_hash_find (struct rx_hash * table,
					    unsigned long hash,
					    void * value,
					    struct rx_hash_rules * rules);
RX_DECL struct rx_hash_item * rx_hash_store (struct rx_hash * table,
					     unsigned long hash,
					     void * value,
					     struct rx_hash_rules * rules);
RX_DECL void rx_hash_free (struct rx_hash_item * it, struct rx_hash_rules * rules);
RX_DECL void rx_free_hash_table (struct rx_hash * tab, rx_hash_freefn freefn,
				 struct rx_hash_rules * rules);
RX_DECL rx_Bitset rx_cset (struct rx *rx);
RX_DECL rx_Bitset rx_copy_cset (struct rx *rx, rx_Bitset a);
RX_DECL void rx_free_cset (struct rx * rx, rx_Bitset c);
RX_DECL struct rexp_node * rexp_node (struct rx *rx,
				      enum rexp_node_type type);
RX_DECL struct rexp_node * rx_mk_r_cset (struct rx * rx,
					 rx_Bitset b);
RX_DECL struct rexp_node * rx_mk_r_concat (struct rx * rx,
					   struct rexp_node * a,
					   struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_alternate (struct rx * rx,
					      struct rexp_node * a,
					      struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_opt (struct rx * rx,
					struct rexp_node * a);
RX_DECL struct rexp_node * rx_mk_r_star (struct rx * rx,
					 struct rexp_node * a);
RX_DECL struct rexp_node * rx_mk_r_2phase_star (struct rx * rx,
						struct rexp_node * a,
						struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_side_effect (struct rx * rx,
						rx_side_effect a);
RX_DECL struct rexp_node * rx_mk_r_data  (struct rx * rx,
					  void * a);
RX_DECL void rx_free_rexp (struct rx * rx, struct rexp_node * node);
RX_DECL struct rexp_node * rx_copy_rexp (struct rx *rx,
					 struct rexp_node *node);
RX_DECL struct rx_nfa_state * rx_nfa_state (struct rx *rx);
RX_DECL void rx_free_nfa_state (struct rx_nfa_state * n);
RX_DECL struct rx_nfa_state * rx_id_to_nfa_state (struct rx * rx,
						  int id);
RX_DECL struct rx_nfa_edge * rx_nfa_edge (struct rx *rx,
					  enum rx_nfa_etype type,
					  struct rx_nfa_state *start,
					  struct rx_nfa_state *dest);
RX_DECL void rx_free_nfa_edge (struct rx_nfa_edge * e);
RX_DECL void rx_free_nfa (struct rx *rx);
RX_DECL int rx_build_nfa (struct rx *rx,
			  struct rexp_node *rexp,
			  struct rx_nfa_state **start,
			  struct rx_nfa_state **end);
RX_DECL void rx_name_nfa_states (struct rx *rx);
RX_DECL int rx_eclose_nfa (struct rx *rx);
RX_DECL void rx_delete_epsilon_transitions (struct rx *rx);
RX_DECL int rx_compactify_nfa (struct rx *rx,
			       void **mem, unsigned long *size);
RX_DECL void rx_release_superset (struct rx *rx,
				  struct rx_superset *set);
RX_DECL struct rx_superset * rx_superset_cons (struct rx * rx,
					       struct rx_nfa_state *car, struct rx_superset *cdr);
RX_DECL struct rx_superset * rx_superstate_eclosure_union
  (struct rx * rx, struct rx_superset *set, struct rx_nfa_state_set *ecl);
RX_DECL struct rx_superstate * rx_superstate (struct rx *rx,
					      struct rx_superset *set);
RX_DECL struct rx_inx * rx_handle_cache_miss
  (struct rx *rx, struct rx_superstate *super, unsigned char chr, void *data);
RX_DECL reg_errcode_t rx_compile (__const__ char *pattern, int size,
				  reg_syntax_t syntax,
				  struct re_pattern_buffer * rxb);
RX_DECL void rx_blow_up_fastmap (struct re_pattern_buffer * rxb);
#else /* STDC */
RX_DECL int rx_bitset_is_equal ();
RX_DECL int rx_bitset_is_subset ();
RX_DECL int rx_bitset_empty ();
RX_DECL void rx_bitset_null ();
RX_DECL void rx_bitset_universe ();
RX_DECL void rx_bitset_complement ();
RX_DECL void rx_bitset_assign ();
RX_DECL void rx_bitset_union ();
RX_DECL void rx_bitset_intersection ();
RX_DECL void rx_bitset_difference ();
RX_DECL void rx_bitset_revdifference ();
RX_DECL void rx_bitset_xor ();
RX_DECL unsigned long rx_bitset_hash ();
RX_DECL struct rx_hash_item * rx_hash_find ();
RX_DECL struct rx_hash_item * rx_hash_store ();
RX_DECL void rx_hash_free ();
RX_DECL void rx_free_hash_table ();
RX_DECL rx_Bitset rx_cset ();
RX_DECL rx_Bitset rx_copy_cset ();
RX_DECL void rx_free_cset ();
RX_DECL struct rexp_node * rexp_node ();
RX_DECL struct rexp_node * rx_mk_r_cset ();
RX_DECL struct rexp_node * rx_mk_r_concat ();
RX_DECL struct rexp_node * rx_mk_r_alternate ();
RX_DECL struct rexp_node * rx_mk_r_opt ();
RX_DECL struct rexp_node * rx_mk_r_star ();
RX_DECL struct rexp_node * rx_mk_r_2phase_star ();
RX_DECL struct rexp_node * rx_mk_r_side_effect ();
RX_DECL struct rexp_node * rx_mk_r_data  ();
RX_DECL void rx_free_rexp ();
RX_DECL struct rexp_node * rx_copy_rexp ();
RX_DECL struct rx_nfa_state * rx_nfa_state ();
RX_DECL void rx_free_nfa_state ();
RX_DECL struct rx_nfa_state * rx_id_to_nfa_state ();
RX_DECL struct rx_nfa_edge * rx_nfa_edge ();
RX_DECL void rx_free_nfa_edge ();
RX_DECL void rx_free_nfa ();
RX_DECL int rx_build_nfa ();
RX_DECL void rx_name_nfa_states ();
RX_DECL int rx_eclose_nfa ();
RX_DECL void rx_delete_epsilon_transitions ();
RX_DECL int rx_compactify_nfa ();
RX_DECL void rx_release_superset ();
RX_DECL struct rx_superset * rx_superset_cons ();
RX_DECL struct rx_superset * rx_superstate_eclosure_union ();
RX_DECL struct rx_superstate * rx_superstate ();
RX_DECL struct rx_inx * rx_handle_cache_miss ();
RX_DECL reg_errcode_t rx_compile ();
RX_DECL void rx_blow_up_fastmap ();
#endif /* STDC */


#endif /* RX_WANT_RX_DEFS */



#ifdef __STDC__
extern int re_search_2 (struct re_pattern_buffer *rxb,
			__const__ char * string1, int size1,
			__const__ char * string2, int size2,
			int startpos, int range,
			struct re_registers *regs,
			int stop);
extern int re_search (struct re_pattern_buffer * rxb, __const__ char *string,
		      int size, int startpos, int range,
		      struct re_registers *regs);
extern int re_match_2 (struct re_pattern_buffer * rxb,
		       __const__ char * string1, int size1,
		       __const__ char * string2, int size2,
		       int pos, struct re_registers *regs, int stop);
extern int re_match (struct re_pattern_buffer * rxb,
		     __const__ char * string,
		     int size, int pos,
		     struct re_registers *regs);
extern reg_syntax_t re_set_syntax (reg_syntax_t syntax);
extern void re_set_registers (struct re_pattern_buffer *bufp,
			      struct re_registers *regs,
			      unsigned num_regs,
			      regoff_t * starts, regoff_t * ends);
extern __const__ char * re_compile_pattern (__const__ char *pattern,
					int length,
					struct re_pattern_buffer * rxb);
extern int re_compile_fastmap (struct re_pattern_buffer * rxb);
extern char * re_comp (__const__ char *s);
extern int re_exec (__const__ char *s);
extern int regcomp (regex_t * preg, __const__ char * pattern, int cflags);
extern int regexec (__const__ regex_t *preg, __const__ char *string,
		    size_t nmatch, regmatch_t pmatch[],
		    int eflags);
extern size_t regerror (int errcode, __const__ regex_t *preg,
			char *errbuf, size_t errbuf_size);
extern void regfree (regex_t *preg);

#else /* STDC */
extern int re_search_2 ();
extern int re_search ();
extern int re_match_2 ();
extern int re_match ();
extern reg_syntax_t re_set_syntax ();
extern void re_set_registers ();
extern __const__ char * re_compile_pattern ();
extern int re_compile_fastmap ();
extern char * re_comp ();
extern int re_exec ();
extern int regcomp ();
extern int regexec ();
extern size_t regerror ();
extern void regfree ();

#endif /* STDC */



#ifdef RX_WANT_RX_DEFS

struct rx_counter_frame
{
  int tag;
  int val;
  struct rx_counter_frame * inherited_from; /* If this is a copy. */
  struct rx_counter_frame * cdr;
};

struct rx_backtrack_frame
{
  char * counter_stack_sp;

  /* A frame is used to save the matchers state when it crosses a 
   * backtracking point.  The `stk_' fields correspond to variables
   * in re_search_2 (just strip off thes `stk_').  They are documented
   * tere.
   */
  struct rx_superstate * stk_super;
  unsigned int stk_c;
  struct rx_string_position stk_test_pos;
  int stk_last_l;
  int stk_last_r;
  int stk_test_ret;

  /* This is the list of options left to explore at the backtrack
   * point for which this frame was created. 
   */
  struct rx_distinct_future * df;
  struct rx_distinct_future * first_df;

#ifdef RX_DEBUG
   int stk_line_no;
#endif
};

struct rx_stack_chunk
{
  struct rx_stack_chunk * next_chunk;
  int bytes_left;
  char * sp;
};

enum rx_outer_entry
{
  rx_outer_start,
  rx_outer_fastmap,
  rx_outer_test,
  rx_outer_restore_pos
};

enum rx_fastmap_return
{
  rx_fastmap_continuation,
  rx_fastmap_error,
  rx_fastmap_ok,
  rx_fastmap_fail
};

enum rx_fastmap_entry
{
  rx_fastmap_start,
  rx_fastmap_string_break
};

enum rx_test_return
{
  rx_test_continuation,
  rx_test_error,
  rx_test_fail,
  rx_test_ok
};

enum rx_test_internal_return
{
  rx_test_internal_error,
  rx_test_found_first,
  rx_test_line_finished
};

enum rx_test_match_entry
{
  rx_test_start,
  rx_test_cache_hit_loop,
  rx_test_backreference_check,
  rx_test_backtrack_return
};

struct rx_search_state
{
  /* Two groups of registers are kept.  The group with the register state
   * of the current test match, and the group that holds the state at the end
   * of the best known match, if any.
   *
   * For some patterns, there may also be registers saved on the stack.
   */
  unsigned num_regs;		/* Includes an element for register zero. */
  regoff_t * lparen;		/* scratch space for register returns */
  regoff_t * rparen;
  regoff_t * best_lpspace;	/* in case the user doesn't want these */
  regoff_t * best_rpspace;	/* values, we still need space to store
				 * them.  Normally, this memoryis unused
				 * and the space pointed to by REGS is 
				 * used instead.
				 */
  
  int last_l;			/* Highest index of a valid lparen. */
  int last_r;			/* It's dual. */
  
  int * best_lparen;		/* This contains the best known register */
  int * best_rparen;		/* assignments. 
				 * This may point to the same mem as
				 * best_lpspace, or it might point to memory
				 * passed by the caller.
				 */
  int best_last_l;		/* best_last_l:best_lparen::last_l:lparen */
  int best_last_r;


  unsigned char * translate;  

  struct rx_string_position outer_pos;

  struct rx_superstate * start_super;
  int nfa_choice;
  int first_found;		/* If true, return after finding any match. */
  int ret_val;

  /* For continuations... */
  enum rx_outer_entry outer_search_resume_pt;
  struct re_pattern_buffer * saved_rxb;
  int saved_startpos;
  int saved_range;
  int saved_stop;
  int saved_total_size;
  rx_get_burst_fn saved_get_burst;
  rx_back_check_fn saved_back_check;
  struct re_registers * saved_regs;
  
  /**
   ** state for fastmap
   **/
  char * fastmap;
  int fastmap_chr;
  int fastmap_val;

  /* for continuations in the fastmap procedure: */
  enum rx_fastmap_entry fastmap_resume_pt;

  /**
   ** state for test_match 
   **/

  /* The current superNFA position of the matcher. */
  struct rx_superstate * super;
  
  /* The matcher interprets a series of instruction frames.
   * This is the `instruction counter' for the interpretation.
   */
  struct rx_inx * ifr;
  
  /* We insert a ghost character in the string to prime
   * the nfa.  test_pos.pos, test_pos.str_half, and test_pos.end_half
   * keep track of the test-match position and string-half.
   */
  unsigned char c;
  
  /* Position within the string. */
  struct rx_string_position test_pos;

  struct rx_stack_chunk * counter_stack;
  struct rx_stack_chunk * backtrack_stack;
  int backtrack_frame_bytes;
  int chunk_bytes;
  struct rx_stack_chunk * free_chunks;

  /* To return from this function, set test_ret and 
   * `goto test_do_return'.
   *
   * Possible return values are:
   *     1   --- end of string while the superNFA is still going
   *     0   --- internal error (out of memory)
   *	-1   --- search completed by reaching the superNFA fail state
   *    -2   --- a match was found, maybe not the longest.
   *
   * When the search is complete (-1), best_last_r indicates whether
   * a match was found.
   *
   * -2 is return only if search_state.first_found is non-zero.
   *
   * if search_state.first_found is non-zero, a return of -1 indicates no match,
   * otherwise, best_last_r has to be checked.
   */
  int test_ret;

  int could_have_continued;
  
#ifdef RX_DEBUG
  int backtrack_depth;
  /* There is a search tree with every node as set of deterministic
   * transitions in the super nfa.  For every branch of a 
   * backtrack point is an edge in the tree.
   * This counts up a pre-order of nodes in that tree.
   * It's saved on the search stack and printed when debugging. 
   */
  int line_no;
  int lines_found;
#endif


  /* For continuations within the match tester */
  enum rx_test_match_entry test_match_resume_pt;
  struct rx_inx * saved_next_tr_table;
  struct rx_inx * saved_this_tr_table;
  int saved_reg;
  struct rx_backtrack_frame * saved_bf;
  
};


extern char rx_slowmap[];
extern unsigned char rx_id_translation[];

static __inline__ void
init_fastmap (rxb, search_state)
     struct re_pattern_buffer * rxb;
     struct rx_search_state * search_state;
{
  search_state->fastmap = (rxb->fastmap
			   ? (char *)rxb->fastmap
			   : (char *)rx_slowmap);
  /* Update the fastmap now if not correct already. 
   * When the regexp was compiled, the fastmap was computed
   * and stored in a bitset.  This expands the bitset into a
   * character array containing 1s and 0s.
   */
  if ((search_state->fastmap == rxb->fastmap) && !rxb->fastmap_accurate)
    rx_blow_up_fastmap (rxb);
  search_state->fastmap_chr = -1;
  search_state->fastmap_val = 0;
  search_state->fastmap_resume_pt = rx_fastmap_start;
}

static __inline__ void
uninit_fastmap (rxb, search_state)
     struct re_pattern_buffer * rxb;
     struct rx_search_state * search_state;
{
  /* Unset the fastmap sentinel */
  if (search_state->fastmap_chr >= 0)
    search_state->fastmap[search_state->fastmap_chr]
      = search_state->fastmap_val;
}

static __inline__ int
fastmap_search (rxb, stop, get_burst, app_closure, search_state)
     struct re_pattern_buffer * rxb;
     int stop;
     rx_get_burst_fn get_burst;
     void * app_closure;
     struct rx_search_state * search_state;
{
  enum rx_fastmap_entry pc;

  if (0)
    {
    return_continuation:
      search_state->fastmap_resume_pt = pc;
      return rx_fastmap_continuation;
    }

  pc = search_state->fastmap_resume_pt;

  switch (pc)
    {
    default:
      return rx_fastmap_error;
    case rx_fastmap_start:
    init_fastmap_sentinal:
      /* For the sake of fast fastmapping, set a sentinal in the fastmap.
       * This sentinal will trap the fastmap loop when it reaches the last
       * valid character in a string half.
       *
       * This must be reset when the fastmap/search loop crosses a string 
       * boundry, and before returning to the caller.  So sometimes,
       * the fastmap loop is restarted with `continue', othertimes by
       * `goto init_fastmap_sentinal'.
       */
      if (search_state->outer_pos.size)
	{
	  search_state->fastmap_chr = ((search_state->outer_pos.search_direction == 1)
				       ? *(search_state->outer_pos.end - 1)
				       : *search_state->outer_pos.string);
	  search_state->fastmap_val
	    = search_state->fastmap[search_state->fastmap_chr];
	  search_state->fastmap[search_state->fastmap_chr] = 1;
	}
      else
	{
	  search_state->fastmap_chr = -1;
	  search_state->fastmap_val = 0;
	}
      
      if (search_state->outer_pos.pos >= search_state->outer_pos.end)
	goto fastmap_hit_bound;
      else
	{
	  if (search_state->outer_pos.search_direction == 1)
	    {
	      if (search_state->fastmap_val)
		{
		  for (;;)
		    {
		      while (!search_state->fastmap[*search_state->outer_pos.pos])
			++search_state->outer_pos.pos;
		      return rx_fastmap_ok;
		    }
		}
	      else
		{
		  for (;;)
		    {
		      while (!search_state->fastmap[*search_state->outer_pos.pos])
			++search_state->outer_pos.pos;
		      if (*search_state->outer_pos.pos != search_state->fastmap_chr)
			return rx_fastmap_ok;
		      else 
			{
			  ++search_state->outer_pos.pos;
			  if (search_state->outer_pos.pos == search_state->outer_pos.end)
			    goto fastmap_hit_bound;
			}
		    }
		}
	    }
	  else
	    {
	      __const__ unsigned char * bound;
	      bound = search_state->outer_pos.string - 1;
	      if (search_state->fastmap_val)
		{
		  for (;;)
		    {
		      while (!search_state->fastmap[*search_state->outer_pos.pos])
			--search_state->outer_pos.pos;
		      return rx_fastmap_ok;
		    }
		}
	      else
		{
		  for (;;)
		    {
		      while (!search_state->fastmap[*search_state->outer_pos.pos])
			--search_state->outer_pos.pos;
		      if ((*search_state->outer_pos.pos != search_state->fastmap_chr) || search_state->fastmap_val)
			return rx_fastmap_ok;
		      else 
			{
			  --search_state->outer_pos.pos;
			  if (search_state->outer_pos.pos == bound)
			    goto fastmap_hit_bound;
			}
		    }
		}
	    }
	}
      
    case rx_fastmap_string_break:
    fastmap_hit_bound:
      {
	/* If we hit a bound, it may be time to fetch another burst
	 * of string, or it may be time to return a continuation to 
 	 * the caller, or it might be time to fail.
	 */

	int burst_state;
	burst_state = get_burst (&search_state->outer_pos, app_closure, stop);
	switch (burst_state)
	  {
	  default:
	  case rx_get_burst_error:
	    return rx_fastmap_error;
	  case rx_get_burst_continuation:
	    {
	      pc = rx_fastmap_string_break;
	      goto return_continuation;
	    }
	  case rx_get_burst_ok:
	    goto init_fastmap_sentinal;
	  case rx_get_burst_no_more:
	    /* ...not a string split, simply no more string. 
	     *
	     * When searching backward, running out of string
	     * is reason to quit.
	     *
	     * When searching forward, we allow the possibility
	     * of an (empty) match after the last character in the
	     * virtual string.  So, fall through to the matcher
	     */
	    return (  (search_state->outer_pos.search_direction == 1)
		    ? rx_fastmap_ok
		    : rx_fastmap_fail);
	  }
      }
    }

}



#ifdef emacs
/* The `emacs' switch turns on certain matching commands
 * that make sense only in Emacs. 
 */
#include "config.h"
#include "lisp.h"
#include "buffer.h"
#include "syntax.h"
#endif /* emacs */

/* Setting RX_MEMDBUG is useful if you have dbmalloc.  Maybe with similar
 * packages too.
 */
#ifdef RX_MEMDBUG
#include <malloc.h>
#endif /* RX_RX_MEMDBUG */

/* We used to test for `BSTRING' here, but only GCC and Emacs define
 * `BSTRING', as far as I know, and neither of them use this code.  
 */
#if HAVE_STRING_H || STDC_HEADERS
#include <string.h>

#ifndef bcmp
#define bcmp(s1, s2, n)	memcmp ((s1), (s2), (n))
#endif

#ifndef bcopy
#define bcopy(s, d, n)	memcpy ((d), (s), (n))
#endif

#ifndef bzero
#define bzero(s, n)	memset ((s), 0, (n))
#endif

#else /*  HAVE_STRING_H || STDC_HEADERS */
#include <strings.h>
#endif   /* not (HAVE_STRING_H || STDC_HEADERS) */

#ifdef STDC_HEADERS
#include <stdlib.h>
#else /* not STDC_HEADERS */
char *malloc ();
char *realloc ();
#endif /* not STDC_HEADERS */




/* How many characters in the character set.  */
#define CHAR_SET_SIZE (1 << CHARBITS)

#ifndef emacs
/* Define the syntax basics for \<, \>, etc.
 * This must be nonzero for the wordchar and notwordchar pattern
 * commands in re_match_2.
 */
#ifndef Sword 
#define Sword 1
#endif
#define SYNTAX(c) re_syntax_table[c]
RX_DECL char re_syntax_table[CHAR_SET_SIZE];
#endif /* not emacs */


/* Test if at very beginning or at very end of the virtual concatenation
 *  of `string1' and `string2'.  If only one string, it's `string2'.  
 */

#define AT_STRINGS_BEG() \
  (   -1		 \
   == ((search_state.test_pos.pos - search_state.test_pos.string) \
       + search_state.test_pos.offset))

#define AT_STRINGS_END() \
  (   (total_size - 1)	 \
   == ((search_state.test_pos.pos - search_state.test_pos.string) \
       + search_state.test_pos.offset))


/* Test if POS + 1 points to a character which is word-constituent.  We have
 * two special cases to check for: if past the end of string1, look at
 * the first character in string2; and if before the beginning of
 * string2, look at the last character in string1.
 *
 * Assumes `string1' exists, so use in conjunction with AT_STRINGS_BEG ().  
 */
#define LETTER_P(POS,OFF)						\
  (   SYNTAX (fetch_char(POS, OFF, app_closure, stop))			\
   == Sword)

/* Test if the character at D and the one after D differ with respect
 * to being word-constituent.  
 */
#define AT_WORD_BOUNDARY(d)						\
  (AT_STRINGS_BEG () || AT_STRINGS_END () || LETTER_P (d,0) != LETTER_P (d, 1))


#ifdef RX_SUPPORT_CONTINUATIONS
#define RX_STACK_ALLOC(BYTES) malloc(BYTES)
#define RX_STACK_FREE(MEM) free(MEM)
#else
#define RX_STACK_ALLOC(BYTES) alloca(BYTES)
#define RX_STACK_FREE(MEM) \
      ((struct rx_stack_chunk *)MEM)->next_chunk = search_state.free_chunks; \
      search_state.free_chunks = ((struct rx_stack_chunk *)MEM);

#endif

#define PUSH(CHUNK_VAR,BYTES)   \
  if (!CHUNK_VAR || (CHUNK_VAR->bytes_left < (BYTES)))  \
    {					\
      struct rx_stack_chunk * new_chunk;	\
      if (search_state.free_chunks)			\
	{				\
	  new_chunk = search_state.free_chunks;	\
	  search_state.free_chunks = search_state.free_chunks->next_chunk; \
	}				\
      else				\
	{				\
	  new_chunk = (struct rx_stack_chunk *)RX_STACK_ALLOC(search_state.chunk_bytes); \
	  if (!new_chunk)		\
	    {				\
	      search_state.ret_val = 0;		\
	      goto test_do_return;	\
	    }				\
	}				\
      new_chunk->sp = (char *)new_chunk + sizeof (struct rx_stack_chunk); \
      new_chunk->bytes_left = (search_state.chunk_bytes \
			       - (BYTES) \
			       - sizeof (struct rx_stack_chunk)); \
      new_chunk->next_chunk = CHUNK_VAR; \
      CHUNK_VAR = new_chunk;		\
    } \
  else \
    (CHUNK_VAR->sp += (BYTES)), (CHUNK_VAR->bytes_left -= (BYTES))

#define POP(CHUNK_VAR,BYTES) \
  if (CHUNK_VAR->sp == ((char *)CHUNK_VAR + sizeof(*CHUNK_VAR))) \
    { \
      struct rx_stack_chunk * new_chunk = CHUNK_VAR->next_chunk; \
      RX_STACK_FREE(CHUNK_VAR); \
      CHUNK_VAR = new_chunk; \
    } \
  else \
    (CHUNK_VAR->sp -= BYTES), (CHUNK_VAR->bytes_left += BYTES)



#define SRCH_TRANSLATE(C)  search_state.translate[(unsigned char) (C)]




#ifdef __STDC__
RX_DECL __inline__ int
rx_search  (struct re_pattern_buffer * rxb,
	    int startpos,
	    int range,
	    int stop,
	    int total_size,
	    rx_get_burst_fn get_burst,
	    rx_back_check_fn back_check,
	    rx_fetch_char_fn fetch_char,
	    void * app_closure,
	    struct re_registers * regs,
	    struct rx_search_state * resume_state,
	    struct rx_search_state * save_state)
#else
RX_DECL __inline__ int
rx_search  (rxb, startpos, range, stop, total_size,
	    get_burst, back_check, fetch_char,
	    app_closure, regs, resume_state, save_state)
     struct re_pattern_buffer * rxb;
     int startpos;
     int range;
     int stop;
     int total_size;
     rx_get_burst_fn get_burst;
     rx_back_check_fn back_check;
     rx_fetch_char_fn fetch_char;
     void * app_closure;
     struct re_registers * regs;
     struct rx_search_state * resume_state;
     struct rx_search_state * save_state;
#endif
{
  int pc;
  int test_state;
  struct rx_search_state search_state;

  search_state.free_chunks = 0;
  if (!resume_state)
    pc = rx_outer_start;
  else
    {
      search_state = *resume_state;
      regs = search_state.saved_regs;
      rxb = search_state.saved_rxb;
      startpos = search_state.saved_startpos;
      range = search_state.saved_range;
      stop = search_state.saved_stop;
      total_size = search_state.saved_total_size;
      get_burst = search_state.saved_get_burst;
      back_check = search_state.saved_back_check;
      pc = search_state.outer_search_resume_pt;
      if (0)
	{
	return_continuation:
	  if (save_state)
	    {
	      *save_state = search_state;
	      save_state->saved_regs = regs;
	      save_state->saved_rxb = rxb;
	      save_state->saved_startpos = startpos;
	      save_state->saved_range = range;
	      save_state->saved_stop = stop;
	      save_state->saved_total_size = total_size;
	      save_state->saved_get_burst = get_burst;
	      save_state->saved_back_check = back_check;
	      save_state->outer_search_resume_pt = pc;
	    }
	  return rx_search_continuation;
	}
    }

  switch (pc)
    {
    case rx_outer_start:
      search_state.ret_val = rx_search_fail;
      (  search_state.lparen
       = search_state.rparen
       = search_state.best_lpspace
       = search_state.best_rpspace
       = 0);
      
      /* figure the number of registers we may need for use in backreferences.
       * the number here includes an element for register zero.  
       */
      search_state.num_regs = rxb->re_nsub + 1;
      
      
      /* check for out-of-range startpos.  */
      if ((startpos < 0) || (startpos > total_size))
	return rx_search_fail;
      
      /* fix up range if it might eventually take us outside the string. */
      {
	int endpos;
	endpos = startpos + range;
	if (endpos < -1)
	  range = (-1 - startpos);
	else if (endpos > (total_size + 1))
	  range = total_size - startpos;
      }
      
      /* if the search isn't to be a backwards one, don't waste time in a
       * long search for a pattern that says it is anchored.
       */
      if (rxb->begbuf_only && (range > 0))
	{
	  if (startpos > 0)
	    return rx_search_fail;
	  else
	    range = 1;
	}
      
      /* decide whether to use internal or user-provided reg buffers. */
      if (!regs || rxb->no_sub)
	{
	  search_state.best_lpspace =
	    (regoff_t *)REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t));
	  search_state.best_rpspace =
	    (regoff_t *)REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t));
	  search_state.best_lparen = search_state.best_lpspace;
	  search_state.best_rparen = search_state.best_rpspace;
	}
      else
	{	
	  /* have the register data arrays been allocated?  */
	  if (rxb->regs_allocated == REGS_UNALLOCATED)
	    { /* no.  so allocate them with malloc.  we need one
		 extra element beyond `search_state.num_regs' for the `-1' marker
		 gnu code uses.  */
	      regs->num_regs = MAX (RE_NREGS, rxb->re_nsub + 1);
	      regs->start = ((regoff_t *)
			     malloc (regs->num_regs * sizeof ( regoff_t)));
	      regs->end = ((regoff_t *)
			   malloc (regs->num_regs * sizeof ( regoff_t)));
	      if (regs->start == 0 || regs->end == 0)
		return rx_search_error;
	      rxb->regs_allocated = REGS_REALLOCATE;
	    }
	  else if (rxb->regs_allocated == REGS_REALLOCATE)
	    { /* yes.  if we need more elements than were already
		 allocated, reallocate them.  if we need fewer, just
		 leave it alone.  */
	      if (regs->num_regs < search_state.num_regs + 1)
		{
		  regs->num_regs = search_state.num_regs + 1;
		  regs->start = ((regoff_t *)
				 realloc (regs->start,
					  regs->num_regs * sizeof (regoff_t)));
		  regs->end = ((regoff_t *)
			       realloc (regs->end,
					regs->num_regs * sizeof ( regoff_t)));
		  if (regs->start == 0 || regs->end == 0)
		    return rx_search_error;
		}
	    }
	  else if (rxb->regs_allocated != REGS_FIXED)
	    return rx_search_error;
	  
	  if (regs->num_regs < search_state.num_regs + 1)
	    {
	      search_state.best_lpspace =
		((regoff_t *)
		 REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t)));
	      search_state.best_rpspace =
		((regoff_t *)
		 REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t)));
	      search_state.best_lparen = search_state.best_lpspace;
	      search_state.best_rparen = search_state.best_rpspace;
	    }
	  else
	    {
	      search_state.best_lparen = regs->start;
	      search_state.best_rparen = regs->end;
	    }
	}
      
      search_state.lparen =
	(regoff_t *) REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t));
      search_state.rparen =
	(regoff_t *) REGEX_ALLOCATE (search_state.num_regs * sizeof(regoff_t)); 
      
      if (! (   search_state.best_rparen
	     && search_state.best_lparen
	     && search_state.lparen && search_state.rparen))
	return rx_search_error;
      
      search_state.best_last_l = search_state.best_last_r = -1;
      
      search_state.translate = (rxb->translate
				? rxb->translate
				: rx_id_translation);
      
      
      
      /*
       * two nfa's were compiled.  
       * `0' is complete.
       * `1' faster but gets registers wrong and ends too soon.
       */
      search_state.nfa_choice = (regs && !rxb->least_subs) ? '\0' : '\1';
      
      /* we have the option to look for the best match or the first
       * one we can find.  if the user isn't asking for register information,
       * we don't need to find the best match.
       */
      search_state.first_found = !regs;
      
      if (range >= 0)
	{
	  search_state.outer_pos.search_end = startpos + range;
	  search_state.outer_pos.search_direction = 1;
	}
      else
	{
	  search_state.outer_pos.search_end = startpos + range;
	  search_state.outer_pos.search_direction = -1;
	}
      
      /* the vacuous search always turns up nothing. */
      if ((search_state.outer_pos.search_direction == 1)
	  ? (startpos > search_state.outer_pos.search_end)
	  : (startpos < search_state.outer_pos.search_end))
	return rx_search_fail;
      
      /* now we build the starting state of the supernfa. */
      {
	struct rx_superset * start_contents;
	struct rx_nfa_state_set * start_nfa_set;
	
	/* we presume here that the nfa start state has only one
	 * possible future with no side effects.  
	 */
	start_nfa_set = rxb->start->futures->destset;
	if (   rxb->rx.start_set
	    && (rxb->rx.start_set->starts_for == &rxb->rx))
	  start_contents = rxb->rx.start_set;
	else
	  {
	    start_contents =
	      rx_superstate_eclosure_union (&rxb->rx,
					    rx_superset_cons (&rxb->rx, 0, 0),
					    start_nfa_set);
	    
	    if (!start_contents)
	      return rx_search_fail;
	    
	    start_contents->starts_for = &rxb->rx;
	    rxb->rx.start_set = start_contents;
	  }
	if (   start_contents->superstate
	    && (start_contents->superstate->rx_id == rxb->rx.rx_id))
	  {
	    search_state.start_super = start_contents->superstate;
	    rx_lock_superstate (&rxb->rx, search_state.start_super);
	  }
	else
	  {
	    rx_protect_superset (&rxb->rx, start_contents);
	    
	    search_state.start_super = rx_superstate (&rxb->rx, start_contents);
	    if (!search_state.start_super)
	      return rx_search_fail;
	    rx_lock_superstate (&rxb->rx, search_state.start_super);
	    rx_release_superset (&rxb->rx, start_contents);
	  }
      }
      
      
      /* The outer_pos tracks the position within the strings
       * as seen by loop that calls fastmap_search.
       *
       * The caller supplied get_burst function actually 
       * gives us pointers to chars.
       * 
       * Communication with the get_burst function is through an
       * rx_string_position structure.  Here, the structure for
       * outer_pos is initialized.   It is set to point to the
       * NULL string, at an offset of STARTPOS.  STARTPOS is out
       * of range of the NULL string, so the first call to 
       * getburst will patch up the rx_string_position to point
       * to valid characters.
       */

      (  search_state.outer_pos.string
       = search_state.outer_pos.end
       = 0);

      search_state.outer_pos.offset = 0;
      search_state.outer_pos.size = 0;
      search_state.outer_pos.pos = (unsigned char *)startpos;
      init_fastmap (rxb, &search_state);

      search_state.fastmap_resume_pt = rx_fastmap_start;
    case rx_outer_fastmap:
      /* do { */
    pseudo_do:
      {
	{
	  int fastmap_state;
	  fastmap_state = fastmap_search (rxb, stop, get_burst, app_closure,
					  &search_state);
	  switch (fastmap_state)
	    {
	    case rx_fastmap_continuation:
	      pc = rx_outer_fastmap;
	      goto return_continuation;
	    case rx_fastmap_fail:
	      goto finish;
	    case rx_fastmap_ok:
	      break;
	    }
	}
	
	/* now the fastmap loop has brought us to a plausible 
	 * starting point for a match.  so, it's time to run the
	 * nfa and see if a match occured.
	 */
	startpos = (  search_state.outer_pos.pos
		    - search_state.outer_pos.string
		    + search_state.outer_pos.offset);
#if 0
/*|*/	if ((range > 0) && (startpos == search_state.outer_pos.search_end))
/*|*/	  goto finish;
#endif
      }

      search_state.test_match_resume_pt = rx_test_start;
      /* do interrupted for entry point... */
    case rx_outer_test:
      /* ...do continued */
      {
	goto test_match;
      test_returns_to_search:
	switch (test_state)
	  {
	  case rx_test_continuation:
	    pc = rx_outer_test;
	    goto return_continuation;
	  case rx_test_error:
	    search_state.ret_val = rx_search_error;
	    goto finish;
	  case rx_test_fail:
	    break;
	  case rx_test_ok:
	    goto finish;
	  }
	search_state.outer_pos.pos += search_state.outer_pos.search_direction;
	startpos += search_state.outer_pos.search_direction;
#if 0
/*|*/	if (search_state.test_pos.pos < search_state.test_pos.end)
/*|*/	  break;
#endif
      }
      /* do interrupted for entry point... */
    case rx_outer_restore_pos:
      {
	int x;
	x = get_burst (&search_state.outer_pos, app_closure, stop);
	switch (x)
	  {
	  case rx_get_burst_continuation:
	    pc = rx_outer_restore_pos;
	    goto return_continuation;
	  case rx_get_burst_error:
	    search_state.ret_val = rx_search_error;
	    goto finish;
	  case rx_get_burst_no_more:
	    if (rxb->can_match_empty)
	      break;
	    goto finish;
	  case rx_get_burst_ok:
	    break;
	  }
      } /* } while (...see below...) */

      if ((search_state.outer_pos.search_direction == 1)
	  ? (startpos <= search_state.outer_pos.search_end)
	  : (startpos > search_state.outer_pos.search_end))
	goto pseudo_do;

	
    finish:
      uninit_fastmap (rxb, &search_state);
      if (search_state.start_super)
	rx_unlock_superstate (&rxb->rx, search_state.start_super);
      
#ifdef regex_malloc
      if (search_state.lparen) free (search_state.lparen);
      if (search_state.rparen) free (search_state.rparen);
      if (search_state.best_lpspace) free (search_state.best_lpspace);
      if (search_state.best_rpspace) free (search_state.best_rpspace);
#endif
      return search_state.ret_val;
    }


 test_match:
  {
    enum rx_test_match_entry test_pc;
    int inx;
    test_pc = search_state.test_match_resume_pt;
    if (test_pc == rx_test_start)
      {
#ifdef RX_DEBUG
	search_state.backtrack_depth = 0;
#endif
	search_state.last_l = search_state.last_r = 0;
	search_state.lparen[0] = startpos;
	search_state.super = search_state.start_super;
	search_state.c = search_state.nfa_choice;
	search_state.test_pos.pos = search_state.outer_pos.pos - 1;    
	search_state.test_pos.string = search_state.outer_pos.string;
	search_state.test_pos.end = search_state.outer_pos.end;
	search_state.test_pos.offset = search_state.outer_pos.offset;
	search_state.test_pos.size = search_state.outer_pos.size;
	search_state.test_pos.search_direction = 1;
	search_state.counter_stack = 0;
	search_state.backtrack_stack = 0;
	search_state.backtrack_frame_bytes =
	  (sizeof (struct rx_backtrack_frame)
	   + (rxb->match_regs_on_stack
	      ? sizeof (regoff_t) * (search_state.num_regs + 1) * 2
	      : 0));
	search_state.chunk_bytes = search_state.backtrack_frame_bytes * 64;
	search_state.test_ret = rx_test_line_finished;
	search_state.could_have_continued = 0;
      }  
    /* This is while (1)...except that the body of the loop is interrupted 
     * by some alternative entry points.
     */
  pseudo_while_1:
    switch (test_pc)
      {
      case rx_test_cache_hit_loop:
	goto resume_continuation_1;
      case rx_test_backreference_check:
	goto resume_continuation_2;
      case rx_test_backtrack_return:
	goto resume_continuation_3;
      case rx_test_start:
#ifdef RX_DEBUG
	/* There is a search tree with every node as set of deterministic
	 * transitions in the super nfa.  For every branch of a 
	 * backtrack point is an edge in the tree.
	 * This counts up a pre-order of nodes in that tree.
	 * It's saved on the search stack and printed when debugging. 
	 */
	search_state.line_no = 0;
	search_state.lines_found = 0;
#endif
	
      top_of_cycle:
	/* A superstate is basicly a transition table, indexed by 
	 * characters from the string being tested, and containing 
	 * RX_INX (`instruction frame') structures.
	 */
	search_state.ifr = &search_state.super->transitions [search_state.c];
	
      recurse_test_match:
	/* This is the point to which control is sent when the
	 * test matcher `recurses'.  Before jumping here, some variables
	 * need to be saved on the stack and the next instruction frame
	 * has to be computed.
	 */
	
      restart:
	/* Some instructions don't advance the matcher, but just
	 * carry out some side effects and fetch a new instruction.
	 * To dispatch that new instruction, `goto restart'.
	 */
	
	{
	  struct rx_inx * next_tr_table;
	  struct rx_inx * this_tr_table;
	  /* The fastest route through the loop is when the instruction 
	   * is RX_NEXT_CHAR.  This case is detected when SEARCH_STATE.IFR->DATA
	   * is non-zero.  In that case, it points to the next
	   * superstate. 
	   *
	   * This allows us to not bother fetching the bytecode.
	   */
	  next_tr_table = (struct rx_inx *)search_state.ifr->data;
	  this_tr_table = search_state.super->transitions;
	  while (next_tr_table)
	    {
#ifdef RX_DEBUG_0
	      if (rx_debug_trace)
		{
		  struct rx_superset * setp;
		  
		  fprintf (stderr, "%d %d>> re_next_char @ %d (%d)",
			   search_state.line_no,
			   search_state.backtrack_depth,
			   (search_state.test_pos.pos - search_state.test_pos.string
			    + search_state.test_pos.offset), search_state.c);
		  
		  search_state.super =
		    ((struct rx_superstate *)
		     ((char *)this_tr_table
		      - ((unsigned long)
			 ((struct rx_superstate *)0)->transitions)));
		  
		  setp = search_state.super->contents;
		  fprintf (stderr, "   superstet (rx=%d, &=%x: ",
			   rxb->rx.rx_id, setp);
		  while (setp)
		    {
		      fprintf (stderr, "%d ", setp->id);
		      setp = setp->cdr;
		    }
		  fprintf (stderr, "\n");
		}
#endif
	      this_tr_table = next_tr_table;
	      ++search_state.test_pos.pos;
	      if (search_state.test_pos.pos == search_state.test_pos.end)
		{
		  int burst_state;
		try_burst_1:
		  burst_state = get_burst (&search_state.test_pos,
					   app_closure, stop);
		  switch (burst_state)
		    {
		    case rx_get_burst_continuation:
		      search_state.saved_this_tr_table = this_tr_table;
		      search_state.saved_next_tr_table = next_tr_table;
		      test_pc = rx_test_cache_hit_loop;
		      goto test_return_continuation;
		      
		    resume_continuation_1:
		      /* Continuation one jumps here to do its work: */
		      search_state.saved_this_tr_table = this_tr_table;
		      search_state.saved_next_tr_table = next_tr_table;
		      goto try_burst_1;
		      
		    case rx_get_burst_ok:
		      /* get_burst succeeded...keep going */
		      break;
		      
		    case rx_get_burst_no_more:
		      search_state.test_ret = rx_test_line_finished;
		      search_state.could_have_continued = 1;
		      goto test_do_return;
		      
		    case rx_get_burst_error:
		      /* An error... */
		      search_state.test_ret = rx_test_internal_error;
		      goto test_do_return;
		    }
		}
	      search_state.c = *search_state.test_pos.pos;
	      search_state.ifr = this_tr_table + search_state.c;
	      next_tr_table = (struct rx_inx *)search_state.ifr->data;
	    } /* Fast loop through cached transition tables */
	  
	  /* Here when we ran out of cached next-char transitions. 
	   * So, it will be necessary to do a more expensive
	   * dispatch on the current instruction.  The superstate
	   * pointer is allowed to become invalid during next-char
	   * transitions -- now we must bring it up to date.
	   */
	  search_state.super =
	    ((struct rx_superstate *)
	     ((char *)this_tr_table
	      - ((unsigned long)
		 ((struct rx_superstate *)0)->transitions)));
	}
	
	/* We've encountered an instruction other than next-char.
	 * Dispatch that instruction:
	 */
	inx = (int)search_state.ifr->inx;
#ifdef RX_DEBUG_0
	if (rx_debug_trace)
	  {
	    struct rx_superset * setp = search_state.super->contents;
	    
	    fprintf (stderr, "%d %d>> %s @ %d (%d)", search_state.line_no,
		     search_state.backtrack_depth,
		     inx_names[inx],
		     (search_state.test_pos.pos - search_state.test_pos.string
		      + (test_pos.half == 0 ? 0 : size1)), search_state.c);
	    
	    fprintf (stderr, "   superstet (rx=%d, &=%x: ",
		     rxb->rx.rx_id, setp);
	    while (setp)
	      {
		fprintf (stderr, "%d ", setp->id);
		setp = setp->cdr;
	      }
	    fprintf (stderr, "\n");
	  }
#endif
	switch ((enum rx_opcode)inx)
	  {
	  case rx_do_side_effects:
	    
	    /*  RX_DO_SIDE_EFFECTS occurs when we cross epsilon 
	     *  edges associated with parentheses, backreferencing, etc.
	     */
	    {
	      struct rx_distinct_future * df =
		(struct rx_distinct_future *)search_state.ifr->data_2;
	      struct rx_se_list * el = df->effects;
	      /* Side effects come in lists.  This walks down
	       * a list, dispatching.
	       */
	      while (el)
		{
		  long effect;
		  effect = (long)el->car;
		  if (effect < 0)
		    {
#ifdef RX_DEBUG_0
		      if (rx_debug_trace)
			{
			  struct rx_superset * setp = search_state.super->contents;
			  
			  fprintf (stderr, "....%d %d>> %s\n", search_state.line_no,
				   search_state.backtrack_depth,
				   efnames[-effect]);
			}
#endif
		      switch ((enum re_side_effects) effect)

			{
			case re_se_pushback:
			  search_state.ifr = &df->future_frame;
			  if (!search_state.ifr->data)
			    {
			      struct rx_superstate * sup;
			      sup = search_state.super;
			      rx_lock_superstate (rx, sup);
			      if (!rx_handle_cache_miss (&rxb->rx,
							 search_state.super,
							 search_state.c,
							 (search_state.ifr
							  ->data_2)))
				{
				  rx_unlock_superstate (rx, sup);
				  search_state.test_ret = rx_test_internal_error;
				  goto test_do_return;
				}
			      rx_unlock_superstate (rx, sup);
			    }
			  /* --search_state.test_pos.pos; */
			  search_state.c = 't';
			  search_state.super
			    = ((struct rx_superstate *)
			       ((char *)search_state.ifr->data
				- (long)(((struct rx_superstate *)0)
					 ->transitions)));
			  goto top_of_cycle;
			  break;
			case re_se_push0:
			  {
			    struct rx_counter_frame * old_cf
			      = (search_state.counter_stack
				 ? ((struct rx_counter_frame *)
				    search_state.counter_stack->sp)
				 : 0);
			    struct rx_counter_frame * cf;
			    PUSH (search_state.counter_stack,
				  sizeof (struct rx_counter_frame));
			    cf = ((struct rx_counter_frame *)
				  search_state.counter_stack->sp);
			    cf->tag = re_se_iter;
			    cf->val = 0;
			    cf->inherited_from = 0;
			    cf->cdr = old_cf;
			    break;
			  }
			case re_se_fail:
			  goto test_do_return;
			case re_se_begbuf:
			  if (!AT_STRINGS_BEG ())
			    goto test_do_return;
			  break;
			case re_se_endbuf:
			  if (!AT_STRINGS_END ())
			    goto test_do_return;
			  break;
			case re_se_wordbeg:
			  if (   LETTER_P (&search_state.test_pos, 1)
			      && (   AT_STRINGS_BEG()
				  || !LETTER_P (&search_state.test_pos, 0)))
			    break;
			  else
			    goto test_do_return;
			case re_se_wordend:
			  if (   !AT_STRINGS_BEG ()
			      && LETTER_P (&search_state.test_pos, 0)
			      && (AT_STRINGS_END ()
				  || !LETTER_P (&search_state.test_pos, 1)))
			    break;
			  else
			    goto test_do_return;
			case re_se_wordbound:
			  if (AT_WORD_BOUNDARY (&search_state.test_pos))
			    break;
			  else
			    goto test_do_return;
			case re_se_notwordbound:
			  if (!AT_WORD_BOUNDARY (&search_state.test_pos))
			    break;
			  else
			    goto test_do_return;
			case re_se_hat:
			  if (AT_STRINGS_BEG ())
			    {
			      if (rxb->not_bol)
				goto test_do_return;
			      else
				break;
			    }
			  else
			    {
			      char pos_c = *search_state.test_pos.pos;
			      if (   (SRCH_TRANSLATE (pos_c)
				      == SRCH_TRANSLATE('\n'))
				  && rxb->newline_anchor)
				break;
			      else
				goto test_do_return;
			    }
			case re_se_dollar:
			  if (AT_STRINGS_END ())
			    {
			      if (rxb->not_eol)
				goto test_do_return;
			      else
				break;
			    }
			  else
			    {
			      if (   (   SRCH_TRANSLATE (fetch_char
						    (&search_state.test_pos, 1,
						     app_closure, stop))
				      == SRCH_TRANSLATE ('\n'))
				  && rxb->newline_anchor)
				break;
			      else
				goto test_do_return;
			    }
			  
			case re_se_try:
			  /* This is the first side effect in every
			   * expression.
			   *
			   *  FOR NO GOOD REASON...get rid of it...
			   */
			  break;
			  
			case re_se_pushpos:
			  {
			    int urhere =
			      ((int)(search_state.test_pos.pos
				     - search_state.test_pos.string)
			       + search_state.test_pos.offset);
			    struct rx_counter_frame * old_cf
			      = (search_state.counter_stack
				 ? ((struct rx_counter_frame *)
				    search_state.counter_stack->sp)
				 : 0);
			    struct rx_counter_frame * cf;
			    PUSH(search_state.counter_stack,
				 sizeof (struct rx_counter_frame));
			    cf = ((struct rx_counter_frame *)
				  search_state.counter_stack->sp);
			    cf->tag = re_se_pushpos;
			    cf->val = urhere;
			    cf->inherited_from = 0;
			    cf->cdr = old_cf;
			    break;
			  }
			  
			case re_se_chkpos:
			  {
			    int urhere =
			      ((int)(search_state.test_pos.pos
				     - search_state.test_pos.string)
			       + search_state.test_pos.offset);
			    struct rx_counter_frame * cf
			      = ((struct rx_counter_frame *)
				 search_state.counter_stack->sp);
			    if (cf->val == urhere)
			      goto test_do_return;
			    cf->val = urhere;
			    break;
			  }
			  break;
			  
			case re_se_poppos:
			  POP(search_state.counter_stack,
			      sizeof (struct rx_counter_frame));
			  break;
			  
			  
			case re_se_at_dot:
			case re_se_syntax:
			case re_se_not_syntax:
#ifdef emacs
			  /* 
			   * this release lacks emacs support
			   */
#endif
			  break;
			case re_se_win:
			case re_se_lparen:
			case re_se_rparen:
			case re_se_backref:
			case re_se_iter:
			case re_se_end_iter:
			case re_se_tv:
			case re_floogle_flap:
			  search_state.ret_val = 0;
			  goto test_do_return;
			}
		    }
		  else
		    {
#ifdef RX_DEBUG_0
		      if (rx_debug_trace)
			fprintf (stderr, "....%d %d>> %s %d %d\n", search_state.line_no,
				 search_state.backtrack_depth,
				 efnames2[rxb->se_params [effect].se],
				 rxb->se_params [effect].op1,
				 rxb->se_params [effect].op2);
#endif
		      switch (rxb->se_params [effect].se)
			{
			case re_se_win:
			  /* This side effect indicates that we've 
			   * found a match, though not necessarily the 
			   * best match.  This is a fancy assignment to 
			   * register 0 unless the caller didn't 
			   * care about registers.  In which case,
			   * this stops the match.
			   */
			  {
			    int urhere =
			      ((int)(search_state.test_pos.pos
				     - search_state.test_pos.string)
			       + search_state.test_pos.offset);
			    
			    if (   (search_state.best_last_r < 0)
				|| (urhere + 1 > search_state.best_rparen[0]))
			      {
				/* Record the best known and keep
				 * looking.
				 */
				int x;
				for (x = 0; x <= search_state.last_l; ++x)
				  search_state.best_lparen[x] = search_state.lparen[x];
				search_state.best_last_l = search_state.last_l;
				for (x = 0; x <= search_state.last_r; ++x)
				  search_state.best_rparen[x] = search_state.rparen[x];
				search_state.best_rparen[0] = urhere + 1;
				search_state.best_last_r = search_state.last_r;
			      }
			    /* If we're not reporting the match-length 
			     * or other register info, we need look no
			     * further.
			     */
			    if (search_state.first_found)
			      {
				search_state.test_ret = rx_test_found_first;
				goto test_do_return;
			      }
			  }
			  break;
			case re_se_lparen:
			  {
			    int urhere =
			      ((int)(search_state.test_pos.pos
				     - search_state.test_pos.string)
			       + search_state.test_pos.offset);
			    
			    int reg = rxb->se_params [effect].op1;
#if 0
			    if (reg > search_state.last_l)
#endif
			      {
				search_state.lparen[reg] = urhere + 1;
				/* In addition to making this assignment,
				 * we now know that lower numbered regs
				 * that haven't already been assigned,
				 * won't be.  We make sure they're
				 * filled with -1, so they can be
				 * recognized as unassigned.
				 */
				if (search_state.last_l < reg)
				  while (++search_state.last_l < reg)
				    search_state.lparen[search_state.last_l] = -1;
			      }
			    break;
			  }
			  
			case re_se_rparen:
			  {
			    int urhere =
			      ((int)(search_state.test_pos.pos
				     - search_state.test_pos.string)
			       + search_state.test_pos.offset);
			    int reg = rxb->se_params [effect].op1;
			    search_state.rparen[reg] = urhere + 1;
			    if (search_state.last_r < reg)
			      {
				while (++search_state.last_r < reg)
				  search_state.rparen[search_state.last_r]
				    = -1;
			      }
			    break;
			  }
			  
			case re_se_backref:
			  {
			    int reg = rxb->se_params [effect].op1;
			    if (   reg > search_state.last_r
				|| search_state.rparen[reg] < 0)
			      goto test_do_return;
			    
			    {
			      int backref_status;
			    check_backreference:
			      backref_status
				= back_check (&search_state.test_pos,
					      search_state.lparen[reg],
					      search_state.rparen[reg],
					      search_state.translate,
					      app_closure,
					      stop);
			      switch (backref_status)
				{
				case rx_back_check_continuation:
				  search_state.saved_reg = reg;
				  test_pc = rx_test_backreference_check;
				  goto test_return_continuation;
				resume_continuation_2:
				  reg = search_state.saved_reg;
				  goto check_backreference;
				case rx_back_check_fail:
				  /* Fail */
				  goto test_do_return;
				case rx_back_check_pass:
				  /* pass --
				   * test_pos now advanced to last
				   * char matched by backref
				   */
				  break;
				}
			    }
			    break;
			  }
			case re_se_iter:
			  {
			    struct rx_counter_frame * csp
			      = ((struct rx_counter_frame *)
				 search_state.counter_stack->sp);
			    if (csp->val == rxb->se_params[effect].op2)
			      goto test_do_return;
			    else
			      ++csp->val;
			    break;
			  }
			case re_se_end_iter:
			  {
			    struct rx_counter_frame * csp
			      = ((struct rx_counter_frame *)
				 search_state.counter_stack->sp);
			    if (csp->val < rxb->se_params[effect].op1)
			      goto test_do_return;
			    else
			      {
				struct rx_counter_frame * source = csp;
				while (source->inherited_from)
				  source = source->inherited_from;
				if (!source || !source->cdr)
				  {
				    POP(search_state.counter_stack,
					sizeof(struct rx_counter_frame));
				  }
				else
				  {
				    source = source->cdr;
				    csp->val = source->val;
				    csp->tag = source->tag;
				    csp->cdr = 0;
				    csp->inherited_from = source;
				  }
			      }
			    break;
			  }
			case re_se_tv:
			  /* is a noop */
			  break;
			case re_se_try:
			case re_se_pushback:
			case re_se_push0:
			case re_se_pushpos:
			case re_se_chkpos:
			case re_se_poppos:
			case re_se_at_dot:
			case re_se_syntax:
			case re_se_not_syntax:
			case re_se_begbuf:
			case re_se_hat:
			case re_se_wordbeg:
			case re_se_wordbound:
			case re_se_notwordbound:
			case re_se_wordend:
			case re_se_endbuf:
			case re_se_dollar:
			case re_se_fail:
			case re_floogle_flap:
			  search_state.ret_val = 0;
			  goto test_do_return;
			}
		    }
		  el = el->cdr;
		}
	      /* Now the side effects are done,
	       * so get the next instruction.
	       * and move on.
	       */
	      search_state.ifr = &df->future_frame;
	      goto restart;
	    }
	    
	  case rx_backtrack_point:
	    {
	      /* A backtrack point indicates that we've reached a
	       * non-determinism in the superstate NFA.  This is a
	       * loop that exhaustively searches the possibilities.
	       *
	       * A backtracking strategy is used.  We keep track of what
	       * registers are valid so we can erase side effects.
	       *
	       * First, make sure there is some stack space to hold 
	       * our state.
	       */
	      
	      struct rx_backtrack_frame * bf;
	      
	      PUSH(search_state.backtrack_stack,
		   search_state.backtrack_frame_bytes);
#ifdef RX_DEBUG_0
	      ++search_state.backtrack_depth;
#endif
	      
	      bf = ((struct rx_backtrack_frame *)
		    search_state.backtrack_stack->sp);
	      {
		bf->stk_super = search_state.super;
		/* We prevent the current superstate from being
		 * deleted from the superstate cache.
		 */
		rx_lock_superstate (&rxb->rx, search_state.super);
#ifdef RX_DEBUG_0
		bf->stk_search_state.line_no = search_state.line_no;
#endif
		bf->stk_c = search_state.c;
		bf->stk_test_pos = search_state.test_pos;
		bf->stk_last_l = search_state.last_l;
		bf->stk_last_r = search_state.last_r;
		bf->df = ((struct rx_super_edge *)
			  search_state.ifr->data_2)->options;
		bf->first_df = bf->df;
		bf->counter_stack_sp = (search_state.counter_stack
					? search_state.counter_stack->sp
					: 0);
		bf->stk_test_ret = search_state.test_ret;
		if (rxb->match_regs_on_stack)
		  {
		    int x;
		    regoff_t * stk =
		      (regoff_t *)((char *)bf + sizeof (*bf));
		    for (x = 0; x <= search_state.last_l; ++x)
		      stk[x] = search_state.lparen[x];
		    stk += x;
		    for (x = 0; x <= search_state.last_r; ++x)
		      stk[x] = search_state.rparen[x];
		  }
	      }
	      
	      /* Here is a while loop whose body is mainly a function
	       * call and some code to handle a return from that
	       * function.
	       *
	       * From here on for the rest of `case backtrack_point' it
	       * is unsafe to assume that the search_state copies of 
	       * variables saved on the backtracking stack are valid
	       * -- so read their values from the backtracking stack.
	       *
	       * This lets us use one generation fewer stack saves in
	       * the call-graph of a search.
	       */
	      
	    while_non_det_options:
#ifdef RX_DEBUG_0
	      ++search_state.lines_found;
	      if (rx_debug_trace)
		fprintf (stderr, "@@@ %d calls %d @@@\n",
			 search_state.line_no, search_state.lines_found);
	      
	      search_state.line_no = search_state.lines_found;
#endif
	      
	      if (bf->df->next_same_super_edge[0] == bf->first_df)
		{
		  /* This is a tail-call optimization -- we don't recurse
		   * for the last of the possible futures.
		   */
		  search_state.ifr = (bf->df->effects
				      ? &bf->df->side_effects_frame
				      : &bf->df->future_frame);
		  
		  rx_unlock_superstate (&rxb->rx, search_state.super);
		  POP(search_state.backtrack_stack,
		      search_state.backtrack_frame_bytes);
#ifdef RX_DEBUG
		  --search_state.backtrack_depth;
#endif
		  goto restart;
		}
	      else
		{
		  if (search_state.counter_stack)
		    {
		      struct rx_counter_frame * old_cf
			= ((struct rx_counter_frame *)search_state.counter_stack->sp);
		      struct rx_counter_frame * cf;
		      PUSH(search_state.counter_stack, sizeof (struct rx_counter_frame));
		      cf = ((struct rx_counter_frame *)search_state.counter_stack->sp);
		      cf->tag = old_cf->tag;
		      cf->val = old_cf->val;
		      cf->inherited_from = old_cf;
		      cf->cdr = 0;
		    }			
		  /* `Call' this test-match block */
		  search_state.ifr = (bf->df->effects
				      ? &bf->df->side_effects_frame
				      : &bf->df->future_frame);
		  goto recurse_test_match;
		}
	      
	      /* Returns in this block are accomplished by
	       * goto test_do_return.  There are two cases.
	       * If there is some search-stack left,
	       * then it is a return from a `recursive' call.
	       * If there is no search-stack left, then
	       * we should return to the fastmap/search loop.
	       */
	      
	    test_do_return:
	      
	      if (!search_state.backtrack_stack)
		{
#ifdef RX_DEBUG_0
		  if (rx_debug_trace)
		    fprintf (stderr, "!!! %d bails returning %d !!!\n",
			     search_state.line_no, search_state.test_ret);
#endif
		  
		  /* No more search-stack -- this test is done. */
		  if (search_state.test_ret != rx_test_internal_error)
		    goto return_from_test_match;
		  else
		    goto error_in_testing_match;
		}
	      
	      /* Returning from a recursive call to 
	       * the test match block:
	       */
	      
	      bf = ((struct rx_backtrack_frame *)
		    search_state.backtrack_stack->sp);
#ifdef RX_DEBUG_0
	      if (rx_debug_trace)
		fprintf (stderr, "+++ %d returns %d (to %d)+++\n",
			 search_state.line_no,
			 search_state.test_ret,
			 bf->stk_search_state.line_no);
#endif
	      
	      while (search_state.counter_stack
		     && (!bf->counter_stack_sp
			 || (bf->counter_stack_sp
			     != search_state.counter_stack->sp)))
		{
		  POP(search_state.counter_stack,
		      sizeof (struct rx_counter_frame));
		}
	      
	      if (search_state.test_ret == rx_test_internal_error)
		{
		  POP (search_state.backtrack_stack,
		       search_state.backtrack_frame_bytes);
		  search_state.test_ret = rx_test_internal_error;
		  goto test_do_return;
		}
	      
	      /* If a non-longest match was found and that is good 
	       * enough, return immediately.
	       */
	      if (   (search_state.test_ret == rx_test_found_first)
		  && search_state.first_found)
		{
		  rx_unlock_superstate (&rxb->rx, bf->stk_super);
		  POP (search_state.backtrack_stack,
		       search_state.backtrack_frame_bytes);
		  goto test_do_return;
		}
	      
	      search_state.test_ret = bf->stk_test_ret;
	      search_state.last_l = bf->stk_last_l;
	      search_state.last_r = bf->stk_last_r;
	      bf->df = bf->df->next_same_super_edge[0];
	      search_state.super = bf->stk_super;
	      search_state.c = bf->stk_c;
#ifdef RX_DEBUG_0
	      search_state.line_no = bf->stk_search_state.line_no;
#endif
	      
	      if (rxb->match_regs_on_stack)
		{
		  int x;
		  regoff_t * stk =
		    (regoff_t *)((char *)bf + sizeof (*bf));
		  for (x = 0; x <= search_state.last_l; ++x)
		    search_state.lparen[x] = stk[x];
		  stk += x;
		  for (x = 0; x <= search_state.last_r; ++x)
		    search_state.rparen[x] = stk[x];
		}
	      
	      {
		int x;
	      try_burst_2:
		x = get_burst (&bf->stk_test_pos, app_closure, stop);
		switch (x)
		  {
		  case rx_get_burst_continuation:
		    search_state.saved_bf = bf;
		    test_pc = rx_test_backtrack_return;
		    goto test_return_continuation;
		  resume_continuation_3:
		    bf = search_state.saved_bf;
		    goto try_burst_2;
		  case rx_get_burst_no_more:
		    /* Since we've been here before, it is some kind of
		     * error that we can't return.
		     */
		  case rx_get_burst_error:
		    search_state.test_ret = rx_test_internal_error;
		    goto test_do_return;
		  case rx_get_burst_ok:
		    break;
		  }
	      }
	      search_state.test_pos = bf->stk_test_pos;
	      goto while_non_det_options;
	    }
	    
	    
	  case rx_cache_miss:
	    /* Because the superstate NFA is lazily constructed,
	     * and in fact may erode from underneath us, we sometimes
	     * have to construct the next instruction from the hard way.
	     * This invokes one step in the lazy-conversion.
	     */
	    search_state.ifr = rx_handle_cache_miss (&rxb->rx,
						     search_state.super,
						     search_state.c,
						     search_state.ifr->data_2);
	    if (!search_state.ifr)
	      {
		search_state.test_ret = rx_test_internal_error;
		goto test_do_return;
	      }
	    goto restart;
	    
	  case rx_backtrack:
	    /* RX_BACKTRACK means that we've reached the empty
	     * superstate, indicating that match can't succeed
	     * from this point.
	     */
	    goto test_do_return;
	    
	  case rx_next_char:
	  case rx_error_inx:
	  case rx_num_instructions:
	    search_state.ret_val = 0;
	    goto test_do_return;
	  }
	goto pseudo_while_1;
      }
    
    /* Healthy exits from the test-match loop do a 
     * `goto return_from_test_match'   On the other hand, 
     * we might end up here.
     */
  error_in_testing_match:
    test_state = rx_test_error;
    goto test_returns_to_search;
    
    /***** fastmap/search loop body
     *	      considering the results testing for a match
     */
    
  return_from_test_match:
    
    if (search_state.best_last_l >= 0)
      {
	if (regs && (regs->start != search_state.best_lparen))
	  {
	    bcopy (search_state.best_lparen, regs->start,
		   regs->num_regs * sizeof (int));
	    bcopy (search_state.best_rparen, regs->end,
		   regs->num_regs * sizeof (int));
	  }
	if (regs && !rxb->no_sub)
	  {
	    int q;
	    int bound = (regs->num_regs > search_state.num_regs
			 ? regs->num_regs
			 : search_state.num_regs);
	    regoff_t * s = regs->start;
	    regoff_t * e = regs->end;
	    for (q = search_state.best_last_l + 1;  q < bound; ++q)
	      s[q] = e[q] = -1;
	  }
	search_state.ret_val = search_state.best_lparen[0];
	test_state = rx_test_ok;
	goto test_returns_to_search;
      }
    else
      {
	test_state = rx_test_fail;
	goto test_returns_to_search;
      }
    
  test_return_continuation:
    search_state.test_match_resume_pt = test_pc;
    test_state = rx_test_continuation;
    goto test_returns_to_search;
  }
}



#endif /* RX_WANT_RX_DEFS */



#else /* RX_WANT_SE_DEFS */
  /* Integers are used to represent side effects.
   *
   * Simple side effects are given negative integer names by these enums.
   * 
   * Non-negative names are reserved for complex effects.
   *
   * Complex effects are those that take arguments.  For example, 
   * a register assignment associated with a group is complex because
   * it requires an argument to tell which group is being matched.
   * 
   * The integer name of a complex effect is an index into rxb->se_params.
   */
 
  RX_DEF_SE(1, re_se_try, = -1)		/* Epsilon from start state */

  RX_DEF_SE(0, re_se_pushback, = re_se_try - 1)
  RX_DEF_SE(0, re_se_push0, = re_se_pushback -1)
  RX_DEF_SE(0, re_se_pushpos, = re_se_push0 - 1)
  RX_DEF_SE(0, re_se_chkpos, = re_se_pushpos -1)
  RX_DEF_SE(0, re_se_poppos, = re_se_chkpos - 1)

  RX_DEF_SE(1, re_se_at_dot, = re_se_poppos - 1)	/* Emacs only */
  RX_DEF_SE(0, re_se_syntax, = re_se_at_dot - 1) /* Emacs only */
  RX_DEF_SE(0, re_se_not_syntax, = re_se_syntax - 1) /* Emacs only */

  RX_DEF_SE(1, re_se_begbuf, = re_se_not_syntax - 1) /* match beginning of buffer */
  RX_DEF_SE(1, re_se_hat, = re_se_begbuf - 1) /* match beginning of line */

  RX_DEF_SE(1, re_se_wordbeg, = re_se_hat - 1) 
  RX_DEF_SE(1, re_se_wordbound, = re_se_wordbeg - 1)
  RX_DEF_SE(1, re_se_notwordbound, = re_se_wordbound - 1)

  RX_DEF_SE(1, re_se_wordend, = re_se_notwordbound - 1)
  RX_DEF_SE(1, re_se_endbuf, = re_se_wordend - 1)

  /* This fails except at the end of a line. 
   * It deserves to go here since it is typicly one of the last steps 
   * in a match.
   */
  RX_DEF_SE(1, re_se_dollar, = re_se_endbuf - 1)

  /* Simple effects: */
  RX_DEF_SE(1, re_se_fail, = re_se_dollar - 1)

  /* Complex effects.  These are used in the 'se' field of 
   * a struct re_se_params.  Indexes into the se array
   * are stored as instructions on nfa edges.
   */
  RX_DEF_CPLX_SE(1, re_se_win, = 0)
  RX_DEF_CPLX_SE(1, re_se_lparen, = re_se_win + 1)
  RX_DEF_CPLX_SE(1, re_se_rparen, = re_se_lparen + 1)
  RX_DEF_CPLX_SE(0, re_se_backref, = re_se_rparen + 1)
  RX_DEF_CPLX_SE(0, re_se_iter, = re_se_backref + 1) 
  RX_DEF_CPLX_SE(0, re_se_end_iter, = re_se_iter + 1)
  RX_DEF_CPLX_SE(0, re_se_tv, = re_se_end_iter + 1)

#endif

#endif
