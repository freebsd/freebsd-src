/* Header file for encrypted-stream library.
 * Written by Ken Raeburn (Raeburn@Cygnus.COM).
 * Copyright (C) 1991, 1992, 1994 by Cygnus Support.
 *
 * Permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation.
 * Cygnus Support makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef K5STREAM_H
#define K5STREAM_H

typedef struct kstream_int {                    /* Object we pass around */
  int fd;                                       /* Open socket descriptor */
  int (*encrypt)(struct kstream_data_block *, /* output */
		 struct kstream_data_block *, /* input */
		 struct kstream *kstream);
  int (*decrypt)(struct kstream_data_block *, /* output */
		 struct kstream_data_block *, /* input */
		 struct kstream *kstream);
} *kstream;

typedef void *kstream_ptr;                      /* Data send on the kstream */

struct kstream_data_block {
  kstream_ptr ptr;
  size_t length;
};

struct kstream_crypt_ctl_block {
  int (*encrypt)(struct kstream_data_block *, /* output */
		 struct kstream_data_block *, /* input */
		 kstream);
  int (*decrypt)(struct kstream_data_block *, /* output */
		 struct kstream_data_block *, /* input */
		 kstream);
  int (*init)(kstream, kstream_ptr);
  void (*destroy)(kstream);
};


/* Prototypes */

int kstream_destroy(kstream);
void kstream_set_buffer_mode(kstream, int);
kstream kstream_create_from_fd(int fd,
			       const struct kstream_crypt_ctl_block *,
			       kstream_ptr);
int kstream_write(kstream, void *, size_t);
int kstream_read(kstream, void *, size_t);

#endif /* K5STREAM_H */
