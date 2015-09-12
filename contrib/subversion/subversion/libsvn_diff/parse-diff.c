/*
 * parse-diff.c: functions for parsing diff files
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "svn_hash.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_string.h"
#include "svn_utf.h"
#include "svn_dirent_uri.h"
#include "svn_diff.h"

#include "private/svn_eol_private.h"
#include "private/svn_dep_compat.h"

/* Helper macro for readability */
#define starts_with(str, start)  \
  (strncmp((str), (start), strlen(start)) == 0)

/* Like strlen() but for string literals. */
#define STRLEN_LITERAL(str) (sizeof(str) - 1)

/* This struct describes a range within a file, as well as the
 * current cursor position within the range. All numbers are in bytes. */
struct svn_diff__hunk_range {
  apr_off_t start;
  apr_off_t end;
  apr_off_t current;
};

struct svn_diff_hunk_t {
  /* The patch this hunk belongs to. */
  svn_patch_t *patch;

  /* APR file handle to the patch file this hunk came from. */
  apr_file_t *apr_file;

  /* Ranges used to keep track of this hunk's texts positions within
   * the patch file. */
  struct svn_diff__hunk_range diff_text_range;
  struct svn_diff__hunk_range original_text_range;
  struct svn_diff__hunk_range modified_text_range;

  /* Hunk ranges as they appeared in the patch file.
   * All numbers are lines, not bytes. */
  svn_linenum_t original_start;
  svn_linenum_t original_length;
  svn_linenum_t modified_start;
  svn_linenum_t modified_length;

  /* Number of lines of leading and trailing hunk context. */
  svn_linenum_t leading_context;
  svn_linenum_t trailing_context;
};

void
svn_diff_hunk_reset_diff_text(svn_diff_hunk_t *hunk)
{
  hunk->diff_text_range.current = hunk->diff_text_range.start;
}

void
svn_diff_hunk_reset_original_text(svn_diff_hunk_t *hunk)
{
  if (hunk->patch->reverse)
    hunk->modified_text_range.current = hunk->modified_text_range.start;
  else
    hunk->original_text_range.current = hunk->original_text_range.start;
}

void
svn_diff_hunk_reset_modified_text(svn_diff_hunk_t *hunk)
{
  if (hunk->patch->reverse)
    hunk->original_text_range.current = hunk->original_text_range.start;
  else
    hunk->modified_text_range.current = hunk->modified_text_range.start;
}

svn_linenum_t
svn_diff_hunk_get_original_start(const svn_diff_hunk_t *hunk)
{
  return hunk->patch->reverse ? hunk->modified_start : hunk->original_start;
}

svn_linenum_t
svn_diff_hunk_get_original_length(const svn_diff_hunk_t *hunk)
{
  return hunk->patch->reverse ? hunk->modified_length : hunk->original_length;
}

svn_linenum_t
svn_diff_hunk_get_modified_start(const svn_diff_hunk_t *hunk)
{
  return hunk->patch->reverse ? hunk->original_start : hunk->modified_start;
}

svn_linenum_t
svn_diff_hunk_get_modified_length(const svn_diff_hunk_t *hunk)
{
  return hunk->patch->reverse ? hunk->original_length : hunk->modified_length;
}

svn_linenum_t
svn_diff_hunk_get_leading_context(const svn_diff_hunk_t *hunk)
{
  return hunk->leading_context;
}

svn_linenum_t
svn_diff_hunk_get_trailing_context(const svn_diff_hunk_t *hunk)
{
  return hunk->trailing_context;
}

/* Try to parse a positive number from a decimal number encoded
 * in the string NUMBER. Return parsed number in OFFSET, and return
 * TRUE if parsing was successful. */
static svn_boolean_t
parse_offset(svn_linenum_t *offset, const char *number)
{
  svn_error_t *err;
  apr_uint64_t val;

  err = svn_cstring_strtoui64(&val, number, 0, SVN_LINENUM_MAX_VALUE, 10);
  if (err)
    {
      svn_error_clear(err);
      return FALSE;
    }

  *offset = (svn_linenum_t)val;

  return TRUE;
}

/* Try to parse a hunk range specification from the string RANGE.
 * Return parsed information in *START and *LENGTH, and return TRUE
 * if the range parsed correctly. Note: This function may modify the
 * input value RANGE. */
static svn_boolean_t
parse_range(svn_linenum_t *start, svn_linenum_t *length, char *range)
{
  char *comma;

  if (*range == 0)
    return FALSE;

  comma = strstr(range, ",");
  if (comma)
    {
      if (strlen(comma + 1) > 0)
        {
          /* Try to parse the length. */
          if (! parse_offset(length, comma + 1))
            return FALSE;

          /* Snip off the end of the string,
           * so we can comfortably parse the line
           * number the hunk starts at. */
          *comma = '\0';
        }
       else
         /* A comma but no length? */
         return FALSE;
    }
  else
    {
      *length = 1;
    }

  /* Try to parse the line number the hunk starts at. */
  return parse_offset(start, range);
}

/* Try to parse a hunk header in string HEADER, putting parsed information
 * into HUNK. Return TRUE if the header parsed correctly. ATAT is the
 * character string used to delimit the hunk header.
 * Do all allocations in POOL. */
static svn_boolean_t
parse_hunk_header(const char *header, svn_diff_hunk_t *hunk,
                  const char *atat, apr_pool_t *pool)
{
  const char *p;
  const char *start;
  svn_stringbuf_t *range;

  p = header + strlen(atat);
  if (*p != ' ')
    /* No. */
    return FALSE;
  p++;
  if (*p != '-')
    /* Nah... */
    return FALSE;
  /* OK, this may be worth allocating some memory for... */
  range = svn_stringbuf_create_ensure(31, pool);
  start = ++p;
  while (*p && *p != ' ')
    {
      p++;
    }

  if (*p != ' ')
    /* No no no... */
    return FALSE;

  svn_stringbuf_appendbytes(range, start, p - start);

  /* Try to parse the first range. */
  if (! parse_range(&hunk->original_start, &hunk->original_length, range->data))
    return FALSE;

  /* Clear the stringbuf so we can reuse it for the second range. */
  svn_stringbuf_setempty(range);
  p++;
  if (*p != '+')
    /* Eeek! */
    return FALSE;
  /* OK, this may be worth copying... */
  start = ++p;
  while (*p && *p != ' ')
    {
      p++;
    }
  if (*p != ' ')
    /* No no no... */
    return FALSE;

  svn_stringbuf_appendbytes(range, start, p - start);

  /* Check for trailing @@ */
  p++;
  if (! starts_with(p, atat))
    return FALSE;

  /* There may be stuff like C-function names after the trailing @@,
   * but we ignore that. */

  /* Try to parse the second range. */
  if (! parse_range(&hunk->modified_start, &hunk->modified_length, range->data))
    return FALSE;

  /* Hunk header is good. */
  return TRUE;
}

/* Read a line of original or modified hunk text from the specified
 * RANGE within FILE. FILE is expected to contain unidiff text.
 * Leading unidiff symbols ('+', '-', and ' ') are removed from the line,
 * Any lines commencing with the VERBOTEN character are discarded.
 * VERBOTEN should be '+' or '-', depending on which form of hunk text
 * is being read.
 *
 * All other parameters are as in svn_diff_hunk_readline_original_text()
 * and svn_diff_hunk_readline_modified_text().
 */
static svn_error_t *
hunk_readline_original_or_modified(apr_file_t *file,
                                   struct svn_diff__hunk_range *range,
                                   svn_stringbuf_t **stringbuf,
                                   const char **eol,
                                   svn_boolean_t *eof,
                                   char verboten,
                                   apr_pool_t *result_pool,
                                   apr_pool_t *scratch_pool)
{
  apr_size_t max_len;
  svn_boolean_t filtered;
  apr_off_t pos;
  svn_stringbuf_t *str;

  if (range->current >= range->end)
    {
      /* We're past the range. Indicate that no bytes can be read. */
      *eof = TRUE;
      if (eol)
        *eol = NULL;
      *stringbuf = svn_stringbuf_create_empty(result_pool);
      return SVN_NO_ERROR;
    }

  pos = 0;
  SVN_ERR(svn_io_file_seek(file, APR_CUR, &pos,  scratch_pool));
  SVN_ERR(svn_io_file_seek(file, APR_SET, &range->current, scratch_pool));
  do
    {
      max_len = range->end - range->current;
      SVN_ERR(svn_io_file_readline(file, &str, eol, eof, max_len,
                                   result_pool, scratch_pool));
      range->current = 0;
      SVN_ERR(svn_io_file_seek(file, APR_CUR, &range->current, scratch_pool));
      filtered = (str->data[0] == verboten || str->data[0] == '\\');
    }
  while (filtered && ! *eof);

  if (filtered)
    {
      /* EOF, return an empty string. */
      *stringbuf = svn_stringbuf_create_ensure(0, result_pool);
    }
  else if (str->data[0] == '+' || str->data[0] == '-' || str->data[0] == ' ')
    {
      /* Shave off leading unidiff symbols. */
      *stringbuf = svn_stringbuf_create(str->data + 1, result_pool);
    }
  else
    {
      /* Return the line as-is. */
      *stringbuf = svn_stringbuf_dup(str, result_pool);
    }

  SVN_ERR(svn_io_file_seek(file, APR_SET, &pos, scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_diff_hunk_readline_original_text(svn_diff_hunk_t *hunk,
                                     svn_stringbuf_t **stringbuf,
                                     const char **eol,
                                     svn_boolean_t *eof,
                                     apr_pool_t *result_pool,
                                     apr_pool_t *scratch_pool)
{
  return svn_error_trace(
    hunk_readline_original_or_modified(hunk->apr_file,
                                       hunk->patch->reverse ?
                                         &hunk->modified_text_range :
                                         &hunk->original_text_range,
                                       stringbuf, eol, eof,
                                       hunk->patch->reverse ? '-' : '+',
                                       result_pool, scratch_pool));
}

svn_error_t *
svn_diff_hunk_readline_modified_text(svn_diff_hunk_t *hunk,
                                     svn_stringbuf_t **stringbuf,
                                     const char **eol,
                                     svn_boolean_t *eof,
                                     apr_pool_t *result_pool,
                                     apr_pool_t *scratch_pool)
{
  return svn_error_trace(
    hunk_readline_original_or_modified(hunk->apr_file,
                                       hunk->patch->reverse ?
                                         &hunk->original_text_range :
                                         &hunk->modified_text_range,
                                       stringbuf, eol, eof,
                                       hunk->patch->reverse ? '+' : '-',
                                       result_pool, scratch_pool));
}

svn_error_t *
svn_diff_hunk_readline_diff_text(svn_diff_hunk_t *hunk,
                                 svn_stringbuf_t **stringbuf,
                                 const char **eol,
                                 svn_boolean_t *eof,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool)
{
  svn_diff_hunk_t dummy;
  svn_stringbuf_t *line;
  apr_size_t max_len;
  apr_off_t pos;

  if (hunk->diff_text_range.current >= hunk->diff_text_range.end)
    {
      /* We're past the range. Indicate that no bytes can be read. */
      *eof = TRUE;
      if (eol)
        *eol = NULL;
      *stringbuf = svn_stringbuf_create_empty(result_pool);
      return SVN_NO_ERROR;
    }

  pos = 0;
  SVN_ERR(svn_io_file_seek(hunk->apr_file, APR_CUR, &pos, scratch_pool));
  SVN_ERR(svn_io_file_seek(hunk->apr_file, APR_SET,
                           &hunk->diff_text_range.current, scratch_pool));
  max_len = hunk->diff_text_range.end - hunk->diff_text_range.current;
  SVN_ERR(svn_io_file_readline(hunk->apr_file, &line, eol, eof, max_len,
                               result_pool,
                   scratch_pool));
  hunk->diff_text_range.current = 0;
  SVN_ERR(svn_io_file_seek(hunk->apr_file, APR_CUR,
                           &hunk->diff_text_range.current, scratch_pool));
  SVN_ERR(svn_io_file_seek(hunk->apr_file, APR_SET, &pos, scratch_pool));

  if (hunk->patch->reverse)
    {
      if (parse_hunk_header(line->data, &dummy, "@@", scratch_pool))
        {
          /* Line is a hunk header, reverse it. */
          line = svn_stringbuf_createf(result_pool,
                                       "@@ -%lu,%lu +%lu,%lu @@",
                                       hunk->modified_start,
                                       hunk->modified_length,
                                       hunk->original_start,
                                       hunk->original_length);
        }
      else if (parse_hunk_header(line->data, &dummy, "##", scratch_pool))
        {
          /* Line is a hunk header, reverse it. */
          line = svn_stringbuf_createf(result_pool,
                                       "## -%lu,%lu +%lu,%lu ##",
                                       hunk->modified_start,
                                       hunk->modified_length,
                                       hunk->original_start,
                                       hunk->original_length);
        }
      else
        {
          if (line->data[0] == '+')
            line->data[0] = '-';
          else if (line->data[0] == '-')
            line->data[0] = '+';
        }
    }

  *stringbuf = line;

  return SVN_NO_ERROR;
}

/* Parse *PROP_NAME from HEADER as the part after the INDICATOR line.
 * Allocate *PROP_NAME in RESULT_POOL.
 * Set *PROP_NAME to NULL if no valid property name was found. */
static svn_error_t *
parse_prop_name(const char **prop_name, const char *header,
                const char *indicator, apr_pool_t *result_pool)
{
  SVN_ERR(svn_utf_cstring_to_utf8(prop_name,
                                  header + strlen(indicator),
                                  result_pool));
  if (**prop_name == '\0')
    *prop_name = NULL;
  else if (! svn_prop_name_is_valid(*prop_name))
    {
      svn_stringbuf_t *buf = svn_stringbuf_create(*prop_name, result_pool);
      svn_stringbuf_strip_whitespace(buf);
      *prop_name = (svn_prop_name_is_valid(buf->data) ? buf->data : NULL);
    }

  return SVN_NO_ERROR;
}

/* Return the next *HUNK from a PATCH in APR_FILE.
 * If no hunk can be found, set *HUNK to NULL.
 * Set IS_PROPERTY to TRUE if we have a property hunk. If the returned HUNK
 * is the first belonging to a certain property, then PROP_NAME and
 * PROP_OPERATION will be set too. If we have a text hunk, PROP_NAME will be
 * NULL.  If IGNORE_WHITESPACE is TRUE, lines without leading spaces will be
 * treated as context lines.  Allocate results in RESULT_POOL.
 * Use SCRATCH_POOL for all other allocations. */
static svn_error_t *
parse_next_hunk(svn_diff_hunk_t **hunk,
                svn_boolean_t *is_property,
                const char **prop_name,
                svn_diff_operation_kind_t *prop_operation,
                svn_patch_t *patch,
                apr_file_t *apr_file,
                svn_boolean_t ignore_whitespace,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  static const char * const minus = "--- ";
  static const char * const text_atat = "@@";
  static const char * const prop_atat = "##";
  svn_stringbuf_t *line;
  svn_boolean_t eof, in_hunk, hunk_seen;
  apr_off_t pos, last_line;
  apr_off_t start, end;
  apr_off_t original_end;
  apr_off_t modified_end;
  svn_linenum_t original_lines;
  svn_linenum_t modified_lines;
  svn_linenum_t leading_context;
  svn_linenum_t trailing_context;
  svn_boolean_t changed_line_seen;
  enum {
    noise_line,
    original_line,
    modified_line,
    context_line
  } last_line_type;
  apr_pool_t *iterpool;

  *prop_operation = svn_diff_op_unchanged;

  /* We only set this if we have a property hunk header. */
  *prop_name = NULL;
  *is_property = FALSE;

  if (apr_file_eof(apr_file) == APR_EOF)
    {
      /* No more hunks here. */
      *hunk = NULL;
      return SVN_NO_ERROR;
    }

  in_hunk = FALSE;
  hunk_seen = FALSE;
  leading_context = 0;
  trailing_context = 0;
  changed_line_seen = FALSE;
  original_end = 0;
  modified_end = 0;
  *hunk = apr_pcalloc(result_pool, sizeof(**hunk));

  /* Get current seek position -- APR has no ftell() :( */
  pos = 0;
  SVN_ERR(svn_io_file_seek(apr_file, APR_CUR, &pos, scratch_pool));

  /* Start out assuming noise. */
  last_line_type = noise_line;

  iterpool = svn_pool_create(scratch_pool);
  do
    {

      svn_pool_clear(iterpool);

      /* Remember the current line's offset, and read the line. */
      last_line = pos;
      SVN_ERR(svn_io_file_readline(apr_file, &line, NULL, &eof, APR_SIZE_MAX,
                                   iterpool, iterpool));

      /* Update line offset for next iteration. */
      pos = 0;
      SVN_ERR(svn_io_file_seek(apr_file, APR_CUR, &pos, iterpool));

      /* Lines starting with a backslash indicate a missing EOL:
       * "\ No newline at end of file" or "end of property". */
      if (line->data[0] == '\\')
        {
          if (in_hunk)
            {
              char eolbuf[2];
              apr_size_t len;
              apr_off_t off;
              apr_off_t hunk_text_end;

              /* Comment terminates the hunk text and says the hunk text
               * has no trailing EOL. Snip off trailing EOL which is part
               * of the patch file but not part of the hunk text. */
              off = last_line - 2;
              SVN_ERR(svn_io_file_seek(apr_file, APR_SET, &off, iterpool));
              len = sizeof(eolbuf);
              SVN_ERR(svn_io_file_read_full2(apr_file, eolbuf, len, &len,
                                             &eof, iterpool));
              if (eolbuf[0] == '\r' && eolbuf[1] == '\n')
                hunk_text_end = last_line - 2;
              else if (eolbuf[1] == '\n' || eolbuf[1] == '\r')
                hunk_text_end = last_line - 1;
              else
                hunk_text_end = last_line;

              if (last_line_type == original_line && original_end == 0)
                original_end = hunk_text_end;
              else if (last_line_type == modified_line && modified_end == 0)
                modified_end = hunk_text_end;
              else if (last_line_type == context_line)
                {
                  if (original_end == 0)
                    original_end = hunk_text_end;
                  if (modified_end == 0)
                    modified_end = hunk_text_end;
                }

              SVN_ERR(svn_io_file_seek(apr_file, APR_SET, &pos, iterpool));
            }

          continue;
        }

      if (in_hunk)
        {
          char c;
          static const char add = '+';
          static const char del = '-';

          if (! hunk_seen)
            {
              /* We're reading the first line of the hunk, so the start
               * of the line just read is the hunk text's byte offset. */
              start = last_line;
            }

          c = line->data[0];
          if (original_lines > 0 && modified_lines > 0 &&
              ((c == ' ')
               /* Tolerate chopped leading spaces on empty lines. */
               || (! eof && line->len == 0)
               /* Maybe tolerate chopped leading spaces on non-empty lines. */
               || (ignore_whitespace && c != del && c != add)))
            {
              /* It's a "context" line in the hunk. */
              hunk_seen = TRUE;
              original_lines--;
              modified_lines--;
              if (changed_line_seen)
                trailing_context++;
              else
                leading_context++;
              last_line_type = context_line;
            }
          else if (original_lines > 0 && c == del)
            {
              /* It's a "deleted" line in the hunk. */
              hunk_seen = TRUE;
              changed_line_seen = TRUE;

              /* A hunk may have context in the middle. We only want
                 trailing lines of context. */
              if (trailing_context > 0)
                trailing_context = 0;

              original_lines--;
              last_line_type = original_line;
            }
          else if (modified_lines > 0 && c == add)
            {
              /* It's an "added" line in the hunk. */
              hunk_seen = TRUE;
              changed_line_seen = TRUE;

              /* A hunk may have context in the middle. We only want
                 trailing lines of context. */
              if (trailing_context > 0)
                trailing_context = 0;

              modified_lines--;
              last_line_type = modified_line;
            }
          else
            {
              if (eof)
                {
                  /* The hunk ends at EOF. */
                  end = pos;
                }
              else
                {
                  /* The start of the current line marks the first byte
                   * after the hunk text. */
                  end = last_line;
                }

              if (original_end == 0)
                original_end = end;
              if (modified_end == 0)
                modified_end = end;
              break; /* Hunk was empty or has been read. */
            }
        }
      else
        {
          if (starts_with(line->data, text_atat))
            {
              /* Looks like we have a hunk header, try to rip it apart. */
              in_hunk = parse_hunk_header(line->data, *hunk, text_atat,
                                          iterpool);
              if (in_hunk)
                {
                  original_lines = (*hunk)->original_length;
                  modified_lines = (*hunk)->modified_length;
                  *is_property = FALSE;
                }
              }
          else if (starts_with(line->data, prop_atat))
            {
              /* Looks like we have a property hunk header, try to rip it
               * apart. */
              in_hunk = parse_hunk_header(line->data, *hunk, prop_atat,
                                          iterpool);
              if (in_hunk)
                {
                  original_lines = (*hunk)->original_length;
                  modified_lines = (*hunk)->modified_length;
                  *is_property = TRUE;
                }
            }
          else if (starts_with(line->data, "Added: "))
            {
              SVN_ERR(parse_prop_name(prop_name, line->data, "Added: ",
                                      result_pool));
              if (*prop_name)
                *prop_operation = svn_diff_op_added;
            }
          else if (starts_with(line->data, "Deleted: "))
            {
              SVN_ERR(parse_prop_name(prop_name, line->data, "Deleted: ",
                                      result_pool));
              if (*prop_name)
                *prop_operation = svn_diff_op_deleted;
            }
          else if (starts_with(line->data, "Modified: "))
            {
              SVN_ERR(parse_prop_name(prop_name, line->data, "Modified: ",
                                      result_pool));
              if (*prop_name)
                *prop_operation = svn_diff_op_modified;
            }
          else if (starts_with(line->data, minus)
                   || starts_with(line->data, "diff --git "))
            /* This could be a header of another patch. Bail out. */
            break;
        }
    }
  /* Check for the line length since a file may not have a newline at the
   * end and we depend upon the last line to be an empty one. */
  while (! eof || line->len > 0);
  svn_pool_destroy(iterpool);

  if (! eof)
    /* Rewind to the start of the line just read, so subsequent calls
     * to this function or svn_diff_parse_next_patch() don't end
     * up skipping the line -- it may contain a patch or hunk header. */
    SVN_ERR(svn_io_file_seek(apr_file, APR_SET, &last_line, scratch_pool));

  if (hunk_seen && start < end)
    {
      (*hunk)->patch = patch;
      (*hunk)->apr_file = apr_file;
      (*hunk)->leading_context = leading_context;
      (*hunk)->trailing_context = trailing_context;
      (*hunk)->diff_text_range.start = start;
      (*hunk)->diff_text_range.current = start;
      (*hunk)->diff_text_range.end = end;
      (*hunk)->original_text_range.start = start;
      (*hunk)->original_text_range.current = start;
      (*hunk)->original_text_range.end = original_end;
      (*hunk)->modified_text_range.start = start;
      (*hunk)->modified_text_range.current = start;
      (*hunk)->modified_text_range.end = modified_end;
    }
  else
    /* Something went wrong, just discard the result. */
    *hunk = NULL;

  return SVN_NO_ERROR;
}

/* Compare function for sorting hunks after parsing.
 * We sort hunks by their original line offset. */
static int
compare_hunks(const void *a, const void *b)
{
  const svn_diff_hunk_t *ha = *((const svn_diff_hunk_t *const *)a);
  const svn_diff_hunk_t *hb = *((const svn_diff_hunk_t *const *)b);

  if (ha->original_start < hb->original_start)
    return -1;
  if (ha->original_start > hb->original_start)
    return 1;
  return 0;
}

/* Possible states of the diff header parser. */
enum parse_state
{
   state_start,           /* initial */
   state_git_diff_seen,   /* diff --git */
   state_git_tree_seen,   /* a tree operation, rather then content change */
   state_git_minus_seen,  /* --- /dev/null; or --- a/ */
   state_git_plus_seen,   /* +++ /dev/null; or +++ a/ */
   state_move_from_seen,  /* rename from foo.c */
   state_copy_from_seen,  /* copy from foo.c */
   state_minus_seen,      /* --- foo.c */
   state_unidiff_found,   /* valid start of a regular unidiff header */
   state_git_header_found /* valid start of a --git diff header */
};

/* Data type describing a valid state transition of the parser. */
struct transition
{
  const char *expected_input;
  enum parse_state required_state;

  /* A callback called upon each parser state transition. */
  svn_error_t *(*fn)(enum parse_state *new_state, char *input,
                     svn_patch_t *patch, apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool);
};

/* UTF-8 encode and canonicalize the content of LINE as FILE_NAME. */
static svn_error_t *
grab_filename(const char **file_name, const char *line, apr_pool_t *result_pool,
              apr_pool_t *scratch_pool)
{
  const char *utf8_path;
  const char *canon_path;

  /* Grab the filename and encode it in UTF-8. */
  /* TODO: Allow specifying the patch file's encoding.
   *       For now, we assume its encoding is native. */
  /* ### This can fail if the filename cannot be represented in the current
   * ### locale's encoding. */
  SVN_ERR(svn_utf_cstring_to_utf8(&utf8_path,
                                  line,
                                  scratch_pool));

  /* Canonicalize the path name. */
  canon_path = svn_dirent_canonicalize(utf8_path, scratch_pool);

  *file_name = apr_pstrdup(result_pool, canon_path);

  return SVN_NO_ERROR;
}

/* Parse the '--- ' line of a regular unidiff. */
static svn_error_t *
diff_minus(enum parse_state *new_state, char *line, svn_patch_t *patch,
           apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  /* If we can find a tab, it separates the filename from
   * the rest of the line which we can discard. */
  char *tab = strchr(line, '\t');
  if (tab)
    *tab = '\0';

  SVN_ERR(grab_filename(&patch->old_filename, line + STRLEN_LITERAL("--- "),
                        result_pool, scratch_pool));

  *new_state = state_minus_seen;

  return SVN_NO_ERROR;
}

/* Parse the '+++ ' line of a regular unidiff. */
static svn_error_t *
diff_plus(enum parse_state *new_state, char *line, svn_patch_t *patch,
           apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  /* If we can find a tab, it separates the filename from
   * the rest of the line which we can discard. */
  char *tab = strchr(line, '\t');
  if (tab)
    *tab = '\0';

  SVN_ERR(grab_filename(&patch->new_filename, line + STRLEN_LITERAL("+++ "),
                        result_pool, scratch_pool));

  *new_state = state_unidiff_found;

  return SVN_NO_ERROR;
}

/* Parse the first line of a git extended unidiff. */
static svn_error_t *
git_start(enum parse_state *new_state, char *line, svn_patch_t *patch,
          apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  const char *old_path_start;
  char *old_path_end;
  const char *new_path_start;
  const char *new_path_end;
  char *new_path_marker;
  const char *old_path_marker;

  /* ### Add handling of escaped paths
   * http://www.kernel.org/pub/software/scm/git/docs/git-diff.html:
   *
   * TAB, LF, double quote and backslash characters in pathnames are
   * represented as \t, \n, \" and \\, respectively. If there is need for
   * such substitution then the whole pathname is put in double quotes.
   */

  /* Our line should look like this: 'diff --git a/path b/path'.
   *
   * If we find any deviations from that format, we return with state reset
   * to start.
   */
  old_path_marker = strstr(line, " a/");

  if (! old_path_marker)
    {
      *new_state = state_start;
      return SVN_NO_ERROR;
    }

  if (! *(old_path_marker + 3))
    {
      *new_state = state_start;
      return SVN_NO_ERROR;
    }

  new_path_marker = strstr(old_path_marker, " b/");

  if (! new_path_marker)
    {
      *new_state = state_start;
      return SVN_NO_ERROR;
    }

  if (! *(new_path_marker + 3))
    {
      *new_state = state_start;
      return SVN_NO_ERROR;
    }

  /* By now, we know that we have a line on the form '--git diff a/.+ b/.+'
   * We only need the filenames when we have deleted or added empty
   * files. In those cases the old_path and new_path is identical on the
   * 'diff --git' line.  For all other cases we fetch the filenames from
   * other header lines. */
  old_path_start = line + STRLEN_LITERAL("diff --git a/");
  new_path_end = line + strlen(line);
  new_path_start = old_path_start;

  while (TRUE)
    {
      ptrdiff_t len_old;
      ptrdiff_t len_new;

      new_path_marker = strstr(new_path_start, " b/");

      /* No new path marker, bail out. */
      if (! new_path_marker)
        break;

      old_path_end = new_path_marker;
      new_path_start = new_path_marker + STRLEN_LITERAL(" b/");

      /* No path after the marker. */
      if (! *new_path_start)
        break;

      len_old = old_path_end - old_path_start;
      len_new = new_path_end - new_path_start;

      /* Are the paths before and after the " b/" marker the same? */
      if (len_old == len_new
          && ! strncmp(old_path_start, new_path_start, len_old))
        {
          *old_path_end = '\0';
          SVN_ERR(grab_filename(&patch->old_filename, old_path_start,
                                result_pool, scratch_pool));

          SVN_ERR(grab_filename(&patch->new_filename, new_path_start,
                                result_pool, scratch_pool));
          break;
        }
    }

  /* We assume that the path is only modified until we've found a 'tree'
   * header */
  patch->operation = svn_diff_op_modified;

  *new_state = state_git_diff_seen;
  return SVN_NO_ERROR;
}

/* Parse the '--- ' line of a git extended unidiff. */
static svn_error_t *
git_minus(enum parse_state *new_state, char *line, svn_patch_t *patch,
          apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  /* If we can find a tab, it separates the filename from
   * the rest of the line which we can discard. */
  char *tab = strchr(line, '\t');
  if (tab)
    *tab = '\0';

  if (starts_with(line, "--- /dev/null"))
    SVN_ERR(grab_filename(&patch->old_filename, "/dev/null",
                          result_pool, scratch_pool));
  else
    SVN_ERR(grab_filename(&patch->old_filename, line + STRLEN_LITERAL("--- a/"),
                          result_pool, scratch_pool));

  *new_state = state_git_minus_seen;
  return SVN_NO_ERROR;
}

/* Parse the '+++ ' line of a git extended unidiff. */
static svn_error_t *
git_plus(enum parse_state *new_state, char *line, svn_patch_t *patch,
          apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  /* If we can find a tab, it separates the filename from
   * the rest of the line which we can discard. */
  char *tab = strchr(line, '\t');
  if (tab)
    *tab = '\0';

  if (starts_with(line, "+++ /dev/null"))
    SVN_ERR(grab_filename(&patch->new_filename, "/dev/null",
                          result_pool, scratch_pool));
  else
    SVN_ERR(grab_filename(&patch->new_filename, line + STRLEN_LITERAL("+++ b/"),
                          result_pool, scratch_pool));

  *new_state = state_git_header_found;
  return SVN_NO_ERROR;
}

/* Parse the 'rename from ' line of a git extended unidiff. */
static svn_error_t *
git_move_from(enum parse_state *new_state, char *line, svn_patch_t *patch,
              apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  SVN_ERR(grab_filename(&patch->old_filename,
                        line + STRLEN_LITERAL("rename from "),
                        result_pool, scratch_pool));

  *new_state = state_move_from_seen;
  return SVN_NO_ERROR;
}

/* Parse the 'rename to ' line of a git extended unidiff. */
static svn_error_t *
git_move_to(enum parse_state *new_state, char *line, svn_patch_t *patch,
            apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  SVN_ERR(grab_filename(&patch->new_filename,
                        line + STRLEN_LITERAL("rename to "),
                        result_pool, scratch_pool));

  patch->operation = svn_diff_op_moved;

  *new_state = state_git_tree_seen;
  return SVN_NO_ERROR;
}

/* Parse the 'copy from ' line of a git extended unidiff. */
static svn_error_t *
git_copy_from(enum parse_state *new_state, char *line, svn_patch_t *patch,
              apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  SVN_ERR(grab_filename(&patch->old_filename,
                        line + STRLEN_LITERAL("copy from "),
                        result_pool, scratch_pool));

  *new_state = state_copy_from_seen;
  return SVN_NO_ERROR;
}

/* Parse the 'copy to ' line of a git extended unidiff. */
static svn_error_t *
git_copy_to(enum parse_state *new_state, char *line, svn_patch_t *patch,
            apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  SVN_ERR(grab_filename(&patch->new_filename, line + STRLEN_LITERAL("copy to "),
                        result_pool, scratch_pool));

  patch->operation = svn_diff_op_copied;

  *new_state = state_git_tree_seen;
  return SVN_NO_ERROR;
}

/* Parse the 'new file ' line of a git extended unidiff. */
static svn_error_t *
git_new_file(enum parse_state *new_state, char *line, svn_patch_t *patch,
             apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  patch->operation = svn_diff_op_added;

  /* Filename already retrieved from diff --git header. */

  *new_state = state_git_tree_seen;
  return SVN_NO_ERROR;
}

/* Parse the 'deleted file ' line of a git extended unidiff. */
static svn_error_t *
git_deleted_file(enum parse_state *new_state, char *line, svn_patch_t *patch,
                 apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  patch->operation = svn_diff_op_deleted;

  /* Filename already retrieved from diff --git header. */

  *new_state = state_git_tree_seen;
  return SVN_NO_ERROR;
}

/* Add a HUNK associated with the property PROP_NAME to PATCH. */
static svn_error_t *
add_property_hunk(svn_patch_t *patch, const char *prop_name,
                  svn_diff_hunk_t *hunk, svn_diff_operation_kind_t operation,
                  apr_pool_t *result_pool)
{
  svn_prop_patch_t *prop_patch;

  prop_patch = svn_hash_gets(patch->prop_patches, prop_name);

  if (! prop_patch)
    {
      prop_patch = apr_palloc(result_pool, sizeof(svn_prop_patch_t));
      prop_patch->name = prop_name;
      prop_patch->operation = operation;
      prop_patch->hunks = apr_array_make(result_pool, 1,
                                         sizeof(svn_diff_hunk_t *));

      svn_hash_sets(patch->prop_patches, prop_name, prop_patch);
    }

  APR_ARRAY_PUSH(prop_patch->hunks, svn_diff_hunk_t *) = hunk;

  return SVN_NO_ERROR;
}

struct svn_patch_file_t
{
  /* The APR file handle to the patch file. */
  apr_file_t *apr_file;

  /* The file offset at which the next patch is expected. */
  apr_off_t next_patch_offset;
};

svn_error_t *
svn_diff_open_patch_file(svn_patch_file_t **patch_file,
                         const char *local_abspath,
                         apr_pool_t *result_pool)
{
  svn_patch_file_t *p;

  p = apr_palloc(result_pool, sizeof(*p));
  SVN_ERR(svn_io_file_open(&p->apr_file, local_abspath,
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT,
                           result_pool));
  p->next_patch_offset = 0;
  *patch_file = p;

  return SVN_NO_ERROR;
}

/* Parse hunks from APR_FILE and store them in PATCH->HUNKS.
 * Parsing stops if no valid next hunk can be found.
 * If IGNORE_WHITESPACE is TRUE, lines without
 * leading spaces will be treated as context lines.
 * Allocate results in RESULT_POOL.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
parse_hunks(svn_patch_t *patch, apr_file_t *apr_file,
            svn_boolean_t ignore_whitespace,
            apr_pool_t *result_pool, apr_pool_t *scratch_pool)
{
  svn_diff_hunk_t *hunk;
  svn_boolean_t is_property;
  const char *last_prop_name;
  const char *prop_name;
  svn_diff_operation_kind_t prop_operation;
  apr_pool_t *iterpool;

  last_prop_name = NULL;

  patch->hunks = apr_array_make(result_pool, 10, sizeof(svn_diff_hunk_t *));
  patch->prop_patches = apr_hash_make(result_pool);
  iterpool = svn_pool_create(scratch_pool);
  do
    {
      svn_pool_clear(iterpool);

      SVN_ERR(parse_next_hunk(&hunk, &is_property, &prop_name, &prop_operation,
                              patch, apr_file, ignore_whitespace, result_pool,
                              iterpool));

      if (hunk && is_property)
        {
          if (! prop_name)
            prop_name = last_prop_name;
          else
            last_prop_name = prop_name;
          SVN_ERR(add_property_hunk(patch, prop_name, hunk, prop_operation,
                                    result_pool));
        }
      else if (hunk)
        {
          APR_ARRAY_PUSH(patch->hunks, svn_diff_hunk_t *) = hunk;
          last_prop_name = NULL;
        }

    }
  while (hunk);
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* State machine for the diff header parser.
 * Expected Input   Required state          Function to call */
static struct transition transitions[] =
{
  {"--- ",          state_start,            diff_minus},
  {"+++ ",          state_minus_seen,       diff_plus},
  {"diff --git",    state_start,            git_start},
  {"--- a/",        state_git_diff_seen,    git_minus},
  {"--- a/",        state_git_tree_seen,    git_minus},
  {"--- /dev/null", state_git_tree_seen,    git_minus},
  {"+++ b/",        state_git_minus_seen,   git_plus},
  {"+++ /dev/null", state_git_minus_seen,   git_plus},
  {"rename from ",  state_git_diff_seen,    git_move_from},
  {"rename to ",    state_move_from_seen,   git_move_to},
  {"copy from ",    state_git_diff_seen,    git_copy_from},
  {"copy to ",      state_copy_from_seen,   git_copy_to},
  {"new file ",     state_git_diff_seen,    git_new_file},
  {"deleted file ", state_git_diff_seen,    git_deleted_file},
};

svn_error_t *
svn_diff_parse_next_patch(svn_patch_t **patch,
                          svn_patch_file_t *patch_file,
                          svn_boolean_t reverse,
                          svn_boolean_t ignore_whitespace,
                          apr_pool_t *result_pool,
                          apr_pool_t *scratch_pool)
{
  apr_off_t pos, last_line;
  svn_boolean_t eof;
  svn_boolean_t line_after_tree_header_read = FALSE;
  apr_pool_t *iterpool;
  enum parse_state state = state_start;

  if (apr_file_eof(patch_file->apr_file) == APR_EOF)
    {
      /* No more patches here. */
      *patch = NULL;
      return SVN_NO_ERROR;
    }

  *patch = apr_pcalloc(result_pool, sizeof(**patch));

  pos = patch_file->next_patch_offset;
  SVN_ERR(svn_io_file_seek(patch_file->apr_file, APR_SET, &pos, scratch_pool));

  iterpool = svn_pool_create(scratch_pool);
  do
    {
      svn_stringbuf_t *line;
      svn_boolean_t valid_header_line = FALSE;
      int i;

      svn_pool_clear(iterpool);

      /* Remember the current line's offset, and read the line. */
      last_line = pos;
      SVN_ERR(svn_io_file_readline(patch_file->apr_file, &line, NULL, &eof,
                                   APR_SIZE_MAX, iterpool, iterpool));

      if (! eof)
        {
          /* Update line offset for next iteration. */
          pos = 0;
          SVN_ERR(svn_io_file_seek(patch_file->apr_file, APR_CUR, &pos,
                                   iterpool));
        }

      /* Run the state machine. */
      for (i = 0; i < (sizeof(transitions) / sizeof(transitions[0])); i++)
        {
          if (starts_with(line->data, transitions[i].expected_input)
              && state == transitions[i].required_state)
            {
              SVN_ERR(transitions[i].fn(&state, line->data, *patch,
                                        result_pool, iterpool));
              valid_header_line = TRUE;
              break;
            }
        }

      if (state == state_unidiff_found || state == state_git_header_found)
        {
          /* We have a valid diff header, yay! */
          break;
        }
      else if (state == state_git_tree_seen && line_after_tree_header_read)
        {
          /* git patches can contain an index line after the file mode line */
          if (!starts_with(line->data, "index "))
          {
            /* We have a valid diff header for a patch with only tree changes.
             * Rewind to the start of the line just read, so subsequent calls
             * to this function don't end up skipping the line -- it may
             * contain a patch. */
            SVN_ERR(svn_io_file_seek(patch_file->apr_file, APR_SET, &last_line,
                    scratch_pool));
            break;
          }
        }
      else if (state == state_git_tree_seen)
        {
          line_after_tree_header_read = TRUE;
        }
      else if (! valid_header_line && state != state_start
               && state != state_git_diff_seen
               && !starts_with(line->data, "index "))
        {
          /* We've encountered an invalid diff header.
           *
           * Rewind to the start of the line just read - it may be a new
           * header that begins there. */
          SVN_ERR(svn_io_file_seek(patch_file->apr_file, APR_SET, &last_line,
                                   scratch_pool));
          state = state_start;
        }

    }
  while (! eof);

  (*patch)->reverse = reverse;
  if (reverse)
    {
      const char *temp;
      temp = (*patch)->old_filename;
      (*patch)->old_filename = (*patch)->new_filename;
      (*patch)->new_filename = temp;
    }

  if ((*patch)->old_filename == NULL || (*patch)->new_filename == NULL)
    {
      /* Something went wrong, just discard the result. */
      *patch = NULL;
    }
  else
    SVN_ERR(parse_hunks(*patch, patch_file->apr_file, ignore_whitespace,
                        result_pool, iterpool));

  svn_pool_destroy(iterpool);

  patch_file->next_patch_offset = 0;
  SVN_ERR(svn_io_file_seek(patch_file->apr_file, APR_CUR,
                           &patch_file->next_patch_offset, scratch_pool));

  if (*patch)
    {
      /* Usually, hunks appear in the patch sorted by their original line
       * offset. But just in case they weren't parsed in this order for
       * some reason, we sort them so that our caller can assume that hunks
       * are sorted as if parsed from a usual patch. */
      qsort((*patch)->hunks->elts, (*patch)->hunks->nelts,
            (*patch)->hunks->elt_size, compare_hunks);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_diff_close_patch_file(svn_patch_file_t *patch_file,
                          apr_pool_t *scratch_pool)
{
  return svn_error_trace(svn_io_file_close(patch_file->apr_file,
                                           scratch_pool));
}
