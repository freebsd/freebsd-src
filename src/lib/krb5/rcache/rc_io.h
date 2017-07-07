/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_io.h */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * Declarations for the I/O sub-package of the replay cache
 */

#ifndef KRB5_RC_IO_H
#define KRB5_RC_IO_H

typedef struct krb5_rc_iostuff {
    int fd;
#ifdef MSDOS_FILESYSTEM
    long mark;
#else
    off_t mark; /* on newer systems, should be pos_t */
#endif
    char *fn;
} krb5_rc_iostuff;

/* first argument is always iostuff for result file */

krb5_error_code
krb5_rc_io_creat(krb5_context, krb5_rc_iostuff *, char **);

krb5_error_code
krb5_rc_io_open(krb5_context, krb5_rc_iostuff *, char *);

krb5_error_code
krb5_rc_io_move(krb5_context, krb5_rc_iostuff *, krb5_rc_iostuff *);

krb5_error_code
krb5_rc_io_write(krb5_context, krb5_rc_iostuff *, krb5_pointer, unsigned int);

krb5_error_code
krb5_rc_io_read(krb5_context, krb5_rc_iostuff *, krb5_pointer, unsigned int);

krb5_error_code
krb5_rc_io_close(krb5_context, krb5_rc_iostuff *);

krb5_error_code
krb5_rc_io_destroy(krb5_context, krb5_rc_iostuff *);

krb5_error_code
krb5_rc_io_mark(krb5_context, krb5_rc_iostuff *);

krb5_error_code
krb5_rc_io_unmark(krb5_context, krb5_rc_iostuff *);

krb5_error_code
krb5_rc_io_sync(krb5_context, krb5_rc_iostuff *);

long
krb5_rc_io_size(krb5_context, krb5_rc_iostuff *);
#endif
