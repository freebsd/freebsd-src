// script.cc -- handle linker scripts for gold.

#include "gold.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "options.h"
#include "fileread.h"
#include "workqueue.h"
#include "readsyms.h"
#include "yyscript.h"
#include "script.h"
#include "script-c.h"

namespace gold
{

// A token read from a script file.  We don't implement keywords here;
// all keywords are simply represented as a string.

class Token
{
 public:
  // Token classification.
  enum Classification
  {
    // Token is invalid.
    TOKEN_INVALID,
    // Token indicates end of input.
    TOKEN_EOF,
    // Token is a string of characters.
    TOKEN_STRING,
    // Token is an operator.
    TOKEN_OPERATOR,
    // Token is a number (an integer).
    TOKEN_INTEGER
  };

  // We need an empty constructor so that we can put this STL objects.
  Token()
    : classification_(TOKEN_INVALID), value_(), opcode_(0),
      lineno_(0), charpos_(0)
  { }

  // A general token with no value.
  Token(Classification classification, int lineno, int charpos)
    : classification_(classification), value_(), opcode_(0),
      lineno_(lineno), charpos_(charpos)
  {
    gold_assert(classification == TOKEN_INVALID
		|| classification == TOKEN_EOF);
  }

  // A general token with a value.
  Token(Classification classification, const std::string& value,
	int lineno, int charpos)
    : classification_(classification), value_(value), opcode_(0),
      lineno_(lineno), charpos_(charpos)
  {
    gold_assert(classification != TOKEN_INVALID
		&& classification != TOKEN_EOF);
  }

  // A token representing a string of characters.
  Token(const std::string& s, int lineno, int charpos)
    : classification_(TOKEN_STRING), value_(s), opcode_(0),
      lineno_(lineno), charpos_(charpos)
  { }

  // A token representing an operator.
  Token(int opcode, int lineno, int charpos)
    : classification_(TOKEN_OPERATOR), value_(), opcode_(opcode),
      lineno_(lineno), charpos_(charpos)
  { }

  // Return whether the token is invalid.
  bool
  is_invalid() const
  { return this->classification_ == TOKEN_INVALID; }

  // Return whether this is an EOF token.
  bool
  is_eof() const
  { return this->classification_ == TOKEN_EOF; }

  // Return the token classification.
  Classification
  classification() const
  { return this->classification_; }

  // Return the line number at which the token starts.
  int
  lineno() const
  { return this->lineno_; }

  // Return the character position at this the token starts.
  int
  charpos() const
  { return this->charpos_; }

  // Get the value of a token.

  const std::string&
  string_value() const
  {
    gold_assert(this->classification_ == TOKEN_STRING);
    return this->value_;
  }

  int
  operator_value() const
  {
    gold_assert(this->classification_ == TOKEN_OPERATOR);
    return this->opcode_;
  }

  int64_t
  integer_value() const
  {
    gold_assert(this->classification_ == TOKEN_INTEGER);
    return strtoll(this->value_.c_str(), NULL, 0);
  }

 private:
  // The token classification.
  Classification classification_;
  // The token value, for TOKEN_STRING or TOKEN_INTEGER.
  std::string value_;
  // The token value, for TOKEN_OPERATOR.
  int opcode_;
  // The line number where this token started (one based).
  int lineno_;
  // The character position within the line where this token started
  // (one based).
  int charpos_;
};

// This class handles lexing a file into a sequence of tokens.  We
// don't expect linker scripts to be large, so we just read them and
// tokenize them all at once.

class Lex
{
 public:
  Lex(Input_file* input_file)
    : input_file_(input_file), tokens_()
  { }

  // Tokenize the file.  Return the final token, which will be either
  // an invalid token or an EOF token.  An invalid token indicates
  // that tokenization failed.
  Token
  tokenize();

  // A token sequence.
  typedef std::vector<Token> Token_sequence;

  // Return the tokens.
  const Token_sequence&
  tokens() const
  { return this->tokens_; }

 private:
  Lex(const Lex&);
  Lex& operator=(const Lex&);

  // Read the file into a string buffer.
  void
  read_file(std::string*);

  // Make a general token with no value at the current location.
  Token
  make_token(Token::Classification c, const char* p) const
  { return Token(c, this->lineno_, p - this->linestart_ + 1); }

  // Make a general token with a value at the current location.
  Token
  make_token(Token::Classification c, const std::string& v, const char* p)
    const
  { return Token(c, v, this->lineno_, p - this->linestart_ + 1); }

  // Make an operator token at the current location.
  Token
  make_token(int opcode, const char* p) const
  { return Token(opcode, this->lineno_, p - this->linestart_ + 1); }

  // Make an invalid token at the current location.
  Token
  make_invalid_token(const char* p)
  { return this->make_token(Token::TOKEN_INVALID, p); }

  // Make an EOF token at the current location.
  Token
  make_eof_token(const char* p)
  { return this->make_token(Token::TOKEN_EOF, p); }

  // Return whether C can be the first character in a name.  C2 is the
  // next character, since we sometimes need that.
  static inline bool
  can_start_name(char c, char c2);

  // Return whether C can appear in a name which has already started.
  static inline bool
  can_continue_name(char c);

  // Return whether C, C2, C3 can start a hex number.
  static inline bool
  can_start_hex(char c, char c2, char c3);

  // Return whether C can appear in a hex number.
  static inline bool
  can_continue_hex(char c);

  // Return whether C can start a non-hex number.
  static inline bool
  can_start_number(char c);

  // Return whether C can appear in a non-hex number.
  static inline bool
  can_continue_number(char c)
  { return Lex::can_start_number(c); }

  // If C1 C2 C3 form a valid three character operator, return the
  // opcode.  Otherwise return 0.
  static inline int
  three_char_operator(char c1, char c2, char c3);

  // If C1 C2 form a valid two character operator, return the opcode.
  // Otherwise return 0.
  static inline int
  two_char_operator(char c1, char c2);

  // If C1 is a valid one character operator, return the opcode.
  // Otherwise return 0.
  static inline int
  one_char_operator(char c1);

  // Read the next token.
  Token
  get_token(const char**);

  // Skip a C style /* */ comment.  Return false if the comment did
  // not end.
  bool
  skip_c_comment(const char**);

  // Skip a line # comment.  Return false if there was no newline.
  bool
  skip_line_comment(const char**);

  // Build a token CLASSIFICATION from all characters that match
  // CAN_CONTINUE_FN.  The token starts at START.  Start matching from
  // MATCH.  Set *PP to the character following the token.
  inline Token
  gather_token(Token::Classification, bool (*can_continue_fn)(char),
	       const char* start, const char* match, const char** pp);

  // Build a token from a quoted string.
  Token
  gather_quoted_string(const char** pp);

  // The file we are reading.
  Input_file* input_file_;
  // The token sequence we create.
  Token_sequence tokens_;
  // The current line number.
  int lineno_;
  // The start of the current line in the buffer.
  const char* linestart_;
};

// Read the whole file into memory.  We don't expect linker scripts to
// be large, so we just use a std::string as a buffer.  We ignore the
// data we've already read, so that we read aligned buffers.

void
Lex::read_file(std::string* contents)
{
  contents->clear();
  off_t off = 0;
  off_t got;
  unsigned char buf[BUFSIZ];
  do
    {
      this->input_file_->file().read(off, sizeof buf, buf, &got);
      contents->append(reinterpret_cast<char*>(&buf[0]), got);
    }
  while (got == sizeof buf);
}

// Return whether C can be the start of a name, if the next character
// is C2.  A name can being with a letter, underscore, period, or
// dollar sign.  Because a name can be a file name, we also permit
// forward slash, backslash, and tilde.  Tilde is the tricky case
// here; GNU ld also uses it as a bitwise not operator.  It is only
// recognized as the operator if it is not immediately followed by
// some character which can appear in a symbol.  That is, "~0" is a
// symbol name, and "~ 0" is an expression using bitwise not.  We are
// compatible.

inline bool
Lex::can_start_name(char c, char c2)
{
  switch (c)
    {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'Q': case 'P': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'q': case 'p': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case '_': case '.': case '$': case '/': case '\\':
      return true;

    case '~':
      return can_continue_name(c2);

    default:
      return false;
    }
}

// Return whether C can continue a name which has already started.
// Subsequent characters in a name are the same as the leading
// characters, plus digits and "=+-:[],?*".  So in general the linker
// script language requires spaces around operators.

inline bool
Lex::can_continue_name(char c)
{
  switch (c)
    {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'Q': case 'P': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'q': case 'p': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case '_': case '.': case '$': case '/': case '\\':
    case '~':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '=': case '+': case '-': case ':': case '[': case ']':
    case ',': case '?': case '*':
      return true;

    default:
      return false;
    }
}

// For a number we accept 0x followed by hex digits, or any sequence
// of digits.  The old linker accepts leading '$' for hex, and
// trailing HXBOD.  Those are for MRI compatibility and we don't
// accept them.  The old linker also accepts trailing MK for mega or
// kilo.  Those are mentioned in the documentation, and we accept
// them.

// Return whether C1 C2 C3 can start a hex number.

inline bool
Lex::can_start_hex(char c1, char c2, char c3)
{
  if (c1 == '0' && (c2 == 'x' || c2 == 'X'))
    return Lex::can_continue_hex(c3);
  return false;
}

// Return whether C can appear in a hex number.

inline bool
Lex::can_continue_hex(char c)
{
  switch (c)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      return true;

    default:
      return false;
    }
}

// Return whether C can start a non-hex number.

inline bool
Lex::can_start_number(char c)
{
  switch (c)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return true;

    default:
      return false;
    }
}

// If C1 C2 C3 form a valid three character operator, return the
// opcode (defined in the yyscript.h file generated from yyscript.y).
// Otherwise return 0.

inline int
Lex::three_char_operator(char c1, char c2, char c3)
{
  switch (c1)
    {
    case '<':
      if (c2 == '<' && c3 == '=')
	return LSHIFTEQ;
      break;
    case '>':
      if (c2 == '>' && c3 == '=')
	return RSHIFTEQ;
      break;
    default:
      break;
    }
  return 0;
}

// If C1 C2 form a valid two character operator, return the opcode
// (defined in the yyscript.h file generated from yyscript.y).
// Otherwise return 0.

inline int
Lex::two_char_operator(char c1, char c2)
{
  switch (c1)
    {
    case '=':
      if (c2 == '=')
	return EQ;
      break;
    case '!':
      if (c2 == '=')
	return NE;
      break;
    case '+':
      if (c2 == '=')
	return PLUSEQ;
      break;
    case '-':
      if (c2 == '=')
	return MINUSEQ;
      break;
    case '*':
      if (c2 == '=')
	return MULTEQ;
      break;
    case '/':
      if (c2 == '=')
	return DIVEQ;
      break;
    case '|':
      if (c2 == '=')
	return OREQ;
      if (c2 == '|')
	return OROR;
      break;
    case '&':
      if (c2 == '=')
	return ANDEQ;
      if (c2 == '&')
	return ANDAND;
      break;
    case '>':
      if (c2 == '=')
	return GE;
      if (c2 == '>')
	return RSHIFT;
      break;
    case '<':
      if (c2 == '=')
	return LE;
      if (c2 == '<')
	return LSHIFT;
      break;
    default:
      break;
    }
  return 0;
}

// If C1 is a valid operator, return the opcode.  Otherwise return 0.

inline int
Lex::one_char_operator(char c1)
{
  switch (c1)
    {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '!':
    case '&':
    case '|':
    case '^':
    case '~':
    case '<':
    case '>':
    case '=':
    case '?':
    case ',':
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case ':':
    case ';':
      return c1;
    default:
      return 0;
    }
}

// Skip a C style comment.  *PP points to just after the "/*".  Return
// false if the comment did not end.

bool
Lex::skip_c_comment(const char** pp)
{
  const char* p = *pp;
  while (p[0] != '*' || p[1] != '/')
    {
      if (*p == '\0')
	{
	  *pp = p;
	  return false;
	}

      if (*p == '\n')
	{
	  ++this->lineno_;
	  this->linestart_ = p + 1;
	}
      ++p;
    }

  *pp = p + 2;
  return true;
}

// Skip a line # comment.  Return false if there was no newline.

bool
Lex::skip_line_comment(const char** pp)
{
  const char* p = *pp;
  size_t skip = strcspn(p, "\n");
  if (p[skip] == '\0')
    {
      *pp = p + skip;
      return false;
    }

  p += skip + 1;
  ++this->lineno_;
  this->linestart_ = p;
  *pp = p;

  return true;
}

// Build a token CLASSIFICATION from all characters that match
// CAN_CONTINUE_FN.  Update *PP.

inline Token
Lex::gather_token(Token::Classification classification,
		  bool (*can_continue_fn)(char),
		  const char* start,
		  const char* match,
		  const char **pp)
{
  while ((*can_continue_fn)(*match))
    ++match;
  *pp = match;
  return this->make_token(classification,
			  std::string(start, match - start),
			  start);
}

// Build a token from a quoted string.

Token
Lex::gather_quoted_string(const char** pp)
{
  const char* start = *pp;
  const char* p = start;
  ++p;
  size_t skip = strcspn(p, "\"\n");
  if (p[skip] != '"')
    return this->make_invalid_token(start);
  *pp = p + skip + 1;
  return this->make_token(Token::TOKEN_STRING,
			  std::string(p, skip),
			  start);
}

// Return the next token at *PP.  Update *PP.  General guideline: we
// require linker scripts to be simple ASCII.  No unicode linker
// scripts.  In particular we can assume that any '\0' is the end of
// the input.

Token
Lex::get_token(const char** pp)
{
  const char* p = *pp;

  while (true)
    {
      if (*p == '\0')
	{
	  *pp = p;
	  return this->make_eof_token(p);
	}

      // Skip whitespace quickly.
      while (*p == ' ' || *p == '\t')
	++p;

      if (*p == '\n')
	{
	  ++p;
	  ++this->lineno_;
	  this->linestart_ = p;
	  continue;
	}

      // Skip C style comments.
      if (p[0] == '/' && p[1] == '*')
	{
	  int lineno = this->lineno_;
	  int charpos = p - this->linestart_ + 1;

	  *pp = p + 2;
	  if (!this->skip_c_comment(pp))
	    return Token(Token::TOKEN_INVALID, lineno, charpos);
	  p = *pp;

	  continue;
	}

      // Skip line comments.
      if (*p == '#')
	{
	  *pp = p + 1;
	  if (!this->skip_line_comment(pp))
	    return this->make_eof_token(p);
	  p = *pp;
	  continue;
	}

      // Check for a name.
      if (Lex::can_start_name(p[0], p[1]))
	return this->gather_token(Token::TOKEN_STRING,
				  Lex::can_continue_name,
				  p, p + 2, pp);

      // We accept any arbitrary name in double quotes, as long as it
      // does not cross a line boundary.
      if (*p == '"')
	{
	  *pp = p;
	  return this->gather_quoted_string(pp);
	}

      // Check for a number.

      if (Lex::can_start_hex(p[0], p[1], p[2]))
	return this->gather_token(Token::TOKEN_INTEGER,
				  Lex::can_continue_hex,
				  p, p + 3, pp);

      if (Lex::can_start_number(p[0]))
	return this->gather_token(Token::TOKEN_INTEGER,
				  Lex::can_continue_number,
				  p, p + 1, pp);

      // Check for operators.

      int opcode = Lex::three_char_operator(p[0], p[1], p[2]);
      if (opcode != 0)
	{
	  *pp = p + 3;
	  return this->make_token(opcode, p);
	}

      opcode = Lex::two_char_operator(p[0], p[1]);
      if (opcode != 0)
	{
	  *pp = p + 2;
	  return this->make_token(opcode, p);
	}

      opcode = Lex::one_char_operator(p[0]);
      if (opcode != 0)
	{
	  *pp = p + 1;
	  return this->make_token(opcode, p);
	}

      return this->make_token(Token::TOKEN_INVALID, p);
    }
}

// Tokenize the file.  Return the final token.

Token
Lex::tokenize()
{
  std::string contents;
  this->read_file(&contents);

  const char* p = contents.c_str();

  this->lineno_ = 1;
  this->linestart_ = p;

  while (true)
    {
      Token t(this->get_token(&p));

      // Don't let an early null byte fool us into thinking that we've
      // reached the end of the file.
      if (t.is_eof()
	  && static_cast<size_t>(p - contents.c_str()) < contents.length())
	t = this->make_invalid_token(p);

      if (t.is_invalid() || t.is_eof())
	return t;

      this->tokens_.push_back(t);
    }
}

// A trivial task which waits for THIS_BLOCKER to be clear and then
// clears NEXT_BLOCKER.  THIS_BLOCKER may be NULL.

class Script_unblock : public Task
{
 public:
  Script_unblock(Task_token* this_blocker, Task_token* next_blocker)
    : this_blocker_(this_blocker), next_blocker_(next_blocker)
  { }

  ~Script_unblock()
  {
    if (this->this_blocker_ != NULL)
      delete this->this_blocker_;
  }

  Is_runnable_type
  is_runnable(Workqueue*)
  {
    if (this->this_blocker_ != NULL && this->this_blocker_->is_blocked())
      return IS_BLOCKED;
    return IS_RUNNABLE;
  }

  Task_locker*
  locks(Workqueue* workqueue)
  {
    return new Task_locker_block(*this->next_blocker_, workqueue);
  }

  void
  run(Workqueue*)
  { }

 private:
  Task_token* this_blocker_;
  Task_token* next_blocker_;
};

// This class holds data passed through the parser to the lexer and to
// the parser support functions.  This avoids global variables.  We
// can't use global variables because we need not be called in the
// main thread.

class Parser_closure
{
 public:
  Parser_closure(const char* filename,
		 const Position_dependent_options& posdep_options,
		 bool in_group,
		 const Lex::Token_sequence* tokens)
    : filename_(filename), posdep_options_(posdep_options),
      in_group_(in_group), tokens_(tokens),
      next_token_index_(0), inputs_(NULL)
  { }

  // Return the file name.
  const char*
  filename() const
  { return this->filename_; }

  // Return the position dependent options.  The caller may modify
  // this.
  Position_dependent_options&
  position_dependent_options()
  { return this->posdep_options_; }

  // Return whether this script is being run in a group.
  bool
  in_group() const
  { return this->in_group_; }

  // Whether we are at the end of the token list.
  bool
  at_eof() const
  { return this->next_token_index_ >= this->tokens_->size(); }

  // Return the next token.
  const Token*
  next_token()
  {
    const Token* ret = &(*this->tokens_)[this->next_token_index_];
    ++this->next_token_index_;
    return ret;
  }

  // Return the list of input files, creating it if necessary.  This
  // is a space leak--we never free the INPUTS_ pointer.
  Input_arguments*
  inputs()
  {
    if (this->inputs_ == NULL)
      this->inputs_ = new Input_arguments();
    return this->inputs_;
  }

  // Return whether we saw any input files.
  bool
  saw_inputs() const
  { return this->inputs_ != NULL && !this->inputs_->empty(); }

 private:
  // The name of the file we are reading.
  const char* filename_;
  // The position dependent options.
  Position_dependent_options posdep_options_;
  // Whether we are currently in a --start-group/--end-group.
  bool in_group_;

  // The tokens to be returned by the lexer.
  const Lex::Token_sequence* tokens_;
  // The index of the next token to return.
  unsigned int next_token_index_;
  // New input files found to add to the link.
  Input_arguments* inputs_;
};

// FILE was found as an argument on the command line.  Try to read it
// as a script.  We've already read BYTES of data into P, but we
// ignore that.  Return true if the file was handled.

bool
read_input_script(Workqueue* workqueue, const General_options& options,
		  Symbol_table* symtab, Layout* layout,
		  const Dirsearch& dirsearch, Input_objects* input_objects,
		  Input_group* input_group,
		  const Input_argument* input_argument,
		  Input_file* input_file, const unsigned char*, off_t,
		  Task_token* this_blocker, Task_token* next_blocker)
{
  Lex lex(input_file);
  if (lex.tokenize().is_invalid())
    return false;

  Parser_closure closure(input_file->filename().c_str(),
			 input_argument->file().options(),
			 input_group != NULL,
			 &lex.tokens());

  if (yyparse(&closure) != 0)
    return false;

  // THIS_BLOCKER must be clear before we may add anything to the
  // symbol table.  We are responsible for unblocking NEXT_BLOCKER
  // when we are done.  We are responsible for deleting THIS_BLOCKER
  // when it is unblocked.

  if (!closure.saw_inputs())
    {
      // The script did not add any files to read.  Note that we are
      // not permitted to call NEXT_BLOCKER->unblock() here even if
      // THIS_BLOCKER is NULL, as we are not in the main thread.
      workqueue->queue(new Script_unblock(this_blocker, next_blocker));
      return true;
    }

  for (Input_arguments::const_iterator p = closure.inputs()->begin();
       p != closure.inputs()->end();
       ++p)
    {
      Task_token* nb;
      if (p + 1 == closure.inputs()->end())
	nb = next_blocker;
      else
	{
	  nb = new Task_token();
	  nb->add_blocker();
	}
      workqueue->queue(new Read_symbols(options, input_objects, symtab,
					layout, dirsearch, &*p,
					input_group, this_blocker, nb));
      this_blocker = nb;
    }

  return true;
}

// Manage mapping from keywords to the codes expected by the bison
// parser.

class Keyword_to_parsecode
{
 public:
  // The structure which maps keywords to parsecodes.
  struct Keyword_parsecode
  {
    // Keyword.
    const char* keyword;
    // Corresponding parsecode.
    int parsecode;
  };

  // Return the parsecode corresponding KEYWORD, or 0 if it is not a
  // keyword.
  static int
  keyword_to_parsecode(const char* keyword);

 private:
  // The array of all keywords.
  static const Keyword_parsecode keyword_parsecodes_[];

  // The number of keywords.
  static const int keyword_count;
};

// Mapping from keyword string to keyword parsecode.  This array must
// be kept in sorted order.  Parsecodes are looked up using bsearch.
// This array must correspond to the list of parsecodes in yyscript.y.

const Keyword_to_parsecode::Keyword_parsecode
Keyword_to_parsecode::keyword_parsecodes_[] =
{
  { "ABSOLUTE", ABSOLUTE },
  { "ADDR", ADDR },
  { "ALIGN", ALIGN_K },
  { "ASSERT", ASSERT_K },
  { "AS_NEEDED", AS_NEEDED },
  { "AT", AT },
  { "BIND", BIND },
  { "BLOCK", BLOCK },
  { "BYTE", BYTE },
  { "CONSTANT", CONSTANT },
  { "CONSTRUCTORS", CONSTRUCTORS },
  { "COPY", COPY },
  { "CREATE_OBJECT_SYMBOLS", CREATE_OBJECT_SYMBOLS },
  { "DATA_SEGMENT_ALIGN", DATA_SEGMENT_ALIGN },
  { "DATA_SEGMENT_END", DATA_SEGMENT_END },
  { "DATA_SEGMENT_RELRO_END", DATA_SEGMENT_RELRO_END },
  { "DEFINED", DEFINED },
  { "DSECT", DSECT },
  { "ENTRY", ENTRY },
  { "EXCLUDE_FILE", EXCLUDE_FILE },
  { "EXTERN", EXTERN },
  { "FILL", FILL },
  { "FLOAT", FLOAT },
  { "FORCE_COMMON_ALLOCATION", FORCE_COMMON_ALLOCATION },
  { "GROUP", GROUP },
  { "HLL", HLL },
  { "INCLUDE", INCLUDE },
  { "INFO", INFO },
  { "INHIBIT_COMMON_ALLOCATION", INHIBIT_COMMON_ALLOCATION },
  { "INPUT", INPUT },
  { "KEEP", KEEP },
  { "LENGTH", LENGTH },
  { "LOADADDR", LOADADDR },
  { "LONG", LONG },
  { "MAP", MAP },
  { "MAX", MAX_K },
  { "MEMORY", MEMORY },
  { "MIN", MIN_K },
  { "NEXT", NEXT },
  { "NOCROSSREFS", NOCROSSREFS },
  { "NOFLOAT", NOFLOAT },
  { "NOLOAD", NOLOAD },
  { "ONLY_IF_RO", ONLY_IF_RO },
  { "ONLY_IF_RW", ONLY_IF_RW },
  { "ORIGIN", ORIGIN },
  { "OUTPUT", OUTPUT },
  { "OUTPUT_ARCH", OUTPUT_ARCH },
  { "OUTPUT_FORMAT", OUTPUT_FORMAT },
  { "OVERLAY", OVERLAY },
  { "PHDRS", PHDRS },
  { "PROVIDE", PROVIDE },
  { "PROVIDE_HIDDEN", PROVIDE_HIDDEN },
  { "QUAD", QUAD },
  { "SEARCH_DIR", SEARCH_DIR },
  { "SECTIONS", SECTIONS },
  { "SEGMENT_START", SEGMENT_START },
  { "SHORT", SHORT },
  { "SIZEOF", SIZEOF },
  { "SIZEOF_HEADERS", SIZEOF_HEADERS },
  { "SORT_BY_ALIGNMENT", SORT_BY_ALIGNMENT },
  { "SORT_BY_NAME", SORT_BY_NAME },
  { "SPECIAL", SPECIAL },
  { "SQUAD", SQUAD },
  { "STARTUP", STARTUP },
  { "SUBALIGN", SUBALIGN },
  { "SYSLIB", SYSLIB },
  { "TARGET", TARGET_K },
  { "TRUNCATE", TRUNCATE },
  { "VERSION", VERSIONK },
  { "global", GLOBAL },
  { "l", LENGTH },
  { "len", LENGTH },
  { "local", LOCAL },
  { "o", ORIGIN },
  { "org", ORIGIN },
  { "sizeof_headers", SIZEOF_HEADERS },
};

const int Keyword_to_parsecode::keyword_count =
  (sizeof(Keyword_to_parsecode::keyword_parsecodes_)
   / sizeof(Keyword_to_parsecode::keyword_parsecodes_[0]));

// Comparison function passed to bsearch.

extern "C"
{

static int
ktt_compare(const void* keyv, const void* kttv)
{
  const char* key = static_cast<const char*>(keyv);
  const Keyword_to_parsecode::Keyword_parsecode* ktt =
    static_cast<const Keyword_to_parsecode::Keyword_parsecode*>(kttv);
  return strcmp(key, ktt->keyword);
}

} // End extern "C".

int
Keyword_to_parsecode::keyword_to_parsecode(const char* keyword)
{
  void* kttv = bsearch(keyword,
		       Keyword_to_parsecode::keyword_parsecodes_,
		       Keyword_to_parsecode::keyword_count,
		       sizeof(Keyword_to_parsecode::keyword_parsecodes_[0]),
		       ktt_compare);
  if (kttv == NULL)
    return 0;
  Keyword_parsecode* ktt = static_cast<Keyword_parsecode*>(kttv);
  return ktt->parsecode;
}

} // End namespace gold.

// The remaining functions are extern "C", so it's clearer to not put
// them in namespace gold.

using namespace gold;

// This function is called by the bison parser to return the next
// token.

extern "C" int
yylex(YYSTYPE* lvalp, void* closurev)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);

  if (closure->at_eof())
    return 0;

  const Token* token = closure->next_token();

  switch (token->classification())
    {
    default:
    case Token::TOKEN_INVALID:
    case Token::TOKEN_EOF:
      gold_unreachable();

    case Token::TOKEN_STRING:
      {
	const char* str = token->string_value().c_str();
	int parsecode = Keyword_to_parsecode::keyword_to_parsecode(str);
	if (parsecode != 0)
	  return parsecode;
	lvalp->string = str;
	return STRING;
      }

    case Token::TOKEN_OPERATOR:
      return token->operator_value();

    case Token::TOKEN_INTEGER:
      lvalp->integer = token->integer_value();
      return INTEGER;
    }
}

// This function is called by the bison parser to report an error.

extern "C" void
yyerror(void* closurev, const char* message)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);

  fprintf(stderr, _("%s: %s: %s\n"),
	  program_name, closure->filename(), message);
  gold_exit(false);
}

// Called by the bison parser to add a file to the link.

extern "C" void
script_add_file(void* closurev, const char* name)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);
  Input_file_argument file(name, false, closure->position_dependent_options());
  closure->inputs()->add_file(file);
}

// Called by the bison parser to start a group.  If we are already in
// a group, that means that this script was invoked within a
// --start-group --end-group sequence on the command line, or that
// this script was found in a GROUP of another script.  In that case,
// we simply continue the existing group, rather than starting a new
// one.  It is possible to construct a case in which this will do
// something other than what would happen if we did a recursive group,
// but it's hard to imagine why the different behaviour would be
// useful for a real program.  Avoiding recursive groups is simpler
// and more efficient.

extern "C" void
script_start_group(void* closurev)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);
  if (!closure->in_group())
    closure->inputs()->start_group();
}

// Called by the bison parser at the end of a group.

extern "C" void
script_end_group(void* closurev)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);
  if (!closure->in_group())
    closure->inputs()->end_group();
}

// Called by the bison parser to start an AS_NEEDED list.

extern "C" void
script_start_as_needed(void* closurev)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);
  closure->position_dependent_options().set_as_needed();
}

// Called by the bison parser at the end of an AS_NEEDED list.

extern "C" void
script_end_as_needed(void* closurev)
{
  Parser_closure* closure = static_cast<Parser_closure*>(closurev);
  closure->position_dependent_options().clear_as_needed();
}
