/*
 * Copyright (c) 2019 Google LLC
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SSHSIG_H
#define SSHSIG_H

struct sshbuf;
struct sshkey;
struct sshsigopt;

typedef int sshsig_signer(struct sshkey *, u_char **, size_t *,
    const u_char *, size_t, const char *, u_int, void *);

/* Buffer-oriented API */

/*
 * Creates a detached SSH signature for a given buffer.
 * Returns 0 on success or a negative SSH_ERR_* error code on failure.
 * out is populated with the detached signature, or NULL on failure.
 */
int sshsig_signb(struct sshkey *key, const char *hashalg,
    const struct sshbuf *message, const char *sig_namespace,
    struct sshbuf **out, sshsig_signer *signer, void *signer_ctx);

/*
 * Verifies that a detached signature is valid and optionally returns key
 * used to sign via argument.
 * Returns 0 on success or a negative SSH_ERR_* error code on failure.
 */
int sshsig_verifyb(struct sshbuf *signature,
    const struct sshbuf *message, const char *sig_namespace,
    struct sshkey **sign_keyp);

/* File/FD-oriented API */

/*
 * Creates a detached SSH signature for a given file.
 * Returns 0 on success or a negative SSH_ERR_* error code on failure.
 * out is populated with the detached signature, or NULL on failure.
 */
int sshsig_sign_fd(struct sshkey *key, const char *hashalg,
    int fd, const char *sig_namespace, struct sshbuf **out,
    sshsig_signer *signer, void *signer_ctx);

/*
 * Verifies that a detached signature over a file is valid and optionally
 * returns key used to sign via argument.
 * Returns 0 on success or a negative SSH_ERR_* error code on failure.
 */
int sshsig_verify_fd(struct sshbuf *signature, int fd,
    const char *sig_namespace, struct sshkey **sign_keyp);

/* Utility functions */

/*
 * Return a base64 encoded "ASCII armoured" version of a raw signature.
 */
int sshsig_armor(const struct sshbuf *blob, struct sshbuf **out);

/*
 * Decode a base64 encoded armoured signature to a raw signature.
 */
int sshsig_dearmor(struct sshbuf *sig, struct sshbuf **out);

/*
 * Checks whether a particular key/principal/namespace is permitted by
 * an allowed_keys file. Returns 0 on success.
 */
int sshsig_check_allowed_keys(const char *path, const struct sshkey *sign_key,
    const char *principal, const char *ns);

/* Parse zero or more allowed_keys signature options */
struct sshsigopt *sshsigopt_parse(const char *opts,
    const char *path, u_long linenum, const char **errstrp);

/* Free signature options */
void sshsigopt_free(struct sshsigopt *opts);

#endif /* SSHSIG_H */
