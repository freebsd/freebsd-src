/* Routines for building, ordering, and printing the keyword list.
   Copyright (C) 1989-1998, 2000 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

#include <stdio.h>
#include <string.h> /* declares strncpy(), strchr() */
#include <stdlib.h> /* declares malloc(), free(), abs(), exit(), abort() */
#include <ctype.h>  /* declares isprint() */
#include <assert.h> /* defines assert() */
#include <limits.h> /* defines SCHAR_MAX etc. */
#include "options.h"
#include "read-line.h"
#include "hash-table.h"
#include "key-list.h"
#include "trace.h"
#include "version.h"

/* Make the hash table 8 times larger than the number of keyword entries. */
static const int TABLE_MULTIPLE     = 10;

/* Efficiently returns the least power of two greater than or equal to X! */
#define POW(X) ((!X)?1:(X-=1,X|=X>>1,X|=X>>2,X|=X>>4,X|=X>>8,X|=X>>16,(++X)))

int Key_List::determined[MAX_ALPHA_SIZE];

/* Destructor dumps diagnostics during debugging. */

Key_List::~Key_List (void)
{
  T (Trace t ("Key_List::~Key_List");)
  if (option[DEBUG])
    {
      fprintf (stderr, "\nDumping key list information:\ntotal non-static linked keywords = %d"
               "\ntotal keywords = %d\ntotal duplicates = %d\nmaximum key length = %d\n",
               list_len, total_keys, total_duplicates, max_key_len);
      dump ();
      fprintf (stderr, "End dumping list.\n\n");
    }
}

/* Gathers the input stream into a buffer until one of two things occur:

   1. We read a '%' followed by a '%'
   2. We read a '%' followed by a '}'

   The first symbolizes the beginning of the keyword list proper,
   The second symbolizes the end of the C source code to be generated
   verbatim in the output file.

   I assume that the keys are separated from the optional preceding struct
   declaration by a consecutive % followed by either % or } starting in
   the first column. The code below uses an expandible buffer to scan off
   and return a pointer to all the code (if any) appearing before the delimiter. */

const char *
Key_List::get_special_input (char delimiter)
{
  T (Trace t ("Key_List::get_special_input");)
  int size  = 80;
  char *buf = new char[size];
  int c, i;

  for (i = 0; (c = getchar ()) != EOF; i++)
    {
      if (c == '%')
        {
          if ((c = getchar ()) == delimiter)
            {

              while ((c = getchar ()) != '\n')
                ; /* discard newline */

              if (i == 0)
                return "";
              else
                {
                  buf[delimiter == '%' && buf[i - 2] == ';' ? i - 2 : i - 1] = '\0';
                  return buf;
                }
            }
          else
            buf[i++] = '%';
        }
      else if (i >= size) /* Yikes, time to grow the buffer! */
        {
          char *temp = new char[size *= 2];
          int j;

          for (j = 0; j < i; j++)
            temp[j] = buf[j];

          buf = temp;
        }
      buf[i] = c;
    }

  return 0;        /* Problem here. */
}

/* Stores any C text that must be included verbatim into the
   generated code output. */

const char *
Key_List::save_include_src (void)
{
  T (Trace t ("Key_List::save_include_src");)
  int c;

  if ((c = getchar ()) != '%')
    ungetc (c, stdin);
  else if ((c = getchar ()) != '{')
    {
      fprintf (stderr, "internal error, %c != '{' on line %d in file %s", c, __LINE__, __FILE__);
      exit (1);
    }
  else
    return get_special_input ('}');
  return "";
}

/* Determines from the input file whether the user wants to build a table
   from a user-defined struct, or whether the user is content to simply
   use the default array of keys. */

const char *
Key_List::get_array_type (void)
{
  T (Trace t ("Key_List::get_array_type");)
  return get_special_input ('%');
}

/* strcspn - find length of initial segment of S consisting entirely
   of characters not from REJECT (borrowed from Henry Spencer's
   ANSI string package, when GNU libc comes out I'll replace this...). */

#ifndef strcspn
inline int
Key_List::strcspn (const char *s, const char *reject)
{
  T (Trace t ("Key_List::strcspn");)
  const char *scan;
  const char *rej_scan;
  int   count = 0;

  for (scan = s; *scan; scan++)
    {

      for (rej_scan = reject; *rej_scan; rej_scan++)
        if (*scan == *rej_scan)
          return count;

      count++;
    }

  return count;
}
#endif

/* Sets up the Return_Type, the Struct_Tag type and the Array_Type
   based upon various user Options. */

void
Key_List::set_output_types (void)
{
  T (Trace t ("Key_List::set_output_types");)
  if (option[TYPE])
    {
      array_type = get_array_type ();
      if (!array_type)
        /* Something's wrong, but we'll catch it later on, in read_keys()... */
        return;
      /* Yow, we've got a user-defined type... */
      int i = strcspn (array_type, "{\n\0");
      /* Remove trailing whitespace. */
      while (i > 0 && strchr (" \t", array_type[i-1]))
        i--;
      int struct_tag_length = i;

      /* Set `struct_tag' to a naked "struct something". */
      char *structtag = new char[struct_tag_length + 1];
      strncpy (structtag, array_type, struct_tag_length);
      structtag[struct_tag_length] = '\0';
      struct_tag = structtag;

      /* The return type of the lookup function is "struct something *".
         No "const" here, because if !option[CONST], some user code might want
         to modify the structure. */
      char *rettype = new char[struct_tag_length + 3];
      strncpy (rettype, array_type, struct_tag_length);
      rettype[struct_tag_length] = ' ';
      rettype[struct_tag_length + 1] = '*';
      rettype[struct_tag_length + 2] = '\0';
      return_type = rettype;
    }
}

/* Extracts a key from an input line and creates a new List_Node for it. */

static List_Node *
parse_line (const char *line, const char *delimiters)
{
  if (*line == '"')
    {
      /* Parse a string in ANSI C syntax. */
      char *key = new char[strlen(line)];
      char *kp = key;
      const char *lp = line + 1;

      for (; *lp;)
        {
          char c = *lp;

          if (c == '\0')
            {
              fprintf (stderr, "unterminated string: %s\n", line);
              exit (1);
            }
          else if (c == '\\')
            {
              c = *++lp;
              switch (c)
                {
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7':
                  {
                    int code = 0;
                    int count = 0;
                    while (count < 3 && *lp >= '0' && *lp <= '7')
                      {
                        code = (code << 3) + (*lp - '0');
                        lp++;
                        count++;
                      }
                    if (code > UCHAR_MAX)
                      fprintf (stderr, "octal escape out of range: %s\n", line);
                    *kp = (char) code;
                    break;
                  }
                case 'x':
                  {
                    int code = 0;
                    int count = 0;
                    lp++;
                    while ((*lp >= '0' && *lp <= '9')
                           || (*lp >= 'A' && *lp <= 'F')
                           || (*lp >= 'a' && *lp <= 'f'))
                      {
                        code = (code << 4)
                               + (*lp >= 'A' && *lp <= 'F' ? *lp - 'A' + 10 :
                                  *lp >= 'a' && *lp <= 'f' ? *lp - 'a' + 10 :
                                  *lp - '0');
                        lp++;
                        count++;
                      }
                    if (count == 0)
                      fprintf (stderr, "hexadecimal escape without any hex digits: %s\n", line);
                    if (code > UCHAR_MAX)
                      fprintf (stderr, "hexadecimal escape out of range: %s\n", line);
                    *kp = (char) code;
                    break;
                  }
                case '\\': case '\'': case '"':
                  *kp = c;
                  lp++;
                  break;
                case 'n':
                  *kp = '\n';
                  lp++;
                  break;
                case 't':
                  *kp = '\t';
                  lp++;
                  break;
                case 'r':
                  *kp = '\r';
                  lp++;
                  break;
                case 'f':
                  *kp = '\f';
                  lp++;
                  break;
                case 'b':
                  *kp = '\b';
                  lp++;
                  break;
                case 'a':
                  *kp = '\a';
                  lp++;
                  break;
                case 'v':
                  *kp = '\v';
                  lp++;
                  break;
                default:
                  fprintf (stderr, "invalid escape sequence in string: %s\n", line);
                  exit (1);
                }
            }
          else if (c == '"')
            break;
          else
            {
              *kp = c;
              lp++;
            }
          kp++;
        }
      lp++;
      if (*lp != '\0')
        {
          if (strchr (delimiters, *lp) == NULL)
            {
              fprintf (stderr, "string not followed by delimiter: %s\n", line);
              exit (1);
            }
          lp++;
        }
      return new List_Node (key, kp - key, option[TYPE] ? lp : "");
    }
  else
    {
      /* Not a string. Look for the delimiter. */
      int len = strcspn (line, delimiters);
      const char *rest;

      if (line[len] == '\0')
        rest = "";
      else
        /* Skip the first delimiter. */
        rest = &line[len + 1];
      return new List_Node (line, len, option[TYPE] ? rest : "");
    }
}

/* Reads in all keys from standard input and creates a linked list pointed
   to by Head.  This list is then quickly checked for ``links,'' i.e.,
   unhashable elements possessing identical key sets and lengths. */

void
Key_List::read_keys (void)
{
  T (Trace t ("Key_List::read_keys");)
  char *ptr;

  include_src = save_include_src ();
  set_output_types ();

  /* Oops, problem with the input file. */
  if (! (ptr = Read_Line::get_line ()))
    {
      fprintf (stderr, "No words in input file, did you forget to prepend %s or use -t accidentally?\n", "%%");
      exit (1);
    }

  /* Read in all the keywords from the input file. */
  else
    {
      const char *delimiter = option.get_delimiter ();
      List_Node  *temp, *trail = 0;

      head = parse_line (ptr, delimiter);

      for (temp = head;
           (ptr = Read_Line::get_line ()) && strcmp (ptr, "%%");
           temp = temp->next)
        {
          temp->next = parse_line (ptr, delimiter);
          total_keys++;
        }

      /* See if any additional C code is included at end of this file. */
      if (ptr)
        additional_code = 1;

      /* Hash table this number of times larger than keyword number. */
      int table_size = (list_len = total_keys) * TABLE_MULTIPLE;

#if LARGE_STACK_ARRAYS
      /* By allocating the memory here we save on dynamic allocation overhead.
         Table must be a power of 2 for the hash function scheme to work. */
      List_Node *table[POW (table_size)];
#else
      // Note: we don't use new, because that invokes a custom operator new.
      int malloc_size = POW (table_size) * sizeof(List_Node*);
      if (malloc_size == 0) malloc_size = 1;
      List_Node **table = (List_Node**)malloc(malloc_size);
      if (table == NULL)
        abort ();
#endif

      /* Make large hash table for efficiency. */
      Hash_Table found_link (table, table_size, option[NOLENGTH]);

      /* Test whether there are any links and also set the maximum length of
        an identifier in the keyword list. */

      for (temp = head; temp; temp = temp->next)
        {
          List_Node *ptr = found_link.insert (temp);

          /* Check for links.  We deal with these by building an equivalence class
             of all duplicate values (i.e., links) so that only 1 keyword is
             representative of the entire collection.  This *greatly* simplifies
             processing during later stages of the program. */

          if (ptr)
            {
              total_duplicates++;
              list_len--;
              trail->next = temp->next;
              temp->link  = ptr->link;
              ptr->link   = temp;

              /* Complain if user hasn't enabled the duplicate option. */
              if (!option[DUP] || option[DEBUG])
                fprintf (stderr, "Key link: \"%.*s\" = \"%.*s\", with key set \"%.*s\".\n",
                                 temp->key_length, temp->key,
                                 ptr->key_length, ptr->key,
                                 temp->char_set_length, temp->char_set);
            }
          else
            trail = temp;

          /* Update minimum and maximum keyword length, if needed. */
          if (max_key_len < temp->key_length)
            max_key_len = temp->key_length;
          if (min_key_len > temp->key_length)
            min_key_len = temp->key_length;
        }

#if !LARGE_STACK_ARRAYS
      free ((char *) table);
#endif

      /* Exit program if links exists and option[DUP] not set, since we can't continue */
      if (total_duplicates)
        {
          if (option[DUP])
            fprintf (stderr, "%d input keys have identical hash values, examine output carefully...\n",
                             total_duplicates);
          else
            {
              fprintf (stderr, "%d input keys have identical hash values,\ntry different key positions or use option -D.\n",
                               total_duplicates);
              exit (1);
            }
        }
      /* Exit program if an empty string is used as key, since the comparison
         expressions don't work correctly for looking up an empty string. */
      if (min_key_len == 0)
        {
          fprintf (stderr, "Empty input key is not allowed.\nTo recognize an empty input key, your code should check for\nlen == 0 before calling the gperf generated lookup function.\n");
          exit (1);
        }
      if (option[ALLCHARS])
        option.set_keysig_size (max_key_len);
    }
}

/* Recursively merges two sorted lists together to form one sorted list. The
   ordering criteria is by frequency of occurrence of elements in the key set
   or by the hash value.  This is a kludge, but permits nice sharing of
   almost identical code without incurring the overhead of a function
   call comparison. */

List_Node *
Key_List::merge (List_Node *list1, List_Node *list2)
{
  T (Trace t ("Key_List::merge");)
  List_Node *result;
  List_Node **resultp = &result;
  for (;;)
    {
      if (!list1)
        {
          *resultp = list2;
          break;
        }
      if (!list2)
        {
          *resultp = list1;
          break;
        }
      if ((occurrence_sort && list1->occurrence < list2->occurrence)
	    || (hash_sort && list1->hash_value > list2->hash_value))
        {
          *resultp = list2;
          resultp = &list2->next; list2 = list1; list1 = *resultp;
        }
      else
        {
          *resultp = list1;
          resultp = &list1->next; list1 = *resultp;
        }
    }
  return result;
}

/* Applies the merge sort algorithm to recursively sort the key list by
   frequency of occurrence of elements in the key set. */

List_Node *
Key_List::merge_sort (List_Node *head)
{
  T (Trace t ("Key_List::merge_sort");)
  if (!head || !head->next)
    return head;
  else
    {
      List_Node *middle = head;
      List_Node *temp   = head->next->next;

      while (temp)
        {
          temp   = temp->next;
          middle = middle->next;
          if (temp)
            temp = temp->next;
        }

      temp         = middle->next;
      middle->next = 0;
      return merge (merge_sort (head), merge_sort (temp));
    }
}

/* Returns the frequency of occurrence of elements in the key set. */

inline int
Key_List::get_occurrence (List_Node *ptr)
{
  T (Trace t ("Key_List::get_occurrence");)
  int value = 0;

  const char *p = ptr->char_set;
  unsigned int i = ptr->char_set_length;
  for (; i > 0; p++, i--)
    value += occurrences[(unsigned char)(*p)];

  return value;
}

/* Enables the index location of all key set elements that are now
   determined. */

inline void
Key_List::set_determined (List_Node *ptr)
{
  T (Trace t ("Key_List::set_determined");)

  const char *p = ptr->char_set;
  unsigned int i = ptr->char_set_length;
  for (; i > 0; p++, i--)
    determined[(unsigned char)(*p)] = 1;
}

/* Returns TRUE if PTR's key set is already completely determined. */

inline int
Key_List::already_determined (List_Node *ptr)
{
  T (Trace t ("Key_List::already_determined");)
  int is_determined = 1;

  const char *p = ptr->char_set;
  unsigned int i = ptr->char_set_length;
  for (; is_determined && i > 0; p++, i--)
    is_determined = determined[(unsigned char)(*p)];

  return is_determined;
}

/* Reorders the table by first sorting the list so that frequently occuring
   keys appear first, and then the list is reorded so that keys whose values
   are already determined will be placed towards the front of the list.  This
   helps prune the search time by handling inevitable collisions early in the
   search process.  See Cichelli's paper from Jan 1980 JACM for details.... */

void
Key_List::reorder (void)
{
  T (Trace t ("Key_List::reorder");)
  List_Node *ptr;
  for (ptr = head; ptr; ptr = ptr->next)
    ptr->occurrence = get_occurrence (ptr);

  hash_sort = 0;
  occurrence_sort = 1;

  for (ptr = head = merge_sort (head); ptr->next; ptr = ptr->next)
    {
      set_determined (ptr);

      if (already_determined (ptr->next))
        continue;
      else
        {
          List_Node *trail_ptr = ptr->next;
          List_Node *run_ptr   = trail_ptr->next;

          for (; run_ptr; run_ptr = trail_ptr->next)
            {

              if (already_determined (run_ptr))
                {
                  trail_ptr->next = run_ptr->next;
                  run_ptr->next   = ptr->next;
                  ptr = ptr->next = run_ptr;
                }
              else
                trail_ptr = run_ptr;
            }
        }
    }
}

/* ============================ Output routines ============================ */

/* The "const " qualifier. */
static const char *const_always;

/* The "const " qualifier, for read-only arrays. */
static const char *const_readonly_array;

/* The "const " qualifier, for the array type. */
static const char *const_for_struct;

/* Returns the smallest unsigned C type capable of holding integers up to N. */

static const char *
smallest_integral_type (int n)
{
  if (n <= UCHAR_MAX) return "unsigned char";
  if (n <= USHRT_MAX) return "unsigned short";
  return "unsigned int";
}

/* Returns the smallest signed C type capable of holding integers
   from MIN to MAX. */

static const char *
smallest_integral_type (int min, int max)
{
  if (option[ANSIC] | option[CPLUSPLUS])
    if (min >= SCHAR_MIN && max <= SCHAR_MAX) return "signed char";
  if (min >= SHRT_MIN && max <= SHRT_MAX) return "short";
  return "int";
}

/* A cast from `char' to a valid array index. */
static const char *char_to_index;

/* ------------------------------------------------------------------------- */

/* Computes the maximum and minimum hash values.  Since the
   list is already sorted by hash value all we need to do is
   find the final item! */

void
Key_List::compute_min_max (void)
{
  T (Trace t ("Key_List::compute_min_max");)
  List_Node *temp;
  for (temp = head; temp->next; temp = temp->next)
    ;

  min_hash_value = head->hash_value;
  max_hash_value = temp->hash_value;
}

/* ------------------------------------------------------------------------- */

/* Returns the number of different hash values. */

int
Key_List::num_hash_values (void)
{
  T (Trace t ("Key_List::num_hash_values");)
  int count = 1;
  List_Node *temp;
  int value;

  for (temp = head, value = temp->hash_value; temp->next; )
    {
      temp = temp->next;
      if (value != temp->hash_value)
        {
          value = temp->hash_value;
          count++;
        }
    }
  return count;
}

/* -------------------- Output_Constants and subclasses -------------------- */

/* This class outputs an enumeration defining some constants. */

struct Output_Constants
{
  virtual void output_start () = 0;
  virtual void output_item (const char *name, int value) = 0;
  virtual void output_end () = 0;
  Output_Constants () {}
  virtual ~Output_Constants () {}
};

/* This class outputs an enumeration in #define syntax. */

struct Output_Defines : public Output_Constants
{
  virtual void output_start ();
  virtual void output_item (const char *name, int value);
  virtual void output_end ();
  Output_Defines () {}
  virtual ~Output_Defines () {}
};

void Output_Defines::output_start ()
{
  T (Trace t ("Output_Defines::output_start");)
  printf ("\n");
}

void Output_Defines::output_item (const char *name, int value)
{
  T (Trace t ("Output_Defines::output_item");)
  printf ("#define %s %d\n", name, value);
}

void Output_Defines::output_end ()
{
  T (Trace t ("Output_Defines::output_end");)
}

/* This class outputs an enumeration using `enum'. */

struct Output_Enum : public Output_Constants
{
  virtual void output_start ();
  virtual void output_item (const char *name, int value);
  virtual void output_end ();
  Output_Enum (const char *indent) : indentation (indent) {}
  virtual ~Output_Enum () {}
private:
  const char *indentation;
  int pending_comma;
};

void Output_Enum::output_start ()
{
  T (Trace t ("Output_Enum::output_start");)
  printf ("%senum\n"
          "%s  {\n",
          indentation, indentation);
  pending_comma = 0;
}

void Output_Enum::output_item (const char *name, int value)
{
  T (Trace t ("Output_Enum::output_item");)
  if (pending_comma)
    printf (",\n");
  printf ("%s    %s = %d", indentation, name, value);
  pending_comma = 1;
}

void Output_Enum::output_end ()
{
  T (Trace t ("Output_Enum::output_end");)
  if (pending_comma)
    printf ("\n");
  printf ("%s  };\n\n", indentation);
}

/* Outputs the maximum and minimum hash values etc. */

void
Key_List::output_constants (struct Output_Constants& style)
{
  T (Trace t ("Key_List::output_constants");)

  style.output_start ();
  style.output_item ("TOTAL_KEYWORDS", total_keys);
  style.output_item ("MIN_WORD_LENGTH", min_key_len);
  style.output_item ("MAX_WORD_LENGTH", max_key_len);
  style.output_item ("MIN_HASH_VALUE", min_hash_value);
  style.output_item ("MAX_HASH_VALUE", max_hash_value);
  style.output_end ();
}

/* ------------------------------------------------------------------------- */

/* Outputs a keyword, as a string: enclosed in double quotes, escaping
   backslashes, double quote and unprintable characters. */

static void
output_string (const char *key, int len)
{
  T (Trace t ("output_string");)

  putchar ('"');
  for (; len > 0; len--)
    {
      unsigned char c = (unsigned char) *key++;
      if (isprint (c))
        {
          if (c == '"' || c == '\\')
            putchar ('\\');
          putchar (c);
        }
      else
        {
          /* Use octal escapes, not hexadecimal escapes, because some old
             C compilers didn't understand hexadecimal escapes, and because
             hexadecimal escapes are not limited to 2 digits, thus needing
             special care if the following character happens to be a digit. */
          putchar ('\\');
          putchar ('0' + ((c >> 6) & 7));
          putchar ('0' + ((c >> 3) & 7));
          putchar ('0' + (c & 7));
        }
    }
  putchar ('"');
}

/* ------------------------------------------------------------------------- */

/* Outputs a type and a const specifier.
   The output is terminated with a space. */

static void
output_const_type (const char *const_string, const char *type_string)
{
  if (type_string[strlen(type_string)-1] == '*')
    printf ("%s %s", type_string, const_string);
  else
    printf ("%s%s ", const_string, type_string);
}

/* ----------------------- Output_Expr and subclasses ----------------------- */

/* This class outputs a general expression. */

struct Output_Expr
{
  virtual void output_expr () const = 0;
  Output_Expr () {}
  virtual ~Output_Expr () {}
};

/* This class outputs an expression formed by a single string. */

struct Output_Expr1 : public Output_Expr
{
  virtual void output_expr () const;
  Output_Expr1 (const char *piece1) : p1 (piece1) {}
  virtual ~Output_Expr1 () {}
private:
  const char *p1;
};

void Output_Expr1::output_expr () const
{
  T (Trace t ("Output_Expr1::output_expr");)
  printf ("%s", p1);
}

#if 0 /* unused */

/* This class outputs an expression formed by the concatenation of two
   strings. */

struct Output_Expr2 : public Output_Expr
{
  virtual void output_expr () const;
  Output_Expr2 (const char *piece1, const char *piece2)
    : p1 (piece1), p2 (piece2) {}
  virtual ~Output_Expr2 () {}
private:
  const char *p1;
  const char *p2;
};

void Output_Expr2::output_expr () const
{
  T (Trace t ("Output_Expr2::output_expr");)
  printf ("%s%s", p1, p2);
}

#endif

/* --------------------- Output_Compare and subclasses --------------------- */

/* This class outputs a comparison expression. */

struct Output_Compare
{
  virtual void output_comparison (const Output_Expr& expr1,
                                  const Output_Expr& expr2) const = 0;
  Output_Compare () {}
  virtual ~Output_Compare () {}
};

/* This class outputs a comparison using strcmp. */

struct Output_Compare_Strcmp : public Output_Compare
{
  virtual void output_comparison (const Output_Expr& expr1,
                                  const Output_Expr& expr2) const;
  Output_Compare_Strcmp () {}
  virtual ~Output_Compare_Strcmp () {}
};

void Output_Compare_Strcmp::output_comparison (const Output_Expr& expr1,
                                               const Output_Expr& expr2) const
{
  T (Trace t ("Output_Compare_Strcmp::output_comparison");)
  printf ("*");
  expr1.output_expr ();
  printf (" == *");
  expr2.output_expr ();
  printf (" && !strcmp (");
  expr1.output_expr ();
  printf (" + 1, ");
  expr2.output_expr ();
  printf (" + 1)");
}

/* This class outputs a comparison using strncmp.
   Note that the length of expr1 will be available through the local variable
   `len'. */

struct Output_Compare_Strncmp : public Output_Compare
{
  virtual void output_comparison (const Output_Expr& expr1,
                                  const Output_Expr& expr2) const;
  Output_Compare_Strncmp () {}
  virtual ~Output_Compare_Strncmp () {}
};

void Output_Compare_Strncmp::output_comparison (const Output_Expr& expr1,
                                                const Output_Expr& expr2) const
{
  T (Trace t ("Output_Compare_Strncmp::output_comparison");)
  printf ("*");
  expr1.output_expr ();
  printf (" == *");
  expr2.output_expr ();
  printf (" && !strncmp (");
  expr1.output_expr ();
  printf (" + 1, ");
  expr2.output_expr ();
  printf (" + 1, len - 1) && ");
  expr2.output_expr ();
  printf ("[len] == '\\0'");
}

/* This class outputs a comparison using memcmp.
   Note that the length of expr1 (available through the local variable `len')
   must be verified to be equal to the length of expr2 prior to this
   comparison. */

struct Output_Compare_Memcmp : public Output_Compare
{
  virtual void output_comparison (const Output_Expr& expr1,
                                  const Output_Expr& expr2) const;
  Output_Compare_Memcmp () {}
  virtual ~Output_Compare_Memcmp () {}
};

void Output_Compare_Memcmp::output_comparison (const Output_Expr& expr1,
                                               const Output_Expr& expr2) const
{
  T (Trace t ("Output_Compare_Memcmp::output_comparison");)
  printf ("*");
  expr1.output_expr ();
  printf (" == *");
  expr2.output_expr ();
  printf (" && !memcmp (");
  expr1.output_expr ();
  printf (" + 1, ");
  expr2.output_expr ();
  printf (" + 1, len - 1)");
}

/* ------------------------------------------------------------------------- */

/* Generates C code for the hash function that returns the
   proper encoding for each key word. */

void
Key_List::output_hash_function (void)
{
  T (Trace t ("Key_List::output_hash_function");)
  const int max_column  = 10;
  int field_width;

  /* Calculate maximum number of digits required for MAX_HASH_VALUE. */
  field_width = 2;
  for (int trunc = max_hash_value; (trunc /= 10) > 0;)
    field_width++;

  /* Output the function's head. */
  if (option[CPLUSPLUS])
    printf ("inline ");
  else if (option[KRC] | option[C] | option[ANSIC])
    printf ("#ifdef __GNUC__\n"
            "__inline\n"
            "#else\n"
            "#ifdef __cplusplus\n"
            "inline\n"
            "#endif\n"
            "#endif\n");

  if (option[KRC] | option[C] | option[ANSIC])
    printf ("static ");
  printf ("unsigned int\n");
  if (option[CPLUSPLUS])
    printf ("%s::", option.get_class_name ());
  printf ("%s ", option.get_hash_name ());
  if (option[KRC] || option[C] || option [ANSIC] || option[CPLUSPLUS])
    printf (option[KRC] ?
	      "(str, len)\n"
              "     register char *str;\n"
              "     register unsigned int len;\n" :
	    option[C] ?
	      "(str, len)\n"
              "     register const char *str;\n"
              "     register unsigned int len;\n" :
	      "(register const char *str, register unsigned int len)\n");

  /* Note that when the hash function is called, it has already been verified
     that  min_key_len <= len <= max_key_len. */

  /* Output the function's body. */
  printf ("{\n");

  /* First the asso_values array. */
  printf ("  static %s%s asso_values[] =\n"
          "    {",
          const_readonly_array,
          smallest_integral_type (max_hash_value + 1));

  for (int count = 0; count < ALPHA_SIZE; count++)
    {
      if (count > 0)
        printf (",");
      if (!(count % max_column))
        printf ("\n     ");
      printf ("%*d", field_width,
              occurrences[count] ? asso_values[count] : max_hash_value + 1);
    }

  printf ("\n"
          "    };\n");

  /* Optimize special case of ``-k 1,$'' */
  if (option[DEFAULTCHARS])
    printf ("  return %sasso_values[%sstr[len - 1]] + asso_values[%sstr[0]];\n",
            option[NOLENGTH] ? "" : "len + ",
            char_to_index, char_to_index);
  else
    {
      int key_pos;

      option.reset ();

      /* Get first (also highest) key position. */
      key_pos = option.get ();

      if (!option[ALLCHARS] && (key_pos == WORD_END || key_pos <= min_key_len))
        {
          /* We can perform additional optimizations here:
             Write it out as a single expression. Note that the values
             are added as `int's even though the asso_values array may
             contain `unsigned char's or `unsigned short's. */

          printf ("  return %s",
                  option[NOLENGTH] ? "" : "len + ");

          for (; key_pos != WORD_END; )
            {
              printf ("asso_values[%sstr[%d]]", char_to_index, key_pos - 1);
              if ((key_pos = option.get ()) != EOS)
                printf (" + ");
              else
                break;
            }

          if (key_pos == WORD_END)
            printf ("asso_values[%sstr[len - 1]]", char_to_index);

          printf (";\n");
        }
      else
        {
          /* We've got to use the correct, but brute force, technique. */
          printf ("  register int hval = %s;\n\n"
                  "  switch (%s)\n"
                  "    {\n"
                  "      default:\n",
                  option[NOLENGTH] ? "0" : "len",
                  option[NOLENGTH] ? "len" : "hval");

          /* User wants *all* characters considered in hash. */
          if (option[ALLCHARS])
            {
              for (int i = max_key_len; i > 0; i--)
                printf ("      case %d:\n"
                        "        hval += asso_values[%sstr[%d]];\n",
                        i, char_to_index, i - 1);

              printf ("        break;\n"
                      "    }\n"
                      "  return hval;\n");
            }
          else                  /* do the hard part... */
            {
              while (key_pos != WORD_END && key_pos > max_key_len)
                if ((key_pos = option.get ()) == EOS)
                  break;

              if (key_pos != EOS && key_pos != WORD_END)
                {
                  int i = key_pos;
                  do
                    {
                      for ( ; i >= key_pos; i--)
                        printf ("      case %d:\n", i);

                      printf ("        hval += asso_values[%sstr[%d]];\n",
                              char_to_index, key_pos - 1);

                      key_pos = option.get ();
                    }
                  while (key_pos != EOS && key_pos != WORD_END);

                  for ( ; i >= min_key_len; i--)
                    printf ("      case %d:\n", i);
                }

              printf ("        break;\n"
                      "    }\n"
                      "  return hval");
              if (key_pos == WORD_END)
                printf (" + asso_values[%sstr[len - 1]]", char_to_index);
              printf (";\n");
            }
        }
    }
  printf ("}\n\n");
}

/* ------------------------------------------------------------------------- */

/* Prints out a table of keyword lengths, for use with the
   comparison code in generated function ``in_word_set''. */

void
Key_List::output_keylength_table (void)
{
  T (Trace t ("Key_List::output_keylength_table");)
  const int  columns = 14;
  int        index;
  int        column;
  const char *indent    = option[GLOBAL] ? "" : "  ";
  List_Node *temp;

  printf ("%sstatic %s%s lengthtable[] =\n%s  {",
          indent, const_readonly_array,
          smallest_integral_type (max_key_len),
          indent);

  /* Generate an array of lengths, similar to output_keyword_table. */

  column = 0;
  for (temp = head, index = 0; temp; temp = temp->next)
    {
      if (option[SWITCH] && !option[TYPE]
          && !(temp->link
               || (temp->next && temp->hash_value == temp->next->hash_value)))
        continue;

      if (index < temp->hash_value && !option[SWITCH] && !option[DUP])
        {
          /* Some blank entries. */
          for ( ; index < temp->hash_value; index++)
            {
              if (index > 0)
                printf (",");
              if ((column++ % columns) == 0)
                printf ("\n%s   ", indent);
              printf ("%3d", 0);
            }
        }

      if (index > 0)
        printf (",");
      if ((column++ % columns) == 0)
        printf("\n%s   ", indent);
      printf ("%3d", temp->key_length);

      /* Deal with links specially. */
      if (temp->link) // implies option[DUP]
        for (List_Node *links = temp->link; links; links = links->link)
          {
            ++index;
            printf (",");
            if ((column++ % columns) == 0)
              printf("\n%s   ", indent);
            printf ("%3d", links->key_length);
          }

      index++;
    }

  printf ("\n%s  };\n", indent);
  if (option[GLOBAL])
    printf ("\n");
}

/* ------------------------------------------------------------------------- */

static void
output_keyword_entry (List_Node *temp, const char *indent)
{
  printf ("%s    ", indent);
  if (option[TYPE])
    printf ("{");
  output_string (temp->key, temp->key_length);
  if (option[TYPE])
    {
      if (strlen (temp->rest) > 0)
        printf (",%s", temp->rest);
      printf ("}");
    }
  if (option[DEBUG])
    printf (" /* hash value = %d, index = %d */",
            temp->hash_value, temp->index);
}

static void
output_keyword_blank_entries (int count, const char *indent)
{
  int columns;
  if (option[TYPE])
    {
      columns = 58 / (6 + strlen (option.get_initializer_suffix()));
      if (columns == 0)
        columns = 1;
    }
  else
    {
      columns = 9;
    }
  int column = 0;
  for (int i = 0; i < count; i++)
    {
      if ((column % columns) == 0)
        {
          if (i > 0)
            printf (",\n");
          printf ("%s    ", indent);
        }
      else
        {
          if (i > 0)
            printf (", ");
        }
      if (option[TYPE])
        printf ("{\"\"%s}", option.get_initializer_suffix());
      else
        printf ("\"\"");
      column++;
    }
}

/* Prints out the array containing the key words for the hash function. */

void
Key_List::output_keyword_table (void)
{
  T (Trace t ("Key_List::output_keyword_table");)
  const char *indent  = option[GLOBAL] ? "" : "  ";
  int         index;
  List_Node  *temp;

  printf ("%sstatic ",
          indent);
  output_const_type (const_readonly_array, struct_tag);
  printf ("%s[] =\n"
          "%s  {\n",
          option.get_wordlist_name (),
          indent);

  /* Generate an array of reserved words at appropriate locations. */

  for (temp = head, index = 0; temp; temp = temp->next)
    {
      if (option[SWITCH] && !option[TYPE]
          && !(temp->link
               || (temp->next && temp->hash_value == temp->next->hash_value)))
        continue;

      if (index > 0)
        printf (",\n");

      if (index < temp->hash_value && !option[SWITCH] && !option[DUP])
        {
          /* Some blank entries. */
          output_keyword_blank_entries (temp->hash_value - index, indent);
          printf (",\n");
          index = temp->hash_value;
        }

      temp->index = index;

      output_keyword_entry (temp, indent);

      /* Deal with links specially. */
      if (temp->link) // implies option[DUP]
        for (List_Node *links = temp->link; links; links = links->link)
          {
            links->index = ++index;
            printf (",\n");
            output_keyword_entry (links, indent);
          }

      index++;
    }
  if (index > 0)
    printf ("\n");

  printf ("%s  };\n\n", indent);
}

/* ------------------------------------------------------------------------- */

/* Generates the large, sparse table that maps hash values into
   the smaller, contiguous range of the keyword table. */

void
Key_List::output_lookup_array (void)
{
  T (Trace t ("Key_List::output_lookup_array");)
  if (option[DUP])
    {
      const int DEFAULT_VALUE = -1;

      /* Because of the way output_keyword_table works, every duplicate set is
         stored contiguously in the wordlist array. */
      struct duplicate_entry
        {
          int hash_value;  /* Hash value for this particular duplicate set. */
          int index;       /* Index into the main keyword storage array. */
          int count;       /* Number of consecutive duplicates at this index. */
        };

#if LARGE_STACK_ARRAYS
      duplicate_entry duplicates[total_duplicates];
      int lookup_array[max_hash_value + 1 + 2*total_duplicates];
#else
      // Note: we don't use new, because that invokes a custom operator new.
      duplicate_entry *duplicates = (duplicate_entry *)
        malloc (total_duplicates * sizeof(duplicate_entry) + 1);
      int *lookup_array = (int *)
        malloc ((max_hash_value + 1 + 2*total_duplicates) * sizeof(int));
      if (duplicates == NULL || lookup_array == NULL)
        abort();
#endif
      int lookup_array_size = max_hash_value + 1;
      duplicate_entry *dup_ptr = &duplicates[0];
      int *lookup_ptr = &lookup_array[max_hash_value + 1 + 2*total_duplicates];

      while (lookup_ptr > lookup_array)
        *--lookup_ptr = DEFAULT_VALUE;

      /* Now dup_ptr = &duplicates[0] and lookup_ptr = &lookup_array[0]. */

      for (List_Node *temp = head; temp; temp = temp->next)
        {
          int hash_value = temp->hash_value;
          lookup_array[hash_value] = temp->index;
          if (option[DEBUG])
            fprintf (stderr, "keyword = %.*s, index = %d\n",
                     temp->key_length, temp->key, temp->index);
          if (temp->link
              || (temp->next && hash_value == temp->next->hash_value))
            {
              /* Start a duplicate entry. */
              dup_ptr->hash_value = hash_value;
              dup_ptr->index      = temp->index;
              dup_ptr->count      = 1;

              for (;;)
                {
                  for (List_Node *ptr = temp->link; ptr; ptr = ptr->link)
                    {
                      dup_ptr->count++;
                      if (option[DEBUG])
                        fprintf (stderr,
                                 "static linked keyword = %.*s, index = %d\n",
                                 ptr->key_length, ptr->key, ptr->index);
                    }

                  if (!(temp->next && hash_value == temp->next->hash_value))
                    break;

                  temp = temp->next;

                  dup_ptr->count++;
                  if (option[DEBUG])
                    fprintf (stderr, "dynamic linked keyword = %.*s, index = %d\n",
                             temp->key_length, temp->key, temp->index);
                }
              assert (dup_ptr->count >= 2);
              dup_ptr++;
            }
        }

      while (dup_ptr > duplicates)
        {
          dup_ptr--;

          if (option[DEBUG])
            fprintf (stderr,
                     "dup_ptr[%zd]: hash_value = %d, index = %d, count = %d\n",
                     dup_ptr - duplicates,
                     dup_ptr->hash_value, dup_ptr->index, dup_ptr->count);

          int i;
          /* Start searching for available space towards the right part
             of the lookup array. */
          for (i = dup_ptr->hash_value; i < lookup_array_size-1; i++)
            if (lookup_array[i] == DEFAULT_VALUE
                && lookup_array[i + 1] == DEFAULT_VALUE)
              goto found_i;
          /* If we didn't find it to the right look to the left instead... */
          for (i = dup_ptr->hash_value-1; i >= 0; i--)
            if (lookup_array[i] == DEFAULT_VALUE
                && lookup_array[i + 1] == DEFAULT_VALUE)
              goto found_i;
          /* Append to the end of lookup_array. */
          i = lookup_array_size;
          lookup_array_size += 2;
        found_i:
          /* Put in an indirection from dup_ptr->hash_value to i.
             At i and i+1 store dup_ptr->index and dup_ptr->count. */
          assert (lookup_array[dup_ptr->hash_value] == dup_ptr->index);
          lookup_array[dup_ptr->hash_value] = - 1 - total_keys - i;
          lookup_array[i] = - total_keys + dup_ptr->index;
          lookup_array[i + 1] = - dup_ptr->count;
          /* All these three values are <= -2, distinct from DEFAULT_VALUE. */
        }

      /* The values of the lookup array are now known. */

      int min = INT_MAX;
      int max = INT_MIN;
      lookup_ptr = lookup_array + lookup_array_size;
      while (lookup_ptr > lookup_array)
        {
          int val = *--lookup_ptr;
          if (min > val)
            min = val;
          if (max < val)
            max = val;
        }

      const char *indent = option[GLOBAL] ? "" : "  ";
      printf ("%sstatic %s%s lookup[] =\n"
              "%s  {",
              indent, const_readonly_array, smallest_integral_type (min, max),
              indent);

      int field_width;
      /* Calculate maximum number of digits required for MIN..MAX. */
      {
        field_width = 2;
        for (int trunc = max; (trunc /= 10) > 0;)
          field_width++;
      }
      if (min < 0)
        {
          int neg_field_width = 2;
          for (int trunc = -min; (trunc /= 10) > 0;)
            neg_field_width++;
          neg_field_width++; /* account for the minus sign */
          if (field_width < neg_field_width)
            field_width = neg_field_width;
        }

      const int columns = 42 / field_width;
      int column;

      column = 0;
      for (int i = 0; i < lookup_array_size; i++)
        {
          if (i > 0)
            printf (",");
          if ((column++ % columns) == 0)
            printf("\n%s   ", indent);
          printf ("%*d", field_width, lookup_array[i]);
        }
      printf ("\n%s  };\n\n", indent);

#if !LARGE_STACK_ARRAYS
      free ((char *) duplicates);
      free ((char *) lookup_array);
#endif
    }
}

/* ------------------------------------------------------------------------- */

/* Generate all the tables needed for the lookup function. */

void
Key_List::output_lookup_tables (void)
{
  T (Trace t ("Key_List::output_lookup_tables");)

  if (option[SWITCH])
    {
      /* Use the switch in place of lookup table. */
      if (option[LENTABLE] && (option[DUP] && total_duplicates > 0))
        output_keylength_table ();
      if (option[TYPE] || (option[DUP] && total_duplicates > 0))
        output_keyword_table ();
    }
  else
    {
      /* Use the lookup table, in place of switch. */
      if (option[LENTABLE])
        output_keylength_table ();
      output_keyword_table ();
      output_lookup_array ();
    }
}

/* ------------------------------------------------------------------------- */

/* Output a single switch case (including duplicates). Advance list. */

static List_Node *
output_switch_case (List_Node *list, int indent, int *jumps_away)
{
  T (Trace t ("output_switch_case");)

  if (option[DEBUG])
    printf ("%*s/* hash value = %4d, keyword = \"%.*s\" */\n",
            indent, "", list->hash_value, list->key_length, list->key);

  if (option[DUP]
      && (list->link
          || (list->next && list->hash_value == list->next->hash_value)))
    {
      if (option[LENTABLE])
        printf ("%*slengthptr = &lengthtable[%d];\n",
                indent, "", list->index);
      printf ("%*swordptr = &%s[%d];\n",
              indent, "", option.get_wordlist_name (), list->index);

      int count = 0;
      for (List_Node *temp = list; ; temp = temp->next)
        {
          for (List_Node *links = temp; links; links = links->link)
            count++;
          if (!(temp->next && temp->hash_value == temp->next->hash_value))
            break;
        }

      printf ("%*swordendptr = wordptr + %d;\n"
              "%*sgoto multicompare;\n",
              indent, "", count,
              indent, "");
      *jumps_away = 1;
    }
  else
    {
      if (option[LENTABLE])
        {
          printf ("%*sif (len == %d)\n"
                  "%*s  {\n",
                  indent, "", list->key_length,
                  indent, "");
          indent += 4;
        }
      printf ("%*sresword = ",
              indent, "");
      if (option[TYPE])
        printf ("&%s[%d]", option.get_wordlist_name (), list->index);
      else
        output_string (list->key, list->key_length);
      printf (";\n");
      printf ("%*sgoto compare;\n",
              indent, "");
      if (option[LENTABLE])
        {
          indent -= 4;
          printf ("%*s  }\n",
                  indent, "");
        }
      else
        *jumps_away = 1;
    }

  while (list->next && list->hash_value == list->next->hash_value)
    list = list->next;
  list = list->next;
  return list;
}

/* Output a total of size cases, grouped into num_switches switch statements,
   where 0 < num_switches <= size. */

static void
output_switches (List_Node *list, int num_switches, int size, int min_hash_value, int max_hash_value, int indent)
{
  T (Trace t ("output_switches");)

  if (option[DEBUG])
    printf ("%*s/* know %d <= key <= %d, contains %d cases */\n",
            indent, "", min_hash_value, max_hash_value, size);

  if (num_switches > 1)
    {
      int part1 = num_switches / 2;
      int part2 = num_switches - part1;
      int size1 = (int)((double)size / (double)num_switches * (double)part1 + 0.5);
      int size2 = size - size1;

      List_Node *temp = list;
      for (int count = size1; count > 0; count--)
        {
          while (temp->hash_value == temp->next->hash_value)
            temp = temp->next;
          temp = temp->next;
        }

      printf ("%*sif (key < %d)\n"
              "%*s  {\n",
              indent, "", temp->hash_value,
              indent, "");

      output_switches (list, part1, size1, min_hash_value, temp->hash_value-1, indent+4);

      printf ("%*s  }\n"
              "%*selse\n"
              "%*s  {\n",
              indent, "", indent, "", indent, "");

      output_switches (temp, part2, size2, temp->hash_value, max_hash_value, indent+4);

      printf ("%*s  }\n",
              indent, "");
    }
  else
    {
      /* Output a single switch. */
      int lowest_case_value = list->hash_value;
      if (size == 1)
        {
          int jumps_away = 0;
          assert (min_hash_value <= lowest_case_value);
          assert (lowest_case_value <= max_hash_value);
          if (min_hash_value == max_hash_value)
            output_switch_case (list, indent, &jumps_away);
          else
            {
              printf ("%*sif (key == %d)\n"
                      "%*s  {\n",
                      indent, "", lowest_case_value,
                      indent, "");
              output_switch_case (list, indent+4, &jumps_away);
              printf ("%*s  }\n",
                      indent, "");
            }
        }
      else
        {
          if (lowest_case_value == 0)
            printf ("%*sswitch (key)\n", indent, "");
          else
            printf ("%*sswitch (key - %d)\n", indent, "", lowest_case_value);
          printf ("%*s  {\n",
                  indent, "");
          for (; size > 0; size--)
            {
              int jumps_away = 0;
              printf ("%*s    case %d:\n",
                      indent, "", list->hash_value - lowest_case_value);
              list = output_switch_case (list, indent+6, &jumps_away);
              if (!jumps_away)
                printf ("%*s      break;\n",
                        indent, "");
            }
          printf ("%*s  }\n",
                  indent, "");
        }
    }
}

/* Generates C code to perform the keyword lookup. */

void
Key_List::output_lookup_function_body (const Output_Compare& comparison)
{
  T (Trace t ("Key_List::output_lookup_function_body");)

  printf ("  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)\n"
          "    {\n"
          "      register int key = %s (str, len);\n\n",
          option.get_hash_name ());

  if (option[SWITCH])
    {
      int switch_size = num_hash_values ();
      int num_switches = option.get_total_switches ();
      if (num_switches > switch_size)
        num_switches = switch_size;

      printf ("      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)\n"
              "        {\n");
      if (option[DUP])
        {
          if (option[LENTABLE])
            printf ("          register %s%s *lengthptr;\n",
                    const_always, smallest_integral_type (max_key_len));
          printf ("          register ");
          output_const_type (const_readonly_array, struct_tag);
          printf ("*wordptr;\n");
          printf ("          register ");
          output_const_type (const_readonly_array, struct_tag);
          printf ("*wordendptr;\n");
        }
      if (option[TYPE])
        {
          printf ("          register ");
          output_const_type (const_readonly_array, struct_tag);
          printf ("*resword;\n\n");
        }
      else
        printf ("          register %sresword;\n\n",
                struct_tag);

      output_switches (head, num_switches, switch_size, min_hash_value, max_hash_value, 10);

      if (option[DUP])
        {
          int indent = 8;
          printf ("%*s  return 0;\n"
                  "%*smulticompare:\n"
                  "%*s  while (wordptr < wordendptr)\n"
                  "%*s    {\n",
                  indent, "", indent, "", indent, "", indent, "");
          if (option[LENTABLE])
            {
              printf ("%*s      if (len == *lengthptr)\n"
                      "%*s        {\n",
                      indent, "", indent, "");
              indent += 4;
            }
          printf ("%*s      register %schar *s = ",
                  indent, "", const_always);
          if (option[TYPE])
            printf ("wordptr->%s", option.get_key_name ());
          else
            printf ("*wordptr");
          printf (";\n\n"
                  "%*s      if (",
                  indent, "");
          comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("s"));
          printf (")\n"
                  "%*s        return %s;\n",
                  indent, "",
                  option[TYPE] ? "wordptr" : "s");
          if (option[LENTABLE])
            {
              indent -= 4;
              printf ("%*s        }\n",
                      indent, "");
            }
          if (option[LENTABLE])
            printf ("%*s      lengthptr++;\n",
                    indent, "");
          printf ("%*s      wordptr++;\n"
                  "%*s    }\n",
                  indent, "", indent, "");
        }
      printf ("          return 0;\n"
              "        compare:\n");
      if (option[TYPE])
        {
          printf ("          {\n"
                  "            register %schar *s = resword->%s;\n\n"
                  "            if (",
                  const_always, option.get_key_name ());
          comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("s"));
          printf (")\n"
                  "              return resword;\n"
                  "          }\n");
        }
      else
        {
          printf ("          if (");
          comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("resword"));
          printf (")\n"
                  "            return resword;\n");
        }
      printf ("        }\n");
    }
  else
    {
      printf ("      if (key <= MAX_HASH_VALUE && key >= 0)\n");

      if (option[DUP])
        {
          int indent = 8;
          printf ("%*s{\n"
                  "%*s  register int index = lookup[key];\n\n"
                  "%*s  if (index >= 0)\n",
                  indent, "", indent, "", indent, "");
          if (option[LENTABLE])
            {
              printf ("%*s    {\n"
                      "%*s      if (len == lengthtable[index])\n",
                      indent, "", indent, "");
              indent += 4;
            }
          printf ("%*s    {\n"
                  "%*s      register %schar *s = %s[index]",
                  indent, "",
                  indent, "", const_always, option.get_wordlist_name ());
          if (option[TYPE])
            printf (".%s", option.get_key_name ());
          printf (";\n\n"
                  "%*s      if (",
                  indent, "");
          comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("s"));
          printf (")\n"
                  "%*s        return ",
                  indent, "");
          if (option[TYPE])
            printf ("&%s[index]", option.get_wordlist_name ());
          else
            printf ("s");
          printf (";\n"
                  "%*s    }\n",
                  indent, "");
          if (option[LENTABLE])
            {
              indent -= 4;
              printf ("%*s    }\n", indent, "");
            }
          if (total_duplicates > 0)
            {
              printf ("%*s  else if (index < -TOTAL_KEYWORDS)\n"
                      "%*s    {\n"
                      "%*s      register int offset = - 1 - TOTAL_KEYWORDS - index;\n",
                      indent, "", indent, "", indent, "");
              if (option[LENTABLE])
                printf ("%*s      register %s%s *lengthptr = &lengthtable[TOTAL_KEYWORDS + lookup[offset]];\n",
                        indent, "", const_always, smallest_integral_type (max_key_len));
              printf ("%*s      register ",
                      indent, "");
              output_const_type (const_readonly_array, struct_tag);
              printf ("*wordptr = &%s[TOTAL_KEYWORDS + lookup[offset]];\n",
                      option.get_wordlist_name ());
              printf ("%*s      register ",
                      indent, "");
              output_const_type (const_readonly_array, struct_tag);
              printf ("*wordendptr = wordptr + -lookup[offset + 1];\n\n");
              printf ("%*s      while (wordptr < wordendptr)\n"
                      "%*s        {\n",
                      indent, "", indent, "");
              if (option[LENTABLE])
                {
                  printf ("%*s          if (len == *lengthptr)\n"
                          "%*s            {\n",
                          indent, "", indent, "");
                  indent += 4;
                }
              printf ("%*s          register %schar *s = ",
                      indent, "", const_always);
              if (option[TYPE])
                printf ("wordptr->%s", option.get_key_name ());
              else
                printf ("*wordptr");
              printf (";\n\n"
                      "%*s          if (",
                      indent, "");
              comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("s"));
              printf (")\n"
                      "%*s            return %s;\n",
                      indent, "",
                      option[TYPE] ? "wordptr" : "s");
              if (option[LENTABLE])
                {
                  indent -= 4;
                  printf ("%*s            }\n",
                          indent, "");
                }
              if (option[LENTABLE])
                printf ("%*s          lengthptr++;\n",
                        indent, "");
              printf ("%*s          wordptr++;\n"
                      "%*s        }\n"
                      "%*s    }\n",
                      indent, "", indent, "", indent, "");
            }
          printf ("%*s}\n",
                  indent, "");
        }
      else
        {
          int indent = 8;
          if (option[LENTABLE])
            {
              printf ("%*sif (len == lengthtable[key])\n",
                      indent, "");
              indent += 2;
            }

          printf ("%*s{\n"
                  "%*s  register %schar *s = %s[key]",
                  indent, "",
                  indent, "", const_always, option.get_wordlist_name ());

          if (option[TYPE])
            printf (".%s", option.get_key_name ());

          printf (";\n\n"
                  "%*s  if (",
                  indent, "");
          comparison.output_comparison (Output_Expr1 ("str"), Output_Expr1 ("s"));
          printf (")\n"
                  "%*s    return ",
                  indent, "");
          if (option[TYPE])
            printf ("&%s[key]", option.get_wordlist_name ());
          else
            printf ("s");
          printf (";\n"
                  "%*s}\n",
                  indent, "");
        }
    }
  printf ("    }\n"
          "  return 0;\n");
}

/* Generates C code for the lookup function. */

void
Key_List::output_lookup_function (void)
{
  T (Trace t ("Key_List::output_lookup_function");)

  /* Output the function's head. */
  if (option[KRC] | option[C] | option[ANSIC])
    printf ("#ifdef __GNUC__\n"
            "__inline\n"
            "#endif\n");

  printf ("%s%s\n",
          const_for_struct, return_type);
  if (option[CPLUSPLUS])
    printf ("%s::", option.get_class_name ());
  printf ("%s ", option.get_function_name ());
  if (option[KRC] || option[C] || option[ANSIC] || option[CPLUSPLUS])
    printf (option[KRC] ?
	      "(str, len)\n"
              "     register char *str;\n"
              "     register unsigned int len;\n" :
	    option[C] ?
	      "(str, len)\n"
              "     register const char *str;\n"
              "     register unsigned int len;\n" :
	    "(register const char *str, register unsigned int len)\n");

  /* Output the function's body. */
  printf ("{\n");

  if (option[ENUM] && !option[GLOBAL])
    {
      Output_Enum style ("  ");
      output_constants (style);
    }

  if (!option[GLOBAL])
    output_lookup_tables ();

  if (option[LENTABLE])
    output_lookup_function_body (Output_Compare_Memcmp ());
  else
    {
      if (option[COMP])
        output_lookup_function_body (Output_Compare_Strncmp ());
      else
        output_lookup_function_body (Output_Compare_Strcmp ());
    }

  printf ("}\n");
}

/* ------------------------------------------------------------------------- */

/* Generates the hash function and the key word recognizer function
   based upon the user's Options. */

void
Key_List::output (void)
{
  T (Trace t ("Key_List::output");)

  compute_min_max ();

  if (option[C] | option[ANSIC] | option[CPLUSPLUS])
    {
      const_always = "const ";
      const_readonly_array = (option[CONST] ? "const " : "");
      const_for_struct = ((option[CONST] && option[TYPE]) ? "const " : "");
    }
  else
    {
      const_always = "";
      const_readonly_array = "";
      const_for_struct = "";
    }

  if (!option[TYPE])
    {
      return_type = (const_always[0] ? "const char *" : "char *");
      struct_tag = (const_always[0] ? "const char *" : "char *");
    }

  char_to_index = (option[SEVENBIT] ? "" : "(unsigned char)");

  printf ("/* ");
  if (option[KRC])
    printf ("KR-C");
  else if (option[C])
    printf ("C");
  else if (option[ANSIC])
    printf ("ANSI-C");
  else if (option[CPLUSPLUS])
    printf ("C++");
  printf (" code produced by gperf version %s */\n", version_string);
  Options::print_options ();

  printf ("%s\n", include_src);

  if (option[TYPE] && !option[NOTYPE]) /* Output type declaration now, reference it later on.... */
    printf ("%s;\n", array_type);

  if (option[INCLUDE])
    printf ("#include <string.h>\n"); /* Declare strlen(), strcmp(), strncmp(). */

  if (!option[ENUM])
    {
      Output_Defines style;
      output_constants (style);
    }
  else if (option[GLOBAL])
    {
      Output_Enum style ("");
      output_constants (style);
    }

  printf ("/* maximum key range = %d, duplicates = %d */\n\n",
          max_hash_value - min_hash_value + 1, total_duplicates);

  if (option[CPLUSPLUS])
    printf ("class %s\n"
            "{\n"
            "private:\n"
            "  static inline unsigned int %s (const char *str, unsigned int len);\n"
            "public:\n"
            "  static %s%s%s (const char *str, unsigned int len);\n"
            "};\n"
            "\n",
            option.get_class_name (), option.get_hash_name (),
            const_for_struct, return_type, option.get_function_name ());

  output_hash_function ();

  if (option[GLOBAL])
    output_lookup_tables ();

  output_lookup_function ();

  if (additional_code)
    for (int c; (c = getchar ()) != EOF; putchar (c))
      ;

  fflush (stdout);
}

/* ========================= End of Output routines ========================= */

/* Sorts the keys by hash value. */

void
Key_List::sort (void)
{
  T (Trace t ("Key_List::sort");)
  hash_sort       = 1;
  occurrence_sort = 0;

  head = merge_sort (head);
}

/* Dumps the key list to stderr stream. */

void
Key_List::dump ()
{
  T (Trace t ("Key_List::dump");)
  int field_width = option.get_max_keysig_size ();

  fprintf (stderr, "\nList contents are:\n(hash value, key length, index, %*s, keyword):\n",
           field_width, "char_set");

  for (List_Node *ptr = head; ptr; ptr = ptr->next)
    fprintf (stderr, "%11d,%11d,%6d, %*.*s, %.*s\n",
             ptr->hash_value, ptr->key_length, ptr->index,
             field_width, ptr->char_set_length, ptr->char_set,
             ptr->key_length, ptr->key);
}

/* Simple-minded constructor action here... */

Key_List::Key_List (void)
{
  T (Trace t ("Key_List::Key_List");)
  total_keys       = 1;
  max_key_len      = INT_MIN;
  min_key_len      = INT_MAX;
  array_type       = 0;
  return_type      = 0;
  struct_tag       = 0;
  head             = 0;
  total_duplicates = 0;
  additional_code  = 0;
}

/* Returns the length of entire key list. */

int
Key_List::keyword_list_length (void)
{
  T (Trace t ("Key_List::keyword_list_length");)
  return list_len;
}

/* Returns length of longest key read. */

int
Key_List::max_key_length (void)
{
  T (Trace t ("Key_List::max_key_length");)
  return max_key_len;
}

