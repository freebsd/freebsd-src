/*-
 * Copyright (c) 2003-2012 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

static struct archive_read_filter_bidder *
get_last_bidder(struct archive_read *a)
{
  size_t i;

  for (i = 0; i < sizeof(a->bidders) / sizeof(a->bidders[0]); i++)
  {
    if (a->bidders[i].vtable == NULL)
      break;
  }

  return a->bidders + (i - 1);
}

int
archive_read_append_filter(struct archive *_a, int code)
{
  struct archive_read *a = (struct archive_read *)_a;
  int r1, r2;

  r2 = (ARCHIVE_OK);
  switch (code)
  {
    case ARCHIVE_FILTER_NONE:
      /* No filter to add, so do nothing.
       * NOTE: An initial "NONE" type filter is always set at the end of the
       * filter chain.
       */
      r1 = (ARCHIVE_OK);
      break;
    case ARCHIVE_FILTER_GZIP:
      r1 = archive_read_support_filter_gzip(_a);
      break;
    case ARCHIVE_FILTER_BZIP2:
      r1 = archive_read_support_filter_bzip2(_a);
      break;
    case ARCHIVE_FILTER_COMPRESS:
      r1 = archive_read_support_filter_compress(_a);
      break;
    case ARCHIVE_FILTER_PROGRAM:
      archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
          "Cannot append program filter using archive_read_append_filter");
      return (ARCHIVE_FATAL);
    case ARCHIVE_FILTER_LZMA:
      r1 = archive_read_support_filter_lzma(_a);
      break;
    case ARCHIVE_FILTER_XZ:
      r1 = archive_read_support_filter_xz(_a);
      break;
    case ARCHIVE_FILTER_UU:
      r1 = archive_read_support_filter_uu(_a);
      break;
    case ARCHIVE_FILTER_RPM:
      r1 = archive_read_support_filter_rpm(_a);
      break;
    case ARCHIVE_FILTER_LZ4:
      r1 = archive_read_support_filter_lz4(_a);
      break;
    case ARCHIVE_FILTER_ZSTD:
      r1 = archive_read_support_filter_zstd(_a);
      break;
    case ARCHIVE_FILTER_LZIP:
      r1 = archive_read_support_filter_lzip(_a);
      break;
    case ARCHIVE_FILTER_LZOP:
      r1 = archive_read_support_filter_lzop(_a);
      break;
    case ARCHIVE_FILTER_LRZIP:
      r1 = archive_read_support_filter_lrzip(_a);
      break;
    case ARCHIVE_FILTER_GRZIP:
      r1 = archive_read_support_filter_grzip(_a);
      break;
    default:
      archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
          "Invalid filter code specified");
      return (ARCHIVE_FATAL);
  }

  if (r1 > ARCHIVE_FATAL && code != ARCHIVE_FILTER_NONE)
  {
    struct archive_read_filter_bidder *b;
    struct archive_read_filter *f;

    f = calloc(1, sizeof(*f));
    if (f == NULL)
    {
      archive_set_error(&a->archive, ENOMEM, "Out of memory");
      return (ARCHIVE_FATAL);
    }
    b = get_last_bidder(a);
    f->bidder = b;
    f->archive = a;
    f->upstream = a->filter;
    a->filter = f;
    r2 = (b->vtable->init)(a->filter);
    if (r2 != ARCHIVE_OK) {
      __archive_read_free_filters(a);
      return (ARCHIVE_FATAL);
    }
  }

  a->bypass_filter_bidding = 1;
  return (r1 < r2) ? r1 : r2;
}

int
archive_read_append_filter_program(struct archive *_a, const char *cmd)
{
  return (archive_read_append_filter_program_signature(_a, cmd, NULL, 0));
}

int
archive_read_append_filter_program_signature(struct archive *_a,
  const char *cmd, const void *signature, size_t signature_len)
{
  struct archive_read *a = (struct archive_read *)_a;
  struct archive_read_filter_bidder *b;
  struct archive_read_filter *f;
  int r;

  if (archive_read_support_filter_program_signature(_a, cmd, signature,
    signature_len) != (ARCHIVE_OK))
    return (ARCHIVE_FATAL);

  f = calloc(1, sizeof(*f));
  if (f == NULL)
  {
    archive_set_error(&a->archive, ENOMEM, "Out of memory");
    return (ARCHIVE_FATAL);
  }
  b = get_last_bidder(a);
  f->bidder = b;
  f->archive = a;
  f->upstream = a->filter;
  a->filter = f;
  r = (b->vtable->init)(a->filter);
  if (r != ARCHIVE_OK) {
    __archive_read_free_filters(a);
    return (ARCHIVE_FATAL);
  }

  a->bypass_filter_bidding = 1;
  return r;
}
