/*
 *
 * K5stream
 *
 * Emulates the kstream package in Kerberos 4
 *
 */

#include <stdio.h>
#include <io.h>
#include <malloc.h>
#include "telnet.h"
#include "k5stream.h"
#include "auth.h"

int
kstream_destroy(kstream ks)
{
  if (ks != NULL) {
    auth_destroy(ks);                       /* Destroy authorizing */

    closesocket(ks->fd);                    /* Close the socket??? */
    free(ks);
  }
  return 0;
}

void
kstream_set_buffer_mode(kstream ks, int mode)
{
}


kstream
kstream_create_from_fd(int fd,
		       const struct kstream_crypt_ctl_block *ctl,
		       kstream_ptr data)
{
  kstream ks;
  int n;
  BOOL on = 1;

  ks = malloc(sizeof(struct kstream_int));
  if (ks == NULL)
    return NULL;

  ks->fd = fd;

  setsockopt(ks->fd, SOL_SOCKET, SO_OOBINLINE, (const char *)&on, sizeof(on));

  n = auth_init(ks, data);                   /* Initialize authorizing */
  if (n) {
    free(ks);
    return NULL;
  }

  ks->encrypt = NULL;
  ks->decrypt = NULL;

  return ks;
}

int
kstream_write(kstream ks, void *p_data, size_t p_len)
{
  int n;
  struct kstream_data_block i;

#ifdef DEBUG
  hexdump("plaintext:", p_data, p_len);
#endif

  if (ks->encrypt) {
    i.ptr = p_data;
    i.length = p_len;
    ks->encrypt(&i, NULL, NULL);
#ifdef DEBUG
    hexdump("cyphertext:", p_data, p_len);
#endif
  }

  n = send(ks->fd, p_data, p_len, 0);        /* Write the data */

  return n;                                   /* higher layer does retries */
}


int
kstream_read(kstream ks, void *p_data, size_t p_len)
{
  int n;
  struct kstream_data_block i;

  n = recv(ks->fd, p_data, p_len, 0);        /* read the data */

  if (n < 0)
    return n;

#ifdef DEBUG
  hexdump("input data:", p_data, n);
#endif

  if (ks->decrypt) {
    extern int encrypt_flag;

    if (encrypt_flag == 2)
      encrypt_flag = 1;

    i.ptr = p_data;
    i.length = n;
    ks->decrypt(&i, NULL, NULL);
#ifdef DEBUG
    hexdump("decrypted data:", p_data, n);
#endif
  }

  return n;                                   /* higher layer does retries */
}
