// -*- C++ -*-

// <groff_src_dir>/src/libs/libdriver/input.cpp

/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002, 2003
   Free Software Foundation, Inc.

   Written by James Clark (jjc@jclark.com)
   Major rewrite 2001 by Bernd Warken (bwarken@mayn.de)

   Last update: 04 Apr 2003

   This file is part of groff, the GNU roff text processing system.

   groff is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   groff is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with groff; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.
*/

/* Description

   This file implements the parser for the intermediate groff output,
   see groff_out(5), and does the printout for the given device.

   All parsed information is processed within the function do_file().
   A device postprocessor just needs to fill in the methods for the class
   `printer' (or rather a derived class) without having to worry about
   the syntax of the intermediate output format.  Consequently, the
   programming of groff postprocessors is similar to the development of
   device drivers.

   The prototyping for this file is done in driver.h (and error.h).
*/

/* Changes of the 2001 rewrite of this file.

   The interface to the outside and the handling of the global
   variables was not changed, but internally many necessary changes
   were performed.

   The main aim for this rewrite is to provide a first step towards
   making groff fully compatible with classical troff without pain.

   Bugs fixed
   - Unknown subcommands of `D' and `x' are now ignored like in the
     classical case, but a warning is issued.  This was also
     implemented for the other commands.
   - A warning is emitted if `x stop' is missing.
   - `DC' and `DE' commands didn't position to the right end after
     drawing (now they do), see discussion below.
   - So far, `x stop' was ignored.  Now it terminates the processing
     of the current intermediate output file like the classical troff.
   - The command `c' didn't check correctly on white-space.
   - The environment stack wasn't suitable for the color extensions
     (replaced by a class).
   - The old groff parser could only handle a prologue with the first
     3 lines having a fixed structure, while classical troff specified
     the sequence of the first 3 commands without further
     restrictions.  Now the parser is smart about additional
     white space, comments, and empty lines in the prologue.
   - The old parser allowed space characters only as syntactical
     separators, while classical troff had tab characters as well.
     Now any sequence of tabs and/or spaces is a syntactical
     separator between commands and/or arguments.
   - Range checks for numbers implemented.

   New and improved features
   - The color commands `m' and `DF' are added.
   - The old color command `Df' is now converted and delegated to `DFg'.
   - The command `F' is implemented as `use intended file name'.  It
     checks whether its argument agrees with the file name used so far,
     otherwise a warning is issued.  Then the new name is remembered
     and used for the following error messages.
   - For the positioning after drawing commands, an alternative, easier
     scheme is provided, but not yet activated; it can be chosen by
     undefining the preprocessor macro STUPID_DRAWING_POSITIONING.
     It extends the rule of the classical troff output language in a
     logical way instead of the rather strange actual positioning.
     For details, see the discussion below.
   - For the `D' commands that only set the environment, the calling of
     pr->send_draw() was removed because this doesn't make sense for
     the `DF' commands; the (changed) environment is sent with the
     next command anyway.
   - Error handling was clearly separated into warnings and fatal.
   - The error behavior on additional arguments for `D' and `x'
     commands with a fixed number of arguments was changed from being
     ignored (former groff) to issue a warning and ignore (now), see
     skip_line_x().  No fatal was chosen because both string and
     integer arguments can occur.
   - The gtroff program issues a trailing dummy integer argument for
     some drawing commands with an odd number of arguments to make the
     number of arguments even, e.g. the DC and Dt commands; this is
     honored now.
   - All D commands with a variable number of args expect an even
     number of trailing integer arguments, so fatal on error was
     implemented.
   - Disable environment stack and the commands `{' and `}' by making
     them conditional on macro USE_ENV_STACK; actually, this is
     undefined by default.  There isn't any known application for these
     features.

   Cosmetics
   - Nested `switch' commands are avoided by using more functions.
     Dangerous 'fall-through's avoided.
   - Commands and functions are sorted alphabetically (where possible).
   - Dynamic arrays/buffers are now implemented as container classes.
   - Some functions had an ugly return structure; this has been
     streamlined by using classes.
   - Use standard C math functions for number handling, so getting rid
     of differences to '0'.
   - The macro `IntArg' has been created for an easier transition
     to guaranteed 32 bits integers (`int' is enough for GNU, while
     ANSI only guarantees `long int' to have a length of 32 bits).
   - The many usages of type `int' are differentiated by using `Char',
     `bool', and `IntArg' where appropriate.
   - To ease the calls of the local utility functions, the parser
     variables `current_file', `npages', and `current_env'
     (formerly env) were made global to the file (formerly they were
     local to the do_file() function)
   - Various comments were added.

   TODO
   - Get rid of the stupid drawing positioning.
   - Can the `Dt' command be completely handled by setting environment
     within do_file() instead of sending to pr?
   - Integer arguments must be >= 32 bits, use conditional #define.
   - Add scaling facility for classical device independence and
     non-groff devices.  Classical troff output had a quasi device
     independence by scaling the intermediate output to the resolution
     of the postprocessor device if different from the one specified
     with `x T', groff have not.  So implement full quasi device
     indepedence, including the mapping of the strange classical
     devices to the postprocessor device (seems to be reasonably
     easy).
   - The external, global pointer variables are not optimally handled.
     - The global variables `current_filename',
       `current_source_filename', and `current_lineno' are only used for
       error reporting.  So implement a static class `Error'
       (`::' calls).
     - The global `device' is the name used during the formatting
       process; there should be a new variable for the device name used
       during the postprocessing.
  - Implement the B-spline drawing `D~' for all graphical devices.
  - Make `environment' a class with an overflow check for its members
    and a delete method to get rid of delete_current_env().
  - Implement the `EnvStack' to use `new' instead of `malloc'.
  - The class definitions of this document could go into a new file.
  - The comments in this section should go to a `Changelog' or some
    `README' file in this directory.
*/

/*
  Discussion of the positioning by drawing commands

  There was some confusion about the positioning of the graphical
  pointer at the printout after having executed a `D' command.
  The classical troff manual of Osanna & Kernighan specified,

    `The position after a graphical object has been drawn is
     at its end; for circles and ellipses, the "end" is at the
     right side.'

  From this, it follows that
  - all open figures (args, splines, and lines) should position at their
    final point.
  - all circles and ellipses should position at their right-most point
    (as if 2 halves had been drawn).
  - all closed figures apart from circles and ellipses shouldn't change
    the position because they return to their origin.
  - all setting commands should not change position because they do not
    draw any graphical object.

  In the case of the open figures, this means that the horizontal
  displacement is the sum of all odd arguments and the vertical offset
  the sum of all even arguments, called the alternate arguments sum
  displacement in the following.

  Unfortunately, groff did not implement this simple rule.  The former
  documentation in groff_out(5) differed from the source code, and
  neither of them is compatible with the classical rule.

  The former groff_out(5) specified to use the alternative arguments
  sum displacement for calculating the drawing positioning of
  non-classical commands, including the `Dt' command (setting-only)
  and closed polygons.  Applying this to the new groff color commands
  will lead to disaster.  For their arguments can take large values (>
  65000).  On low resolution devices, the displacement of such large
  values will corrupt the display or kill the printer.  So the
  nonsense specification has come to a natural end anyway.

  The groff source code, however, had no positioning for the
  setting-only commands (esp. `Dt'), the right-end positioning for
  outlined circles and ellipses, and the alternative argument sum
  displacement for all other commands (including filled circles and
  ellipses).

  The reason why no one seems to have suffered from this mayhem so
  far is that the graphical objects are usually generated by
  preprocessors like pic that do not depend on the automatic
  positioning.  When using the low level `\D' escape sequences or `D'
  output commands, the strange positionings can be circumvented by
  absolute positionings or by tricks like `\Z'.

  So doing an exorcism on the strange, incompatible displacements might
  not harm any existing documents, but will make the usage of the
  graphical escape sequences and commands natural.

  That's why the rewrite of this file returned to the reasonable,
  classical specification with its clear end-of-drawing rule that is
  suitable for all cases.  But a macro STUPID_DRAWING_POSITIONING is
  provided for testing the funny former behavior.

  The new rule implies the following behavior.
  - Setting commands (`Dt', `Df', `DF') and polygons (`Dp' and `DP')
    do not change position now.
  - Filled circles and ellipses (`DC' and `DE') position at their
    most right point (outlined ones `Dc' and `De' did this anyway).
  - As before, all open graphical objects position to their final
    drawing point (alternate sum of the command arguments).

*/

#ifndef STUPID_DRAWING_POSITIONING
// uncomment next line if all non-classical D commands shall position
// to the strange alternate sum of args displacement
#define STUPID_DRAWING_POSITIONING
#endif

// Decide whether the commands `{' and `}' for different environments
// should be used.
#undef USE_ENV_STACK

#include "driver.h"
#include "device.h"

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>


/**********************************************************************
                           local types
 **********************************************************************/

// integer type used in the fields of struct environment (see printer.h)
typedef int EnvInt;

// integer arguments of groff_out commands, must be >= 32 bits
typedef int IntArg;

// color components of groff_out color commands, must be >= 32 bits
typedef unsigned int ColorArg;

// Array for IntArg values.
class IntArray {
  size_t num_allocated;
  size_t num_stored;
  IntArg *data;
public:
  IntArray(void);
  IntArray(const size_t);
  ~IntArray(void);
  const IntArg operator[](const size_t i) const
  {
    if (i >= num_stored)
      fatal("index out of range");
    return (IntArg) data[i];
  }
  void append(IntArg);
  const IntArg * const
    get_data(void) const { return (IntArg *) data; }
  const size_t len(void) const { return num_stored; }
};

// Characters read from the input queue.
class Char {
  int data;
public:
  Char(void) : data('\0') {}
  Char(const int c) : data(c) {}
  bool operator==(char c) const { return (data == c) ? true : false; }
  bool operator==(int c) const { return (data == c) ? true : false; }
  bool operator==(const Char c) const
		  { return (data == c.data) ? true : false; }
  bool operator!=(char c) const { return !(*this == c); }
  bool operator!=(int c) const { return !(*this == c); }
  bool operator!=(const Char c) const { return !(*this == c); }
  operator int() const { return (int) data; }
  operator unsigned char() const { return (unsigned char) data; }
  operator char() const { return (char) data; }
};

// Buffer for string arguments (Char, not char).
class StringBuf {
  size_t num_allocated;
  size_t num_stored;
  Char *data;			// not terminated by '\0'
public:
  StringBuf(void);		// allocate without storing
  ~StringBuf(void);
  void append(const Char);	// append character to `data'
  char *make_string(void);	// return new copy of `data' with '\0'
  bool is_empty(void) {		// true if none stored
    return (num_stored > 0) ? false : true;
  }
  void reset(void);		// set `num_stored' to 0
};

#ifdef USE_ENV_STACK
class EnvStack {
  environment **data;
  size_t num_allocated;
  size_t num_stored;
public:
  EnvStack(void);
  ~EnvStack(void);
  environment *pop(void);
  void push(environment *e);
};
#endif // USE_ENV_STACK


/**********************************************************************
                          external variables
 **********************************************************************/

// exported as extern by error.h (called from driver.h)
// needed for error messages (see ../libgroff/error.cpp)
const char *current_filename = 0; // printable name of the current file
				  // printable name of current source file
const char *current_source_filename = 0;
int current_lineno = 0;		  // current line number of printout

// exported as extern by device.h;
const char *device = 0;		  // cancel former init with literal

printer *pr;

// Note:
//
//   We rely on an implementation of the `new' operator which aborts
//   gracefully if it can't allocate memory (e.g. from libgroff/new.cpp).


/**********************************************************************
                        static local variables
 **********************************************************************/

FILE *current_file = 0;		// current input stream for parser

// npages: number of pages processed so far (including current page),
//         _not_ the page number in the printout (can be set with `p').
int npages = 0;

const ColorArg
COLORARG_MAX = (ColorArg) 65536U; // == 0xFFFF + 1 == 0x10000

const IntArg
INTARG_MAX = (IntArg) 0x7FFFFFFF; // maximal signed 32 bits number

// parser environment, created and deleted by each run of do_file()
environment *current_env = 0;

#ifdef USE_ENV_STACK
const size_t
envp_size = sizeof(environment *);
#endif // USE_ENV_STACK


/**********************************************************************
                        function declarations
 **********************************************************************/

// utility functions
ColorArg color_from_Df_command(IntArg);
				// transform old color into new
void delete_current_env(void);	// delete global var current_env
void fatal_command(char);	// abort for invalid command
inline Char get_char(void);	// read next character from input stream
ColorArg get_color_arg(void);	// read in argument for new color cmds
IntArray *get_D_fixed_args(const size_t);
				// read in fixed number of integer
				// arguments
IntArray *get_D_fixed_args_odd_dummy(const size_t);
				// read in a fixed number of integer
				// arguments plus optional dummy
IntArray *get_D_variable_args(void);
                                // variable, even number of int args
char *get_extended_arg(void);	// argument for `x X' (several lines)
IntArg get_integer_arg(void);	// read in next integer argument
IntArray *get_possibly_integer_args();
				// 0 or more integer arguments
char *get_string_arg(void);	// read in next string arg, ended by WS
inline bool is_space_or_tab(const Char);
				// test on space/tab char
Char next_arg_begin(void);	// skip white space on current line
Char next_command(void);	// go to next command, evt. diff. line
inline bool odd(const int);	// test if integer is odd
void position_to_end_of_args(const IntArray * const);
				// positioning after drawing
void remember_filename(const char *);
				// set global current_filename
void remember_source_filename(const char *);
				// set global current_source_filename
void send_draw(const Char, const IntArray * const);
				// call pr->draw
void skip_line(void);		// unconditionally skip to next line
bool skip_line_checked(void);	// skip line, false if args are left
void skip_line_fatal(void);	// skip line, fatal if args are left
void skip_line_warn(void);	// skip line, warn if args are left
void skip_line_D(void);		// skip line in D commands
void skip_line_x(void);		// skip line in x commands
void skip_to_end_of_line(void);	// skip to the end of the current line
inline void unget_char(const Char);
				// restore character onto input

// parser subcommands
void parse_color_command(color *);
				// color sub(sub)commands m and DF
void parse_D_command(void);	// graphical subcommands
bool parse_x_command(void);	// device controller subcommands


/**********************************************************************
                         class methods
 **********************************************************************/

#ifdef USE_ENV_STACK
EnvStack::EnvStack(void)
{
  num_allocated = 4;
  // allocate pointer to array of num_allocated pointers to environment
  data = (environment **) malloc(envp_size * num_allocated);
  if (data == 0)
    fatal("could not allocate environment data");
  num_stored = 0;
}

EnvStack::~EnvStack(void)
{
  for (size_t i = 0; i < num_stored; i++)
    delete data[i];
  free(data);
}

// return top element from stack and decrease stack pointer
//
// the calling function must take care of properly deleting the result
environment *
EnvStack::pop(void)
{
  num_stored--;
  environment *result = data[num_stored];
  data[num_stored] = 0;
  return result;
}

// copy argument and push this onto the stack
void
EnvStack::push(environment *e)
{
  environment *e_copy = new environment;
  if (num_stored >= num_allocated) {
    environment **old_data = data;
    num_allocated *= 2;
    data = (environment **) malloc(envp_size * num_allocated);
    if (data == 0)
      fatal("could not allocate data");
    for (size_t i = 0; i < num_stored; i++)
      data[i] = old_data[i];
    free(old_data);
  }
  e_copy->col = new color;
  e_copy->fill = new color;
  *e_copy->col = *e->col;
  *e_copy->fill = *e->fill;
  e_copy->fontno = e->fontno;
  e_copy->height = e->height;
  e_copy->hpos = e->hpos;
  e_copy->size = e->size;
  e_copy->slant = e->slant;
  e_copy->vpos = e->vpos;
  data[num_stored] = e_copy;
  num_stored++;
}
#endif // USE_ENV_STACK

IntArray::IntArray(void)
{
  num_allocated = 4;
  data = new IntArg[num_allocated];
  num_stored = 0;
}

IntArray::IntArray(const size_t n)
{
  if (n <= 0)
    fatal("number of integers to be allocated must be > 0");
  num_allocated = n;
  data = new IntArg[num_allocated];
  num_stored = 0;
}

IntArray::~IntArray(void)
{
  a_delete data;
}

void
IntArray::append(IntArg x)
{
  if (num_stored >= num_allocated) {
    IntArg *old_data = data;
    num_allocated *= 2;
    data = new IntArg[num_allocated];
    for (size_t i = 0; i < num_stored; i++)
      data[i] = old_data[i];
    a_delete old_data;
  }
  data[num_stored] = x;
  num_stored++;
}

StringBuf::StringBuf(void)
{
  num_stored = 0;
  num_allocated = 128;
  data = new Char[num_allocated];
}

StringBuf::~StringBuf(void)
{
  a_delete data;
}

void
StringBuf::append(const Char c)
{
  if (num_stored >= num_allocated) {
    Char *old_data = data;
    num_allocated *= 2;
    data = new Char[num_allocated];
    for (size_t i = 0; i < num_stored; i++)
      data[i] = old_data[i];
    a_delete old_data;
  }
  data[num_stored] = c;
  num_stored++;
}

char *
StringBuf::make_string(void)
{
  char *result = new char[num_stored + 1];
  for (size_t i = 0; i < num_stored; i++)
    result[i] = (char) data[i];
  result[num_stored] = '\0';
  return result;
}

void
StringBuf::reset(void)
{
  num_stored = 0;
}

/**********************************************************************
                        utility functions
 **********************************************************************/

//////////////////////////////////////////////////////////////////////
/* color_from_Df_command:
   Process the gray shade setting command Df.

   Transform Df style color into DF style color.
   Df color: 0-1000, 0 is white
   DF color: 0-65536, 0 is black

   The Df command is obsoleted by command DFg, but kept for
   compatibility.
*/
ColorArg
color_from_Df_command(IntArg Df_gray)
{
  return ColorArg((1000-Df_gray) * COLORARG_MAX / 1000); // scaling
}

//////////////////////////////////////////////////////////////////////
/* delete_current_env():
   Delete global variable current_env and its pointer members.

   This should be a class method of environment.
*/
void delete_current_env(void)
{
  delete current_env->col;
  delete current_env->fill;
  delete current_env;
}

//////////////////////////////////////////////////////////////////////
/* fatal_command():
   Emit error message about invalid command and abort.
*/
void
fatal_command(char command)
{
  fatal("`%1' command invalid before first `p' command", command);
}

//////////////////////////////////////////////////////////////////////
/* get_char():
   Retrieve the next character from the input queue.

   Return: The retrieved character (incl. EOF), converted to Char.
*/
inline Char
get_char(void)
{
  return (Char) getc(current_file);
}

//////////////////////////////////////////////////////////////////////
/* get_color_arg():
   Retrieve an argument suitable for the color commands m and DF.

   Return: The retrieved color argument.
*/
ColorArg
get_color_arg(void)
{
  IntArg x = get_integer_arg();
  if (x < 0 || x > (IntArg)COLORARG_MAX) {
    error("color component argument out of range");
    x = 0;
  }
  return (ColorArg) x;
}

//////////////////////////////////////////////////////////////////////
/* get_D_fixed_args():
   Get a fixed number of integer arguments for D commands.

   Fatal if wrong number of arguments.
   Too many arguments on the line raise a warning.
   A line skip is done.

   number: In-parameter, the number of arguments to be retrieved.
   ignore: In-parameter, ignore next argument -- GNU troff always emits
           pairs of parameters for `D' extensions added by groff.
           Default is `false'.

   Return: New IntArray containing the arguments.
*/
IntArray *
get_D_fixed_args(const size_t number)
{
  if (number <= 0)
    fatal("requested number of arguments must be > 0");
  IntArray *args = new IntArray(number);
  for (size_t i = 0; i < number; i++)
    args->append(get_integer_arg());
  skip_line_D();
  return args;
}

//////////////////////////////////////////////////////////////////////
/* get_D_fixed_args_odd_dummy():
   Get a fixed number of integer arguments for D commands and optionally
   ignore a dummy integer argument if the requested number is odd.

   The gtroff program adds a dummy argument to some commands to get
   an even number of arguments.
   Error if the number of arguments differs from the scheme above.
   A line skip is done.

   number: In-parameter, the number of arguments to be retrieved.

   Return: New IntArray containing the arguments.
*/
IntArray *
get_D_fixed_args_odd_dummy(const size_t number)
{
  if (number <= 0)
    fatal("requested number of arguments must be > 0");
  IntArray *args = new IntArray(number);
  for (size_t i = 0; i < number; i++)
    args->append(get_integer_arg());
  if (odd(number)) {
    IntArray *a = get_possibly_integer_args();
    if (a->len() > 1)
      error("too many arguments");
    delete a;
  }
  skip_line_D();
  return args;
}

//////////////////////////////////////////////////////////////////////
/* get_D_variable_args():
   Get a variable even number of integer arguments for D commands.

   Get as many integer arguments as possible from the rest of the
   current line.
   - The arguments are separated by an arbitrary sequence of space or
     tab characters.
   - A comment, a newline, or EOF indicates the end of processing.
   - Error on non-digit characters different from these.
   - A final line skip is performed (except for EOF).

   Return: New IntArray of the retrieved arguments.
*/
IntArray *
get_D_variable_args()
{
  IntArray *args = get_possibly_integer_args();
  size_t n = args->len();
  if (n <= 0)
    error("no arguments found");
  if (odd(n))
    error("even number of arguments expected");
  skip_line_D();
  return args;
}

//////////////////////////////////////////////////////////////////////
/* get_extended_arg():
   Retrieve extended arg for `x X' command.

   - Skip leading spaces and tabs, error on EOL or newline.
   - Return everything before the next NL or EOF ('#' is not a comment);
     as long as the following line starts with '+' this is returned
     as well, with the '+' replaced by a newline.
   - Final line skip is always performed.

   Return: Allocated (new) string of retrieved text argument.
*/
char *
get_extended_arg(void)
{
  StringBuf buf = StringBuf();
  Char c = next_arg_begin();
  while ((int) c != EOF) {
    if ((int) c == '\n') {
      current_lineno++;
      c = get_char();
      if ((int) c == '+')
	buf.append((Char) '\n');
      else {
	unget_char(c);		// first character of next line
	break;
      }
    }
    else
      buf.append(c);
    c = get_char();
  }
  return buf.make_string();
}

//////////////////////////////////////////////////////////////////////
/* get_integer_arg(): Retrieve integer argument.

   Skip leading spaces and tabs, collect an optional '-' and all
   following decimal digits (at least one) up to the next non-digit,
   which is restored onto the input queue.

   Fatal error on all other situations.

   Return: Retrieved integer.
*/
IntArg
get_integer_arg(void)
{
  StringBuf buf = StringBuf();
  Char c = next_arg_begin();
  if ((int) c == '-') {
    buf.append(c);
    c = get_char();
  }
  if (!isdigit((int) c))
    error("integer argument expected");
  while (isdigit((int) c)) {
    buf.append(c);
    c = get_char();
  }
  // c is not a digit
  unget_char(c);
  char *s = buf.make_string();
  errno = 0;
  long int number = strtol(s, 0, 10);
  if (errno != 0
      || number > INTARG_MAX || number < -INTARG_MAX) {
    error("integer argument too large");
    number = 0;
  }
  a_delete s;
  return (IntArg) number;
}

//////////////////////////////////////////////////////////////////////
/* get_possibly_integer_args():
   Parse the rest of the input line as a list of integer arguments.

   Get as many integer arguments as possible from the rest of the
   current line, even none.
   - The arguments are separated by an arbitrary sequence of space or
     tab characters.
   - A comment, a newline, or EOF indicates the end of processing.
   - Error on non-digit characters different from these.
   - No line skip is performed.

   Return: New IntArray of the retrieved arguments.
*/
IntArray *
get_possibly_integer_args()
{
  bool done = false;
  StringBuf buf = StringBuf();
  Char c = get_char();
  IntArray *args = new IntArray();
  while (!done) {
    buf.reset();
    while (is_space_or_tab(c))
      c = get_char();
    if (c == '-') {
      Char c1 = get_char();
      if (isdigit((int) c1)) {
	buf.append(c);
	c = c1;
      }
      else
	unget_char(c1);
    }
    while (isdigit((int) c)) {
      buf.append(c);
      c = get_char();
    }
    if (!buf.is_empty()) {
      char *s = buf.make_string();
      errno = 0;
      long int x = strtol(s, 0, 10);
      if (errno
	  || x > INTARG_MAX || x < -INTARG_MAX) {
	error("invalid integer argument, set to 0");
	x = 0;
      }
      args->append((IntArg) x);
      a_delete s;
    }
    // Here, c is not a digit.
    // Terminate on comment, end of line, or end of file, while
    // space or tab indicate continuation; otherwise error.
    switch((int) c) {
    case '#':
      skip_to_end_of_line();
      done = true;
      break;
    case '\n':
      done = true;
      unget_char(c);
      break;
    case EOF:
      done = true;
      break;
    case ' ':
    case '\t':
      break;
    default:
      error("integer argument expected");
      break;
    }
  }
  return args;
}

//////////////////////////////////////////////////////////////////////
/* get_string_arg():
   Retrieve string arg.

   - Skip leading spaces and tabs; error on EOL or newline.
   - Return all following characters before the next space, tab,
     newline, or EOF character (in-word '#' is not a comment character).
   - The terminating space, tab, newline, or EOF character is restored
     onto the input queue, so no line skip.

   Return: Retrieved string as char *, allocated by 'new'.
*/
char *
get_string_arg(void)
{
  StringBuf buf = StringBuf();
  Char c = next_arg_begin();
  while (!is_space_or_tab(c)
	 && c != Char('\n') && c != Char(EOF)) {
    buf.append(c);
    c = get_char();
  }
  unget_char(c);		// restore white space
  return buf.make_string();
}

//////////////////////////////////////////////////////////////////////
/* is_space_or_tab():
   Test a character if it is a space or tab.

   c: In-parameter, character to be tested.

   Return: True, if c is a space or tab character, false otherwise.
*/
inline bool
is_space_or_tab(const Char c)
{
  return (c == Char(' ') || c == Char('\t')) ? true : false;
}

//////////////////////////////////////////////////////////////////////
/* next_arg_begin():
   Return first character of next argument.

   Skip space and tab characters; error on newline or EOF.

   Return: The first character different from these (including '#').
*/
Char
next_arg_begin(void)
{
  Char c;
  while (1) {
    c = get_char();
    switch ((int) c) {
    case ' ':
    case '\t':
      break;
    case '\n':
    case EOF:
      error("missing argument");
      break;
    default:			// first essential character
      return c;
    }
  }
}

//////////////////////////////////////////////////////////////////////
/* next_command():
   Find the first character of the next command.

   Skip spaces, tabs, comments (introduced by #), and newlines.

   Return: The first character different from these (including EOF).
*/
Char
next_command(void)
{
  Char c;
  while (1) {
    c = get_char();
    switch ((int) c) {
    case ' ':
    case '\t':
      break;
    case '\n':
      current_lineno++;
      break;
    case '#':			// comment
      skip_line();
      break;
    default:			// EOF or first essential character
      return c;
    }
  }
}

//////////////////////////////////////////////////////////////////////
/* odd():
   Test whether argument is an odd number.

   n: In-parameter, the integer to be tested.

   Return: True if odd, false otherwise.
*/
inline bool
odd(const int n)
{
  return (n & 1 == 1) ? true : false;
}

//////////////////////////////////////////////////////////////////////
/* position_to_end_of_args():
   Move graphical pointer to end of drawn figure.

   This is used by the D commands that draw open geometrical figures.
   The algorithm simply sums up all horizontal displacements (arguments
   with even number) for the horizontal component.  Similarly, the
   vertical component is the sum of the odd arguments.

   args: In-parameter, the arguments of a former drawing command.
*/
void
position_to_end_of_args(const IntArray * const args)
{
  size_t i;
  const size_t n = args->len();
  for (i = 0; i < n; i += 2)
    current_env->hpos += (*args)[i];
  for (i = 1; i < n; i += 2)
    current_env->vpos += (*args)[i];
}

//////////////////////////////////////////////////////////////////////
/* remember_filename():
   Set global variable current_filename.

   The actual filename is stored in current_filename.  This is used by
   the postprocessors, expecting the name "<standard input>" for stdin.

   filename: In-out-parameter; is changed to the new value also.
*/
void
remember_filename(const char *filename)
{
  char *fname;
  if (strcmp(filename, "-") == 0)
    fname = "<standard input>";
  else
    fname = (char *) filename;
  size_t len = strlen(fname) + 1;
  if (current_filename != 0)
    free((char *)current_filename);
  current_filename = (const char *) malloc(len);
  if (current_filename == 0)
    fatal("can't malloc space for filename");
  strncpy((char *)current_filename, (char *)fname, len);
}

//////////////////////////////////////////////////////////////////////
/* remember_source_filename():
   Set global variable current_source_filename.

   The actual filename is stored in current_filename.  This is used by
   the postprocessors, expecting the name "<standard input>" for stdin.

   filename: In-out-parameter; is changed to the new value also.
*/
void
remember_source_filename(const char *filename)
{
  char *fname;
  if (strcmp(filename, "-") == 0)
    fname = "<standard input>";
  else
    fname = (char *) filename;
  size_t len = strlen(fname) + 1;
  if (current_source_filename != 0)
    free((char *)current_source_filename);
  current_source_filename = (const char *) malloc(len);
  if (current_source_filename == 0)
    fatal("can't malloc space for filename");
  strncpy((char *)current_source_filename, (char *)fname, len);
}

//////////////////////////////////////////////////////////////////////
/* send_draw():
   Call draw method of printer class.

   subcmd: Letter of actual D subcommand.
   args: Array of integer arguments of actual D subcommand.
*/
void
send_draw(const Char subcmd, const IntArray * const args)
{
  EnvInt n = (EnvInt) args->len();
  pr->draw((int) subcmd, (IntArg *) args->get_data(), n, current_env);
}

//////////////////////////////////////////////////////////////////////
/* skip_line():
   Go to next line within the input queue.

   Skip the rest of the current line, including the newline character.
   The global variable current_lineno is adjusted.
   No errors are raised.
*/
void
skip_line(void)
{
  Char c = get_char();
  while (1) {
    if (c == '\n') {
      current_lineno++;
      break;
    }
    if (c == EOF)
      break;
    c = get_char();
  }
}

//////////////////////////////////////////////////////////////////////
/* skip_line_checked ():
   Check that there aren't any arguments left on the rest of the line,
   then skip line.

   Spaces, tabs, and a comment are allowed before newline or EOF.
   All other characters raise an error.
*/
bool
skip_line_checked(void)
{
  bool ok = true;
  Char c = get_char();
  while (is_space_or_tab(c))
    c = get_char();
  switch((int) c) {
  case '#':			// comment
    skip_line();
    break;
  case '\n':
    current_lineno++;
    break;
  case EOF:
    break;
  default:
    ok = false;
    skip_line();
    break;
  }
  return ok;
}

//////////////////////////////////////////////////////////////////////
/* skip_line_fatal ():
   Fatal error if arguments left, otherwise skip line.

   Spaces, tabs, and a comment are allowed before newline or EOF.
   All other characters trigger the error.
*/
void
skip_line_fatal(void)
{
  bool ok = skip_line_checked();
  if (!ok) {
    current_lineno--;
    error("too many arguments");
    current_lineno++;
  }
}

//////////////////////////////////////////////////////////////////////
/* skip_line_warn ():
   Skip line, but warn if arguments are left on actual line.

   Spaces, tabs, and a comment are allowed before newline or EOF.
   All other characters raise a warning
*/
void
skip_line_warn(void)
{
  bool ok = skip_line_checked();
  if (!ok) {
    current_lineno--;
    warning("too many arguments on current line");
    current_lineno++;
  }
}

//////////////////////////////////////////////////////////////////////
/* skip_line_D ():
   Skip line in `D' commands.

   Decide whether in case of an additional argument a fatal error is
   raised (the documented classical behavior), only a warning is
   issued, or the line is just skipped (former groff behavior).
   Actually decided for the warning.
*/
void
skip_line_D(void)
{
  skip_line_warn();
  // or: skip_line_fatal();
  // or: skip_line();
}

//////////////////////////////////////////////////////////////////////
/* skip_line_x ():
   Skip line in `x' commands.

   Decide whether in case of an additional argument a fatal error is
   raised (the documented classical behavior), only a warning is
   issued, or the line is just skipped (former groff behavior).
   Actually decided for the warning.
*/
void
skip_line_x(void)
{
  skip_line_warn();
  // or: skip_line_fatal();
  // or: skip_line();
}

//////////////////////////////////////////////////////////////////////
/* skip_to_end_of_line():
   Go to the end of the current line.

   Skip the rest of the current line, excluding the newline character.
   The global variable current_lineno is not changed.
   No errors are raised.
*/
void
skip_to_end_of_line(void)
{
  Char c = get_char();
  while (1) {
    if (c == '\n') {
      unget_char(c);
      return;
    }
    if (c == EOF)
      return;
    c = get_char();
  }
}

//////////////////////////////////////////////////////////////////////
/* unget_char(c):
   Restore character c onto input queue.

   Write a character back onto the input stream.
   EOF is gracefully handled.

   c: In-parameter; character to be pushed onto the input queue.
*/
inline void
unget_char(const Char c)
{
  if (c != EOF) {
    int ch = (int) c;
    if (ungetc(ch, current_file) == EOF)
      fatal("could not unget character");
  }
}


/**********************************************************************
                       parser subcommands
 **********************************************************************/

//////////////////////////////////////////////////////////////////////
/* parse_color_command:
   Process the commands m and DF, but not Df.

   col: In-out-parameter; the color object to be set, must have
        been initialized before.
*/
void
parse_color_command(color *col)
{
  ColorArg gray = 0;
  ColorArg red = 0, green = 0, blue = 0;
  ColorArg cyan = 0, magenta = 0, yellow = 0, black = 0;
  Char subcmd = next_arg_begin();
  switch((int) subcmd) {
  case 'c':			// DFc or mc: CMY
    cyan = get_color_arg();
    magenta = get_color_arg();
    yellow = get_color_arg();
    col->set_cmy(cyan, magenta, yellow);
    break;
  case 'd':			// DFd or md: set default color
    col->set_default();
    break;
  case 'g':			// DFg or mg: gray
    gray = get_color_arg();
    col->set_gray(gray);
    break;
  case 'k':			// DFk or mk: CMYK
    cyan = get_color_arg();
    magenta = get_color_arg();
    yellow = get_color_arg();
    black = get_color_arg();
    col->set_cmyk(cyan, magenta, yellow, black);
    break;
  case 'r':			// DFr or mr: RGB
    red = get_color_arg();
    green = get_color_arg();
    blue = get_color_arg();
    col->set_rgb(red, green, blue);
    break;
  default:
    error("invalid color scheme `%1'", (int) subcmd);
    break;
  } // end of color subcommands
}

//////////////////////////////////////////////////////////////////////
/* parse_D_command():
   Parse the subcommands of graphical command D.

   This is the part of the do_file() parser that scans the graphical
   subcommands.
   - Error on lacking or wrong arguments.
   - Warning on too many arguments.
   - Line is always skipped.
*/
void
parse_D_command()
{
  Char subcmd = next_arg_begin();
  switch((int) subcmd) {
  case '~':			// D~: draw B-spline
    // actually, this isn't available for some postprocessors
    // fall through
  default:			// unknown options are passed to device
    {
      IntArray *args = get_D_variable_args();
      send_draw(subcmd, args);
      position_to_end_of_args(args);
      delete args;
      break;
    }
  case 'a':			// Da: draw arc
    {
      IntArray *args = get_D_fixed_args(4);
      send_draw(subcmd, args);
      position_to_end_of_args(args);
      delete args;
      break;
    }
  case 'c':			// Dc: draw circle line
    {
      IntArray *args = get_D_fixed_args(1);
      send_draw(subcmd, args);
      // move to right end
      current_env->hpos += (*args)[0];
      delete args;
      break;
    }
  case 'C':			// DC: draw solid circle
    {
      IntArray *args = get_D_fixed_args_odd_dummy(1);
      send_draw(subcmd, args);
      // move to right end
      current_env->hpos += (*args)[0];
      delete args;
      break;
    }
  case 'e':			// De: draw ellipse line
  case 'E':			// DE: draw solid ellipse
    {
      IntArray *args = get_D_fixed_args(2);
      send_draw(subcmd, args);
      // move to right end
      current_env->hpos += (*args)[0];
      delete args;
      break;
    }
  case 'f':			// Df: set fill gray; obsoleted by DFg
    {
      IntArg arg = get_integer_arg();
      if ((arg >= 0) && (arg <= 1000)) {
	// convert arg and treat it like DFg
	ColorArg gray = color_from_Df_command(arg);
        current_env->fill->set_gray(gray);
      }
      else {
	// set fill color to the same value as the current outline color
	delete current_env->fill;
	current_env->fill = new color(current_env->col);
      }
      pr->change_fill_color(current_env);
      // skip unused `vertical' component (\D'...' always emits pairs)
      (void) get_integer_arg();
#   ifdef STUPID_DRAWING_POSITIONING
      current_env->hpos += arg;
#   endif
      skip_line_x();
      break;
    }
  case 'F':			// DF: set fill color, several formats
    parse_color_command(current_env->fill);
    pr->change_fill_color(current_env);
    // no positioning (setting-only command)
    skip_line_x();
    break;
  case 'l':			// Dl: draw line
    {
      IntArray *args = get_D_fixed_args(2);
      send_draw(subcmd, args);
      position_to_end_of_args(args);
      delete args;
      break;
    }
  case 'p':			// Dp: draw closed polygon line
  case 'P':			// DP: draw solid closed polygon
    {
      IntArray *args = get_D_variable_args();
      send_draw(subcmd, args);
#   ifdef STUPID_DRAWING_POSITIONING
      // final args positioning
      position_to_end_of_args(args);
#   endif
      delete args;
      break;
    }
  case 't':			// Dt: set line thickness
    {
      IntArray *args = get_D_fixed_args_odd_dummy(1);
      send_draw(subcmd, args);
#   ifdef STUPID_DRAWING_POSITIONING
      // final args positioning
      position_to_end_of_args(args);
#   endif
      delete args;
      break;
    }
  } // end of D subcommands
}

//////////////////////////////////////////////////////////////////////
/* parse_x_command():
   Parse subcommands of the device control command x.

   This is the part of the do_file() parser that scans the device
   controlling commands.
   - Error on duplicate prologue commands.
   - Error on wrong or lacking arguments.
   - Warning on too many arguments.
   - Line is always skipped.

   Globals:
   - current_env: is set by many subcommands.
   - npages: page counting variable

   Return: boolean in the meaning of `stopped'
           - true if parsing should be stopped (`x stop').
           - false if parsing should continue.
*/
bool
parse_x_command(void)
{
  bool stopped = false;
  char *subcmd_str = get_string_arg();
  char subcmd = subcmd_str[0];
  switch (subcmd) {
  case 'f':			// x font: mount font
    {
      IntArg n = get_integer_arg();
      char *name = get_string_arg();
      pr->load_font(n, name);
      a_delete name;
      skip_line_x();
      break;
    }
  case 'F':			// x Filename: set filename for errors
    {
      char *str_arg = get_string_arg();
      if (str_arg == 0)
	warning("empty argument for `x F' command");
      else {
	remember_source_filename(str_arg);
	a_delete str_arg;
      }
      break;
    }
  case 'H':			// x Height: set character height
    current_env->height = get_integer_arg();
    if (current_env->height == current_env->size)
      current_env->height = 0;
    skip_line_x();
    break;
  case 'i':			// x init: initialize device
    error("duplicate `x init' command");
    skip_line_x();
    break;
  case 'p':			// x pause: pause device
    skip_line_x();
    break;
  case 'r':			// x res: set resolution
    error("duplicate `x res' command");
    skip_line_x();
    break;
  case 's':			// x stop: stop device
    stopped = true;
    skip_line_x();
    break;
  case 'S':			// x Slant: set slant
    current_env->slant = get_integer_arg();
    skip_line_x();
    break;
  case 't':			// x trailer: generate trailer info
    skip_line_x();
    break;
  case 'T':			// x Typesetter: set typesetter
    error("duplicate `x T' command");
    skip_line();
    break;
  case 'u':			// x underline: from .cu
    {
      char *str_arg = get_string_arg();
      pr->special(str_arg, current_env, 'u');
      a_delete str_arg;
      skip_line_x();
      break;
    }
  case 'X':			// x X: send uninterpretedly to device
    {
      char *str_arg = get_extended_arg(); // includes line skip
      if (npages <= 0)
	error("`x X' command invalid before first `p' command");
      else
	pr->special(str_arg, current_env);
      a_delete str_arg;
      break;
    }
  default:			// ignore unknown x commands, but warn
    warning("unknown command `x %1'", subcmd);
    skip_line();
  }
  a_delete subcmd_str;
  return stopped;
}


/**********************************************************************
                     exported part (by driver.h)
 **********************************************************************/

////////////////////////////////////////////////////////////////////////
/* do_file():
   Parse and postprocess groff intermediate output.

   filename: "-" for standard input, normal file name otherwise
*/
void
do_file(const char *filename)
{
  Char command;
  bool stopped = false;		// terminating condition

#ifdef USE_ENV_STACK
  EnvStack env_stack = EnvStack();
#endif // USE_ENV_STACK

  // setup of global variables
  npages = 0;
  current_lineno = 1;
  // `pr' is initialized after the prologue.
  // `device' is set by the 1st prologue command.

  if (filename[0] == '-' && filename[1] == '\0')
    current_file = stdin;
  else {
    errno = 0;
    current_file = fopen(filename, "r");
    if (errno != 0 || current_file == 0) {
      error("can't open file `%1'", filename);
      return;
    }
  }
  remember_filename(filename);

  if (current_env != 0)
    delete_current_env();
  current_env = new environment;
  current_env->col = new color;
  current_env->fill = new color;
  current_env->fontno = -1;
  current_env->height = 0;
  current_env->hpos = -1;
  current_env->slant = 0;
  current_env->size = 0;
  current_env->vpos = -1;

  // parsing of prologue (first 3 commands)
  {
    char *str_arg;
    IntArg int_arg;

    // 1st command `x T'
    command = next_command();	
    if ((int) command == EOF)
      return;
    if ((int) command != 'x')
      fatal("the first command must be `x T'");
    str_arg = get_string_arg();
    if (str_arg[0] != 'T')
      fatal("the first command must be `x T'");
    a_delete str_arg;
    char *tmp_dev = get_string_arg();
    if (pr == 0) {		// note: `pr' initialized after prologue
      device = tmp_dev;
      if (!font::load_desc())
	fatal("couldn't load DESC file, can't continue");
    }
    else {
      if (device == 0 || strcmp(device, tmp_dev) != 0)
	fatal("all files must use the same device");
      a_delete tmp_dev;
    }
    skip_line_x();		// ignore further arguments
    current_env->size = 10 * font::sizescale;

    // 2nd command `x res'
    command = next_command();
    if ((int) command != 'x')
      fatal("the second command must be `x res'");
    str_arg = get_string_arg();
    if (str_arg[0] != 'r')
      fatal("the second command must be `x res'");
    a_delete str_arg;
    int_arg = get_integer_arg();
    EnvInt font_res = font::res;
    if (int_arg != font_res)
      fatal("resolution does not match");
    int_arg = get_integer_arg();
    if (int_arg != font::hor)
      fatal("minimum horizontal motion does not match");
    int_arg = get_integer_arg();
    if (int_arg != font::vert)
      fatal("minimum vertical motion does not match");
    skip_line_x();		// ignore further arguments

    // 3rd command `x init'
    command = next_command();
    if (command != 'x')
      fatal("the third command must be `x init'");
    str_arg = get_string_arg();
    if (str_arg[0] != 'i')
      fatal("the third command must be `x init'");
    a_delete str_arg;
    skip_line_x();
  }

  // parsing of body
  if (pr == 0)
    pr = make_printer();
  while (!stopped) {
    command = next_command();
    if (command == EOF)
      break;
    // spaces, tabs, comments, and newlines are skipped here
    switch ((int) command) {
    case '#':			// #: comment, ignore up to end of line
      skip_line();
      break;
#ifdef USE_ENV_STACK
    case '{':			// {: start a new environment (a copy)
      env_stack.push(current_env);
      break;
    case '}':			// }: pop previous env from stack
      delete_current_env();
      current_env = env_stack.pop();
      break;
#endif // USE_ENV_STACK
    case '0':			// ddc: obsolete jump and print command
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {				// expect 2 digits and a character
	char s[3];
	Char c = next_arg_begin();
	if (npages <= 0)
	  fatal_command(command);
	if (!isdigit((int) c)) {
	  error("digit expected");
	  c = 0;
	}
	s[0] = (char) command;
	s[1] = (char) c;
	s[2] = '\0';
	errno = 0;
	long int x = strtol(s, 0, 10);
	if (errno != 0)
	  error("couldn't convert 2 digits");
	EnvInt hor_pos = (EnvInt) x;
	current_env->hpos += hor_pos;
	c = next_arg_begin();
	if ((int) c == '\n' || (int) c == EOF)
	  error("character argument expected");
	else
	  pr->set_ascii_char((unsigned char) c, current_env);
	break;
      }
    case 'c':			// c: print ascii char without moving
      {
	if (npages <= 0)
	  fatal_command(command);
	Char c = next_arg_begin();
	if (c == '\n' || c == EOF)
	  error("missing argument to `c' command");
	else
	  pr->set_ascii_char((unsigned char) c, current_env);
	break;
      }
    case 'C':			// C: print named special character
      {
	if (npages <= 0)
	  fatal_command(command);
	char *str_arg = get_string_arg();
	pr->set_special_char(str_arg, current_env);
	a_delete str_arg;
	break;
      }
    case 'D':			// drawing commands
      if (npages <= 0)
	fatal_command(command);
      parse_D_command();
      break;
    case 'f':			// f: set font to number
      current_env->fontno = get_integer_arg();
      break;
    case 'F':			// F: obsolete, replaced by `x F'
      {
	char *str_arg = get_string_arg();
	remember_source_filename(str_arg);
	a_delete str_arg;
	break;
      }
    case 'h':			// h: relative horizontal move
      current_env->hpos += (EnvInt) get_integer_arg();
      break;
    case 'H':			// H: absolute horizontal positioning
      current_env->hpos = (EnvInt) get_integer_arg();
      break;
    case 'm':			// m: glyph color
      parse_color_command(current_env->col);
      pr->change_color(current_env);
      break;
    case 'n':			// n: print end of line
				// ignore two arguments (historically)
      if (npages <= 0)
	fatal_command(command);
      pr->end_of_line();
      (void) get_integer_arg();
      (void) get_integer_arg();
      break;
    case 'N':			// N: print char with given int code
      if (npages <= 0)
	fatal_command(command);
      pr->set_numbered_char(get_integer_arg(), current_env);
      break;
    case 'p':			// p: start new page with given number
      if (npages > 0)
	pr->end_page(current_env->vpos);
      npages++;			// increment # of processed pages
      pr->begin_page(get_integer_arg());
      current_env->vpos = 0;
      break;
    case 's':			// s: set point size
      current_env->size = get_integer_arg();
      if (current_env->height == current_env->size)
	current_env->height = 0;
      break;
    case 't':			// t: print a text word
      {
	char c;
	if (npages <= 0)
	  fatal_command(command);
	char *str_arg = get_string_arg();
	size_t i = 0;
	while ((c = str_arg[i++]) != '\0') {
	  EnvInt w;
	  pr->set_ascii_char((unsigned char) c, current_env, &w);
	  current_env->hpos += w;
	}
	a_delete str_arg;
	break;
      }
    case 'u':			// u: print spaced word
      {
	char c;
	if (npages <= 0)
	  fatal_command(command);
	EnvInt kern = (EnvInt) get_integer_arg();
	char *str_arg = get_string_arg();
	size_t i = 0;
	while ((c = str_arg[i++]) != '\0') {
	  EnvInt w;
	  pr->set_ascii_char((unsigned char) c, current_env, &w);
	  current_env->hpos += w + kern;
	}
	a_delete str_arg;
	break;
      }
    case 'v':			// v: relative vertical move
      current_env->vpos += (EnvInt) get_integer_arg();
      break;
    case 'V':			// V: absolute vertical positioning
      current_env->vpos = (EnvInt) get_integer_arg();
      break;
    case 'w':			// w: inform about paddable space
      break;
    case 'x':			// device controlling commands
      stopped = parse_x_command();
      break;
    default:
      warning("unrecognized command `%1'", (unsigned char) command);
      skip_line();
      break;
    } // end of switch
  } // end of while

  // end of file reached
  if (npages > 0)
    pr->end_page(current_env->vpos);
  delete pr;
  fclose(current_file);
  // If `stopped' is not `true' here then there wasn't any `x stop'.
  if (!stopped)
    warning("no final `x stop' command");
  delete_current_env();
}
