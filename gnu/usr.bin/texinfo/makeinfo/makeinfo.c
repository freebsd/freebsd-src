/* Makeinfo -- convert texinfo format files into info files.

   Copyright (C) 1987, 1992, 1993 Free Software Foundation, Inc.

   This file is part of GNU Info.

   Makeinfo is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.  No author or distributor accepts
   responsibility to anyone for the consequences of using it or for
   whether it serves any particular purpose or works at all, unless he
   says so in writing.  Refer to the GNU Emacs General Public License
   for full details.

   Everyone is granted permission to copy, modify and redistribute
   Makeinfo, but only under the conditions described in the GNU Emacs
   General Public License.   A copy of this license is supposed to
   have been given to you along with GNU Emacs so you can know your
   rights and responsibilities.  It should be in a file named COPYING.
   Among other things, the copyright notice and this notice must be
   preserved on all copies.  */

/* This is Makeinfo version 1.55.  If you change the version number of
   Makeinfo, please change it here and at the lines reading:

    int major_version = 1;
    int minor_version = 55;

   in the code below.

   Makeinfo is authored by Brian Fox (bfox@ai.mit.edu). */

/* You can change some of the behaviour of Makeinfo by changing the
   following defines: */

/* Define INDENT_PARAGRAPHS_IN_TABLE if you want the paragraphs which
   appear within an @table, @ftable, or @itemize environment to have
   standard paragraph indentation.  Without this, such paragraphs have
   no starting indentation. */
/* #define INDENT_PARAGRAPHS_IN_TABLE */

/* Define DEFAULT_INDENTATION_INCREMENT as an integer which is the amount
   that @example should increase indentation by.  This incremement is used
   for all insertions which indent the enclosed text. */
#define DEFAULT_INDENTATION_INCREMENT 5

/* Define PARAGRAPH_START_INDENT to be the amount of indentation that
   the first lines of paragraphs receive by default, where no other
   value has been specified.  Users can change this value on the command
   line, with the +paragraph-indent option, or within the texinfo file,
   with the @paragraphindent command. */
#define PARAGRAPH_START_INDENT 3

/* Define DEFAULT_PARAGRAPH_SPACING as the number of blank lines that you
   wish to appear between paragraphs.  A value of 1 creates a single blank
   line between paragraphs.  Paragraphs are defined by 2 or more consecutive
   newlines in the input file (i.e., one or more blank lines). */
#define DEFAULT_PARAGRAPH_SPACING 1

/* **************************************************************** */
/*								    */
/*			Include File Declarations       	    */
/*								    */
/* **************************************************************** */

/* Indent #pragma so that older Cpp's don't try to parse it. */
#if defined (_AIX)
 # pragma alloca
#endif /* _AIX */

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pwd.h>
#include <errno.h>
#if defined (HAVE_VARARGS_H)
#include <varargs.h>
#endif /* HAVE_VARARGS_H */
#include "getopt.h"

#if defined (VMS)
#include <perror.h>
#endif

#if defined (HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif /* !HAVE_STRING_H */

#if defined (TM_IN_SYS_TIME)
#include <sys/time.h>
#else
#include <time.h>
#endif /* !TM_IN_SYS_TIME */

#if defined (HAVE_SYS_FCNTL_H)
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif /* !HAVE_SYS_FCNTL_H */

#include <sys/file.h>

#if defined (__GNUC__)
#define alloca __builtin_alloca
#else
#if defined(HAVE_ALLOCA_H)
#include <alloca.h>
#else /* !HAVE_ALLOCA_H */
#if !defined (_AIX)
extern char *alloca ();
#endif /* !_AIX */
#endif /* !HAVE_ALLOCA_H */
#endif /* !__GNUC__ */

void *xmalloc (), *xrealloc ();
static void isolate_nodename ();

/* Non-zero means that we are currently hacking the insides of an
   insertion which would use a fixed width font. */
static int in_fixed_width_font = 0;

/* Non-zero means that start_paragraph () MUST be called before we pay
   any attention to close_paragraph () calls. */
int must_start_paragraph = 0;

/* Some systems don't declare this function in pwd.h. */
struct passwd *getpwnam ();


/* **************************************************************** */
/*								    */
/*			      Global Defines  			    */
/*								    */
/* **************************************************************** */

/* Error levels */
#define NO_ERROR 0
#define SYNTAX	 2
#define FATAL	 4

/* How to allocate permanent storage for STRING. */
#define savestring(x) \
  ((char *)strcpy ((char *)xmalloc (1 + ((x) ? strlen (x) : 0)), \
		   (x) ? (x) : ""))

/* C's standard macros don't check to make sure that the characters being
   changed are within range.  So I have to check explicitly. */

/* GNU Library doesn't have toupper().  Until GNU gets this fixed, I will
   have to do it. */
#ifndef toupper
#define toupper(c) ((c) - 32)
#endif

#define coerce_to_upper(c) ((islower(c) ? toupper(c) : (c)))
#define coerce_to_lower(c) ((isupper(c) ? tolower(c) : (c)))

#define control_character_bit 0x40 /* %01000000, must be off. */
#define meta_character_bit 0x080/* %10000000, must be on.  */
#define CTL(c) ((c) & (~control_character_bit))
#define UNCTL(c) coerce_to_upper(((c)|control_character_bit))
#define META(c) ((c) | (meta_character_bit))
#define UNMETA(c) ((c) & (~meta_character_bit))

#define whitespace(c) (((c) == '\t') || ((c) == ' '))
#define sentence_ender(c) ((c) == '.' || (c) == '?' || (c) == '!')
#define cr_or_whitespace(c) (((c) == '\t') || ((c) == ' ') || ((c) == '\n'))

#ifndef isletter
#define isletter(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#endif

#ifndef isupper
#define isupper(c) ((c) >= 'A' && (c) <= 'Z')
#endif

#ifndef isdigit
#define isdigit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#define member(c, s) (strchr (s, c) != NULL)

#define COMMAND_PREFIX '@'

/* Stuff for splitting large files. */
#define SPLIT_SIZE_THRESHOLD 70000  /* What's good enough for Stallman... */
#define DEFAULT_SPLIT_SIZE 50000    /* Is probably good enough for me. */
int splitting = 1;		    /* Always true for now. */

typedef int FUNCTION ();	/* So I can say FUNCTION *foo; */


/* **************************************************************** */
/*								    */
/*			    Global Variables			    */
/*								    */
/* **************************************************************** */

/* Global pointer to argv[0]. */
char *progname;

/* The current input file state. */
char *input_filename;
char *input_text;
int size_of_input_text;
int input_text_offset;
int line_number;

#define curchar() input_text[input_text_offset]

#define command_char(c) ((!whitespace(c)) && \
			 ((c) != '\n') && \
			 ((c) != '{') && \
			 ((c) != '}') && \
			 ((c) != '='))

#define skip_whitespace() while (input_text_offset != size_of_input_text \
				 && whitespace(curchar()))\
  input_text_offset++

/* Return non-zero if STRING is the text at input_text + input_text_offset,
   else zero. */
#define looking_at(string) \
  (strncmp (input_text + input_text_offset, string, strlen (string)) == 0)

/* And writing to the output. */

/* The output file name. */
char *output_filename = (char *)NULL;
char *pretty_output_filename;

/* Name of the output file that the user elected to pass on the command line.
   Such a name overrides any name found with the @setfilename command. */
char *command_output_filename = (char *)NULL;

/* A colon separated list of directories to search for files included
   with @include.  This can be controlled with the `-I' option to makeinfo. */
char *include_files_path = (char *)NULL;

/* Current output stream. */
FILE *output_stream;

/* Position in the output file. */
int output_position;

/* Output paragraph buffer. */
unsigned char *output_paragraph;

/* Offset into OUTPUT_PARAGRAPH. */
int output_paragraph_offset;

/* The output paragraph "cursor" horizontal position. */
int output_column = 0;

/* Non-zero means output_paragraph contains text. */
int paragraph_is_open = 0;

#define INITIAL_PARAGRAPH_SPACE 5000
int paragraph_buffer_len = INITIAL_PARAGRAPH_SPACE;

/* Filling.. */
/* Non-zero indicates that filling will take place on long lines. */
int filling_enabled = 1;

/* Non-zero means that words are not to be split, even in long lines.  This
   gets changed for cm_w (). */
int non_splitting_words = 0;

/* Non-zero indicates that filling a line also indents the new line. */
int indented_fill = 0;

/* The column at which long lines are broken. */
int fill_column = 72;

/* The amount of indentation to apply at the start of each line. */
int current_indent = 0;

/* The amount of indentation to add at the starts of paragraphs.
   0 means don't change existing indentation at paragraph starts.
   > 0 is amount to indent new paragraphs by.
   < 0 means indent to column zero by removing indentation if necessary.

   This is normally zero, but some people prefer paragraph starts to be
   somewhat more indented than paragraph bodies.  A pretty value for
   this is 3. */
int paragraph_start_indent = PARAGRAPH_START_INDENT;

/* Non-zero means that the use of paragraph_start_indent is inhibited.
   @example uses this to line up the left columns of the example text.
   A negative value for this variable is incremented each time it is used.
   @noindent uses this to inhibit indentation for a single paragraph.  */
int inhibit_paragraph_indentation = 0;

/* Indentation that is pending insertion.  We have this for hacking lines
   which look blank, but contain whitespace.  We want to treat those as
   blank lines. */
int pending_indent = 0;

/* The amount that indentation increases/decreases by. */
int default_indentation_increment = DEFAULT_INDENTATION_INCREMENT;

/* Non-zero indicates that indentation is temporarily turned off. */
int no_indent = 1;

/* Non-zero means forcing output text to be flushright. */
int force_flush_right = 0;

/* Non-zero means that the footnote style for this document was set on
   the command line, which overrides any other settings. */
int footnote_style_preset = 0;

/* Non-zero means that we automatically number footnotes that have no
   specified marker. */
int number_footnotes = 1;

/* The current footnote number in this node.  Each time a new node is
   started this is reset to 1. */
int current_footnote_number = 1;

/* Command name in the process of being hacked. */
char *command;

/* The index in our internal command table of the currently
   executing command. */
int command_index;

/* A stack of file information records.  If a new file is read in with
   "@input", we remember the old input file state on this stack. */
typedef struct fstack
{
  struct fstack *next;
  char *filename;
  char *text;
  int size;
  int offset;
  int line_number;
} FSTACK;

FSTACK *filestack = (FSTACK *) NULL;

/* Stuff for nodes. */
/* The current nodes node name. */
char *current_node = (char *)NULL;

/* The current nodes section level. */
int current_section = 0;

/* The filename of the current input file.  This is never freed. */
char *node_filename = (char *)NULL;

/* What we remember for each node. */
typedef struct tentry
{
  struct tentry *next_ent;
  char *node;		/* name of this node. */
  char *prev;		/* name of "Prev:" for this node. */
  char *next;		/* name of "Next:" for this node. */
  char *up;		/* name of "Up:" for this node.   */
  int position;		/* output file position of this node. */
  int line_no;		/* defining line in source file. */
  char *filename;	/* The file that this node was found in. */
  int touched;		/* non-zero means this node has been referenced. */
  int flags;		/* Room for growth.  Right now, contains 1 bit. */
} TAG_ENTRY;

/* If node-a has a "Next" for node-b, but node-b has no "Prev" for node-a,
   we turn on this flag bit in node-b's tag entry.  This means that when
   it is time to validate node-b, we don't report an additional error
   if there was no "Prev" field. */
#define PREV_ERROR 0x1
#define NEXT_ERROR 0x2
#define UP_ERROR   0x4
#define NO_WARN	   0x8
#define IS_TOP 	   0x10

TAG_ENTRY *tag_table = (TAG_ENTRY *) NULL;

#define HAVE_MACROS
#if defined (HAVE_MACROS)
/* Macro definitions for user-defined commands. */
typedef struct {
  char *name;			/* Name of the macro. */
  char *definition;		/* Definition text. */
  char *filename;		/* File where this macro is defined. */
  int lineno;			/* Line number within FILENAME. */
} MACRO_DEF;

void add_macro (), execute_macro ();
MACRO_DEF *find_macro (), *delete_macro ();
#endif /* HAVE_MACROS */

/* Menu reference, *note reference, and validation hacking. */

/* The various references that we know about. */
enum reftype
{
  menu_reference, followed_reference
};

/* A structure to remember references with.  A reference to a node is
   either an entry in a menu, or a cross-reference made with [px]ref. */
typedef struct node_ref
{
  struct node_ref *next;
  char *node;			/* Name of node referred to. */
  char *containing_node;	/* Name of node containing this reference. */
  int line_no;			/* Line number where the reference occurs. */
  int section;			/* Section level where the reference occurs. */
  char *filename;		/* Name of file where the reference occurs. */
  enum reftype type;		/* Type of reference, either menu or note. */
} NODE_REF;

/* The linked list of such structures. */
NODE_REF *node_references = (NODE_REF *) NULL;

/* Flag which tells us whether to examine menu lines or not. */
int in_menu = 0;

/* Flags controlling the operation of the program. */

/* Default is to notify users of bad choices. */
int print_warnings = 1;

/* Default is to check node references. */
int validating = 1;

/* Non-zero means do not output "Node: Foo" for node separations. */
int no_headers = 0;

/* Number of errors that we tolerate on a given fileset. */
int max_error_level = 100;

/* Maximum number of references to a single node before complaining. */
int reference_warning_limit = 1000;

/* Non-zero means print out information about what is going on when it
   is going on. */
int verbose_mode = 0;

/* The list of commands that we hack in texinfo.  Each one
   has an associated function.  When the command is encountered in the
   text, the associated function is called with START as the argument.
   If the function expects arguments in braces, it remembers itself on
   the stack.  When the corresponding close brace is encountered, the
   function is called with END as the argument. */

#define START 0
#define END 1

typedef struct brace_element
{
  struct brace_element *next;
  FUNCTION *proc;
  int pos, line;
} BRACE_ELEMENT;

BRACE_ELEMENT *brace_stack = (BRACE_ELEMENT *) NULL;

/* Forward declarations. */

int insert_self (), cm_ignore_line ();

int
  cm_tex (), cm_asterisk (), cm_dots (), cm_bullet (), cm_TeX (),
  cm_copyright (), cm_code (), cm_samp (), cm_file (), cm_kbd (),
  cm_key (), cm_ctrl (), cm_var (), cm_dfn (), cm_emph (), cm_strong (),
  cm_cite (), cm_italic (), cm_bold (), cm_roman (), cm_title (), cm_w (),
  cm_refill (), cm_titlefont ();

int
  cm_chapter (), cm_unnumbered (), cm_appendix (), cm_top (),
  cm_section (), cm_unnumberedsec (), cm_appendixsec (),
  cm_subsection (), cm_unnumberedsubsec (), cm_appendixsubsec (),
  cm_subsubsection (), cm_unnumberedsubsubsec (), cm_appendixsubsubsec (),
  cm_heading (), cm_chapheading (), cm_subheading (), cm_subsubheading (),
  cm_majorheading (), cm_raisesections (), cm_lowersections ();

/* All @defxxx commands map to cm_defun (). */
int cm_defun ();

int
  cm_node (), cm_menu (), cm_xref (), cm_ftable (), cm_vtable (), cm_pxref (),
  cm_inforef (), cm_quotation (), cm_display (), cm_itemize (),
  cm_enumerate (), cm_table (), cm_itemx (), cm_noindent (), cm_setfilename (),
  cm_br (), cm_sp (), cm_page (), cm_group (), cm_center (), cm_include (),
  cm_bye (), cm_item (), cm_end (), cm_infoinclude (), cm_ifinfo (),
  cm_kindex (), cm_cindex (), cm_findex (), cm_pindex (), cm_vindex (),
  cm_tindex (), cm_asis (), cm_synindex (), cm_printindex (), cm_minus (),
  cm_footnote (), cm_force_abbreviated_whitespace (), cm_example (),
  cm_smallexample (), cm_lisp (), cm_format (), cm_exdent (), cm_defindex (),
  cm_defcodeindex (), cm_sc (), cm_result (), cm_expansion (), cm_equiv (),
  cm_print (), cm_error (), cm_point (), cm_today (), cm_flushleft (),
  cm_flushright (), cm_smalllisp (), cm_finalout (), cm_math (),
  cm_cartouche (), cm_ignore_sentence_ender ();

/* Conditionals. */
int cm_set (), cm_clear (), cm_ifset (), cm_ifclear (), cm_value ();

#if defined (HAVE_MACROS)
/* Define a user-defined command which is simple substitution. */
int cm_macro (), cm_unmacro ();
#endif /* HAVE_MACROS */

/* Options. */
int cm_paragraphindent (), cm_footnotestyle ();

/* Internals. */
int do_nothing (), command_name_condition ();
int misplaced_brace (), cm_obsolete ();

typedef struct
{
  char *name;
  FUNCTION *proc;
  int argument_in_braces;
} COMMAND;

/* Stuff for defining commands on the fly. */
COMMAND **user_command_array = (COMMAND **) NULL;
int user_command_array_len = 0;

#define NO_BRACE_ARGS 0
#define BRACE_ARGS 1

static COMMAND CommandTable[] = {
  { "!", cm_ignore_sentence_ender, NO_BRACE_ARGS },
  { "'", insert_self, NO_BRACE_ARGS },
  { "*", cm_asterisk, NO_BRACE_ARGS },
  { ".", cm_ignore_sentence_ender, NO_BRACE_ARGS },
  { ":", cm_force_abbreviated_whitespace, NO_BRACE_ARGS },
  { "?", cm_ignore_sentence_ender, NO_BRACE_ARGS },
  { "|", do_nothing, NO_BRACE_ARGS },
  { "@", insert_self, NO_BRACE_ARGS },
  { " ", insert_self, NO_BRACE_ARGS },
  { "\n", insert_self, NO_BRACE_ARGS },
  { "TeX", cm_TeX, BRACE_ARGS },
  { "`", insert_self, NO_BRACE_ARGS },
  { "appendix", cm_appendix, NO_BRACE_ARGS },
  { "appendixsection", cm_appendixsec, NO_BRACE_ARGS },
  { "appendixsec", cm_appendixsec, NO_BRACE_ARGS },
  { "appendixsubsec", cm_appendixsubsec, NO_BRACE_ARGS },
  { "appendixsubsubsec", cm_appendixsubsubsec, NO_BRACE_ARGS },
  { "asis", cm_asis, BRACE_ARGS },
  { "b", cm_bold, BRACE_ARGS },
  { "br", cm_br, NO_BRACE_ARGS },
  { "bullet", cm_bullet, BRACE_ARGS },
  { "bye", cm_bye, NO_BRACE_ARGS },
  { "c", cm_ignore_line, NO_BRACE_ARGS },
  { "cartouche", cm_cartouche, NO_BRACE_ARGS },
  { "center", cm_center, NO_BRACE_ARGS },
  { "chapheading", cm_chapheading, NO_BRACE_ARGS },
  { "chapter", cm_chapter, NO_BRACE_ARGS },
  { "cindex", cm_cindex, NO_BRACE_ARGS },
  { "cite", cm_cite, BRACE_ARGS },
  { "clear", cm_clear, NO_BRACE_ARGS },
  { "code", cm_code, BRACE_ARGS },
  { "comment", cm_ignore_line, NO_BRACE_ARGS },
  { "contents", do_nothing, NO_BRACE_ARGS },
  { "copyright", cm_copyright, BRACE_ARGS },
  { "ctrl", cm_ctrl, BRACE_ARGS },
  { "defcodeindex", cm_defcodeindex, NO_BRACE_ARGS },
  { "defindex", cm_defindex, NO_BRACE_ARGS },
  { "dfn", cm_dfn, BRACE_ARGS },

/* The `def' commands. */
  { "deffn", cm_defun, NO_BRACE_ARGS },
  { "deffnx", cm_defun, NO_BRACE_ARGS },
  { "defun", cm_defun, NO_BRACE_ARGS },
  { "defunx", cm_defun, NO_BRACE_ARGS },
  { "defmac", cm_defun, NO_BRACE_ARGS },
  { "defmacx", cm_defun, NO_BRACE_ARGS },
  { "defspec", cm_defun, NO_BRACE_ARGS },
  { "defspecx", cm_defun, NO_BRACE_ARGS },
  { "defvr", cm_defun, NO_BRACE_ARGS },
  { "defvrx", cm_defun, NO_BRACE_ARGS },
  { "defvar", cm_defun, NO_BRACE_ARGS },
  { "defvarx", cm_defun, NO_BRACE_ARGS },
  { "defopt", cm_defun, NO_BRACE_ARGS },
  { "defoptx", cm_defun, NO_BRACE_ARGS },
  { "deftypefn", cm_defun, NO_BRACE_ARGS },
  { "deftypefnx", cm_defun, NO_BRACE_ARGS },
  { "deftypefun", cm_defun, NO_BRACE_ARGS },
  { "deftypefunx", cm_defun, NO_BRACE_ARGS },
  { "deftypevr", cm_defun, NO_BRACE_ARGS },
  { "deftypevrx", cm_defun, NO_BRACE_ARGS },
  { "deftypevar", cm_defun, NO_BRACE_ARGS },
  { "deftypevarx", cm_defun, NO_BRACE_ARGS },
  { "defcv", cm_defun, NO_BRACE_ARGS },
  { "defcvx", cm_defun, NO_BRACE_ARGS },
  { "defivar", cm_defun, NO_BRACE_ARGS },
  { "defivarx", cm_defun, NO_BRACE_ARGS },
  { "defop", cm_defun, NO_BRACE_ARGS },
  { "defopx", cm_defun, NO_BRACE_ARGS },
  { "defmethod", cm_defun, NO_BRACE_ARGS },
  { "defmethodx", cm_defun, NO_BRACE_ARGS },
  { "deftypemethod", cm_defun, NO_BRACE_ARGS },
  { "deftypemethodx", cm_defun, NO_BRACE_ARGS },
  { "deftp", cm_defun, NO_BRACE_ARGS },
  { "deftpx", cm_defun, NO_BRACE_ARGS },
/* The end of the `def' commands. */

  { "display", cm_display, NO_BRACE_ARGS },
  { "dots", cm_dots, BRACE_ARGS },
  { "dmn", do_nothing, BRACE_ARGS },
  { "emph", cm_emph, BRACE_ARGS },
  { "end", cm_end, NO_BRACE_ARGS },
  { "enumerate", cm_enumerate, NO_BRACE_ARGS },
  { "equiv", cm_equiv, BRACE_ARGS },
  { "error", cm_error, BRACE_ARGS },
  { "example", cm_example, NO_BRACE_ARGS },
  { "exdent", cm_exdent, NO_BRACE_ARGS },
  { "expansion", cm_expansion, BRACE_ARGS },
  { "file", cm_file, BRACE_ARGS },
  { "findex", cm_findex, NO_BRACE_ARGS },
  { "finalout", do_nothing, NO_BRACE_ARGS },
  { "flushleft", cm_flushleft, NO_BRACE_ARGS },
  { "flushright", cm_flushright, NO_BRACE_ARGS },
  { "format", cm_format, NO_BRACE_ARGS },
  { "ftable", cm_ftable, NO_BRACE_ARGS },
  { "group", cm_group, NO_BRACE_ARGS },
  { "heading", cm_heading, NO_BRACE_ARGS },
  { "headings", cm_ignore_line, NO_BRACE_ARGS },
  { "i", cm_italic, BRACE_ARGS },
  { "iappendix", cm_appendix, NO_BRACE_ARGS },
  { "iappendixsection", cm_appendixsec, NO_BRACE_ARGS },
  { "iappendixsec", cm_appendixsec, NO_BRACE_ARGS },
  { "iappendixsubsec", cm_appendixsubsec, NO_BRACE_ARGS },
  { "iappendixsubsubsec", cm_appendixsubsubsec, NO_BRACE_ARGS },
  { "ichapter", cm_chapter, NO_BRACE_ARGS },
  { "ifclear", cm_ifclear, NO_BRACE_ARGS },
  { "ifinfo", cm_ifinfo, NO_BRACE_ARGS },
  { "ifset", cm_ifset, NO_BRACE_ARGS },
  { "iftex", command_name_condition, NO_BRACE_ARGS },
  { "ignore", command_name_condition, NO_BRACE_ARGS },
  { "include", cm_include, NO_BRACE_ARGS },
  { "inforef", cm_inforef, BRACE_ARGS },
  { "input", cm_include, NO_BRACE_ARGS },
  { "isection", cm_section, NO_BRACE_ARGS },
  { "isubsection", cm_subsection, NO_BRACE_ARGS },
  { "isubsubsection", cm_subsubsection, NO_BRACE_ARGS },
  { "item", cm_item, NO_BRACE_ARGS },
  { "itemize", cm_itemize, NO_BRACE_ARGS },
  { "itemx", cm_itemx, NO_BRACE_ARGS },
  { "iunnumbered", cm_unnumbered, NO_BRACE_ARGS },
  { "iunnumberedsec", cm_unnumberedsec, NO_BRACE_ARGS },
  { "iunnumberedsubsec", cm_unnumberedsubsec, NO_BRACE_ARGS },
  { "iunnumberedsubsubsec", cm_unnumberedsubsubsec, NO_BRACE_ARGS },
  { "kbd", cm_kbd, BRACE_ARGS },
  { "key", cm_key, BRACE_ARGS },
  { "kindex", cm_kindex, NO_BRACE_ARGS },
  { "lowersections", cm_lowersections, NO_BRACE_ARGS },
  { "lisp", cm_lisp, NO_BRACE_ARGS },
  { "macro", cm_macro, NO_BRACE_ARGS },
  { "majorheading", cm_majorheading, NO_BRACE_ARGS },
  { "math", cm_math, BRACE_ARGS },
  { "menu", cm_menu, NO_BRACE_ARGS },
  { "minus", cm_minus, BRACE_ARGS },
  { "need", cm_ignore_line, NO_BRACE_ARGS },
  { "node", cm_node, NO_BRACE_ARGS },
  { "noindent", cm_noindent, NO_BRACE_ARGS },
  { "nwnode", cm_node, NO_BRACE_ARGS },
  { "overfullrule", cm_ignore_line, NO_BRACE_ARGS },
  { "page", do_nothing, NO_BRACE_ARGS },
  { "pindex", cm_pindex, NO_BRACE_ARGS },
  { "point", cm_point, BRACE_ARGS },
  { "print", cm_print, BRACE_ARGS },
  { "printindex", cm_printindex, NO_BRACE_ARGS },
  { "pxref", cm_pxref, BRACE_ARGS },
  { "quotation", cm_quotation, NO_BRACE_ARGS },
  { "r", cm_roman, BRACE_ARGS },
  { "raisesections", cm_raisesections, NO_BRACE_ARGS },
  { "ref", cm_xref, BRACE_ARGS },
  { "refill", cm_refill, NO_BRACE_ARGS },
  { "result", cm_result, BRACE_ARGS },
  { "samp", cm_samp, BRACE_ARGS },
  { "sc", cm_sc, BRACE_ARGS },
  { "section", cm_section, NO_BRACE_ARGS },
  { "set", cm_set, NO_BRACE_ARGS },
  { "setchapternewpage", cm_ignore_line, NO_BRACE_ARGS },
  { "setchapterstyle", cm_ignore_line, NO_BRACE_ARGS },
  { "setfilename", cm_setfilename, NO_BRACE_ARGS },
  { "settitle", cm_ignore_line, NO_BRACE_ARGS },
  { "shortcontents", do_nothing, NO_BRACE_ARGS },
  { "shorttitlepage", command_name_condition, NO_BRACE_ARGS },
  { "smallbook", cm_ignore_line, NO_BRACE_ARGS },
  { "smallexample", cm_smallexample, NO_BRACE_ARGS },
  { "smalllisp", cm_smalllisp, NO_BRACE_ARGS },
  { "sp", cm_sp, NO_BRACE_ARGS },
  { "strong", cm_strong, BRACE_ARGS },
  { "subheading", cm_subheading, NO_BRACE_ARGS },
  { "subsection", cm_subsection, NO_BRACE_ARGS },
  { "subsubheading", cm_subsubheading, NO_BRACE_ARGS },
  { "subsubsection", cm_subsubsection, NO_BRACE_ARGS },
  { "summarycontents", do_nothing, NO_BRACE_ARGS },
  { "syncodeindex", cm_synindex, NO_BRACE_ARGS },
  { "synindex", cm_synindex, NO_BRACE_ARGS },
  { "t", cm_title, BRACE_ARGS },
  { "table", cm_table, NO_BRACE_ARGS },
  { "tex", command_name_condition, NO_BRACE_ARGS },
  { "tindex", cm_tindex, NO_BRACE_ARGS },
  { "titlefont", cm_titlefont, BRACE_ARGS },
  { "titlepage", command_name_condition, NO_BRACE_ARGS },
  { "titlespec", command_name_condition, NO_BRACE_ARGS },
  { "today", cm_today, BRACE_ARGS },
  { "top", cm_top, NO_BRACE_ARGS  },
  { "unmacro", cm_unmacro, NO_BRACE_ARGS },
  { "unnumbered", cm_unnumbered, NO_BRACE_ARGS },
  { "unnumberedsec", cm_unnumberedsec, NO_BRACE_ARGS },
  { "unnumberedsubsec", cm_unnumberedsubsec, NO_BRACE_ARGS },
  { "unnumberedsubsubsec", cm_unnumberedsubsubsec, NO_BRACE_ARGS },
  { "value", cm_value, BRACE_ARGS },
  { "var", cm_var, BRACE_ARGS },
  { "vindex", cm_vindex, NO_BRACE_ARGS },
  { "vtable", cm_vtable, NO_BRACE_ARGS },
  { "w", cm_w, BRACE_ARGS },
  { "xref", cm_xref, BRACE_ARGS },
  { "{", insert_self, NO_BRACE_ARGS },
  { "}", insert_self, NO_BRACE_ARGS },

  /* Some obsoleted commands. */
  { "infotop", cm_obsolete, NO_BRACE_ARGS },
  { "infounnumbered", cm_obsolete, NO_BRACE_ARGS },
  { "infounnumberedsec", cm_obsolete, NO_BRACE_ARGS },
  { "infounnumberedsubsec", cm_obsolete, NO_BRACE_ARGS },
  { "infounnumberedsubsubsec", cm_obsolete, NO_BRACE_ARGS },
  { "infoappendix", cm_obsolete, NO_BRACE_ARGS },
  { "infoappendixsec", cm_obsolete, NO_BRACE_ARGS },
  { "infoappendixsubsec", cm_obsolete, NO_BRACE_ARGS },
  { "infoappendixsubsubsec", cm_obsolete, NO_BRACE_ARGS },
  { "infochapter", cm_obsolete, NO_BRACE_ARGS },
  { "infosection", cm_obsolete, NO_BRACE_ARGS },
  { "infosubsection", cm_obsolete, NO_BRACE_ARGS },
  { "infosubsubsection", cm_obsolete, NO_BRACE_ARGS },

  /* Now @include does what this was supposed to. */
  { "infoinclude", cm_infoinclude, NO_BRACE_ARGS },
  { "footnote", cm_footnote, NO_BRACE_ARGS}, /* self-arg eater */
  { "footnotestyle", cm_footnotestyle, NO_BRACE_ARGS },
  { "paragraphindent", cm_paragraphindent, NO_BRACE_ARGS },

  {(char *) NULL, (FUNCTION *) NULL}, NO_BRACE_ARGS};

int major_version = 1;
int minor_version = 55;

struct option long_options[] =
{
  { "error-limit", 1, 0, 'e' },			/* formerly -el */
  { "fill-column", 1, 0, 'f' },			/* formerly -fc */
  { "footnote-style", 1, 0, 's' },		/* formerly -ft */
  { "no-headers", 0, &no_headers, 1 },		/* Do not output Node: foo */
  { "no-pointer-validate", 0, &validating, 0 }, /* formerly -nv */
  { "no-validate", 0, &validating, 0 },		/* formerly -nv */
  { "no-split", 0, &splitting, 0 },		/* formerly -ns */
  { "no-warn", 0, &print_warnings, 0 },		/* formerly -nw */
  { "number-footnotes", 0, &number_footnotes, 1 },
  { "no-number-footnotes", 0, &number_footnotes, 0 },
  { "output", 1, 0, 'o' },
  { "paragraph-indent", 1, 0, 'p' },		/* formerly -pi */
  { "reference-limit", 1, 0, 'r' },		/* formerly -rl */
  { "verbose", 0, &verbose_mode, 1 },		/* formerly -verbose */
  { "version", 0, 0, 'V' },
  {NULL, 0, NULL, 0}
};

/* Values for calling handle_variable_internal (). */
#define SET	1
#define CLEAR	2
#define IFSET	3
#define IFCLEAR	4

/* **************************************************************** */
/*								    */
/*			Main ()  Start of code  		    */
/*					        		    */
/* **************************************************************** */

/* For each file mentioned in the command line, process it, turning
   texinfo commands into wonderfully formatted output text. */
main (argc, argv)
     int argc;
     char **argv;
{
  extern int errors_printed;
  char *filename_part ();
  int c, ind;

  /* The name of this program is the last filename in argv[0]. */
  progname = filename_part (argv[0]);

  /* Parse argument flags from the input line. */
  while ((c = getopt_long
	  (argc, argv, "D:U:I:f:o:p:e:r:s:V", long_options, &ind))
	 != EOF)
    {
      if (c == 0 && long_options[ind].flag == 0)
	c = long_options[ind].val;

      switch (c)
	{
	  /* User specified variable to set or clear? */
	case 'D':
	case 'U':
	  handle_variable_internal ((c == 'D') ? SET : CLEAR, optarg);
	  break;

	  /* User specified include file path? */
	case 'I':
	  if (!include_files_path)
	    include_files_path = savestring (".");

	  include_files_path = (char *)
	    xrealloc (include_files_path,
		      2 + strlen (include_files_path) + strlen (optarg));
	  strcat (include_files_path, ":");
	  strcat (include_files_path, optarg);
	  break;

	  /* User specified fill_column? */
	case 'f':
	  if (sscanf (optarg, "%d", &fill_column) != 1)
	    usage ();
	  break;

	  /* User specified output file? */
	case 'o':
	  command_output_filename = savestring (optarg);
	  break;

	  /* User specified paragraph indent (paragraph_start_index)? */
	case 'p':
	  if (set_paragraph_indent (optarg) < 0)
	    usage ();
	  break;

	  /* User specified error level? */
	case 'e':
	  if (sscanf (optarg, "%d", &max_error_level) != 1)
	    usage ();
	  break;

	  /* User specified reference warning limit? */
	case 'r':
	  if (sscanf (optarg, "%d", &reference_warning_limit) != 1)
	    usage ();
	  break;

	  /* User specified footnote style? */
	case 's':
	  if (set_footnote_style (optarg) < 0)
	    usage ();
	  footnote_style_preset = 1;
	  break;

	  /* User requested version info? */
	case 'V':
	  print_version_info ();
	  exit (NO_ERROR);
	  break;

	case '?':
	  usage ();
	}
    }

  if (optind == argc)
    usage ();
  else if (verbose_mode)
    print_version_info ();

  /* Remaining arguments are file names of texinfo files.
     Convert them, one by one. */
  while (optind != argc)
    convert (argv[optind++]);

  if (errors_printed)
    exit (SYNTAX);
  else
    exit (NO_ERROR);
}

/* Display the version info of this invocation of Makeinfo. */
print_version_info ()
{
  fprintf (stderr, "This is GNU Makeinfo version %d.%d.\n",
	   major_version, minor_version);
}

/* **************************************************************** */
/*								    */
/*			Generic Utilities			    */
/*								    */
/* **************************************************************** */

/* Just like malloc, but kills the program in case of fatal error. */
void *
xmalloc (nbytes)
     int nbytes;
{
  void *temp = (void *) malloc (nbytes);

  if (nbytes && temp == (void *)NULL)
    memory_error ("xmalloc", nbytes);

  return (temp);
}

/* Like realloc (), but barfs if there isn't enough memory. */
void *
xrealloc (pointer, nbytes)
     void *pointer;
     int nbytes;
{
  void *temp;

  if (!pointer)
    temp = (void *)xmalloc (nbytes);
  else
    temp = (void *)realloc (pointer, nbytes);

  if (nbytes && !temp)
    memory_error ("xrealloc", nbytes);

  return (temp);
}

memory_error (callers_name, bytes_wanted)
     char *callers_name;
     int bytes_wanted;
{
  char printable_string[80];

  sprintf (printable_string,
	   "Virtual memory exhausted in %s ()!  Needed %d bytes.",
	   callers_name, bytes_wanted);

  error (printable_string);
  abort ();
}

/* Tell the user how to use this program. */
usage ()
{
  fprintf (stderr, "Usage: %s [options] texinfo-file...\n\
\n\
This program accepts as input files of texinfo commands and text\n\
and outputs a file suitable for reading with GNU Info.\n\
\n\
The options are:\n\
`-I DIR'              to add DIR to the directory search list for including\n\
                      files with the `@include' command.\n\
-D VAR                to define a variable, as with `@set'.\n\
-U VAR                to undefine a variable, as with `@clear'.\n\
`--no-validate'       to suppress node cross reference validation.\n\
`--no-warn'           to suppress warning messages (errors are still output).\n\
`--no-split'          to suppress the splitting of large files.\n\
`--no-headers'        to suppress the output of Node: Foo headers.\n\
`--verbose'           to print information about what is being done.\n\
`--version'           to print the version number of Makeinfo.\n\
`--output FILE' or `-o FILE'\n\
                      to specify the output file.  When you specify the\n\
                      output file in this way, any `@setfilename' in the\n\
                      input file is ignored.\n\
`--paragraph-indent NUM'\n\
                      to set the paragraph indent to NUM (default %d).\n\
`--fill-column NUM'   to set the filling column to NUM (default %d).\n\
`--error-limit NUM'   to set the error limit to NUM (default %d).\n\
`--reference-limit NUM'\n\
                      to set the reference warning limit to NUM (default %d).\n\
`--footnote-style STYLE'\n\
                      to set the footnote style to STYLE.  STYLE should\n\
                      either be `separate' to place footnotes in their own\n\
                      node, or `end', to place the footnotes at the end of\n\
                      the node in which they are defined (the default).\n\n",
	   progname, paragraph_start_indent,
	   fill_column, max_error_level, reference_warning_limit);
  exit (FATAL);
}

/* **************************************************************** */
/*								    */
/*			Manipulating Lists      		    */
/*					        		    */
/* **************************************************************** */

typedef struct generic_list {
  struct generic_list *next;
} GENERIC_LIST;

/* Reverse the chain of structures in LIST.  Output the new head
   of the chain.  You should always assign the output value of this
   function to something, or you will lose the chain. */
GENERIC_LIST *
reverse_list (list)
     register GENERIC_LIST *list;
{
  register GENERIC_LIST *next;
  register GENERIC_LIST *prev = (GENERIC_LIST *) NULL;

  while (list)
    {
      next = list->next;
      list->next = prev;
      prev = list;
      list = next;
    }
  return (prev);
}


/* **************************************************************** */
/*								    */
/*			Pushing and Popping Files       	    */
/*								    */
/* **************************************************************** */

/* Find and load the file named FILENAME.  Return a pointer to
   the loaded file, or NULL if it can't be loaded. */
char *
find_and_load (filename)
     char *filename;
{
  struct stat fileinfo;
  int file = -1, n, i, count = 0;
  char *fullpath, *result, *get_file_info_in_path ();

  result = fullpath = (char *)NULL;

  fullpath = get_file_info_in_path (filename, include_files_path, &fileinfo);

  if (!fullpath)
    goto error_exit;

  filename = fullpath;

  file = open (filename, O_RDONLY);
  if (file < 0)
    goto error_exit;

  /* Load the file. */
  result = (char *)xmalloc (1 + fileinfo.st_size);

  /* VMS stat lies about the st_size value.  The actual number of
     readable bytes is always less than this value.  The arcane
     mysteries of VMS/RMS are too much to probe, so this hack
    suffices to make things work. */
#if defined (VMS)
  while ((n = read (file, result + count, fileinfo.st_size)) > 0)
    count += n;
  if (n == -1)
#else /* !VMS */
    count = fileinfo.st_size;
    if (read (file, result, fileinfo.st_size) != fileinfo.st_size)
#endif /* !VMS */
  error_exit:
    {
      if (result)
	free (result);

      if (fullpath)
	free (fullpath);

      if (file != -1)
	close (file);

      return ((char *) NULL);
    }
  close (file);

  /* Set the globals to the new file. */
  input_text = result;
  size_of_input_text = count;
  input_filename = savestring (fullpath);
  node_filename = savestring (fullpath);
  input_text_offset = 0;
  line_number = 1;
  /* Not strictly necessary.  This magic prevents read_token () from doing
     extra unnecessary work each time it is called (that is a lot of times).
     The SIZE_OF_INPUT_TEXT is one past the actual end of the text. */
  input_text[size_of_input_text] = '\n';
  return (result);
}

/* Save the state of the current input file. */
pushfile ()
{
  FSTACK *newstack = (FSTACK *) xmalloc (sizeof (FSTACK));
  newstack->filename = input_filename;
  newstack->text = input_text;
  newstack->size = size_of_input_text;
  newstack->offset = input_text_offset;
  newstack->line_number = line_number;
  newstack->next = filestack;

  filestack = newstack;
  push_node_filename ();
}

/* Make the current file globals be what is on top of the file stack. */
popfile ()
{
  extern int executing_string;
  FSTACK *temp = filestack;

  if (!filestack)
    abort ();			/* My fault.  I wonder what I did? */

  /* Make sure that commands with braces have been satisfied. */
  if (!executing_string)
    discard_braces ();

  /* Get the top of the stack into the globals. */
  input_filename = filestack->filename;
  input_text = filestack->text;
  size_of_input_text = filestack->size;
  input_text_offset = filestack->offset;
  line_number = filestack->line_number;

  /* Pop the stack. */
  filestack = filestack->next;
  free (temp);
  pop_node_filename ();
}

/* Flush all open files on the file stack. */
flush_file_stack ()
{
  while (filestack)
    {
      free (input_filename);
      free (input_text);
      popfile ();
    }
}

int node_filename_stack_index = 0;
int node_filename_stack_size = 0;
char **node_filename_stack = (char **)NULL;

push_node_filename ()
{
  if (node_filename_stack_index + 1 > node_filename_stack_size)
    {
      if (!node_filename_stack)
	node_filename_stack =
	  (char **)xmalloc ((node_filename_stack_size += 10)
			    * sizeof (char *));
      else
	node_filename_stack =
	  (char **)xrealloc (node_filename_stack,
			     (node_filename_stack_size + 10)
			     * sizeof (char *));
    }

  node_filename_stack[node_filename_stack_index] = node_filename;
  node_filename_stack_index++;
}

pop_node_filename ()
{
  node_filename = node_filename_stack[--node_filename_stack_index];
}

/* Return just the simple part of the filename; i.e. the
   filename without the path information, or extensions.
   This conses up a new string. */
char *
filename_part (filename)
     char *filename;
{
  char *basename;

  basename = strrchr (filename, '/');
  if (!basename)
    basename = filename;
  else
    basename++;

  basename = savestring (basename);
#if defined (REMOVE_OUTPUT_EXTENSIONS)

  /* See if there is an extension to remove.  If so, remove it. */
  {
    char *temp;

    temp = strrchr (basename, '.');
    if (temp)
      *temp = '\0';
  }
#endif /* REMOVE_OUTPUT_EXTENSIONS */
  return (basename);
}

/* Return the pathname part of filename.  This can be NULL. */
char *
pathname_part (filename)
     char *filename;
{
  char *expand_filename ();
  char *result = (char *) NULL;
  register int i;

  filename = expand_filename (filename, "");

  i = strlen (filename) - 1;

  while (i && filename[i] != '/')
    i--;
  if (filename[i] == '/')
    i++;

  if (i)
    {
      result = (char *)xmalloc (1 + i);
      strncpy (result, filename, i);
      result[i] = '\0';
    }
  free (filename);
  return (result);
}

/* Return the expansion of FILENAME. */
char *
expand_filename (filename, input_name)
     char *filename, *input_name;
{
  char *full_pathname ();
  filename = full_pathname (filename);

  if (filename[0] == '.')
    return (filename);

  if (filename[0] != '/' && input_name[0] == '/')
    {
      /* Make it so that relative names work. */
      char *result;
      int i = strlen (input_name) - 1;

      result = (char *)xmalloc (1 + strlen (input_name) + strlen (filename));
      strcpy (result, input_name);

      while (result[i] != '/' && i)
	i--;

      if (result[i] == '/')
	i++;

      strcpy (&result[i], filename);
      free (filename);
      return (result);
    }
  return (filename);
}

/* Return the full path to FILENAME. */
char *
full_pathname (filename)
     char *filename;
{
  int initial_character;

  if (filename && (initial_character = *filename))
    {
      if (initial_character == '/')
	return (savestring (filename));
      if (initial_character != '~')
	{
	  return (savestring (filename));
	}
      else
	{
	  if (filename[1] == '/')
	    {
	      /* Return the concatenation of HOME and the rest of the string. */
	      char *temp_home;
	      char *temp_name;

	      temp_home = (char *) getenv ("HOME");
	      temp_name = (char *)xmalloc (strlen (&filename[2])
					   + 1
					   + temp_home ? strlen (temp_home)
					   : 0);
	      if (temp_home)
		strcpy (temp_name, temp_home);

	      strcat (temp_name, &filename[2]);
	      return (temp_name);
	    }
	  else
	    {
	      struct passwd *user_entry;
	      int i, c;
	      char *username = (char *)xmalloc (257);
	      char *temp_name;

	      for (i = 1; c = filename[i]; i++)
		{
		  if (c == '/')
		    break;
		  else
		    username[i - 1] = c;
		}
	      if (c)
		username[i - 1] = '\0';

	      user_entry = getpwnam (username);

	      if (!user_entry)
		return (savestring (filename));

	      temp_name = (char *)xmalloc (1 + strlen (user_entry->pw_dir)
					   + strlen (&filename[i]));
	      strcpy (temp_name, user_entry->pw_dir);
	      strcat (temp_name, &filename[i]);
	      return (temp_name);
	    }
	}
    }
  else
    {
      return (savestring (filename));
    }
}

/* **************************************************************** */
/*								    */
/*			Error Handling				    */
/*								    */
/* **************************************************************** */

/* Number of errors encountered. */
int errors_printed = 0;

/* Print the last error gotten from the file system. */
fs_error (filename)
     char *filename;
{
  remember_error ();
  perror (filename);
  return (0);
}

/* Print an error message, and return false. */
#if defined (HAVE_VARARGS_H) && defined (HAVE_VFPRINTF)

int
error (va_alist)
     va_dcl
{
  char *format;
  va_list args;

  remember_error ();
  va_start (args);
  format = va_arg (args, char *);
  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");
}

/* Just like error (), but print the line number as well. */
int
line_error (va_alist)
     va_dcl
{
  char *format;
  va_list args;

  remember_error ();
  va_start (args);
  format = va_arg (args, char *);
  fprintf (stderr, "%s:%d: ", input_filename, line_number);
  vfprintf (stderr, format, args);
  fprintf (stderr, ".\n");
  va_end (args);
  return ((int) 0);
}

int
warning (va_alist)
     va_dcl
{
  char *format;
  va_list args;

  va_start (args);
  format = va_arg (args, char *);
  if (print_warnings)
    {
      fprintf (stderr, "%s:%d: Warning: ", input_filename, line_number);
      vfprintf (stderr, format, args);
      fprintf (stderr, ".\n");
    }
  va_end (args);
  return ((int) 0);
}

#else /* !(HAVE_VARARGS_H && HAVE_VFPRINTF) */

int
error (format, arg1, arg2, arg3, arg4, arg5)
     char *format;
{
  remember_error ();
  fprintf (stderr, format, arg1, arg2, arg3, arg4, arg5);
  fprintf (stderr, "\n");
  return ((int) 0);
}

/* Just like error (), but print the line number as well. */
int
line_error (format, arg1, arg2, arg3, arg4, arg5)
     char *format;
{
  remember_error ();
  fprintf (stderr, "%s:%d: ", input_filename, line_number);
  fprintf (stderr, format, arg1, arg2, arg3, arg4, arg5);
  fprintf (stderr, ".\n");
  return ((int) 0);
}

int
warning (format, arg1, arg2, arg3, arg4, arg5)
     char *format;
{
  if (print_warnings)
    {
      fprintf (stderr, "%s:%d: Warning: ", input_filename, line_number);
      fprintf (stderr, format, arg1, arg2, arg3, arg4, arg5);
      fprintf (stderr, ".\n");
    }
  return ((int) 0);
}

#endif /* !(HAVE_VARARGS_H && HAVE_VFPRINTF) */

/* Remember that an error has been printed.  If this is the first
   error printed, then tell them which program is printing them.
   If more than max_error_level have been printed, then exit the
   program. */
remember_error ()
{
  errors_printed++;
  if (max_error_level && (errors_printed > max_error_level))
    {
      fprintf (stderr, "Too many errors!  Gave up.\n");
      flush_file_stack ();
      cm_bye ();
      exit (1);
    }
}

/* **************************************************************** */
/*								    */
/*			Hacking Tokens and Strings		    */
/*								    */
/* **************************************************************** */

/* Return the next token as a string pointer.  We cons the
   string. */
char *
read_token ()
{
  int i, character;
  char *result;

  /* If the first character to be read is self-delimiting, then that
     is the command itself. */
  character = curchar ();
  if (self_delimiting (character))
    {
      input_text_offset++;
      result = savestring (" ");
      *result = character;
      return (result);
    }

  for (i = 0; ((input_text_offset != size_of_input_text)
	       && (character = curchar ())
	       && command_char (character));
       i++, input_text_offset++);
  result = (char *)xmalloc (i + 1);
  memcpy (result, &input_text[input_text_offset - i], i);
  result[i] = '\0';
  return (result);
}

/* Return non-zero if CHARACTER is self-delimiting. */
int
self_delimiting (character)
     int character;
{
  return (member (character, "{}:.@*'`,!?; \n"));
}

/* Clear whitespace from the front and end of string. */
canon_white (string)
     char *string;
{
  int len = strlen (string);
  int x;

  if (!len)
    return;

  for (x = 0; x < len; x++)
    {
      if (!cr_or_whitespace (string[x]))
	{
	  strcpy (string, string + x);
	  break;
	}
    }
  len = strlen (string);
  if (len)
    len--;
  while (len > -1 && cr_or_whitespace (string[len]))
    len--;
  string[len + 1] = '\0';
}

/* Bash STRING, replacing all whitespace with just one space. */
fix_whitespace (string)
     char *string;
{
  char *temp = (char *)xmalloc (strlen (string) + 1);
  int string_index = 0;
  int temp_index = 0;
  int c;

  canon_white (string);

  while (string[string_index])
    {
      c = temp[temp_index++] = string[string_index++];

      if (c == ' ' || c == '\n' || c == '\t')
	{
	  temp[temp_index - 1] = ' ';
	  while ((c = string[string_index]) && (c == ' ' ||
						c == '\t' ||
						c == '\n'))
	    string_index++;
	}
    }
  temp[temp_index] = '\0';
  strcpy (string, temp);
  free (temp);
}

/* Discard text until the desired string is found.  The string is
   included in the discarded text. */
discard_until (string)
     char *string;
{
  int temp = search_forward (string, input_text_offset);

  int tt = (temp < 0) ? size_of_input_text : temp + strlen (string);
  int from = input_text_offset;

  /* Find out what line we are on. */
  while (from != tt)
    if (input_text[from++] == '\n')
      line_number++;

  if (temp < 0)
    {
      input_text_offset = size_of_input_text - strlen (string);

      if (strcmp (string, "\n") != 0)
	{
	  line_error ("Expected `%s'", string);
	  return;
	}
    }
  else
    input_text_offset = temp;

  input_text_offset += strlen (string);
}

/* Read characters from the file until we are at MATCH.
   Place the characters read into STRING.
   On exit input_text_offset is after the match string.
   Return the offset where the string starts. */
int
get_until (match, string)
     char *match, **string;
{
  int len, current_point, x, new_point, tem;

  current_point = x = input_text_offset;
  new_point = search_forward (match, input_text_offset);

  if (new_point < 0)
    new_point = size_of_input_text;
  len = new_point - current_point;

  /* Keep track of which line number we are at. */
  tem = new_point + (strlen (match) - 1);
  while (x != tem)
    if (input_text[x++] == '\n')
      line_number++;

  *string = (char *)xmalloc (len + 1);

  memcpy (*string, &input_text[current_point], len);
  (*string)[len] = '\0';

  /* Now leave input_text_offset in a consistent state. */
  input_text_offset = tem;

  if (input_text_offset > size_of_input_text)
    input_text_offset = size_of_input_text;

  return (new_point);
}

/* Read characters from the file until we are at MATCH or end of line.
   Place the characters read into STRING.  */
get_until_in_line (match, string)
     char *match, **string;
{
  int real_bottom, temp;

  real_bottom = size_of_input_text;
  temp = search_forward ("\n", input_text_offset);

  if (temp < 0)
    temp = size_of_input_text;

  size_of_input_text = temp;
  get_until (match, string);
  size_of_input_text = real_bottom;
}

get_rest_of_line (string)
     char **string;
{
  get_until ("\n", string);
  canon_white (*string);

  if (curchar () == '\n')	/* as opposed to the end of the file... */
    {
      line_number++;
      input_text_offset++;
    }
}

/* Backup the input pointer to the previous character, keeping track
   of the current line number. */
backup_input_pointer ()
{
  if (input_text_offset)
    {
      input_text_offset--;
      if (curchar () == '\n')
	line_number--;
    }
}

/* Read characters from the file until we are at MATCH or closing brace.
   Place the characters read into STRING.  */
get_until_in_braces (match, string)
     char *match, **string;
{
  int i, brace = 0;
  int match_len = strlen (match);
  char *temp;

  for (i = input_text_offset; i < size_of_input_text; i++)
    {
      if (input_text[i] == '{')
	brace++;
      else if (input_text[i] == '}')
	brace--;
      else if (input_text[i] == '\n')
	line_number++;

      if (brace < 0 ||
	  (brace == 0 && strncmp (input_text + i, match, match_len) == 0))
	break;
    }

  match_len = i - input_text_offset;
  temp = (char *)xmalloc (2 + match_len);
  strncpy (temp, input_text + input_text_offset, match_len);
  temp[match_len] = '\0';
  input_text_offset = i;
  *string = temp;
}

/* **************************************************************** */
/*								    */
/*			Converting the File     		    */
/*								    */
/* **************************************************************** */

/* Convert the file named by NAME.  The output is saved on the file
   named as the argument to the @setfilename command. */
static char *suffixes[] = {
  "",
  ".texinfo",
  ".texi",
  ".txinfo",
  (char *)NULL
};

convert (name)
     char *name;
{
  char *real_output_filename, *expand_filename (), *filename_part ();
  char *filename = (char *)xmalloc (strlen (name) + 50);
  register int i;

  init_tag_table ();
  init_indices ();
  init_internals ();
  init_paragraph ();

  /* Try to load the file specified by NAME.  If the file isn't found, and
     there is no suffix in NAME, then try NAME.texinfo, and NAME.texi. */
  for (i = 0; suffixes[i]; i++)
    {
      strcpy (filename, name);
      strcat (filename, suffixes[i]);

      if (find_and_load (filename))
	break;

      if (!suffixes[i][0] && strrchr (filename, '.'))
	{
	  fs_error (filename);
	  free (filename);
	  return;
	}
    }

  if (!suffixes[i])
    {
      fs_error (name);
      free (filename);
      return;
    }

  input_filename = filename;

  /* Search this file looking for the special string which starts conversion.
     Once found, we may truly begin. */

  input_text_offset = search_forward ("@setfilename", 0);

  if (input_text_offset < 0)
    {
      if (!command_output_filename)
	{
	  error ("No `@setfilename' found in `%s'", name);
	  goto finished;
	}
    }
  else
    input_text_offset += strlen ("@setfilename");

  real_output_filename = (char *)NULL;

  if (!command_output_filename)
    get_until ("\n", &output_filename);
  else
    {
      if (input_text_offset != -1)
	discard_until ("\n");
      else
	input_text_offset = 0;

      real_output_filename = output_filename = command_output_filename;
      command_output_filename = (char *)NULL;
    }

  canon_white (output_filename);
  printf ("Making info file `%s' from `%s'.\n", output_filename, name);

  if (verbose_mode)
    fprintf (stderr, "  The input file contains %d characters.\n",
	     size_of_input_text);

  if (real_output_filename &&
      strcmp (real_output_filename, "-") == 0)
    {
      output_stream = stdout;
    }
  else
    {
      if (!real_output_filename)
	real_output_filename = expand_filename (output_filename, name);

      output_stream = fopen (real_output_filename, "w");
    }

  if (output_stream == NULL)
    {
      fs_error (real_output_filename);
      goto finished;
    }

  /* Make the displayable filename from output_filename.  Only the base
     portion of the filename need be displayed. */
  pretty_output_filename = filename_part (output_filename);

  /* For this file only, count the number of newlines from the top of
     the file to here.  This way, we keep track of line numbers for
     error reporting.  Line_number starts at 1, since the user isn't
     zero-based. */
  {
    int temp = 0;
    line_number = 1;
    while (temp != input_text_offset)
      if (input_text[temp++] == '\n')
	line_number++;
  }

  if (!no_headers)
    {
      add_word_args ("This is Info file %s, produced by Makeinfo-%d.%d from ",
		     output_filename, major_version, minor_version);
      add_word_args ("the input file %s.\n", input_filename);
    }

  close_paragraph ();
  reader_loop ();

finished:
  close_paragraph ();
  flush_file_stack ();
  if (output_stream != NULL)
    {
      output_pending_notes ();
      free_pending_notes ();
      if (tag_table != NULL)
	{
	  tag_table = (TAG_ENTRY *) reverse_list (tag_table);
	  if (!no_headers)
	    write_tag_table ();
	}

      if (output_stream != stdout)
	fclose (output_stream);

      /* If validating, then validate the entire file right now. */
      if (validating)
	validate_file (real_output_filename, tag_table);

      /* This used to test  && !errors_printed.
	 But some files might have legit warnings.  So split anyway.  */
      if (splitting)
	split_file (real_output_filename, 0);
    }
}

free_and_clear (pointer)
     char **pointer;
{
  if ((*pointer) != (char *) NULL)
    {
      free (*pointer);
      *pointer = (char *) NULL;
    }
}

 /* Initialize some state. */
init_internals ()
{
  free_and_clear (&current_node);
  free_and_clear (&output_filename);
  free_and_clear (&command);
  free_and_clear (&input_filename);
  free_node_references ();
  init_insertion_stack ();
  init_brace_stack ();
  command_index = 0;
  in_menu = 0;
}

init_paragraph ()
{
  free_and_clear (&output_paragraph);
  output_paragraph = (unsigned char *)xmalloc (paragraph_buffer_len);
  output_position = 0;
  output_paragraph[0] = '\0';
  output_paragraph_offset = 0;
  output_column = 0;
  paragraph_is_open = 0;
  current_indent = 0;
}

/* Okay, we are ready to start the conversion.  Call the reader on
   some text, and fill the text as it is output.  Handle commands by
   remembering things like open braces and the current file position on a
   stack, and when the corresponding close brace is found, you can call
   the function with the proper arguments. */
reader_loop ()
{
  int character;
  int done = 0;
  int dash_count = 0;

  while (!done)
    {
      if (input_text_offset >= size_of_input_text)
	{
	  if (filestack)
	    {
	      free (input_filename);
	      free (input_text);
	      popfile ();
	    }
	  else
	    break;
	}

      character = curchar ();

      if (!in_fixed_width_font &&
	  (character == '\'' || character == '`') &&
	  input_text[input_text_offset + 1] == character)
	{
	  input_text_offset++;
	  character = '"';
	}

      if (character == '-')
	{
	  dash_count++;
	  if (dash_count == 2 && !in_fixed_width_font)
	    {
	      input_text_offset++;
	      continue;
	    }
	}
      else
	{
	  dash_count = 0;
	}

      /* If this is a whitespace character, then check to see if the line
	 is blank.  If so, advance to the carriage return. */
      if (whitespace (character))
	{
	  register int i = input_text_offset + 1;

	  while (i < size_of_input_text && whitespace (input_text[i]))
	    i++;

	  if (i == size_of_input_text || input_text[i] == '\n')
	    {
	      if (i == size_of_input_text)
		i--;

	      input_text_offset = i;
	      character = curchar ();
	    }
	}

      if (character == '\n')
	{
	  line_number++;

	  /* Check for a menu entry here, since the "escape sequence"
	     that begins menu entrys is "\n* ". */
	  if (in_menu && input_text_offset + 1 < size_of_input_text)
	    {
	      char *glean_node_from_menu (), *tem;

	      /* Note that the value of TEM is discarded, since it is
		 gauranteed to be NULL when glean_node_from_menu () is
		 called with a non-zero argument. */
	      tem = glean_node_from_menu (1);
	    }
	}

      switch (character)
	{
	case COMMAND_PREFIX:
	  read_command ();
	  if (strcmp (command, "bye") == 0)
	    {
	      done = 1;
	      continue;
	    }
	  break;

	case '{':

	  /* Special case.  I'm not supposed to see this character by itself.
	     If I do, it means there is a syntax error in the input text.
	     Report the error here, but remember this brace on the stack so
	     you can ignore its partner. */

	  line_error ("Misplaced `{'");
	  remember_brace (misplaced_brace);

	  /* Don't advance input_text_offset since this happens in
	     remember_brace ().
	     input_text_offset++;
           */
	  break;

	case '}':
	  pop_and_call_brace ();
	  input_text_offset++;
	  break;

	default:
	  add_char (character);
	  input_text_offset++;
	}
    }
}

/* Find the command corresponding to STRING.  If the command
   is found, return a pointer to the data structure.  Otherwise
   return (-1). */
COMMAND *
get_command_entry (string)
     char *string;
{
  register int i;

  for (i = 0; CommandTable[i].name; i++)
    if (strcmp (CommandTable[i].name, string) == 0)
      return (&CommandTable[i]);

  /* This command is not in our predefined command table.  Perhaps
     it is a user defined command. */
  for (i = 0; i < user_command_array_len; i++)
    if (user_command_array[i] &&
	(strcmp (user_command_array[i]->name, string) == 0))
      return (user_command_array[i]);

  /* Nope, we never heard of this command. */
  return ((COMMAND *) -1);
}

/* input_text_offset is right at the command prefix character.
   Read the next token to determine what to do. */
read_command ()
{
  COMMAND *entry;
  input_text_offset++;
  free_and_clear (&command);
  command = read_token ();

#if defined (HAVE_MACROS)
  /* Check to see if this command is a macro.  If so, execute it here. */
  {
    MACRO_DEF *def;

    def = find_macro (command);

    if (def)
      {
	execute_macro (def);
	return;
      }
  }
#endif /* HAVE_MACROS */

  entry = get_command_entry (command);

  if ((int) entry == -1)
    {
      line_error ("Unknown info command `%s'", command);
      return;
    }

  if (entry->argument_in_braces)
    remember_brace (entry->proc);

  (*(entry->proc)) (START);
}

/* Return the string which invokes PROC; a pointer to a function. */
char *
find_proc_name (proc)
     FUNCTION *proc;
{
  register int i;

  for (i = 0; CommandTable[i].name; i++)
    if (proc == CommandTable[i].proc)
      return (CommandTable[i].name);
  return ("NO_NAME!");
}

init_brace_stack ()
{
  brace_stack = (BRACE_ELEMENT *) NULL;
}

remember_brace (proc)
     FUNCTION *proc;
{
  if (curchar () != '{')
    line_error ("@%s expected `{..}'", command);
  else
    input_text_offset++;
  remember_brace_1 (proc, output_paragraph_offset);
}

/* Remember the current output position here.  Save PROC
   along with it so you can call it later. */
remember_brace_1 (proc, position)
     FUNCTION *proc;
     int position;
{
  BRACE_ELEMENT *new = (BRACE_ELEMENT *) xmalloc (sizeof (BRACE_ELEMENT));
  new->next = brace_stack;
  new->proc = proc;
  new->pos = position;
  new->line = line_number;
  brace_stack = new;
}

/* Pop the top of the brace stack, and call the associated function
   with the args END and POS. */
pop_and_call_brace ()
{
  BRACE_ELEMENT *temp;
  FUNCTION *proc;
  int pos;

  if (brace_stack == (BRACE_ELEMENT *) NULL)
    return (line_error ("Unmatched close brace"));

  pos = brace_stack->pos;
  proc = brace_stack->proc;
  temp = brace_stack->next;
  free (brace_stack);
  brace_stack = temp;

  return ((*proc) (END, pos, output_paragraph_offset));
}

/* You call discard_braces () when you shouldn't have any braces on the stack.
   I used to think that this happens for commands that don't take arguments
   in braces, but that was wrong because of things like @code{foo @@}.  So now
   I only detect it at the beginning of nodes. */
discard_braces ()
{
  int temp_line_number = line_number;
  char *proc_name;

  if (!brace_stack)
    return;

  while (brace_stack)
    {
      line_number = brace_stack->line;
      proc_name = find_proc_name (brace_stack->proc);
      line_error ("@%s missing close brace", proc_name);
      line_number = temp_line_number;
      pop_and_call_brace ();
    }
}

get_char_len (character)
     int character;
{
  /* Return the printed length of the character. */
  int len;

  switch (character)
    {
    case '\t':
      len = (output_column + 8) & 0xf7;
      if (len > fill_column)
	len = fill_column - output_column;
      else
	len = len - output_column;
      break;

    case '\n':
      len = fill_column - output_column;
      break;

    default:
      if (character < ' ')
	len = 2;
      else
	len = 1;
    }
  return (len);
}

#if defined (HAVE_VARARGS_H) && defined (HAVE_VSPRINTF)

add_word_args (va_alist)
     va_dcl
{
  char buffer[1000];
  char *format;
  va_list args;

  va_start (args);
  format = va_arg (args, char *);
  vsprintf (buffer, format, args);
  va_end (args);
  add_word (buffer);
}

#else /* !(HAVE_VARARGS_H && HAVE_VSPRINTF) */

add_word_args (format, arg1, arg2, arg3, arg4, arg5)
     char *format;
{
  char buffer[1000];
  sprintf (buffer, format, arg1, arg2, arg3, arg4, arg5);
  add_word (buffer);
}

#endif /* !(HAVE_VARARGS_H && HAVE_VSPRINTF) */

/* Add STRING to output_paragraph. */
add_word (string)
     char *string;
{
  while (*string)
    add_char (*string++);
}

/* Non-zero if the last character inserted has the syntax class of NEWLINE. */
int last_char_was_newline = 1;

/* The actual last inserted character.  Note that this may be something
   other than NEWLINE even if last_char_was_newline is 1. */
int last_inserted_character = 0;

/* Non-zero means that a newline character has already been
   inserted, so close_paragraph () should insert one less. */
int line_already_broken = 0;

/* When non-zero we have finished an insertion (see end_insertion ()) and we
   want to ignore false continued paragraph closings. */
int insertion_paragraph_closed = 0;

/* Add the character to the current paragraph.  If filling_enabled is
   non-zero, then do filling as well. */
add_char (character)
     int character;
{
  /* If we are avoiding outputting headers, and we are currently
     in a menu, then simply return. */
  if (no_headers && in_menu)
    return;

  /* If we are adding a character now, then we don't have to
     ignore close_paragraph () calls any more. */
  if (must_start_paragraph && character != '\n')
    {
      must_start_paragraph = 0;
      line_already_broken = 0;	/* The line is no longer broken. */
      if (current_indent > output_column)
	{
	  indent (current_indent - output_column);
	  output_column = current_indent;
	}
    }

  if (non_splitting_words && member (character, " \t\n"))
    character = ' ' | 0x80;

  insertion_paragraph_closed = 0;

  switch (character)
    {
    case '\n':
      if (!filling_enabled)
	{
	  insert ('\n');

	  if (force_flush_right)
	    {
	      close_paragraph ();
	      /* Hack to force single blank lines out in this mode. */
	      flush_output ();
	    }

	  output_column = 0;

	  if (!no_indent && paragraph_is_open)
	    indent (output_column = current_indent);
	  break;
	}
      else /* CHARACTER is newline, and filling is enabled. */
	{
	  if (sentence_ender (last_inserted_character))
	    {
	      insert (' ');
	      output_column++;
	      last_inserted_character = character;
	    }
	}

      if (last_char_was_newline)
	{
	  close_paragraph ();
	  pending_indent = 0;
	}
      else
	{
	  last_char_was_newline = 1;
	  insert (' ');
	  output_column++;
	}
      break;

    default:
      {
	int len = get_char_len (character);
	int suppress_insert = 0;

	if ((character == ' ') && (last_char_was_newline))
	  {
	    if (!paragraph_is_open)
	      {
		pending_indent++;
		return;
	      }
	  }

	if (!paragraph_is_open)
	  {
	    start_paragraph ();

	    /* If the paragraph is supposed to be indented a certain way,
	       then discard all of the pending whitespace.  Otherwise, we
	       let the whitespace stay. */
	    if (!paragraph_start_indent)
	      indent (pending_indent);
	    pending_indent = 0;
	  }

	if ((output_column += len) > fill_column)
	  {
	    if (filling_enabled)
	      {
		int temp = output_paragraph_offset;
		while (--temp > 0 && output_paragraph[temp] != '\n')
		  {
		    /* If we have found a space, we have the place to break
		       the line. */
		    if (output_paragraph[temp] == ' ')
		      {
			/* Remove trailing whitespace from output. */
			while (temp && whitespace (output_paragraph[temp - 1]))
			  temp--;

			output_paragraph[temp++] = '\n';

			/* We have correctly broken the line where we want
			   to.  What we don't want is spaces following where
			   we have decided to break the line.  We get rid of
			   them. */
			{
			  int t1 = temp;

			  for (;; t1++)
			    {
			      if (t1 == output_paragraph_offset)
				{
				  if (whitespace (character))
				    suppress_insert = 1;
				  break;
				}
			      if (!whitespace (output_paragraph[t1]))
				break;
			    }

			  if (t1 != temp)
			    {
			      strncpy ((char *) &output_paragraph[temp],
				       (char *) &output_paragraph[t1],
				       (output_paragraph_offset - t1));
			      output_paragraph_offset -= (t1 - temp);
			    }
			}

			/* Filled, but now indent if that is right. */
			if (indented_fill && current_indent)
			  {
			    int buffer_len = ((output_paragraph_offset - temp)
					      + current_indent);
			    char *temp_buffer = (char *)xmalloc (buffer_len);
			    int indentation = 0;

			    /* We have to shift any markers that are in
			       front of the wrap point. */
			    {
			      register BRACE_ELEMENT *stack = brace_stack;

			      while (stack)
				{
				  if (stack->pos >= temp)
				    stack->pos += current_indent;
				  stack = stack->next;
				}
			    }

			    while (current_indent > 0 &&
				   indentation != current_indent)
			      temp_buffer[indentation++] = ' ';

			    strncpy ((char *) &temp_buffer[current_indent],
				     (char *) &output_paragraph[temp],
				     buffer_len - current_indent);

			    if (output_paragraph_offset + buffer_len
				>= paragraph_buffer_len)
			      {
				unsigned char *tt = xrealloc
				  (output_paragraph,
				   (paragraph_buffer_len += buffer_len));
				output_paragraph = tt;
			      }
			    strncpy ((char *) &output_paragraph[temp],
				     temp_buffer, buffer_len);
			    output_paragraph_offset += current_indent;
			    free (temp_buffer);
			  }
			output_column = 0;
			while (temp < output_paragraph_offset)
			  output_column +=
			    get_char_len (output_paragraph[temp++]);
			output_column += len;
			break;
		      }
		  }
	      }
	  }

	if (!suppress_insert)
	  {
	    insert (character);
	    last_inserted_character = character;
	  }
	last_char_was_newline = 0;
	line_already_broken = 0;
      }
    }
}

/* Insert CHARACTER into OUTPUT_PARAGRAPH. */
insert (character)
     int character;
{
  output_paragraph[output_paragraph_offset++] = character;
  if (output_paragraph_offset == paragraph_buffer_len)
    {
      output_paragraph =
	xrealloc (output_paragraph, (paragraph_buffer_len += 100));
    }
}

/* Remove upto COUNT characters of whitespace from the
   the current output line.  If COUNT is less than zero,
   then remove until none left. */
kill_self_indent (count)
     int count;
{
  /* Handle infinite case first. */
  if (count < 0)
    {
      output_column = 0;
      while (output_paragraph_offset)
	{
	  if (whitespace (output_paragraph[output_paragraph_offset - 1]))
	    output_paragraph_offset--;
	  else
	    break;
	}
    }
  else
    {
      while (output_paragraph_offset && count--)
	if (whitespace (output_paragraph[output_paragraph_offset - 1]))
	  output_paragraph_offset--;
	else
	  break;
    }
}

/* Non-zero means do not honor calls to flush_output (). */
static int flushing_ignored = 0;

/* Prevent calls to flush_output () from having any effect. */
inhibit_output_flushing ()
{
  flushing_ignored++;
}

/* Allow calls to flush_output () to write the paragraph data. */
uninhibit_output_flushing ()
{
  flushing_ignored--;
}

flush_output ()
{
  register int i;

  if (!output_paragraph_offset || flushing_ignored)
    return;

  for (i = 0; i < output_paragraph_offset; i++)
    {
      if (output_paragraph[i] == (unsigned char)(' ' | 0x80) ||
	  output_paragraph[i] == (unsigned char)('\t' | 0x80) ||
	  output_paragraph[i] == (unsigned char)('\n' | 0x80) ||
	  sentence_ender (UNMETA (output_paragraph[i])))
	output_paragraph[i] &= 0x7f;
    }

  fwrite (output_paragraph, 1, output_paragraph_offset, output_stream);

  output_position += output_paragraph_offset;
  output_paragraph_offset = 0;
}

/* How to close a paragraph controlling the number of lines between
   this one and the last one. */

/* Paragraph spacing is controlled by this variable.  It is the number of
   blank lines that you wish to appear between paragraphs.  A value of
   1 creates a single blank line between paragraphs. */
int paragraph_spacing = DEFAULT_PARAGRAPH_SPACING;

/* Close the current paragraph, leaving no blank lines between them. */
close_single_paragraph ()
{
  close_paragraph_with_lines (0);
}

/* Close a paragraph after an insertion has ended. */
close_insertion_paragraph ()
{
  if (!insertion_paragraph_closed)
    {
      /* Close the current paragraph, breaking the line. */
      close_single_paragraph ();

      /* Start a new paragraph here, inserting whatever indention is correct
	 for the now current insertion level (one above the one that we are
	 ending). */
      start_paragraph ();

      /* Tell close_paragraph () that the previous line has already been
	 broken, so it should insert one less newline. */
      line_already_broken = 1;

      /* Let functions such as add_char () know that we have already found a
	 newline. */
      ignore_blank_line ();
    }
  else
    {
      /* If the insertion paragraph is closed already, then we are seeing
	 two `@end' commands in a row.  Note that the first one we saw was
	 handled in the first part of this if-then-else clause, and at that
	 time start_paragraph () was called, partially to handle the proper
	 indentation of the current line.  However, the indentation level
	 may have just changed again, so we may have to outdent the current
	 line to the new indentation level. */
      if (current_indent < output_column)
	kill_self_indent (output_column - current_indent);
    }

  insertion_paragraph_closed = 1;
}

close_paragraph_with_lines (lines)
     int lines;
{
  int old_spacing = paragraph_spacing;
  paragraph_spacing = lines;
  close_paragraph ();
  paragraph_spacing = old_spacing;
}

/* Close the currently open paragraph. */
close_paragraph ()
{
  register int i;

  /* The insertion paragraph is no longer closed. */
  insertion_paragraph_closed = 0;

  if (paragraph_is_open && !must_start_paragraph)
    {
      register int tindex, c;

      tindex = output_paragraph_offset;

      /* Back up to last non-newline/space character, forcing all such
	 subsequent characters to be newlines.  This isn't strictly
	 necessary, but a couple of functions use the presence of a newline
	 to make decisions. */
      for (tindex = output_paragraph_offset - 1; tindex >= 0; --tindex)
	{
	  c = output_paragraph[tindex];

	  if (c == ' '|| c == '\n')
	    output_paragraph[tindex] = '\n';
	  else
	    break;
	}

      /* All trailing whitespace is ignored. */
      output_paragraph_offset = ++tindex;

      /* Break the line if that is appropriate. */
      if (paragraph_spacing >= 0)
	insert ('\n');

      /* Add as many blank lines as is specified in PARAGRAPH_SPACING. */
      if (!force_flush_right)
	{
	  for (i = 0; i < (paragraph_spacing - line_already_broken); i++)
	    insert ('\n');
	}

      /* If we are doing flush right indentation, then do it now
	 on the paragraph (really a single line). */
      if (force_flush_right)
	do_flush_right_indentation ();

      flush_output ();
      paragraph_is_open = 0;
      no_indent = 0;
      output_column = 0;
    }
  ignore_blank_line ();
}

/* Make the last line just read look as if it were only a newline. */
ignore_blank_line ()
{
  last_inserted_character = '\n';
  last_char_was_newline = 1;
}

/* Align the end of the text in output_paragraph with fill_column. */
do_flush_right_indentation ()
{
  char *temp;
  int temp_len;

  kill_self_indent (-1);

  if (output_paragraph[0] != '\n')
    {
      output_paragraph[output_paragraph_offset] = '\0';

      if (output_paragraph_offset < fill_column)
	{
	  register int i;

	  if (fill_column >= paragraph_buffer_len)
	    output_paragraph =
	      xrealloc (output_paragraph,
			(paragraph_buffer_len += fill_column));

	  temp_len = strlen ((char *)output_paragraph);
	  temp = (char *)xmalloc (temp_len + 1);
	  memcpy (temp, (char *)output_paragraph, temp_len);

	  for (i = 0; i < fill_column - output_paragraph_offset; i++)
	    output_paragraph[i] = ' ';

	  memcpy ((char *)output_paragraph + i, temp, temp_len);
	  free (temp);
	  output_paragraph_offset = fill_column;
	}
    }
}

/* Begin a new paragraph. */
start_paragraph ()
{
  /* First close existing one. */
  if (paragraph_is_open)
    close_paragraph ();

  /* In either case, the insertion paragraph is no longer closed. */
  insertion_paragraph_closed = 0;

  /* However, the paragraph is open! */
  paragraph_is_open = 1;

  /* If we MUST_START_PARAGRAPH, that simply means that start_paragraph ()
     had to be called before we would allow any other paragraph operations
     to have an effect. */
  if (!must_start_paragraph)
    {
      int amount_to_indent = 0;

      /* If doing indentation, then insert the appropriate amount. */
      if (!no_indent)
	{
	  if (inhibit_paragraph_indentation)
	    {
	      amount_to_indent = current_indent;
	      if (inhibit_paragraph_indentation < 0)
		inhibit_paragraph_indentation++;
	    }
	  else if (paragraph_start_indent < 0)
	    amount_to_indent = current_indent;
	  else
	    amount_to_indent = current_indent + paragraph_start_indent;

	  if (amount_to_indent >= output_column)
	    {
	      amount_to_indent -= output_column;
	      indent (amount_to_indent);
	      output_column += amount_to_indent;
	    }
	}
    }
  else
    must_start_paragraph = 0;
}

/* Insert the indentation specified by AMOUNT. */
indent (amount)
     int amount;
{
  register BRACE_ELEMENT *elt = brace_stack;

  /* For every START_POS saved within the brace stack which will be affected
     by this indentation, bump that start pos forward. */
  while (elt)
    {
      if (elt->pos >= output_paragraph_offset)
	elt->pos += amount;
      elt = elt->next;
    }

  while (--amount >= 0)
    insert (' ');
}

/* Search forward for STRING in input_text.
   FROM says where where to start. */
search_forward (string, from)
     char *string;
     int from;
{
  int len = strlen (string);

  while (from < size_of_input_text)
    {
      if (strncmp (input_text + from, string, len) == 0)
	return (from);
      from++;
    }
  return (-1);
}

/* Whoops, Unix doesn't have stricmp. */

/* Case independent string compare. */
stricmp (string1, string2)
     char *string1, *string2;
{
  char ch1, ch2;

  for (;;)
    {
      ch1 = *string1++;
      ch2 = *string2++;

      if (!(ch1 | ch2))
	return (0);

      ch1 = coerce_to_upper (ch1);
      ch2 = coerce_to_upper (ch2);

      if (ch1 != ch2)
	return (ch1 - ch2);
    }
}

enum insertion_type { menu, quotation, lisp, smalllisp, example,
  smallexample, display, itemize, format, enumerate, cartouche, table,
  ftable, vtable, group, ifinfo, flushleft, flushright, ifset, ifclear, deffn,
  defun, defmac, defspec, defvr, defvar, defopt, deftypefn,
  deftypefun, deftypevr, deftypevar, defcv, defivar, defop, defmethod,
  deftypemethod, deftp, bad_type };

char *insertion_type_names[] = { "menu", "quotation", "lisp",
  "smalllisp", "example", "smallexample", "display", "itemize",
  "format", "enumerate", "cartouche", "table", "ftable", "vtable", "group",
  "ifinfo", "flushleft", "flushright", "ifset", "ifclear", "deffn",
  "defun", "defmac", "defspec", "defvr", "defvar", "defopt",
  "deftypefn", "deftypefun", "deftypevr", "deftypevar", "defcv",
  "defivar", "defop", "defmethod", "deftypemethod", "deftp",
  "bad_type" };

int insertion_level = 0;
typedef struct istack_elt
{
  struct istack_elt *next;
  char *item_function;
  int line_number;
  int filling_enabled;
  int indented_fill;
  enum insertion_type insertion;
  int inhibited;
} INSERTION_ELT;

INSERTION_ELT *insertion_stack = (INSERTION_ELT *) NULL;

init_insertion_stack ()
{
  insertion_stack = (INSERTION_ELT *) NULL;
}

/* Return the type of the current insertion. */
enum insertion_type
current_insertion_type ()
{
  if (!insertion_level)
    return (bad_type);
  else
    return (insertion_stack->insertion);
}

/* Return a pointer to the string which is the function to wrap around
   items. */
char *
current_item_function ()
{
  register int level, done;
  register INSERTION_ELT *elt;

  level = insertion_level;
  elt = insertion_stack;
  done = 0;

  /* Skip down through the stack until we find a non-conditional insertion. */
  while (!done)
    {
      switch (elt->insertion)
	{
	case ifinfo:
	case ifset:
	case ifclear:
	case cartouche:
	  elt = elt->next;
	  level--;
	  break;

	default:
	  done = 1;
	}
    }

  if (!level)
    return ((char *) NULL);
  else
    return (elt->item_function);
}

char *
get_item_function ()
{
  char *item_function;
  get_rest_of_line (&item_function);
  backup_input_pointer ();
  canon_white (item_function);
  return (item_function);
}

 /* Push the state of the current insertion on the stack. */
push_insertion (type, item_function)
     enum insertion_type type;
     char *item_function;
{
  INSERTION_ELT *new = (INSERTION_ELT *) xmalloc (sizeof (INSERTION_ELT));

  new->item_function = item_function;
  new->filling_enabled = filling_enabled;
  new->indented_fill = indented_fill;
  new->insertion = type;
  new->line_number = line_number;
  new->inhibited = inhibit_paragraph_indentation;
  new->next = insertion_stack;
  insertion_stack = new;
  insertion_level++;
}

 /* Pop the value on top of the insertion stack into the
    global variables. */
pop_insertion ()
{
  INSERTION_ELT *temp = insertion_stack;

  if (temp == (INSERTION_ELT *) NULL)
    return;

  inhibit_paragraph_indentation = temp->inhibited;
  filling_enabled = temp->filling_enabled;
  indented_fill = temp->indented_fill;
  free_and_clear (&(temp->item_function));
  insertion_stack = insertion_stack->next;
  free (temp);
  insertion_level--;
}

 /* Return a pointer to the print name of this
    enumerated type. */
char *
insertion_type_pname (type)
     enum insertion_type type;
{
  if ((int) type < (int) bad_type)
    return (insertion_type_names[(int) type]);
  else
    return ("Broken-Type in insertion_type_pname");
}

/* Return the insertion_type associated with NAME.
   If the type is not one of the known ones, return BAD_TYPE. */
enum insertion_type
find_type_from_name (name)
     char *name;
{
  int index = 0;
  while (index < (int) bad_type)
    {
      if (strcmp (name, insertion_type_names[index]) == 0)
	return (enum insertion_type) index;
      index++;
    }
  return (bad_type);
}

do_nothing ()
{
}

int
defun_insertion (type)
     enum insertion_type type;
{
  return
    ((type == deffn)
     || (type == defun)
     || (type == defmac)
     || (type == defspec)
     || (type == defvr)
     || (type == defvar)
     || (type == defopt)
     || (type == deftypefn)
     || (type == deftypefun)
     || (type == deftypevr)
     || (type == deftypevar)
     || (type == defcv)
     || (type == defivar)
     || (type == defop)
     || (type == defmethod)
     || (type == deftypemethod)
     || (type == deftp));
}

/* MAX_NS is the maximum nesting level for enumerations.  I picked 100
   which seemed reasonable.  This doesn't control the number of items,
   just the number of nested lists. */
#define max_stack_depth 100
#define ENUM_DIGITS 1
#define ENUM_ALPHA  2
typedef struct {
  int enumtype;
  int enumval;
} DIGIT_ALPHA;

DIGIT_ALPHA enumstack[max_stack_depth];
int enumstack_offset = 0;
int current_enumval = 1;
int current_enumtype = ENUM_DIGITS;
char *enumeration_arg = (char *)NULL;

start_enumerating (at, type)
     int at, type;
{
  if ((enumstack_offset + 1) == max_stack_depth)
    {
      line_error ("Enumeration stack overflow");
      return;
    }
  enumstack[enumstack_offset].enumtype = current_enumtype;
  enumstack[enumstack_offset].enumval = current_enumval;
  enumstack_offset++;
  current_enumval = at;
  current_enumtype = type;
}

stop_enumerating ()
{
  --enumstack_offset;
  if (enumstack_offset < 0)
    enumstack_offset = 0;

  current_enumval = enumstack[enumstack_offset].enumval;
  current_enumtype = enumstack[enumstack_offset].enumtype;
}

/* Place a letter or digits into the output stream. */
enumerate_item ()
{
  char temp[10];

  if (current_enumtype == ENUM_ALPHA)
    {
      if (current_enumval == ('z' + 1) || current_enumval == ('Z' + 1))
	{
	  current_enumval = ((current_enumval - 1) == 'z' ? 'a' : 'A');
	  warning ("Lettering overflow, restarting at %c", current_enumval);
	}
      sprintf (temp, "%c. ", current_enumval);
    }
  else
    sprintf (temp, "%d. ", current_enumval);

  indent (output_column += (current_indent - strlen (temp)));
  add_word (temp);
  current_enumval++;
}

/* This is where the work for all the "insertion" style
   commands is done.  A huge switch statement handles the
   various setups, and generic code is on both sides. */
begin_insertion (type)
     enum insertion_type type;
{
  int no_discard = 0;

  if (defun_insertion (type))
    {
      push_insertion (type, savestring (""));
      no_discard++;
    }
  else
    push_insertion (type, get_item_function ());

  switch (type)
    {
    case menu:
      if (!no_headers)
	close_paragraph ();

      filling_enabled = no_indent = 0;
      inhibit_paragraph_indentation = 1;

      if (!no_headers)
	add_word ("* Menu:\n");

      in_menu++;
      no_discard++;
      break;

      /* I think @quotation is meant to do filling.
	 If you don't want filling, then use @example. */
    case quotation:
      close_single_paragraph ();
      last_char_was_newline = no_indent = 0;
      indented_fill = filling_enabled = 1;
      inhibit_paragraph_indentation = 1;
      current_indent += default_indentation_increment;
      break;

    case display:
    case example:
    case smallexample:
    case lisp:
    case smalllisp:
      /* Just like @example, but no indentation. */
    case format:

      close_single_paragraph ();
      inhibit_paragraph_indentation = 1;
      in_fixed_width_font++;
      filling_enabled = 0;
      last_char_was_newline = 0;

      if (type != format)
	current_indent += default_indentation_increment;

      break;

    case table:
    case ftable:
    case vtable:
    case itemize:
      close_single_paragraph ();
      current_indent += default_indentation_increment;
      filling_enabled = indented_fill = 1;
#if defined (INDENT_PARAGRAPHS_IN_TABLE)
      inhibit_paragraph_indentation = 0;
#else
      inhibit_paragraph_indentation = 1;
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

      /* Make things work for losers who forget the itemize syntax. */
      if (type == itemize)
	{
	  if (!(*insertion_stack->item_function))
	    {
	      free (insertion_stack->item_function);
	      insertion_stack->item_function = savestring ("@bullet");
	    }
	}
      break;

    case enumerate:
      close_single_paragraph ();
      no_indent = 0;
#if defined (INDENT_PARAGRAPHS_IN_TABLE)
      inhibit_paragraph_indentation = 0;
#else
      inhibit_paragraph_indentation = 1;
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

      current_indent += default_indentation_increment;
      filling_enabled = indented_fill = 1;

      if (isdigit (*enumeration_arg))
	start_enumerating (atoi (enumeration_arg), ENUM_DIGITS);
      else
	start_enumerating (*enumeration_arg, ENUM_ALPHA);
      break;

      /* Does nothing special in makeinfo. */
    case group:
      /* Only close the paragraph if we are not inside of an @example. */
      if (!insertion_stack->next ||
	  insertion_stack->next->insertion != example)
	close_single_paragraph ();
      break;

      /* Insertions that are no-ops in info, but do something in TeX. */
    case ifinfo:
    case ifset:
    case ifclear:
    case cartouche:
      if (in_menu)
	no_discard++;
      break;

    case deffn:
    case defun:
    case defmac:
    case defspec:
    case defvr:
    case defvar:
    case defopt:
    case deftypefn:
    case deftypefun:
    case deftypevr:
    case deftypevar:
    case defcv:
    case defivar:
    case defop:
    case defmethod:
    case deftypemethod:
    case deftp:
      inhibit_paragraph_indentation = 1;
      filling_enabled = indented_fill = 1;
      current_indent += default_indentation_increment;
      no_indent = 0;
      break;

    case flushleft:
      close_single_paragraph ();
      inhibit_paragraph_indentation = 1;
      filling_enabled = indented_fill = no_indent = 0;
      break;

    case flushright:
      close_single_paragraph ();
      filling_enabled = indented_fill = no_indent = 0;
      inhibit_paragraph_indentation = 1;
      force_flush_right++;
      break;
    }

  if (!no_discard)
    discard_until ("\n");
}

/* Try to end the insertion with the specified TYPE.
   TYPE, with a value of bad_type,  gets translated to match
   the value currently on top of the stack.
   Otherwise, if TYPE doesn't match the top of the insertion stack,
   give error. */
end_insertion (type)
     enum insertion_type type;
{
  enum insertion_type temp_type;

  if (!insertion_level)
    return;

  temp_type = current_insertion_type ();

  if (type == bad_type)
    type = temp_type;

  if (type != temp_type)
    {
      line_error
	("`%cend' expected `%s', but saw `%s'.", COMMAND_PREFIX,
	 insertion_type_pname (temp_type), insertion_type_pname (type));
      return;
    }

  pop_insertion ();

  switch (type)
    {
      /* Insertions which have no effect on paragraph formatting. */
    case ifinfo:
    case ifset:
    case ifclear:
      break;

    case menu:
      in_menu--;		/* No longer hacking menus. */
      if (!no_headers)
	close_insertion_paragraph ();
      break;

    case enumerate:
      stop_enumerating ();
      close_insertion_paragraph ();
      current_indent -= default_indentation_increment;
      break;

    case flushleft:
    case group:
    case cartouche:
      close_insertion_paragraph ();
      break;

    case format:
    case display:
    case example:
    case smallexample:
    case lisp:
    case smalllisp:
    case quotation:

      /* @quotation is the only one of the above without a fixed width
	 font. */
      if (type != quotation)
	in_fixed_width_font--;

      /* @format is the only fixed_width insertion without a change
	 in indentation. */
      if (type != format)
	current_indent -= default_indentation_increment;

      /* The ending of one of these insertions always marks the
	 start of a new paragraph. */
      close_insertion_paragraph ();
      break;

    case table:
    case ftable:
    case vtable:
    case itemize:
      current_indent -= default_indentation_increment;
      break;

    case flushright:
      force_flush_right--;
      close_insertion_paragraph ();
      break;

      /* Handle the @defun style insertions with a default clause. */
    default:
      current_indent -= default_indentation_increment;
      close_insertion_paragraph ();
      break;
    }
}

/* Insertions cannot cross certain boundaries, such as node beginnings.  In
   code that creates such boundaries, you should call discard_insertions ()
   before doing anything else.  It prints the errors for you, and cleans up
   the insertion stack. */
discard_insertions ()
{
  int real_line_number = line_number;
  while (insertion_stack)
    {
      if (insertion_stack->insertion == ifinfo ||
	  insertion_stack->insertion == ifset ||
	  insertion_stack->insertion == ifclear ||
	  insertion_stack->insertion == cartouche)
	break;
      else
	{
	  char *offender = (char *)
	    insertion_type_pname (insertion_stack->insertion);

	  line_number = insertion_stack->line_number;
	  line_error ("This `%s' doesn't have a matching `%cend %s'", offender,
		      COMMAND_PREFIX, offender);
	  pop_insertion ();
	}
    }
  line_number = real_line_number;
}

/* The actual commands themselves. */

/* Commands which insert themselves. */
insert_self ()
{
  add_word (command);
}

/* Force a line break in the output. */
cm_asterisk ()
{
  close_single_paragraph ();
#if !defined (ASTERISK_NEW_PARAGRAPH)
  cm_noindent ();
#endif /* ASTERISK_NEW_PARAGRAPH */
}

/* Insert ellipsis. */
cm_dots (arg)
     int arg;
{
  if (arg == START)
    add_word ("...");
}

cm_bullet (arg)
     int arg;
{
  if (arg == START)
    add_char ('*');
}

cm_minus (arg)
     int arg;
{
  if (arg == START)
    add_char ('-');
}

/* Insert "TeX". */
cm_TeX (arg)
     int arg;
{
  if (arg == START)
    add_word ("TeX");
}

cm_copyright (arg)
     int arg;
{
  if (arg == START)
    add_word ("(C)");
}

cm_today (arg)
     int arg;
{
  static char * months [12] =
    { "January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December" };
  if (arg == START)
    {
      long timer = (time (0));
      struct tm *ts = (localtime (&timer));
      add_word_args
	("%d %s %d",
	 (ts -> tm_mday),
	 (months [ts -> tm_mon]),
	 ((ts -> tm_year) + 1900));
    }
}

cm_code (arg)
     int arg;
{
  extern int printing_index;

  if (printing_index)
    return;

  if (arg == START)
    {
      in_fixed_width_font++;
      add_char ('`');
    }
  else
    {
      add_word ("'");
      in_fixed_width_font--;
    }
}

cm_samp (arg)
     int arg;
{
  cm_code (arg);
}

cm_file (arg)
     int arg;
{
  cm_code (arg);
}

cm_kbd (arg)
     int arg;
{
  cm_code (arg);
}

cm_key (arg)
     int arg;
{
}

/* Convert the character at position into CTL. */
cm_ctrl (arg, position)
     int arg, position;
{
  if (arg == END)
    output_paragraph[position - 1] = CTL (output_paragraph[position]);
}

/* Small Caps in makeinfo just does all caps. */
cm_sc (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (arg == END)
    {
      while (start_pos < end_pos)
	{
	  output_paragraph[start_pos] =
	    coerce_to_upper (output_paragraph[start_pos]);
	  start_pos++;
	}
    }
}

/* @var in makeinfo just uppercases the text. */
cm_var (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (arg == END)
    {
      while (start_pos < end_pos)
	{
	  output_paragraph[start_pos] =
	    coerce_to_upper (output_paragraph[start_pos]);
	  start_pos++;
	}
    }
}

cm_dfn (arg, position)
     int arg, position;
{
  add_char ('"');
}

cm_emph (arg)
     int arg;
{
  add_char ('*');
}

cm_strong (arg, position)
     int arg, position;
{
  cm_emph (arg);
}

cm_cite (arg, position)
     int arg, position;
{
  if (arg == START)
    add_word ("`");
  else
    add_word ("'");
}

/* Current text is italicized. */
cm_italic (arg, start, end)
     int arg, start, end;
{
}

/* Current text is highlighted. */
cm_bold (arg, start, end)
     int arg, start, end;
{
  cm_italic (arg);
}

/* Current text is in roman font. */
cm_roman (arg, start, end)
     int arg, start, end;
{
}

/* Current text is in roman font. */
cm_titlefont (arg, start, end)
     int arg, start, end;
{
}

/* Italicize titles. */
cm_title (arg, start, end)
     int arg, start, end;
{
  cm_italic (arg);
}

/* @refill is a NOP. */
cm_refill ()
{
}

/* Prevent the argument from being split across two lines. */
cm_w (arg, start, end)
     int arg, start, end;
{
  if (arg == START)
    non_splitting_words++;
  else
    non_splitting_words--;
}


/* Explain that this command is obsolete, thus the user shouldn't
   do anything with it. */
cm_obsolete (arg, start, end)
     int arg, start, end;
{
  if (arg == START)
    warning ("The command `@%s' is obsolete", command);
}

/* Insert the text following input_text_offset up to the end of the line
   in a new, separate paragraph.  Directly underneath it, insert a
   line of WITH_CHAR, the same length of the inserted text. */
insert_and_underscore (with_char)
     int with_char;
{
  int len, i, old_no_indent;
  int starting_pos, ending_pos;
  char *temp;

  close_paragraph ();
  filling_enabled =  indented_fill = 0;
  old_no_indent = no_indent;
  no_indent = 1;
  get_rest_of_line (&temp);

  starting_pos = output_position + output_paragraph_offset;
  execute_string ("%s\n", temp);
  ending_pos = output_position + output_paragraph_offset;
  free (temp);

  len = (ending_pos - starting_pos) - 1;
  for (i = 0; i < len; i++)
    add_char (with_char);
  insert ('\n');
  close_paragraph ();
  filling_enabled = 1;
  no_indent = old_no_indent;
}

/* Here is a structure which associates sectioning commands with
   an integer, hopefully to reflect the `depth' of the current
   section. */
struct {
  char *name;
  int level;
} section_alist[] = {
  { "unnumberedsubsubsec", 5 },
  { "unnumberedsubsec", 4 },
  { "unnumberedsec", 3 },
  { "unnumbered", 2 },
  { "appendixsubsubsec", 5 },
  { "appendixsubsec", 4 },
  { "appendixsec", 3 },
  { "appendixsection", 3 },
  { "appendix", 2 },
  { "subsubsec", 5 },
  { "subsubsection", 5 },
  { "subsection", 4 },
  { "section", 3 },
  { "chapter", 2 },
  { "top", 1 },

  { (char *)NULL, 0 }
};

/* Amount to offset the name of sectioning commands to levels by. */
int section_alist_offset = 0;

/* Shift the meaning of @section to @chapter. */
cm_raisesections ()
{
  discard_until ("\n");
  section_alist_offset--;
}

/* Shift the meaning of @chapter to @section. */
cm_lowersections ()
{
  discard_until ("\n");
  section_alist_offset++;
}

/* Return an integer which identifies the type section present in TEXT. */
int
what_section (text)
     char *text;
{
  register int i, j;
  char *t;

 find_section_command:
  for (j = 0; text[j] && cr_or_whitespace (text[j]); j++);
  if (text[j] != '@')
    return (-1);

  text = text + j + 1;

  /* We skip @c, @comment, and @?index commands. */
  if ((strncmp (text, "comment", strlen ("comment")) == 0) ||
      (text[0] == 'c' && cr_or_whitespace (text[1])) ||
      (strcmp (text + 1, "index") == 0))
    {
      while (*text++ != '\n');
      goto find_section_command;
    }

  /* Handle italicized sectioning commands. */
  if (*text == 'i')
    text++;

  for (j = 0; text[j] && !cr_or_whitespace (text[j]); j++);

  for (i = 0; t = section_alist[i].name; i++)
    {
      if (j == strlen (t) && strncmp (t, text, j) == 0)
	{
	  int return_val;

	  return_val = (section_alist[i].level + section_alist_offset);

	  if (return_val < 0)
	    return_val = 0;
	  else if (return_val > 5)
	    return_val = 5;
	  return (return_val);
	}
    }
  return (-1);
}

/* Treat this just like @unnumbered.  The only difference is
   in node defaulting. */
cm_top ()
{
  static int top_encountered = 0;
  cm_unnumbered ();

  /* It is an error to have more than one @top. */
  if (top_encountered)
    {
      TAG_ENTRY *tag = tag_table;

      line_error ("There already is a node having @top as a section");

      while (tag != (TAG_ENTRY *)NULL)
	{
	  if ((tag->flags & IS_TOP))
	    {
	      int old_line_number = line_number;
	      char *old_input_filename = input_filename;

	      line_number = tag->line_no;
	      input_filename = tag->filename;
	      line_error ("Here is the @top node.");
	      input_filename = old_input_filename;
	      line_number = old_line_number;
	      return;
	    }
	  tag = tag->next_ent;
	}
    }
  else
    {
      top_encountered = 1;

      /* The most recently defined node is the top node. */
      if (tag_table)
	tag_table->flags |= IS_TOP;

      /* Now set the logical hierarchical level of the Top node. */
      {
	int orig_offset = input_text_offset;

	input_text_offset = search_forward ("\n@node", orig_offset);

	if (input_text_offset > 0)
	  {
	    int this_section;

	    /* Move to the end of this line, and find out what the
	       sectioning command is here. */
	    while (input_text[input_text_offset] != '\n')
	      input_text_offset++;

	    if (input_text_offset < size_of_input_text)
	      input_text_offset++;

	    this_section = what_section (input_text + input_text_offset);

	    /* If we found a sectioning command, then give the top section
	       a level of this section - 1. */
	    if (this_section != -1)
	      {
		register int i;

		for (i = 0; section_alist[i].name; i++)
		  if (strcmp (section_alist[i].name, "Top") == 0)
		    {
		      section_alist[i].level = this_section - 1;
		      break;
		    }
	      }
	  }
	input_text_offset = orig_offset;
      }
    }
}

/* Organized by level commands.  That is, "*" == chapter, "=" == section. */
char *scoring_characters = "*=-.";

void
sectioning_underscore (command)
     char *command;
{
  char character;
  int level;

  level = what_section (command);
  level -= 2;

  if (level < 0)
    level = 0;

  character = scoring_characters[level];

  insert_and_underscore (character);
}

/* The remainder of the text on this line is a chapter heading. */
cm_chapter ()
{
  sectioning_underscore ("@chapter");
}

/* The remainder of the text on this line is a section heading. */
cm_section ()
{
  sectioning_underscore ("@section");
}

/* The remainder of the text on this line is a subsection heading. */
cm_subsection ()
{
  sectioning_underscore ("@subsection");
}

/* The remainder of the text on this line is a subsubsection heading. */
cm_subsubsection ()
{
  sectioning_underscore ("@subsubsection");
}

/* The remainder of the text on this line is an unnumbered heading. */
cm_unnumbered ()
{
  cm_chapter ();
}

/* The remainder of the text on this line is an unnumbered section heading. */
cm_unnumberedsec ()
{
  cm_section ();
}

/* The remainder of the text on this line is an unnumbered
   subsection heading. */
cm_unnumberedsubsec ()
{
  cm_subsection ();
}

/* The remainder of the text on this line is an unnumbered
   subsubsection heading. */
cm_unnumberedsubsubsec ()
{
  cm_subsubsection ();
}

/* The remainder of the text on this line is an appendix heading. */
cm_appendix ()
{
  cm_chapter ();
}

/* The remainder of the text on this line is an appendix section heading. */
cm_appendixsec ()
{
  cm_section ();
}

/* The remainder of the text on this line is an appendix subsection heading. */
cm_appendixsubsec ()
{
  cm_subsection ();
}

/* The remainder of the text on this line is an appendix
   subsubsection heading. */
cm_appendixsubsubsec ()
{
  cm_subsubsection ();
}

/* Compatibility functions substitute for chapter, section, etc. */
cm_majorheading ()
{
  cm_chapheading ();
}

cm_chapheading ()
{
  cm_chapter ();
}

cm_heading ()
{
  cm_section ();
}

cm_subheading ()
{
  cm_subsection ();
}

cm_subsubheading ()
{
  cm_subsubsection ();
}


/* **************************************************************** */
/*								    */
/*		   Adding nodes, and making tags		    */
/*								    */
/* **************************************************************** */

/* Start a new tag table. */
init_tag_table ()
{
  while (tag_table != (TAG_ENTRY *) NULL)
    {
      TAG_ENTRY *temp = tag_table;
      free (temp->node);
      free (temp->prev);
      free (temp->next);
      free (temp->up);
      tag_table = tag_table->next_ent;
      free (temp);
    }
}

write_tag_table ()
{
  return (write_tag_table_internal (0));	/* Not indirect. */
}

write_tag_table_indirect ()
{
  return (write_tag_table_internal (1));
}

/* Write out the contents of the existing tag table.
   INDIRECT_P says how to format the output. */
write_tag_table_internal (indirect_p)
     int indirect_p;
{
  TAG_ENTRY *node = tag_table;
  int old_indent = no_indent;

  no_indent = 1;
  filling_enabled = 0;
  must_start_paragraph = 0;
  close_paragraph ();

  if (!indirect_p)
    {
      no_indent = 1;
      insert ('\n');
    }

  add_word_args ("\037\nTag Table:\n%s", indirect_p ? "(Indirect)\n" : "");

  while (node != (TAG_ENTRY *) NULL)
    {
      add_word_args ("Node: %s\177%d\n", node->node, node->position);
      node = node->next_ent;
    }

  add_word ("\037\nEnd Tag Table\n");
  flush_output ();
  no_indent = old_indent;
}

char *
get_node_token ()
{
  char *string;

  get_until_in_line (",", &string);

  if (curchar () == ',')
    input_text_offset++;

  canon_white (string);

  /* Allow things like @@nodename. */
  normalize_node_name (string);

  return (string);
}

/* Given a node name in STRING, remove double @ signs, replacing them
   with just one.  Convert "top" and friends into "Top". */
normalize_node_name (string)
     char *string;
{
  register int i, l = strlen (string);

  for (i = 0; i < l; i++)
    {
      if (string[i] == '@' && string[i + 1] == '@')
	{
	  strncpy (string + i, string + i + 1, l - i);
	  l--;
	}
    }
  if (stricmp (string, "Top") == 0)
    strcpy (string, "Top");
}

/* Look up NAME in the tag table, and return the associated
   tag_entry.  If the node is not in the table return NULL. */
TAG_ENTRY *
find_node (name)
     char *name;
{
  TAG_ENTRY *tag = tag_table;

  while (tag != (TAG_ENTRY *) NULL)
    {
      if (strcmp (tag->node, name) == 0)
	return (tag);
      tag = tag->next_ent;
    }
  return ((TAG_ENTRY *) NULL);
}

/* Remember NODE and associates. */
remember_node (node, prev, next, up, position, line_no, no_warn)
     char *node, *prev, *next, *up;
     int position, line_no, no_warn;
{
  /* Check for existence of this tag already. */
  if (validating)
    {
      register TAG_ENTRY *tag = find_node (node);
      if (tag)
	{
	  line_error ("Node `%s' multiply defined (%d is first definition)",
		      node, tag->line_no);
	  return;
	}
    }

  /* First, make this the current node. */
  current_node = node;

  /* Now add it to the list. */
  {
    TAG_ENTRY *new = (TAG_ENTRY *) xmalloc (sizeof (TAG_ENTRY));
    new->node = node;
    new->prev = prev;
    new->next = next;
    new->up = up;
    new->position = position;
    new->line_no = line_no;
    new->filename = node_filename;
    new->touched = 0;		/* not yet referenced. */
    new->flags = 0;
    if (no_warn)
      new->flags |= NO_WARN;
    new->next_ent = tag_table;
    tag_table = new;
  }
}

/* The order is: nodename, nextnode, prevnode, upnode.
   If all of the NEXT, PREV, and UP fields are empty, they are defaulted.
   You must follow a node command which has those fields defaulted
   with a sectioning command (e.g. @chapter) giving the "level" of that node.
   It is an error not to do so.
   The defaults come from the menu in this nodes parent. */
cm_node ()
{
  char *node, *prev, *next, *up;
  int new_node_pos, defaulting, this_section, no_warn = 0;
  extern int already_outputting_pending_notes;

  if (strcmp (command, "nwnode") == 0)
    no_warn = 1;

  /* Get rid of unmatched brace arguments from previous commands. */
  discard_braces ();

  /* There also might be insertions left lying around that haven't been
     ended yet.  Do that also. */
  discard_insertions ();

  if (!already_outputting_pending_notes)
    {
      close_paragraph ();
      output_pending_notes ();
      free_pending_notes ();
    }

  filling_enabled = indented_fill = 0;
  new_node_pos = output_position;
  current_footnote_number = 1;

  node = get_node_token ();
  next = get_node_token ();
  prev = get_node_token ();
  up = get_node_token ();

  no_indent = 1;
  if (!no_headers)
    add_word_args ("\037\nFile: %s,  Node: %s", pretty_output_filename, node);

  /* Check for defaulting of this node's next, prev, and up fields. */
  defaulting = ((strlen (next) == 0) &&
		(strlen (prev) == 0) &&
		(strlen (up) == 0));

  this_section = what_section (input_text + input_text_offset);

  /* If we are defaulting, then look at the immediately following
     sectioning command (error if none) to determine the node's
     level.  Find the node that contains the menu mentioning this node
     that is one level up (error if not found).  That node is the "Up"
     of this node.  Default the "Next" and "Prev" from the menu. */
  if (defaulting)
    {
      NODE_REF *last_ref = (NODE_REF *)NULL;
      NODE_REF *ref = node_references;

      if (this_section < 0)
	{
	  char *polite_section_name = "top";
	  int i;

	  for (i = 0; section_alist[i].name; i++)
	    if (section_alist[i].level == current_section + 1)
	      {
		polite_section_name = section_alist[i].name;
		break;
	      }

	  line_error
	    ("Node `%s' requires a sectioning command (e.g. @%s)",
	     node, polite_section_name);
	}
      else
	{
	  if (stricmp (node, "Top") == 0)
	    {
	      /* Default the NEXT pointer to be the first menu item in
		 this node, if there is a menu in this node. */
	      {
		int orig_offset, orig_size;
		char *glean_node_from_menu ();

		orig_offset = input_text_offset;
		orig_size = search_forward ("\n@node ", orig_offset);

		if (orig_size < 0)
		  orig_size = size_of_input_text;

		input_text_offset = search_forward ("\n@menu", orig_offset);
		if (input_text_offset > -1)
		  {
		    char *nodename_from_menu = (char *)NULL;

		    input_text_offset =
		      search_forward ("\n* ", input_text_offset);

		    if (input_text_offset != -1)
		      nodename_from_menu = glean_node_from_menu (0);

		    if (nodename_from_menu)
		      {
			free (next);
			next = nodename_from_menu;
			prev = savestring ("(DIR)");
			up = savestring ("(DIR)");
		      }
		  }
		input_text_offset = orig_offset;
	      }
	    }

	  while (ref)
	    {
	      if (ref->section == (this_section - 1) &&
		  ref->type == menu_reference &&
		  strcmp (ref->node, node) == 0)
		{
		  char *containing_node = ref->containing_node;

		  free (up);
		  up = savestring (containing_node);

		  if (last_ref &&
		      last_ref->type == menu_reference &&
		      (strcmp (last_ref->containing_node,
			       containing_node) == 0))
		    {
		      free (next);
		      next = savestring (last_ref->node);
		    }

		  while ((ref->section == this_section - 1) &&
			 (ref->next) &&
			 (ref->next->type != menu_reference))
		    ref = ref->next;

		  if (ref->next && ref->type == menu_reference &&
		      (strcmp (ref->next->containing_node,
			       containing_node) == 0))
		    {
		      free (prev);
		      prev = savestring (ref->next->node);
		    }
		  else if (!ref->next &&
			   stricmp (ref->containing_node, "Top") == 0)
		    {
		      free (prev);
		      prev = savestring (ref->containing_node);
		    }
		  break;
		}
	      last_ref = ref;
	      ref = ref->next;
	    }
	}
    }

  if (!no_headers)
    {
      if (*next)
	add_word_args (",  Next: %s", next);

      if (*prev)
	add_word_args (",  Prev: %s", prev);

      if (*up)
	add_word_args (",  Up: %s", up);
    }

  close_paragraph ();
  no_indent = 0;

  if (!*node)
    {
      line_error ("No node name specified for `@%s' command", command);
      free (node);
      free (next);
      free (prev);
      free (up);
    }
  else
    {
      if (!*next) { free (next); next = (char *)NULL; }
      if (!*prev) { free (prev); prev = (char *)NULL; }
      if (!*up) { free (up); up = (char *)NULL; }
      remember_node (node, prev, next, up, new_node_pos, line_number, no_warn);
    }

  /* Change the section only if there was a sectioning command. */
  if (this_section >= 0)
    current_section = this_section;

  filling_enabled = 1;
}

/* Validation of an info file.
   Scan through the list of tag entrys touching the Prev, Next, and Up
   elements of each.  It is an error not to be able to touch one of them,
   except in the case of external node references, such as "(DIR)".

   If the Prev is different from the Up,
   then the Prev node must have a Next pointing at this node.

   Every node except Top must have an Up.
   The Up node must contain some sort of reference, other than a Next,
   to this node.

   If the Next is different from the Next of the Up,
   then the Next node must have a Prev pointing at this node. */
validate_file (filename, tag_table)
     char *filename;
     TAG_ENTRY *tag_table;
{
  char *old_input_filename = input_filename;
  TAG_ENTRY *tags = tag_table;

  while (tags != (TAG_ENTRY *) NULL)
    {
      register TAG_ENTRY *temp_tag;

      input_filename = tags->filename;
      line_number = tags->line_no;

      /* If this is a "no warn" node, don't validate it in any way. */
      if (tags->flags & NO_WARN)
	{
	  tags = tags->next_ent;
	  continue;
	}

      /* If this node has a Next, then make sure that the Next exists. */
      if (tags->next)
	{
	  validate (tags->next, tags->line_no, "Next");

	  /* If the Next node exists, and there is no Up, then make
	     sure that the Prev of the Next points back. */
	  if (temp_tag = find_node (tags->next))
	    {
	      char *prev;

	      if (temp_tag->flags & NO_WARN)
		{
		  /* Do nothing if we aren't supposed to issue warnings
		     about this node. */
		}
	      else
		{
		  prev = temp_tag->prev;
		  if (!prev || (strcmp (prev, tags->node) != 0))
		    {
		      line_error ("Node `%s''s Next field not pointed back to",
				  tags->node);
		      line_number = temp_tag->line_no;
		      input_filename = temp_tag->filename;
		      line_error
			("This node (`%s') is the one with the bad `Prev'",
			 temp_tag->node);
		      input_filename = tags->filename;
		      line_number = tags->line_no;
		      temp_tag->flags |= PREV_ERROR;
		    }
		}
	    }
	}

      /* Validate the Prev field if there is one, and we haven't already
	 complained about it in some way.  You don't have to have a Prev
	 field at this stage. */
      if (!(tags->flags & PREV_ERROR) && tags->prev)
	{
	  int valid = validate (tags->prev, tags->line_no, "Prev");

	  if (!valid)
	    tags->flags |= PREV_ERROR;
	  else
	    {
	      /* If the Prev field is not the same as the Up field,
		 then the node pointed to by the Prev field must have
		 a Next field which points to this node. */
	      if (tags->up && (strcmp (tags->prev, tags->up) != 0))
		{
		  temp_tag = find_node (tags->prev);

		  /* If we aren't supposed to issue warnings about the
		     target node, do nothing. */
		  if (!temp_tag || (temp_tag->flags & NO_WARN))
		    {
		      /* Do nothing. */
		    }
		  else
		    {
		      if (!temp_tag->next ||
			  (strcmp (temp_tag->next, tags->node) != 0))
			{
			  line_error
			    ("Node `%s''s Prev field not pointed back to",
			     tags->node);
			  line_number = temp_tag->line_no;
			  input_filename = temp_tag->filename;
			  line_error
			    ("This node (`%s') is the one with the bad `Next'",
			     temp_tag->node);
			  input_filename = tags->filename;
			  line_number = tags->line_no;
			  temp_tag->flags |= NEXT_ERROR;
			}
		    }
		}
	    }
	}

      if (!tags->up && (stricmp (tags->node, "Top") != 0))
	line_error ("Node `%s' is missing an \"Up\" field", tags->node);
      else if (tags->up)
	{
	  int valid = validate (tags->up, tags->line_no, "Up");

	  /* If node X has Up: Y, then warn if Y fails to have a menu item
	     or note pointing at X, if Y isn't of the form "(Y)". */
	  if (valid && *tags->up != '(')
	    {
	      NODE_REF *nref, *tref, *list;
	      NODE_REF *find_node_reference ();

	      tref = (NODE_REF *) NULL;
	      list = node_references;

	      for (;;)
		{
		  if (!(nref = find_node_reference (tags->node, list)))
		    break;

		  if (strcmp (nref->containing_node, tags->up) == 0)
		    {
		      if (nref->type != menu_reference)
			{
			  tref = nref;
			  list = nref->next;
			}
		      else
			break;
		    }
		  list = nref->next;
		}

	      if (!nref)
		{
		  temp_tag = find_node (tags->up);
		  line_number = temp_tag->line_no;
		  filename = temp_tag->filename;
		  if (!tref)
		    line_error (
"`%s' has an Up field of `%s', but `%s' has no menu item for `%s'",
				tags->node, tags->up, tags->up, tags->node);
		  line_number = tags->line_no;
		  filename = tags->filename;
		}
	    }
	}
      tags = tags->next_ent;
    }

  validate_other_references (node_references);
  /* We have told the user about the references which didn't exist.
     Now tell him about the nodes which aren't referenced. */

  tags = tag_table;
  while (tags != (TAG_ENTRY *) NULL)
    {
      /* If this node is a "no warn" node, do nothing. */
      if (tags->flags & NO_WARN)
	{
	  tags = tags->next_ent;
	  continue;
	}

      /* Special hack.  If the node in question appears to have
         been referenced more than REFERENCE_WARNING_LIMIT times,
         give a warning. */
      if (tags->touched > reference_warning_limit)
	{
	  input_filename = tags->filename;
	  line_number = tags->line_no;
	  warning ("Node `%s' has been referenced %d times",
		   tags->node, tags->touched);
	}

      if (tags->touched == 0)
	{
	  input_filename = tags->filename;
	  line_number = tags->line_no;

	  /* Notice that the node "Top" is special, and doesn't have to
	     be referenced. */
	  if (stricmp (tags->node, "Top") != 0)
	    warning ("Unreferenced node `%s'", tags->node);
	}
      tags = tags->next_ent;
    }
  input_filename = old_input_filename;
}

/* Return 1 if tag correctly validated, or 0 if not. */
validate (tag, line, label)
     char *tag;
     int line;
     char *label;
{
  TAG_ENTRY *result;

  /* If there isn't a tag to verify, or if the tag is in another file,
     then it must be okay. */
  if (!tag || !*tag || *tag == '(')
    return (1);

  /* Otherwise, the tag must exist. */
  result = find_node (tag);

  if (!result)
    {
      line_number = line;
      line_error (
"Validation error.  `%s' field points to node `%s', which doesn't exist",
		  label, tag);
      return (0);
    }
  result->touched++;
  return (1);
}

/* Split large output files into a series of smaller files.  Each file
   is pointed to in the tag table, which then gets written out as the
   original file.  The new files have the same name as the original file
   with a "-num" attached.  SIZE is the largest number of bytes to allow
   in any single split file. */
split_file (filename, size)
     char *filename;
     int size;
{
  char *root_filename, *root_pathname;
  char *the_file, *filename_part ();
  struct stat fileinfo;
  char *the_header;
  int header_size;

  /* Can only do this to files with tag tables. */
  if (!tag_table)
    return;

  if (size == 0)
    size = DEFAULT_SPLIT_SIZE;

  if ((stat (filename, &fileinfo) != 0) ||
      (fileinfo.st_size < SPLIT_SIZE_THRESHOLD))
    return;

  the_file = find_and_load (filename);
  if (!the_file)
    return;

  root_filename = filename_part (filename);
  root_pathname = pathname_part (filename);

  if (!root_pathname)
    root_pathname = savestring ("");

  /* Start splitting the file.  Walk along the tag table
     outputting sections of the file.  When we have written
     all of the nodes in the tag table, make the top-level
     pointer file, which contains indirect pointers and
     tags for the nodes. */
  {
    int which_file = 1;
    TAG_ENTRY *tags = tag_table;
    char *indirect_info = (char *)NULL;

    /* Remember the `header' of this file.  The first tag in the file is
       the bottom of the header; the top of the file is the start. */
    the_header = (char *)xmalloc (1 + (header_size = tags->position));
    memcpy (the_header, the_file, header_size);

    while (tags)
      {
	int file_top, file_bot, limit;

	/* Have to include the Control-_. */
	file_top = file_bot = tags->position;
	limit = file_top + size;

	/* If the rest of this file is only one node, then
	   that is the entire subfile. */
	if (!tags->next_ent)
	  {
	    int i = tags->position + 1;
	    char last_char = the_file[i];

	    while (i < fileinfo.st_size)
	      {
		if ((the_file[i] == '\037') &&
		    ((last_char == '\n') ||
		     (last_char == '\014')))
		  break;
		else
		  last_char = the_file[i];
		i++;
	      }
	    file_bot = i;
	    tags = tags->next_ent;
	    goto write_region;
	  }

	/* Otherwise, find the largest number of nodes that can fit in
	   this subfile. */
	for (; tags; tags = tags->next_ent)
	  {
	    if (!tags->next_ent)
	      {
		/* This entry is the last node.  Search forward for the end
	           of this node, and that is the end of this file. */
		int i = tags->position + 1;
		char last_char = the_file[i];

		while (i < fileinfo.st_size)
		  {
		    if ((the_file[i] == '\037') &&
			((last_char == '\n') ||
			 (last_char == '\014')))
		      break;
		    else
		      last_char = the_file[i];
		    i++;
		  }
		file_bot = i;

		if (file_bot < limit)
		  {
		    tags = tags->next_ent;
		    goto write_region;
		  }
		else
		  {
		    /* Here we want to write out everything before the last
		       node, and then write the last node out in a file
		       by itself. */
		    file_bot = tags->position;
		    goto write_region;
		  }
	      }

	    if (tags->next_ent->position > limit)
	      {
		if (tags->position == file_top)
		  tags = tags->next_ent;

		file_bot = tags->position;

	      write_region:
		{
		  int fd;
		  char *split_filename;

		  split_filename = (char *) xmalloc
		    (10 + strlen (root_pathname) + strlen (root_filename));
		  sprintf
		    (split_filename,
		     "%s%s-%d", root_pathname, root_filename, which_file);

		  fd = open
		    (split_filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);

		  if ((fd < 0) ||
		      (write (fd, the_header, header_size) != header_size) ||
		      (write (fd, the_file + file_top, file_bot - file_top)
		       != (file_bot - file_top)) ||
		      ((close (fd)) < 0))
		    {
		      perror (split_filename);
		      if (fd != -1)
			close (fd);
		      exit (FATAL);
		    }

		  if (!indirect_info)
		    {
		      indirect_info = the_file + file_top;
		      sprintf (indirect_info, "\037\nIndirect:\n");
		      indirect_info += strlen (indirect_info);
		    }

		  sprintf (indirect_info, "%s-%d: %d\n",
			   root_filename, which_file, file_top);

		  free (split_filename);
		  indirect_info += strlen (indirect_info);
		  which_file++;
		  break;
		}
	      }
	  }
      }

    /* We have sucessfully created the subfiles.  Now write out the
       original again.  We must use `output_stream', or
       write_tag_table_indirect () won't know where to place the output. */
    output_stream = fopen (filename, "w");
    if (!output_stream)
      {
	perror (filename);
	exit (FATAL);
      }

    {
      int distance = indirect_info - the_file;
      fwrite (the_file, 1, distance, output_stream);

      /* Inhibit newlines. */
      paragraph_is_open = 0;

      write_tag_table_indirect ();
      fclose (output_stream);
      free (the_header);
      free (the_file);
      return;
    }
  }
}

/* Some menu hacking.  This is used to remember menu references while
   reading the input file.  After the output file has been written, if
   validation is on, then we use the contents of NODE_REFERENCES as a
   list of nodes to validate. */
char *
reftype_type_string (type)
     enum reftype type;
{
  switch (type)
    {
    case menu_reference:
      return ("Menu");
    case followed_reference:
      return ("Followed-Reference");
    default:
      return ("Internal-bad-reference-type");
    }
}

/* Remember this node name for later validation use. */
remember_node_reference (node, line, type)
     char *node;
     int line;
     enum reftype type;
{
  NODE_REF *temp = (NODE_REF *) xmalloc (sizeof (NODE_REF));

  temp->next = node_references;
  temp->node = savestring (node);
  temp->line_no = line;
  temp->section = current_section;
  temp->type = type;
  temp->containing_node = savestring (current_node);
  temp->filename = node_filename;

  node_references = temp;
}

validate_other_references (ref_list)
     register NODE_REF *ref_list;
{
  char *old_input_filename = input_filename;

  while (ref_list != (NODE_REF *) NULL)
    {
      input_filename = ref_list->filename;
      validate (ref_list->node, ref_list->line_no,
		reftype_type_string (ref_list->type));
      ref_list = ref_list->next;
    }
  input_filename = old_input_filename;
}

/* Find NODE in REF_LIST. */
NODE_REF *
find_node_reference (node, ref_list)
     char *node;
     register NODE_REF *ref_list;
{
  while (ref_list)
    {
      if (strcmp (node, ref_list->node) == 0)
	break;
      ref_list = ref_list->next;
    }
  return (ref_list);
}

free_node_references ()
{
  register NODE_REF *list, *temp;

  list = node_references;

  while (list)
    {
      temp = list;
      free (list->node);
      free (list->containing_node);
      list = list->next;
      free (temp);
    }
  node_references = (NODE_REF *) NULL;
}

  /* This function gets called at the start of every line while inside of
     a menu.  It checks to see if the line starts with "* ", and if so,
     remembers the node reference that this menu refers to.
     input_text_offset is at the \n just before the line start. */
#define menu_starter "* "
char *
glean_node_from_menu (remember_reference)
     int remember_reference;
{
  int i, orig_offset = input_text_offset;
  char *nodename;

  if (strncmp (&input_text[input_text_offset + 1],
	       menu_starter,
	       strlen (menu_starter)) != 0)
    return ((char *)NULL);
  else
    input_text_offset += strlen (menu_starter) + 1;

  get_until_in_line (":", &nodename);
  if (curchar () == ':')
    input_text_offset++;
  canon_white (nodename);

  if (curchar () == ':')
    goto save_node;

  free (nodename);
  get_rest_of_line (&nodename);

  /* Special hack: If the nodename follows the menu item name,
     then we have to read the rest of the line in order to find
     out what the nodename is.  But we still have to read the
     line later, in order to process any formatting commands that
     might be present.  So un-count the carriage return that has just
     been counted. */
  line_number--;

  isolate_nodename (nodename);

save_node:
  input_text_offset = orig_offset;
  normalize_node_name (nodename);
  i = strlen (nodename);
  if (i && nodename[i - 1] == ':')
    nodename[i - 1] = '\0';

  if (remember_reference)
    {
      remember_node_reference (nodename, line_number, menu_reference);
      free (nodename);
      return ((char *)NULL);
    }
  else
    return (nodename);
}

static void
isolate_nodename (nodename)
     char *nodename;
{
  register int i, c;
  int paren_seen, paren;

  if (!nodename)
    return;

  canon_white (nodename);
  paren_seen = paren = i = 0;

  if (*nodename == '.' || !*nodename)
    {
      *nodename = '\0';
      return;
    }

  if (*nodename == '(')
    {
      paren++;
      paren_seen++;
      i++;
    }

  for (; c = nodename[i]; i++)
    {
      if (paren)
	{
	  if (c == '(')
	    paren++;
	  else if (c == ')')
	    paren--;

	  continue;
	}

      /* If the character following the close paren is a space, then this
	 node has no more characters associated with it. */
      if (c == '\t' ||
	  c == '\n' ||
	  c == ','  ||
	  ((paren_seen && nodename[i - 1] == ')') &&
	   (c == ' ' || c == '.')) ||
	  (c == '.' &&
	   ((!nodename[i + 1] ||
	     (cr_or_whitespace (nodename[i + 1])) ||
	     (nodename[i + 1] == ')')))))
	break;
    }
  nodename[i] = '\0';
}

cm_menu ()
{
  begin_insertion (menu);
}


/* **************************************************************** */
/*								    */
/*			Cross Reference Hacking			    */
/*								    */
/* **************************************************************** */

char *
get_xref_token ()
{
  char *string;

  get_until_in_braces (",", &string);
  if (curchar () == ',')
    input_text_offset++;
  fix_whitespace (string);
  return (string);
}

int px_ref_flag = 0;		/* Controls initial output string. */

/* Make a cross reference. */
cm_xref (arg)
{
  if (arg == START)
    {
      char *arg1, *arg2, *arg3, *arg4, *arg5;

      arg1 = get_xref_token ();
      arg2 = get_xref_token ();
      arg3 = get_xref_token ();
      arg4 = get_xref_token ();
      arg5 = get_xref_token ();

      add_word_args ("%s", px_ref_flag ? "*note " : "*Note ");

      if (*arg5 || *arg4)
	{
	  char *node_name;

	  if (!*arg2)
	    {
	      if (*arg3)
		node_name = arg3;
	      else
		node_name = arg1;
	    }
	  else
	    node_name = arg2;

	  execute_string ("%s: (%s)%s", node_name, arg4, arg1);
	  return;
	}
      else
	remember_node_reference (arg1, line_number, followed_reference);

      if (*arg3)
	{
	  if (!*arg2)
	    execute_string ("%s: %s", arg3, arg1);
	  else
	    execute_string ("%s: %s", arg2, arg1);
	  return;
	}

      if (*arg2)
	execute_string ("%s: %s", arg2, arg1);
      else
	execute_string ("%s::", arg1);
    }
  else
    {

      /* Check to make sure that the next non-whitespace character is either
         a period or a comma. input_text_offset is pointing at the "}" which
         ended the xref or pxref command. */
      int temp = input_text_offset + 1;

      if (output_paragraph[output_paragraph_offset - 2] == ':' &&
	  output_paragraph[output_paragraph_offset - 1] == ':')
	return;
      while (temp < size_of_input_text)
	{
	  if (cr_or_whitespace (input_text[temp]))
	    temp++;
	  else
	    {
	      if (input_text[temp] == '.' ||
		  input_text[temp] == ',' ||
		  input_text[temp] == '\t')
		return;
	      else
		{
		  line_error (
		"Cross-reference must be terminated with a period or a comma");
		  return;
		}
	    }
	}
    }
}

cm_pxref (arg)
     int arg;
{
  if (arg == START)
    {
      px_ref_flag++;
      cm_xref (arg);
      px_ref_flag--;
    }
  else
    add_char ('.');
}

cm_inforef (arg)
     int arg;
{
  if (arg == START)
    {
      char *node, *pname, *file;

      node = get_xref_token ();
      pname = get_xref_token ();
      file = get_xref_token ();

      execute_string ("*note %s: (%s)%s", pname, file, node);
    }
}

/* **************************************************************** */
/*								    */
/*			Insertion Command Stubs			    */
/*								    */
/* **************************************************************** */

cm_quotation ()
{
  begin_insertion (quotation);
}

cm_example ()
{
  begin_insertion (example);
}

cm_smallexample ()
{
  begin_insertion (smallexample);
}

cm_lisp ()
{
  begin_insertion (lisp);
}

cm_smalllisp ()
{
  begin_insertion (smalllisp);
}

/* @cartouche/@end cartouche draws box with rounded corners in
   TeX output.  Right now, just a NOP insertion. */
cm_cartouche ()
{
  begin_insertion (cartouche);
}

cm_format ()
{
  begin_insertion (format);
}

cm_display ()
{
  begin_insertion (display);
}

cm_itemize ()
{
  begin_insertion (itemize);
}

cm_enumerate ()
{
  do_enumeration (enumerate, "1");
}

/* Start an enumeration insertion of type TYPE.  If the user supplied
   no argument on the line, then use DEFAULT_STRING as the initial string. */
do_enumeration (type, default_string)
     int type;
     char *default_string;
{
  get_until_in_line (".", &enumeration_arg);
  canon_white (enumeration_arg);

  if (!*enumeration_arg)
    {
      free (enumeration_arg);
      enumeration_arg = savestring (default_string);
    }

  if (!isdigit (*enumeration_arg) && !isletter (*enumeration_arg))
    {
      warning ("%s requires a letter or a digit", insertion_type_pname (type));

      switch (type)
	{
	case enumerate:
	  default_string = "1";
	  break;
	}
      enumeration_arg = savestring (default_string);
    }
  begin_insertion (type);
}

cm_table ()
{
  begin_insertion (table);
}

cm_ftable ()
{
  begin_insertion (ftable);
}

cm_vtable ()
{
  begin_insertion (vtable);
}

cm_group ()
{
  begin_insertion (group);
}

cm_ifinfo ()
{
  begin_insertion (ifinfo);
}

/* Begin an insertion where the lines are not filled or indented. */
cm_flushleft ()
{
  begin_insertion (flushleft);
}

/* Begin an insertion where the lines are not filled, and each line is
   forced to the right-hand side of the page. */
cm_flushright ()
{
  begin_insertion (flushright);
}


/* **************************************************************** */
/*								    */
/*			  Conditional Handling			    */
/*								    */
/* **************************************************************** */

/* A structure which contains `defined' variables. */
typedef struct _defines {
  struct _defines *next;
  char *name;
  char *value;
} DEFINE;

/* The linked list of `set' defines. */
DEFINE *defines = (DEFINE *)NULL;

/* Add NAME to the list of `set' defines. */
set (name, value)
     char *name;
     char *value;
{
  DEFINE *temp;

  for (temp = defines; temp; temp = temp->next)
    if (strcmp (name, temp->name) == 0)
      {
	free (temp->value);
	temp->value = savestring (value);
	return;
      }

  temp = (DEFINE *)xmalloc (sizeof (DEFINE));
  temp->next = defines;
  temp->name = savestring (name);
  temp->value = savestring (value);
  defines = temp;
}

/* Remove NAME from the list of `set' defines. */
clear (name)
     char *name;
{
  register DEFINE *temp, *last;

  last = (DEFINE *)NULL;
  temp = defines;

  while (temp)
    {
      if (strcmp (temp->name, name) == 0)
	{
	  if (last)
	    last->next = temp->next;
	  else
	    defines = temp->next;

	  free (temp->name);
	  free (temp->value);
	  free (temp);
	  break;
	}
      last = temp;
      temp = temp->next;
    }
}

/* Return the value of NAME.  The return value is NULL if NAME is unset. */
char *
set_p (name)
     char *name;
{
  register DEFINE *temp;

  for (temp = defines; temp; temp = temp->next)
    if (strcmp (temp->name, name) == 0)
      return (temp->value);

  return ((char *)NULL);
}

/* Conditionally parse based on the current command name. */
command_name_condition ()
{
  char *discarder;

  discarder = (char *)xmalloc (8 + strlen (command));

  sprintf (discarder, "\n@end %s", command);
  discard_until (discarder);
  discard_until ("\n");

  free (discarder);
}

/* Create a variable whose name appears as the first word on this line. */
cm_set ()
{
  handle_variable (SET);
}

/* Remove a variable whose name appears as the first word on this line. */
cm_clear ()
{
  handle_variable (CLEAR);
}

cm_ifset ()
{
  handle_variable (IFSET);
}

cm_ifclear ()
{
  handle_variable (IFCLEAR);
}

cm_value (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (arg == END)
    {
      char *name, *value;
      name = (char *)&output_paragraph[start_pos];
      output_paragraph[end_pos] = '\0';
      name = savestring (name);
      value = set_p (name);
      output_column -= end_pos - start_pos;
      output_paragraph_offset = start_pos;

      if (value)
        execute_string ("%s", value);
      else
	add_word_args ("{No Value For \"%s\"}", name);

      free (name);
    }
}

/* Set, clear, or conditionalize based on ACTION. */
handle_variable (action)
     int action;
{
  char *name;

  get_rest_of_line (&name);
  backup_input_pointer ();
  canon_white (name);
  handle_variable_internal (action, name);
  free (name);
}

handle_variable_internal (action, name)
     int action;
     char *name;
{
  char *temp;
  int delimiter, additional_text_present = 0;

  /* Only the first word of NAME is a valid tag. */
  temp = name;
  delimiter = 0;
  while (*temp && (delimiter || !whitespace (*temp)))
    {
#if defined (SET_WITH_EQUAL)
      if (*temp == '"' || *temp == '\'')
	{
	  if (*temp == delimiter)
	    delimiter = 0;
	  else
	    delimiter = *temp;
	}
#endif /* SET_WITH_EQUAL */
      temp++;
    }

  if (*temp)
    additional_text_present++;

  *temp = '\0';

  if (!*name)
    line_error ("@%s requires a name", command);
  else
    {
      switch (action)
	{
	case SET:
	  {
	    char *value;

#if defined (SET_WITH_EQUAL)
	    /* Allow a value to be saved along with a variable.  The value is
	       the text following an `=' sign in NAME, if any is present. */

	    for (value = name; *value && *value != '='; value++);

	    if (*value)
	      *value++ = '\0';

	    if (*value == '"' || *value == '\'')
	      {
		value++;
		value[strlen (value) - 1] = '\0';
	      }

#else /* !SET_WITH_EQUAL */
	    /* The VALUE of NAME is the remainder of the line sans
	       whitespace. */
	    if (additional_text_present)
	      {
		value = temp + 1;
		canon_white (value);
	      }
	    else
	      value = "";
#endif /* !SET_WITH_VALUE */

	    set (name, value);
	  }
	  break;

	case CLEAR:
	  clear (name);
	  break;

	case IFSET:
	case IFCLEAR:
	  /* If IFSET and NAME is not set, or if IFCLEAR and NAME is set,
	     read lines from the the file until we reach a matching
	     "@end CONDITION".  This means that we only take note of
	     "@ifset/clear" and "@end" commands. */
	  {
	    char condition[8];
	    int condition_len;

	    if (action == IFSET)
	      strcpy (condition, "ifset");
	    else
	      strcpy (condition, "ifclear");

	    condition_len = strlen (condition);

	  if ((action == IFSET && !set_p (name)) ||
	      (action == IFCLEAR && set_p (name)))
	    {
	      int level = 0, done = 0;

	      while (!done)
		{
		  char *freeable_line, *line;

		  get_rest_of_line (&freeable_line);

		  for (line = freeable_line; whitespace (*line); line++);

		  if (*line == COMMAND_PREFIX &&
		      (strncmp (line + 1, condition, condition_len) == 0))
		    level++;
		  else if (strncmp (line, "@end", 4) == 0)
		    {
		      char *cname = line + 4;
		      char *temp;

		      while (*cname && whitespace (*cname))
			cname++;
		      temp = cname;

		      while (*temp && !whitespace (*temp))
			temp++;
		      *temp = '\0';

		      if (strcmp (cname, condition) == 0)
			{
			  if (!level)
			    {
			      done = 1;
			    }
			  else
			    level--;
			}
		    }
		  free (freeable_line);
		}
	      /* We found the end of a false @ifset/ifclear.  If we are
		 in a menu, back up over the newline that ends the ifset,
		 since that newline may also begin the next menu entry. */
	      break;
	    }
	  else
	    {
	      if (action == IFSET)
		begin_insertion (ifset);
	      else
		begin_insertion (ifclear);
	    }
	  }
	  break;
	}
    }
}

/* **************************************************************** */
/*								    */
/*			@itemx, @item				    */
/*								    */
/* **************************************************************** */

/* Non-zero means a string is in execution, as opposed to a file. */
int executing_string = 0;

/* Execute the string produced by formatting the ARGs with FORMAT.  This
   is like submitting a new file with @include. */
#if defined (HAVE_VARARGS_H) && defined(HAVE_VSPRINTF)

execute_string (va_alist)
     va_dcl
{
  static char temp_string[4000];
  char *format;
  va_list args;

  va_start (args);
  format = va_arg (args, char *);
  vsprintf (temp_string, format, args);
  va_end (args);

#else /* !(HAVE_VARARGS_H && HAVE_VSPRINTF) */

execute_string (format, arg1, arg2, arg3, arg4, arg5)
     char *format;
{
  static char temp_string[4000];
  sprintf (temp_string, format, arg1, arg2, arg3, arg4, arg5);

#endif /* !(HAVE_VARARGS_H && HAVE_VSPRINTF) */

  strcat (temp_string, "@bye\n");
  pushfile ();
  input_text_offset = 0;
  input_text = temp_string;
  input_filename = savestring (input_filename);
  size_of_input_text = strlen (temp_string);

  executing_string++;
  reader_loop ();

  popfile ();
  executing_string--;

  free_and_clear (&command);
  command = savestring ("not bye");
}

int itemx_flag = 0;

cm_itemx ()
{
  itemx_flag++;
  cm_item ();
  itemx_flag--;
}

cm_item ()
{
  char *rest_of_line, *item_func;

  /* Can only hack "@item" while inside of an insertion. */
  if (insertion_level)
    {
      INSERTION_ELT *stack = insertion_stack;
      int original_input_text_offset;

      skip_whitespace ();
      original_input_text_offset = input_text_offset;

      get_rest_of_line (&rest_of_line);
      canon_white (rest_of_line);
      item_func = current_item_function ();

      /* Okay, do the right thing depending on which insertion function
	 is active. */

    switch_top:
      switch (stack->insertion)
	{
	case ifinfo:
	case ifset:
	case ifclear:
	case cartouche:
	  stack = stack->next;
	  if (!stack)
	    goto no_insertion;
	  else
	    goto switch_top;
	  break;

	case menu:
	case quotation:
	case example:
	case smallexample:
	case lisp:
	case format:
	case display:
	case group:
	  line_error ("The `@%s' command is meaningless within a `@%s' block",
		      command,
		      insertion_type_pname (current_insertion_type ()));
	  break;

	case itemize:
	case enumerate:
	  if (itemx_flag)
	    {
	      line_error ("@itemx is not meaningful inside of a `%s' block",
			  insertion_type_pname (current_insertion_type ()));
	    }
	  else
	    {
	      start_paragraph ();
	      kill_self_indent (-1);
	      filling_enabled = indented_fill = 1;

	      if (current_insertion_type () == itemize)
		{
		  indent (output_column = current_indent - 2);

		  /* I need some way to determine whether this command
		     takes braces or not.  I believe the user can type
		     either "@bullet" or "@bullet{}".  Of course, they
		     can also type "o" or "#" or whatever else they want. */
		  if (item_func && *item_func)
		    {
		      if (*item_func == '@')
			if (item_func[strlen (item_func) - 1] != '}')
			  execute_string ("%s{}", item_func);
			else
			  execute_string ("%s", item_func);
		      else
			execute_string ("%s", item_func);
		    }
		  insert (' ');
		  output_column++;
		}
	      else
		enumerate_item ();

	      /* Special hack.  This makes close paragraph ignore you until
		 the start_paragraph () function has been called. */
	      must_start_paragraph = 1;

	      /* Ultra special hack.  It appears that some people incorrectly
		 place text directly after the @item, instead of on a new line
		 by itself.  This happens to work in TeX, so I make it work
		 here. */
	      if (*rest_of_line)
		{
		  line_number--;
		  input_text_offset = original_input_text_offset;
		}
	    }
	  break;

	case table:
	case ftable:
	case vtable:
	  {
	    /* Get rid of extra characters. */
	    kill_self_indent (-1);

	    /* close_paragraph () almost does what we want.  The problem
	       is when paragraph_is_open, and last_char_was_newline, and
	       the last newline has been turned into a space, because
	       filling_enabled. I handle it here. */
	    if (last_char_was_newline && filling_enabled && paragraph_is_open)
	      insert ('\n');
	    close_paragraph ();

#if defined (INDENT_PARAGRAPHS_IN_TABLE)
	    /* Indent on a new line, but back up one indentation level. */
	    {
	      int t;

	      t = inhibit_paragraph_indentation;
	      inhibit_paragraph_indentation = 1;
	      /* At this point, inserting any non-whitespace character will
		 force the existing indentation to be output. */
	      add_char ('i');
	      inhibit_paragraph_indentation = t;
	    }
#else /* !INDENT_PARAGRAPHS_IN_TABLE */
	    add_char ('i');
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

	    output_paragraph_offset--;
	    kill_self_indent (default_indentation_increment + 1);

	    /* Add item's argument to the line. */
	    filling_enabled = 0;
	    if (item_func && *item_func)
 	      execute_string ("%s{%s}", item_func, rest_of_line);
 	    else
 	      execute_string ("%s", rest_of_line);

	    if (current_insertion_type () == ftable)
	      execute_string ("@findex %s\n", rest_of_line);

	    if (current_insertion_type () == vtable)
	      execute_string ("@vindex %s\n", rest_of_line);

	    /* Start a new line, and let start_paragraph ()
	       do the indenting of it for you. */
	    close_single_paragraph ();
	    indented_fill = filling_enabled = 1;
	  }
	}
      free (rest_of_line);
    }
  else
    {
    no_insertion:
      line_error ("@%s found outside of an insertion block", command);
    }
}


/* **************************************************************** */
/*								    */
/*			Defun and Friends       		    */
/*								    */
/* **************************************************************** */

#define DEFUN_SELF_DELIMITING(c)					\
  (((c) == '(')								\
   || ((c) == ')')							\
   || ((c) == '[')							\
   || ((c) == ']'))

struct token_accumulator
{
  unsigned int length;
  unsigned int index;
  char **tokens;
};

void
initialize_token_accumulator (accumulator)
     struct token_accumulator *accumulator;
{
  (accumulator->length) = 0;
  (accumulator->index) = 0;
  (accumulator->tokens) = NULL;
}

void
accumulate_token (accumulator, token)
     struct token_accumulator *accumulator;
     char *token;
{
  if ((accumulator->index) >= (accumulator->length))
    {
      (accumulator->length) += 10;
      (accumulator->tokens) = (char **) xrealloc
	(accumulator->tokens, (accumulator->length * sizeof (char *)));
    }
  accumulator->tokens[accumulator->index] = token;
  accumulator->index += 1;
}

char *
copy_substring (start, end)
     char *start;
     char *end;
{
  char *result, *scan, *scan_result;

  result = (char *) xmalloc ((end - start) + 1);
  scan_result = result;
  scan = start;

  while (scan < end)
    *scan_result++ = *scan++;

  *scan_result = '\0';
  return (result);
}

/* Given `string' pointing at an open brace, skip forward and return a
   pointer to just past the matching close brace. */
int
scan_group_in_string (string_pointer)
     char **string_pointer;
{
  register int c;
  register char *scan_string;
  register unsigned int level = 1;

  scan_string = (*string_pointer) + 1;

  while (1)
    {
      if (level == 0)
	{
	  (*string_pointer) = scan_string;
	  return (1);
	}
      c = (*scan_string++);
      if (c == '\0')
	{
	  /* Tweak line_number to compensate for fact that
	     we gobbled the whole line before coming here. */
	  line_number -= 1;
	  line_error ("Missing `}' in @def arg");
	  line_number += 1;
	  (*string_pointer) = (scan_string - 1);
	  return (0);
	}
      if (c == '{')
	level += 1;
      if (c == '}')
	level -= 1;
    }
}

/* Return a list of tokens from the contents of `string'.
   Commands and brace-delimited groups count as single tokens.
   Contiguous whitespace characters are converted to a token
   consisting of a single space. */
char **
args_from_string (string)
     char *string;
{
  struct token_accumulator accumulator;
  register char *scan_string = string;
  char *token_start, *token_end;

  initialize_token_accumulator (&accumulator);

  while ((*scan_string) != '\0')
    {
      /* Replace arbitrary whitespace by a single space. */
      if (whitespace (*scan_string))
	{
	  scan_string += 1;
	  while (whitespace (*scan_string))
	    scan_string += 1;
	  accumulate_token ((&accumulator), (savestring (" ")));
	  continue;
	}

      /* Commands count as single tokens. */
      if ((*scan_string) == COMMAND_PREFIX)
	{
	  token_start = scan_string;
	  scan_string += 1;
	  if (self_delimiting (*scan_string))
	    scan_string += 1;
	  else
	    {
	      register int c;
	      while (1)
		{
		  c = *scan_string++;

 		  if ((c == '\0') || (c == '{') || (whitespace (c)))
		    {
		      scan_string -= 1;
		      break;
		    }
		}

	      if (*scan_string == '{')
		{
		  char *s = scan_string;
		  (void) scan_group_in_string (&s);
		  scan_string = s;
		}
	    }
	  token_end = scan_string;
	}

      /* Parentheses and brackets are self-delimiting. */
      else if (DEFUN_SELF_DELIMITING (*scan_string))
	{
	  token_start = scan_string;
	  scan_string += 1;
	  token_end = scan_string;
	}

      /* Open brace introduces a group that is a single token. */
      else if (*scan_string == '{')
	{
	  char *s = scan_string;
	  int balanced = scan_group_in_string (&s);

	  token_start = scan_string + 1;
	  scan_string = s;
	  token_end = balanced ? (scan_string - 1) : scan_string;
	}

      /* Otherwise a token is delimited by whitespace, parentheses,
	 brackets, or braces.  A token is also ended by a command. */
      else
	{
	  token_start = scan_string;

	  while (1)
	    {
	      register int c;

	      c = *scan_string++;

	      if (!c ||
		  (whitespace (c) || DEFUN_SELF_DELIMITING (c) ||
		   c == '{' || c == '}'))
		{
		  scan_string--;
		  break;
		}

	      /* If we encounter a command imbedded within a token,
		 then end the token. */
	      if (c == COMMAND_PREFIX)
		{
		  scan_string--;
		  break;
		}
	    }
	  token_end = scan_string;
	}

      accumulate_token
	(&accumulator, copy_substring (token_start, token_end));
    }
  accumulate_token (&accumulator, NULL);
  return (accumulator.tokens);
}

void
process_defun_args (defun_args, auto_var_p)
     char **defun_args;
     int auto_var_p;
{
  int pending_space = 0;

  while (1)
    {
      char *defun_arg = *defun_args++;

      if (defun_arg == NULL)
	break;

      if (defun_arg[0] == ' ')
	{
	  pending_space = 1;
	  continue;
	}

      if (pending_space)
	{
	  add_char (' ');
	  pending_space = 0;
	}

      if (DEFUN_SELF_DELIMITING (defun_arg[0]))
	add_char (defun_arg[0]);
      else if (defun_arg[0] == '&')
	add_word (defun_arg);
      else if (defun_arg[0] == COMMAND_PREFIX)
	execute_string ("%s", defun_arg);
      else if (auto_var_p)
	execute_string ("@var{%s}", defun_arg);
      else
	add_word (defun_arg);
    }
}

char *
next_nonwhite_defun_arg (arg_pointer)
     char ***arg_pointer;
{
  char **scan = (*arg_pointer);
  char *arg = (*scan++);

  if ((arg != 0) && (*arg == ' '))
    arg = *scan++;

  if (arg == 0)
    scan -= 1;

  *arg_pointer = scan;

  return ((arg == 0) ? "" : arg);
}

/* Make the defun type insertion.
   TYPE says which insertion this is.
   X_P says not to start a new insertion if non-zero. */
void
defun_internal (type, x_p)
     enum insertion_type type;
     int x_p;
{
  enum insertion_type base_type;
  char **defun_args, **scan_args;
  char *category, *defined_name, *type_name, *type_name2;

  {
    char *line;
    get_rest_of_line (&line);
    defun_args = (args_from_string (line));
    free (line);
  }

  scan_args = defun_args;

  switch (type)
    {
    case defun:
      category = "Function";
      base_type = deffn;
      break;
    case defmac:
      category = "Macro";
      base_type = deffn;
      break;
    case defspec:
      category = "Special Form";
      base_type = deffn;
      break;
    case defvar:
      category = "Variable";
      base_type = defvr;
      break;
    case defopt:
      category = "User Option";
      base_type = defvr;
      break;
    case deftypefun:
      category = "Function";
      base_type = deftypefn;
      break;
    case deftypevar:
      category = "Variable";
      base_type = deftypevr;
      break;
    case defivar:
      category = "Instance Variable";
      base_type = defcv;
      break;
    case defmethod:
      category = "Method";
      base_type = defop;
      break;
    case deftypemethod:
      category = "Method";
      base_type = deftypemethod;
      break;
    default:
      category = next_nonwhite_defun_arg (&scan_args);
      base_type = type;
      break;
    }

  if ((base_type == deftypefn)
      || (base_type == deftypevr)
      || (base_type == defcv)
      || (base_type == defop)
      || (base_type == deftypemethod))
    type_name = next_nonwhite_defun_arg (&scan_args);

  if (base_type == deftypemethod)
    type_name2 = next_nonwhite_defun_arg (&scan_args);

  defined_name = next_nonwhite_defun_arg (&scan_args);

  /* This hack exists solely for the purposes of formatting the texinfo
     manual.  I couldn't think of a better way.  The token might be
     a simple @@ followed immediately by more text.  If this is the case,
     then the next defun arg is part of this one, and we should concatenate
     them. */
  if (*scan_args && **scan_args && !whitespace (**scan_args) &&
      (strcmp (defined_name, "@@") == 0))
    {
      char *tem = (char *)xmalloc (3 + strlen (scan_args[0]));

      sprintf (tem, "@@%s", scan_args[0]);

      free (scan_args[0]);
      scan_args[0] = tem;
      scan_args++;
      defined_name = tem;
    }

  if (!x_p)
    begin_insertion (type);

  /* Write the definition header line.
     This should start at the normal indentation.  */
  current_indent -= default_indentation_increment;
  start_paragraph ();

  switch (base_type)
    {
    case deffn:
    case defvr:
    case deftp:
      execute_string (" -- %s: %s", category, defined_name);
      break;
    case deftypefn:
    case deftypevr:
      execute_string (" -- %s: %s %s", category, type_name, defined_name);
      break;
    case defcv:
      execute_string (" -- %s of %s: %s", category, type_name, defined_name);
      break;
    case defop:
      execute_string (" -- %s on %s: %s", category, type_name, defined_name);
      break;
    case deftypemethod:
      execute_string (" -- %s on %s: %s %s", category, type_name, type_name2,
		      defined_name);
      break;
    }
  current_indent += default_indentation_increment;

  /* Now process the function arguments, if any.
     If these carry onto the next line, they should be indented by two
     increments to distinguish them from the body of the definition,
     which is indented by one increment.  */
  current_indent += default_indentation_increment;

  switch (base_type)
    {
    case deffn:
    case defop:
      process_defun_args (scan_args, 1);
      break;
    case deftp:
    case deftypefn:
    case deftypemethod:
      process_defun_args (scan_args, 0);
      break;
    }
  current_indent -= default_indentation_increment;
  close_single_paragraph ();

  /* Make an entry in the appropriate index. */
  switch (base_type)
    {
    case deffn:
    case deftypefn:
      execute_string ("@findex %s\n", defined_name);
      break;
    case defvr:
    case deftypevr:
    case defcv:
      execute_string ("@vindex %s\n", defined_name);
      break;
    case defop:
    case deftypemethod:
      execute_string ("@findex %s on %s\n", defined_name, type_name);
      break;
    case deftp:
      execute_string ("@tindex %s\n", defined_name);
      break;
    }

  /* Deallocate the token list. */
  scan_args = defun_args;
  while (1)
    {
      char * arg = (*scan_args++);
      if (arg == NULL)
	break;
      free (arg);
    }
  free (defun_args);
}

/* Add an entry for a function, macro, special form, variable, or option.
   If the name of the calling command ends in `x', then this is an extra
   entry included in the body of an insertion of the same type. */
cm_defun ()
{
  int x_p;
  enum insertion_type type;
  char *temp = savestring (command);

  x_p = (command[strlen (command) - 1] == 'x');

  if (x_p)
    temp[strlen (temp) - 1] = '\0';

  type = find_type_from_name (temp);
  free (temp);

  /* If we are adding to an already existing insertion, then make sure
     that we are already in an insertion of type TYPE. */
  if (x_p &&
      (!insertion_level || insertion_stack->insertion != type))
    {
      line_error ("Must be in a `%s' insertion in order to use `%s'x",
		  command, command);
      discard_until ("\n");
      return;
    }

  defun_internal (type, x_p);
}

/* End existing insertion block. */
cm_end ()
{
  char *temp;
  enum insertion_type type;

  if (!insertion_level)
    {
      line_error ("Unmatched `@%s'", command);
      return;
    }

  get_rest_of_line (&temp);
  canon_white (temp);

  if (strlen (temp) == 0)
    line_error ("`@%s' needs something after it", command);

  type = find_type_from_name (temp);

  if (type == bad_type)
    {
      line_error ("Bad argument to `%s', `%s', using `%s'",
	   command, temp, insertion_type_pname (current_insertion_type ()));
    }
  end_insertion (type);
  free (temp);
}


/* **************************************************************** */
/*								    */
/*			Other Random Commands		   	    */
/*								    */
/* **************************************************************** */

/* This says to inhibit the indentation of the next paragraph, but
   not of following paragraphs.  */
cm_noindent ()
{
  if (!inhibit_paragraph_indentation)
    inhibit_paragraph_indentation = -1;
}

/* I don't know exactly what to do with this.  Should I allow
   someone to switch filenames in the middle of output?  Since the
   file could be partially written, this doesn't seem to make sense.
   Another option: ignore it, since they don't *really* want to
   switch files.  Finally, complain, or at least warn. */
cm_setfilename ()
{
  char *filename;
  get_rest_of_line (&filename);
  /* warning ("`@%s %s' encountered and ignored", command, filename); */
  free (filename);
}

cm_ignore_line ()
{
  discard_until ("\n");
}

/* @br can be immediately followed by `{}', so we have to read those here.
   It should simply close the paragraph. */
cm_br ()
{
  if (looking_at ("{}"))
    input_text_offset += 2;

  if (curchar () == '\n')
    {
      input_text_offset++;
      line_number++;
    }

  close_paragraph ();
}

 /* Insert the number of blank lines passed as argument. */
cm_sp ()
{
  int lines;
  char *line;

  get_rest_of_line (&line);

  if (sscanf (line, "%d", &lines) != 1)
    {
      line_error ("%csp requires a positive numeric argument", COMMAND_PREFIX);
    }
  else
    {
      if (lines < 0)
	lines = 0;

      while (lines--)
	add_char ('\n');
    }
  free (line);
}

/* Start a new line with just this text on it.
   Then center the line of text.
   This always ends the current paragraph. */
cm_center ()
{
  register int i, start, length;
  int fudge_factor = 1;
  unsigned char *line;

  close_paragraph ();
  filling_enabled = indented_fill = 0;
  cm_noindent ();
  start = output_paragraph_offset;
  inhibit_output_flushing ();
  get_rest_of_line ((char **)&line);
  execute_string ((char *)line);
  free (line);
  uninhibit_output_flushing ();

  i = output_paragraph_offset - 1;
  while (i > (start - 1) && output_paragraph[i] == '\n')
	i--;

  output_paragraph_offset = ++i;
  length = output_paragraph_offset - start;

  if (length < (fill_column - fudge_factor))
    {
      line = (unsigned char *)xmalloc (1 + length);
      memcpy (line, (char *)(output_paragraph + start), length);

      i = (fill_column - fudge_factor - length) / 2;
      output_paragraph_offset = start;

      while (i--)
	insert (' ');

      for (i = 0; i < length; i++)
	insert (line[i]);

      free (line);
    }

  insert ('\n');
  close_paragraph ();
  filling_enabled = 1;
}

/* Show what an expression returns. */
cm_result (arg)
     int arg;
{
  if (arg == END)
    add_word ("=>");
}

/* What an expression expands to. */
cm_expansion (arg)
     int arg;
{
  if (arg == END)
    add_word ("==>");
}

/* Indicates two expressions are equivalent. */
cm_equiv (arg)
     int arg;
{
  if (arg == END)
    add_word ("==");
}

/* What an expression may print. */
cm_print (arg)
     int arg;
{
  if (arg == END)
    add_word ("-|");
}

/* An error signaled. */
cm_error (arg)
     int arg;
{
  if (arg == END)
    add_word ("error-->");
}

/* The location of point in an example of a buffer. */
cm_point (arg)
     int arg;
{
  if (arg == END)
    add_word ("-!-");
}

/* Start a new line with just this text on it.
   The text is outdented one level if possible. */
cm_exdent ()
{
  char *line;
  int i = current_indent;

  if (current_indent)
    current_indent -= default_indentation_increment;

  get_rest_of_line (&line);
  close_single_paragraph ();
  execute_string ("%s", line);
  current_indent = i;
  free (line);
  close_single_paragraph ();
}

cm_include ()
{
  cm_infoinclude ();
}

/* Remember this file, and move onto the next. */
cm_infoinclude ()
{
  char *filename;

  close_paragraph ();
  get_rest_of_line (&filename);
  pushfile ();

  /* In verbose mode we print info about including another file. */
  if (verbose_mode)
    {
      register int i = 0;
      register FSTACK *stack = filestack;

      for (i = 0, stack = filestack; stack; stack = stack->next, i++);

      i *= 2;

      printf ("%*s", i, "");
      printf ("%c%s %s\n", COMMAND_PREFIX, command, filename);
      fflush (stdout);
    }

  if (!find_and_load (filename))
    {
      extern const char * const sys_errlist[];
      extern int errno, sys_nerr;
      popfile ();

      /* Cannot "@include foo", in line 5 of "/wh/bar". */
      line_error ("`%c%s %s': %s", COMMAND_PREFIX, command, filename,
		  ((errno < sys_nerr) ?
		   sys_errlist[errno] : "Unknown file system error"));
    }
  free (filename);
}

/* The other side of a malformed expression. */
misplaced_brace ()
{
  line_error ("Misplaced `}'");
}

/* Don't let the filling algorithm insert extra whitespace here. */
cm_force_abbreviated_whitespace ()
{
}

/* Do not let this character signify the end of a sentence, though
   if it was seen without the command prefix it normally would.  We
   do this by turning on the 8th bit of the character. */
cm_ignore_sentence_ender ()
{
  add_char (META ((*command)));
}

/* Signals end of processing.  Easy to make this happen. */
cm_bye ()
{
  input_text_offset = size_of_input_text;
}

cm_asis ()
{
}

cm_math ()
{
}


/* **************************************************************** */
/*								    */
/*			Indexing Stuff				    */
/*								    */
/* **************************************************************** */


/* An index element... */
typedef struct index_elt
{
  struct index_elt *next;
  char *entry;			/* The index entry itself. */
  char *node;			/* The node from whence it came. */
  int code;			/* Non-zero means add `@code{...}' when
				   printing this element. */
  int defining_line;		/* Line number where this entry was written. */
} INDEX_ELT;

/* A list of short-names for each index, and the index to that index in our
   index array, the_indices.  In addition, for each index, it is remembered
   whether that index is a code index or not.  Code indices have @code{}
   inserted around the first word when they are printed with printindex. */
typedef struct
{
  char *name;
  int index;
  int code;
} INDEX_ALIST;

INDEX_ALIST **name_index_alist = (INDEX_ALIST **) NULL;

/* An array of pointers.  Each one is for a different index.  The
   "synindex" command changes which array slot is pointed to by a
   given "index". */
INDEX_ELT **the_indices = (INDEX_ELT **) NULL;

/* The number of defined indices. */
int defined_indices = 0;

/* We predefine these. */
#define program_index 0
#define function_index 1
#define concept_index 2
#define variable_index 3
#define datatype_index 4
#define key_index 5

init_indices ()
{
  int i;

  /* Create the default data structures. */

  /* Initialize data space. */
  if (!the_indices)
    {
      the_indices = (INDEX_ELT **) xmalloc ((1 + defined_indices) *
					    sizeof (INDEX_ELT *));
      the_indices[defined_indices] = (INDEX_ELT *) NULL;

      name_index_alist = (INDEX_ALIST **) xmalloc ((1 + defined_indices) *
						   sizeof (INDEX_ALIST *));
      name_index_alist[defined_indices] = (INDEX_ALIST *) NULL;
    }

  /* If there were existing indices, get rid of them now. */
  for (i = 0; i < defined_indices; i++)
    undefindex (name_index_alist[i]->name);

  /* Add the default indices. */
  defindex ("pg", 0);
  defindex ("fn", 1);		/* "fn" is a code index.  */
  defindex ("cp", 0);
  defindex ("vr", 0);
  defindex ("tp", 0);
  defindex ("ky", 0);

}

/* Find which element in the known list of indices has this name.
   Returns -1 if NAME isn't found. */
int
find_index_offset (name)
     char *name;
{
  register int i;
  for (i = 0; i < defined_indices; i++)
    if (name_index_alist[i] &&
	strcmp (name, name_index_alist[i]->name) == 0)
      return (name_index_alist[i]->index);
  return (-1);
}

/* Return a pointer to the entry of (name . index) for this name.
   Return NULL if the index doesn't exist. */
INDEX_ALIST *
find_index (name)
     char *name;
{
  int offset = find_index_offset (name);
  if (offset > -1)
    return (name_index_alist[offset]);
  else
    return ((INDEX_ALIST *) NULL);
}

/* Given an index name, return the offset in the_indices of this index,
   or -1 if there is no such index. */
translate_index (name)
     char *name;
{
  INDEX_ALIST *which = find_index (name);

  if (which)
    return (which->index);
  else
    return (-1);
}

/* Return the index list which belongs to NAME. */
INDEX_ELT *
index_list (name)
     char *name;
{
  int which = translate_index (name);
  if (which < 0)
    return ((INDEX_ELT *) - 1);
  else
    return (the_indices[which]);
}

/* Please release me, let me go... */
free_index (index)
     INDEX_ELT *index;
{
  INDEX_ELT *temp;

  while ((temp = index) != (INDEX_ELT *) NULL)
    {
      free (temp->entry);
      free (temp->node);
      index = index->next;
      free (temp);
    }
}

/* Flush an index by name. */
undefindex (name)
     char *name;
{
  int i;
  int which = find_index_offset (name);

  if (which < 0)
    return (which);

  i = name_index_alist[which]->index;


  free_index (the_indices[i]);
  the_indices[i] = (INDEX_ELT *) NULL;

  free (name_index_alist[which]->name);
  free (name_index_alist[which]);
  name_index_alist[which] = (INDEX_ALIST *) NULL;
}

/* Define an index known as NAME.  We assign the slot number.
   CODE if non-zero says to make this a code index. */
defindex (name, code)
     char *name;
     int code;
{
  register int i, slot;

  /* If it already exists, flush it. */
  undefindex (name);

  /* Try to find an empty slot. */
  slot = -1;
  for (i = 0; i < defined_indices; i++)
    if (!name_index_alist[i])
      {
	slot = i;
	break;
      }

  if (slot < 0)
    {
      /* No such luck.  Make space for another index. */
      slot = defined_indices;
      defined_indices++;

      name_index_alist = (INDEX_ALIST **)
	xrealloc ((char *)name_index_alist,
		  (1 + defined_indices) * sizeof (INDEX_ALIST *));
      the_indices = (INDEX_ELT **)
	xrealloc ((char *)the_indices,
		  (1 + defined_indices) * sizeof (INDEX_ELT *));
    }

  /* We have a slot.  Start assigning. */
  name_index_alist[slot] = (INDEX_ALIST *) xmalloc (sizeof (INDEX_ALIST));
  name_index_alist[slot]->name = savestring (name);
  name_index_alist[slot]->index = slot;
  name_index_alist[slot]->code = code;

  the_indices[slot] = (INDEX_ELT *) NULL;
}

/* Add the arguments to the current index command to the index NAME. */
index_add_arg (name)
     char *name;
{
  int which;
  char *index_entry;
  INDEX_ALIST *tem;

  tem = find_index (name);

  which = tem ? tem->index : -1;

  get_rest_of_line (&index_entry);
  ignore_blank_line ();

  if (which < 0)
    {
      line_error ("Unknown index reference `%s'", name);
      free (index_entry);
    }
  else
    {
      INDEX_ELT *new = (INDEX_ELT *) xmalloc (sizeof (INDEX_ELT));
      new->next = the_indices[which];
      new->entry = index_entry;
      new->node = current_node;
      new->code = tem->code;
      new->defining_line = line_number - 1;
      the_indices[which] = new;
    }
}

#define INDEX_COMMAND_SUFFIX "index"

/* The function which user defined index commands call. */
gen_index ()
{
  char *name = savestring (command);
  if (strlen (name) >= strlen ("index"))
    name[strlen (name) - strlen ("index")] = '\0';
  index_add_arg (name);
  free (name);
}

/* Define a new index command.  Arg is name of index. */
cm_defindex ()
{
  gen_defindex (0);
}

cm_defcodeindex ()
{
  gen_defindex (1);
}

gen_defindex (code)
     int code;
{
  char *name;
  get_rest_of_line (&name);

  if (find_index (name))
    {
      line_error ("Index `%s' already exists", name);
      free (name);
      return;
    }
  else
    {
      char *temp = (char *) alloca (1 + strlen (name) + strlen ("index"));
      sprintf (temp, "%sindex", name);
      define_user_command (temp, gen_index, 0);
      defindex (name, code);
      free (name);
    }
}

/* Append LIST2 to LIST1.  Return the head of the list. */
INDEX_ELT *
index_append (head, tail)
     INDEX_ELT *head, *tail;
{
  register INDEX_ELT *t_head = head;

  if (!t_head)
    return (tail);

  while (t_head->next)
    t_head = t_head->next;
  t_head->next = tail;
  return (head);
}

/* Expects 2 args, on the same line.  Both are index abbreviations.
   Make the first one be a synonym for the second one, i.e. make the
   first one have the same index as the second one. */
cm_synindex ()
{
  int redirector, redirectee;
  char *temp;

  skip_whitespace ();
  get_until_in_line (" ", &temp);
  redirectee = find_index_offset (temp);
  skip_whitespace ();
  free_and_clear (&temp);
  get_until_in_line (" ", &temp);
  redirector = find_index_offset (temp);
  free (temp);
  if (redirector < 0 || redirectee < 0)
    {
      line_error ("Unknown index reference");
    }
  else
    {
      /* I think that we should let the user make indices synonymous to
         each other without any lossage of info.  This means that one can
         say @synindex cp dt anywhere in the file, and things that used to
         be in cp will go into dt. */
      INDEX_ELT *i1 = the_indices[redirectee], *i2 = the_indices[redirector];

      if (i1 || i2)
	{
	  if (i1)
	    the_indices[redirectee] = index_append (i1, i2);
	  else
	    the_indices[redirectee] = index_append (i2, i1);
	}

      name_index_alist[redirectee]->index =
	name_index_alist[redirector]->index;
    }
}

cm_pindex ()			/* Pinhead index. */
{
  index_add_arg ("pg");
}

cm_vindex ()			/* Variable index. */
{
  index_add_arg ("vr");
}

cm_kindex ()			/* Key index. */
{
  index_add_arg ("ky");
}

cm_cindex ()			/* Concept index. */
{
  index_add_arg ("cp");
}

cm_findex ()			/* Function index. */
{
  index_add_arg ("fn");
}

cm_tindex ()			/* Data Type index. */
{
  index_add_arg ("tp");
}

/* Sorting the index. */
index_element_compare (element1, element2)
     INDEX_ELT **element1, **element2;
{
  /* This needs to ignore leading non-text characters. */
  return (stricmp ((*element1)->entry, (*element2)->entry));
}

/* Sort the index passed in INDEX, returning an array of
   pointers to elements.  The array is terminated with a NULL
   pointer.  We call qsort because it's supposed to be fast.
   I think this looks bad. */
INDEX_ELT **
sort_index (index)
     INDEX_ELT *index;
{
  INDEX_ELT *temp = index;
  INDEX_ELT **array;
  int count = 0;

  while (temp != (INDEX_ELT *) NULL)
    {
      count++;
      temp = temp->next;
    }

  /* We have the length.  Make an array. */

  array = (INDEX_ELT **) xmalloc ((count + 1) * sizeof (INDEX_ELT *));
  count = 0;
  temp = index;

  while (temp != (INDEX_ELT *) NULL)
    {
      array[count++] = temp;
      temp = temp->next;
    }
  array[count] = (INDEX_ELT *) NULL;	/* terminate the array. */

  /* Sort the array. */
  qsort (array, count, sizeof (INDEX_ELT *), index_element_compare);

  return (array);
}

/* Non-zero means that we are in the middle of printing an index. */
int printing_index = 0;

/* Takes one arg, a short name of an index to print.
   Outputs a menu of the sorted elements of the index. */
cm_printindex ()
{
  int item;
  INDEX_ELT *index;
  INDEX_ELT **array;
  char *index_name;
  int old_inhibitions = inhibit_paragraph_indentation;
  int previous_filling_enabled_value = filling_enabled;

  close_paragraph ();
  get_rest_of_line (&index_name);

  index = index_list (index_name);
  if ((int) index == -1)
    {
      line_error ("Unknown index name `%s'", index_name);
      free (index_name);
      return;
    }
  else
    free (index_name);

  array = sort_index (index);

  filling_enabled = 0;
  inhibit_paragraph_indentation = 1;
  close_paragraph ();
  add_word ("* Menu:\n\n");

  printing_index = 1;
  for (item = 0; (index = array[item]); item++)
    {
      int real_line_number = line_number;

      /* Let errors generated while making the index entry point back
	 at the line which contains the entry. */
      line_number = index->defining_line;

      /* If this particular entry should be printed as a "code" index,
	 then wrap the entry with "@code{...}". */
      if (index->code)
	execute_string ("* @code{%s}: ", index->entry);
      else
	execute_string ("* %s: ", index->entry);

      /* Pad the front of the destination nodename so that
	 the output looks nice. */
      if (fill_column > 40 && output_column < 40)
	indent (40 - output_column);

      execute_string ("%s.\n", index->node);

      line_number = real_line_number;
      flush_output ();
    }

  printing_index = 0;
  free (array);
  close_single_paragraph ();
  filling_enabled = previous_filling_enabled_value;
  inhibit_paragraph_indentation = old_inhibitions;
}


/* **************************************************************** */
/*								    */
/*		    Making User Defined Commands		    */
/*								    */
/* **************************************************************** */

define_user_command (name, proc, needs_braces_p)
     char *name;
     FUNCTION *proc;
     int needs_braces_p;
{
  int slot = user_command_array_len;
  user_command_array_len++;

  if (!user_command_array)
    user_command_array = (COMMAND **) xmalloc (1 * sizeof (COMMAND *));

  user_command_array = (COMMAND **) xrealloc (user_command_array,
					      (1 + user_command_array_len) *
					      sizeof (COMMAND *));

  user_command_array[slot] = (COMMAND *) xmalloc (sizeof (COMMAND));
  user_command_array[slot]->name = savestring (name);
  user_command_array[slot]->proc = proc;
  user_command_array[slot]->argument_in_braces = needs_braces_p;
}

/* Make ALIAS run the named FUNCTION.  Copies properties from FUNCTION. */
define_alias (alias, function)
     char *alias, *function;
{
}

/* Set the paragraph indentation variable to the value specified in STRING.
   Values can be:
   `asis': Don't change existing indentation.
   `none': Remove existing indentation.
      NUM: Indent NUM spaces at the starts of paragraphs.
           Note that if NUM is zero, we assume `none'.

   Returns 0 if successful, or non-zero if STRING isn't one of the above. */
int
set_paragraph_indent (string)
     char *string;
{
  if (strcmp (string, "asis") == 0)
    paragraph_start_indent = 0;
  else if (strcmp (string, "none") == 0)
    paragraph_start_indent = -1;
  else
    {
      if (sscanf (string, "%d", &paragraph_start_indent) != 1)
	return (-1);
      else
	{
	  if (paragraph_start_indent == 0)
	    paragraph_start_indent = -1;
	}
    }
  return (0);
}

cm_paragraphindent ()
{
  char *arg;

  get_rest_of_line (&arg);
  if (set_paragraph_indent (arg) != 0)
    line_error ("Bad argument to @paragraphindent");

  free (arg);
}

/* Some support for footnotes. */

/* Footnotes are a new construct in Info.  We don't know the best method
   of implementing them for sure, so we present two possiblities.

   SeparateNode:
	Make them look like followed references, with the reference
	destinations in a makeinfo manufactured node or,

   EndNode:
	Make them appear at the bottom of the node that they originally
	appeared in. */
#define SeparateNode 0
#define EndNode 1

int footnote_style = EndNode;
int first_footnote_this_node = 1;
int footnote_count = 0;

/* Set the footnote style based on he style identifier in STRING. */
int
set_footnote_style (string)
     char *string;
{
  if ((stricmp (string, "separate") == 0) ||
      (stricmp (string, "MN") == 0))
    footnote_style = SeparateNode;
  else if ((stricmp (string, "end") == 0) ||
	   (stricmp (string, "EN") == 0))
    footnote_style = EndNode;
  else
    return (-1);

 return (0);
}

cm_footnotestyle ()
{
  char *arg;

  get_rest_of_line (&arg);

  if (set_footnote_style (arg) != 0)
    line_error ("Bad argument to @footnotestyle");

  free (arg);
}

typedef struct fn
{
  struct fn *next;
  char *marker;
  char *note;
}  FN;

FN *pending_notes = (FN *) NULL;

/* A method for remembering footnotes.  Note that this list gets output
   at the end of the current node. */
remember_note (marker, note)
     char *marker, *note;
{
  FN *temp = (FN *) xmalloc (sizeof (FN));

  temp->marker = savestring (marker);
  temp->note = savestring (note);
  temp->next = pending_notes;
  pending_notes = temp;
  footnote_count++;
}

/* How to get rid of existing footnotes. */
free_pending_notes ()
{
  FN *temp;

  while ((temp = pending_notes) != (FN *) NULL)
    {
      free (temp->marker);
      free (temp->note);
      pending_notes = pending_notes->next;
      free (temp);
    }
  first_footnote_this_node = 1;
  footnote_count = 0;
}

/* What to do when you see a @footnote construct. */

 /* Handle a "footnote".
    footnote *{this is a footnote}
    where "*" is the marker character for this note. */
cm_footnote ()
{
  char *marker;
  char *note;

  get_until ("{", &marker);
  canon_white (marker);

  /* Read the argument in braces. */
  if (curchar () != '{')
    {
      line_error ("`@%s' expected more than just `%s'.  It needs something in `{...}'", command, marker);
      free (marker);
      return;
    }
  else
    {
      int braces = 1;
      int temp = ++input_text_offset;
      int len;

      while (braces)
	{
	  if (temp == size_of_input_text)
	    {
	      line_error ("No closing brace for footnote `%s'", marker);
	      return;
	    }

	  if (input_text[temp] == '{')
	    braces++;
	  else if (input_text[temp] == '}')
	    braces--;
	  else if (input_text[temp] == '\n')
	    line_number ++;

	  temp++;
	}

      len = (temp - input_text_offset) - 1;
      note = (char *)xmalloc (len + 1);
      strncpy (note, &input_text[input_text_offset], len);
      note[len] = '\0';
      input_text_offset = temp;
    }

  if (!current_node || !*current_node)
    {
      line_error ("Footnote defined without parent node");
      free (marker);
      free (note);
      return;
    }

  if (!*marker)
    {
      free (marker);

      if (number_footnotes)
	{
	  marker = (char *)xmalloc (10);
	  sprintf (marker, "%d", current_footnote_number);
	  current_footnote_number++;
	}
      else
	marker = savestring ("*");
    }

  remember_note (marker, note);

  /* Your method should at least insert MARKER. */
  switch (footnote_style)
    {
    case SeparateNode:
      add_word_args ("(%s)", marker);
      if (first_footnote_this_node)
	{
	  char *temp_string;

	  temp_string = (char *)
	    xmalloc ((strlen (current_node)) + (strlen ("-Footnotes")) + 1);

	  add_word_args (" (*note %s-Footnotes::)", current_node);
	  strcpy (temp_string, current_node);
	  strcat (temp_string, "-Footnotes");
	  remember_node_reference (temp_string, line_number, followed_reference);
	  free (temp_string);
	  first_footnote_this_node = 0;
	}
      break;

    case EndNode:
      add_word_args ("(%s)", marker);
      break;

    default:
      break;
    }
  free (marker);
  free (note);
}

/* Non-zero means that we are currently in the process of outputting
   footnotes. */
int already_outputting_pending_notes = 0;

/* Output the footnotes.  We are at the end of the current node. */
output_pending_notes ()
{
  FN *footnote = pending_notes;

  if (!pending_notes)
    return;

  switch (footnote_style)
    {

    case SeparateNode:
      {
	char *old_current_node = current_node;
	char *old_command = savestring (command);

	already_outputting_pending_notes++;
	execute_string ("@node %s-Footnotes,,,%s\n", current_node, current_node);
	already_outputting_pending_notes--;
	current_node = old_current_node;
	free (command);
	command = old_command;
      }
      break;

    case EndNode:
      close_paragraph ();
      in_fixed_width_font++;
      execute_string ("---------- Footnotes ----------\n\n");
      in_fixed_width_font--;
      break;
    }

  /* Handle the footnotes in reverse order. */
  {
    FN **array = (FN **) xmalloc ((footnote_count + 1) * sizeof (FN *));

    array[footnote_count] = (FN *) NULL;

    while (--footnote_count > -1)
      {
	array[footnote_count] = footnote;
	footnote = footnote->next;
      }

    filling_enabled = 1;
    indented_fill = 1;

    while (footnote = array[++footnote_count])
      {

	switch (footnote_style)
	  {
	  case SeparateNode:
	  case EndNode:
	    execute_string ("(%s)  %s", footnote->marker, footnote->note);
	    close_paragraph ();
	    break;
	  }
      }
    close_paragraph ();
    free (array);
  }
}


/* **************************************************************** */
/*                                                                  */
/*              User definable Macros (text substitution)	    */
/*                                                                  */
/* **************************************************************** */

#if defined (HAVE_MACROS)

/* Array of macros and definitions. */
MACRO_DEF **macro_list = (MACRO_DEF **)NULL;

int macro_list_len = 0;		/* Number of elements. */
int macro_list_size = 0;	/* Number of slots in total. */

/* Return the macro definition of NAME or NULL if NAME is not defined. */
MACRO_DEF *
find_macro (name)
     char *name;
{
  register int i;
  register MACRO_DEF *def;

  def = (MACRO_DEF *)NULL;
  for (i = 0; macro_list && (def = macro_list[i]); i++)
    if (strcmp (def->name, name) == 0)
      break;

  return (def);
}

/* Add the macro NAME with DEFINITION to macro_list.  FILENAME is
   the name of the file where this definition can be found, and
   LINENO is the line number within that file.  If a macro already
   exists with NAME, then a warning is produced, and that previous
   definition is overwritten. */
void
add_macro (name, definition, filename, lineno)
     char *name, *definition;
     char *filename;
     int lineno;
{
  register MACRO_DEF *def;

  def = find_macro (name);

  if (!def)
    {
      if (macro_list_len + 2 >= macro_list_size)
	macro_list = (MACRO_DEF **)xrealloc
	  (macro_list, ((macro_list_size += 10) * sizeof (MACRO_DEF *)));

      macro_list[macro_list_len] = (MACRO_DEF *)xmalloc (sizeof (MACRO_DEF));
      macro_list[macro_list_len + 1] = (MACRO_DEF *)NULL;

      def = macro_list[macro_list_len];
      macro_list_len += 1;
      def->name = savestring (name);
    }
  else
    {
      char *temp_filename = input_filename;
      int temp_line = line_number;

      warning ("The macro `%s' is previously defined.", name);

      input_filename = def->filename;
      line_number = def->lineno;

      warning ("Here is the previous definition of `%s'.", name);

      input_filename = temp_filename;
      line_number = temp_line;

      free (def->filename);
      free (def->definition);
    }

  def->filename = savestring (filename);
  def->lineno = lineno;
  def->definition = savestring (definition);
}


/* Delete the macro with name NAME.  The macro is deleted from the list,
   but it is also returned.  If there was no macro defined, NULL is
   returned. */
MACRO_DEF *
delete_macro (name)
     char *name;
{
  register int i;
  register MACRO_DEF *def;

  def = (MACRO_DEF *)NULL;
  for (i = 0; macro_list && (def = macro_list[i]); i++)
    if (strcmp (def->name, name) == 0)
      {
	memcpy (macro_list + i, macro_list + i + 1,
	       ((macro_list_len + 1) - i) * sizeof (MACRO_DEF *));
	break;
      }
  return (def);
}

/* Execute the macro passed in DEF, a pointer to a MACRO_DEF. */
void
execute_macro (def)
     MACRO_DEF *def;
{

  if (def != (MACRO_DEF *)NULL)
    {
      char *line, *string;

      get_until ("\n", &line);

      if (curchar () == '\n')	/* as opposed to the end of the file... */
	{
	  line_number++;
	  input_text_offset++;
	}
      string = (char *)xmalloc (1 + strlen (def->definition) + strlen (line));
      strcpy (string, def->definition);
      strcat (string, line);
      free (line);

      execute_string ("%s\n", string);
      free (string);
    }
}

int
cm_macro ()
{
  register int i;
  char *line, *name, *expansion;

  get_rest_of_line (&line);
  canon_white (line);

  for (i = 0; line[i] && !whitespace (line[i]); i++);
  name = (char *)xmalloc (i);
  strncpy (name, line, i);
  name[i] = '\0';

  while (whitespace (line[i]))
    i++;

  add_macro (name, line + i, input_filename, line_number - 1);
  free (line);
  free (name);
}

int
cm_unmacro ()
{
  register int i;
  char *line, *name;
  MACRO_DEF *def;

  get_rest_of_line (&line);
  canon_white (line);

  for (i = 0; line[i] && !whitespace (line[i]); i++);
  name = (char *)xmalloc (i);
  strncpy (name, line, i);
  name[i] = '\0';

  def = delete_macro (name);

  if (def)
    {
      free (def->filename);
      free (def->name);
      free (def->definition);
      free (def);
    }

  free (line);
  free (name);
}
#endif /* HAVE_MACROS */

/* **************************************************************** */
/*                                                                  */
/*                  Looking For Include Files                       */
/*                                                                  */
/* **************************************************************** */

/* Given a string containing units of information separated by colons,
   return the next one pointed to by INDEX, or NULL if there are no more.
   Advance INDEX to the character after the colon. */
char *
extract_colon_unit (string, index)
     char *string;
     int *index;
{
  int i, start;

  i = *index;

  if (!string || (i >= strlen (string)))
    return ((char *)NULL);

  /* Each call to this routine leaves the index pointing at a colon if
     there is more to the path.  If I is > 0, then increment past the
     `:'.  If I is 0, then the path has a leading colon.  Trailing colons
     are handled OK by the `else' part of the if statement; an empty
     string is returned in that case. */
  if (i && string[i] == ':')
    i++;

  start = i;

  while (string[i] && string[i] != ':') i++;

  *index = i;

  if (i == start)
    {
      if (string[i])
	(*index)++;

      /* Return "" in the case of a trailing `:'. */
      return (savestring (""));
    }
  else
    {
      char *value;

      value = (char *)xmalloc (1 + (i - start));
      strncpy (value, &string[start], (i - start));
      value [i - start] = '\0';

      return (value);
    }
}

/* Return the full pathname for FILENAME by searching along PATH.
   When found, return the stat () info for FILENAME in FINFO.
   If PATH is NULL, only the current directory is searched.
   If the file could not be found, return a NULL pointer. */
char *
get_file_info_in_path (filename, path, finfo)
     char *filename, *path;
     struct stat *finfo;
{
  char *dir;
  int result, index = 0;

  if (path == (char *)NULL)
    path = ".";

  /* Handle absolute pathnames. "./foo", "/foo", "../foo". */
  if (*filename == '/' ||
      (*filename == '.' &&
       (filename[1] == '/' ||
	(filename[1] == '.' && filename[2] == '/'))))
    {
      if (stat (filename, finfo) == 0)
	return (savestring (filename));
      else
	return ((char *)NULL);
    }

  while (dir = extract_colon_unit (path, &index))
    {
      char *fullpath;

      if (!*dir)
	{
	  free (dir);
	  dir = savestring (".");
	}

      fullpath = (char *)xmalloc (2 + strlen (dir) + strlen (filename));
      sprintf (fullpath, "%s/%s", dir, filename);
      free (dir);

      result = stat (fullpath, finfo);

      if (result == 0)
	return (fullpath);
      else
	free (fullpath);
    }
  return ((char *)NULL);
}
