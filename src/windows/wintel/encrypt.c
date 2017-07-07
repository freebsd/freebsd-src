/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* based on @(#)encrypt.c	8.1 (Berkeley) 6/4/93 */

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifdef	ENCRYPTION

#include <stdio.h>

#define isprefix(a, b)   (!strncmp((a), (b), strlen(b)))

#ifdef KRB4
#include <windows.h>
#include <time.h>
#include <string.h>
#include "winsock.h"
#include "kerberos.h"
#endif
#ifdef KRB5
#include <time.h>
#include <string.h>
#include "krb5.h"
#include "com_err.h"
#endif

#include "telnet.h"
#include "encrypt.h"

#define ENCRYPT_NAMES
#include "telnet_arpa.h"

/*
 * These function pointers point to the current routines
 * for encrypting and decrypting data.
 */
void	(*encrypt_output) (unsigned char *, int);
int	(*decrypt_input) (int);

#ifdef DEBUG
int encrypt_debug_mode = 1;
int encrypt_verbose = 1;
#else
int encrypt_verbose = 0;
#endif

static char dbgbuf [10240];

static int decrypt_mode = 0;
static int encrypt_mode = 0;
static int autoencrypt = 1;
static int autodecrypt = 1;
static int havesessionkey = 0;

kstream EncryptKSGlobalHack = NULL;

#define	typemask(x)	((x) > 0 ? 1 << ((x)-1) : 0)

static long i_support_encrypt =
	typemask(ENCTYPE_DES_CFB64) | typemask(ENCTYPE_DES_OFB64);
static long i_support_decrypt =
	typemask(ENCTYPE_DES_CFB64) | typemask(ENCTYPE_DES_OFB64);
static long i_wont_support_encrypt = 0;
static long i_wont_support_decrypt = 0;
#define	I_SUPPORT_ENCRYPT	(i_support_encrypt & ~i_wont_support_encrypt)
#define	I_SUPPORT_DECRYPT	(i_support_decrypt & ~i_wont_support_decrypt)

static long remote_supports_encrypt = 0;
static long remote_supports_decrypt = 0;

static Encryptions encryptions[] = {
  { "DES_CFB64",
    ENCTYPE_DES_CFB64,
    cfb64_encrypt,
    cfb64_decrypt,
    cfb64_init,
    cfb64_start,
    cfb64_is,
    cfb64_reply,
    cfb64_session,
    cfb64_keyid,
    NULL },
  { "DES_OFB64",
    ENCTYPE_DES_OFB64,
    ofb64_encrypt,
    ofb64_decrypt,
    ofb64_init,
    ofb64_start,
    ofb64_is,
    ofb64_reply,
    ofb64_session,
    ofb64_keyid,
    NULL },
  { 0, },
};

static unsigned char str_send[64] = { IAC, SB, TELOPT_ENCRYPT,
				      ENCRYPT_SUPPORT };
static unsigned char str_suplen = 0;
static unsigned char str_start[72] = { IAC, SB, TELOPT_ENCRYPT };
static unsigned char str_end[] = { IAC, SB, TELOPT_ENCRYPT, 0, IAC, SE };

void encrypt_request_end(void);
void encrypt_request_start(unsigned char *, int);
void encrypt_enc_keyid(unsigned char *, int);
void encrypt_dec_keyid(unsigned char *, int);
void encrypt_support(unsigned char *, int);
void encrypt_start(unsigned char *, int);
void encrypt_end(void);

int encrypt_ks_stream(struct kstream_data_block *, /* output */
		      struct kstream_data_block *, /* input */
		      struct kstream *);

int decrypt_ks_stream(struct kstream_data_block *, /* output */
		       struct kstream_data_block *, /* input */
		       struct kstream *);

int
encrypt_ks_stream(struct kstream_data_block *i,
		  struct kstream_data_block *o,
		  struct kstream *ks)
{

  /*
   * this is really quite bogus, since it does an in-place encryption...
   */
  if (encrypt_output) {
    encrypt_output(i->ptr, i->length);
    return 1;
  }

  return 0;
}


int
decrypt_ks_stream(struct kstream_data_block *i,
		  struct kstream_data_block *o,
		  struct kstream *ks)
{
  unsigned int len;
  /*
   * this is really quite bogus, since it does an in-place decryption...
   */
  if (decrypt_input) {
    for (len = 0 ; len < i->length ; len++)
      ((unsigned char *)i->ptr)[len]
	= decrypt_input(((unsigned char *)i->ptr)[len]);
    return 1;
  }

  return 0;
}

int
decrypt_ks_hack(unsigned char *buf, int cnt)
{
  int len;
  /*
   * this is really quite bogus, since it does an in-place decryption...
   */
  for (len = 0 ; len < cnt ; len++)
      buf[len] = decrypt_input(buf[len]);

#ifdef DEBUG
  hexdump("hack:", buf, cnt);
#endif
  return 1;
}

#ifdef DEBUG
int
printsub(char c, unsigned char *s, size_t len)
{
  size_t i;
  char *p = dbgbuf;

  *p++ = c;

  for (i = 0 ; (i < len) && (p - dbgbuf + 3 < sizeof(dbgbuf)) ; i++)
    p += sprintf(p, "%02x ", s[i]);
  dbgbuf[sizeof(dbgbuf) - 1] = '\0';

  strncat(p, "\n", sizeof(dbgbuf) - 1 - (p - dbgbuf));

  OutputDebugString(dbgbuf);

  return 0;
}
#endif

/*
 * parsedat[0] == the suboption we might be negoating,
 */
void
encrypt_parse(kstream ks, unsigned char *parsedat, int end_sub)
{
  char *p = dbgbuf;

#ifdef DEBUG
  printsub('<', parsedat, end_sub);
#endif

  switch(parsedat[1]) {
  case ENCRYPT_START:
    encrypt_start(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_END:
    encrypt_end();
    break;
  case ENCRYPT_SUPPORT:
    encrypt_support(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_REQSTART:
    encrypt_request_start(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_REQEND:
    /*
     * We can always send an REQEND so that we cannot
     * get stuck encrypting.  We should only get this
     * if we have been able to get in the correct mode
     * anyhow.
     */
    encrypt_request_end();
    break;
  case ENCRYPT_IS:
    encrypt_is(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_REPLY:
    encrypt_reply(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_ENC_KEYID:
    encrypt_enc_keyid(parsedat + 2, end_sub - 2);
    break;
  case ENCRYPT_DEC_KEYID:
    encrypt_dec_keyid(parsedat + 2, end_sub - 2);
    break;
  default:
    break;
  }
}

/* XXX */
Encryptions *
findencryption(type)
     int type;
{
  Encryptions *ep = encryptions;

  if (!(I_SUPPORT_ENCRYPT & remote_supports_decrypt & typemask(type)))
    return(0);
  while (ep->type && ep->type != type)
    ++ep;
  return(ep->type ? ep : 0);
}

Encryptions *
finddecryption(int type)
{
  Encryptions *ep = encryptions;

  if (!(I_SUPPORT_DECRYPT & remote_supports_encrypt & typemask(type)))
    return(0);
  while (ep->type && ep->type != type)
    ++ep;
  return(ep->type ? ep : 0);
}

#define	MAXKEYLEN 64

static struct key_info {
  unsigned char keyid[MAXKEYLEN];
  int keylen;
  int dir;
  int *modep;
  Encryptions *(*getcrypt)();
} ki[2] = {
  { { 0 }, 0, DIR_ENCRYPT, &encrypt_mode, findencryption },
  { { 0 }, 0, DIR_DECRYPT, &decrypt_mode, finddecryption },
};

void
encrypt_init(kstream iks, kstream_ptr data)
{
  Encryptions *ep = encryptions;

  i_support_encrypt = i_support_decrypt = 0;
  remote_supports_encrypt = remote_supports_decrypt = 0;
  encrypt_mode = 0;
  decrypt_mode = 0;
  encrypt_output = NULL;
  decrypt_input = NULL;

  str_suplen = 4;

  EncryptKSGlobalHack = iks;

  while (ep->type) {
#ifdef DEBUG
	  if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>I will support %s\n",
	      ENCTYPE_NAME(ep->type));
      OutputDebugString(dbgbuf);
    }
#endif
    i_support_encrypt |= typemask(ep->type);
    i_support_decrypt |= typemask(ep->type);
    if ((i_wont_support_decrypt & typemask(ep->type)) == 0)
      if ((str_send[str_suplen++] = ep->type) == IAC)
	str_send[str_suplen++] = IAC;
    if (ep->init)
      (*ep->init)(0);
    ++ep;
  }
  str_send[str_suplen++] = IAC;
  str_send[str_suplen++] = SE;
}

void
encrypt_send_support()
{
  if (str_suplen) {
    /*
     * If the user has requested that decryption start
     * immediatly, then send a "REQUEST START" before
     * we negotiate the type.
     */
    if (autodecrypt)
      encrypt_send_request_start();
    TelnetSend(EncryptKSGlobalHack, str_send, str_suplen, 0);

#ifdef DEBUG
    printsub('>', &str_send[2], str_suplen - 2);
#endif

    str_suplen = 0;
  }
}

/*
 * Called when ENCRYPT SUPPORT is received.
 */
void
encrypt_support(typelist, cnt)
     unsigned char *typelist;
     int cnt;
{
  register int type, use_type = 0;
  Encryptions *ep;

  /*
   * Forget anything the other side has previously told us.
   */
  remote_supports_decrypt = 0;

  while (cnt-- > 0) {
    type = *typelist++;
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>Remote supports %s (%d)\n",
	      ENCTYPE_NAME(type), type);
      OutputDebugString(dbgbuf);
    }
#endif
    if ((type < ENCTYPE_CNT) &&
	(I_SUPPORT_ENCRYPT & typemask(type))) {
      remote_supports_decrypt |= typemask(type);
      if (use_type == 0)
	use_type = type;
    }
  }
  if (use_type) {
    ep = findencryption(use_type);
    if (!ep)
      return;
    type = ep->start ? (*ep->start)(DIR_ENCRYPT, 0) : 0;
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>(*ep->start)() %s returned %d (%s)\n",
	      ENCTYPE_NAME(use_type), type, ENCRYPT_NAME(type));
      OutputDebugString(dbgbuf);
    }
#endif
    if (type < 0)
      return;
    encrypt_mode = use_type;
    if (type == 0)
      encrypt_start_output(use_type);
  }
}

void
encrypt_is(data, cnt)
     unsigned char *data;
     int cnt;
{
  Encryptions *ep;
  register int type, ret;

  if (--cnt < 0)
    return;
  type = *data++;
  if (type < ENCTYPE_CNT)
    remote_supports_encrypt |= typemask(type);
  if (!(ep = finddecryption(type))) {
#ifdef DEBUG
	  if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>encrypt_reply:  "
	      "Can't find type %s (%d) for initial negotiation\n",
	      ENCTYPE_NAME_OK(type)
	      ? ENCTYPE_NAME(type) : "(unknown)",
	      type);
      OutputDebugString(dbgbuf);
    }
#endif
    return;
  }
  if (!ep->is) {
#ifdef DEBUG
	  if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>encrypt_reply:  "
	      "No initial negotiation needed for type %s (%d)\n",
	      ENCTYPE_NAME_OK(type)
	      ? ENCTYPE_NAME(type) : "(unknown)",
	      type);
      OutputDebugString(dbgbuf);
    }
#endif
    ret = 0;
  } else {
    ret = (*ep->is)(data, cnt);
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, "encrypt_reply:  "
	      "(*ep->is)(%x, %d) returned %s(%d)\n", data, cnt,
	      (ret < 0) ? "FAIL " :
	      (ret == 0) ? "SUCCESS " : "MORE_TO_DO ", ret);
      OutputDebugString(dbgbuf);
    }
#endif
  }
  if (ret < 0) {
    autodecrypt = 0;
  } else {
    decrypt_mode = type;
    if (ret == 0 && autodecrypt)
      encrypt_send_request_start();
  }
}

void
encrypt_reply(data, cnt)
     unsigned char *data;
     int cnt;
{
  Encryptions *ep;
  register int ret, type;

  if (--cnt < 0)
    return;
  type = *data++;
  if (!(ep = findencryption(type))) {
#ifdef DEBUG
	  if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>Can't find type %s (%d) for initial negotiation\n",
	      ENCTYPE_NAME_OK(type)
	      ? ENCTYPE_NAME(type) : "(unknown)",
	      type);
      OutputDebugString(dbgbuf);
    }
#endif
    return;
  }
  if (!ep->reply) {
#ifdef DEBUG
	  if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>No initial negotiation needed for type %s (%d)\n",
	      ENCTYPE_NAME_OK(type)
	      ? ENCTYPE_NAME(type) : "(unknown)",
	      type);
      OutputDebugString(dbgbuf);
    }
#endif
    ret = 0;
  } else {
    ret = (*ep->reply)(data, cnt);
#ifdef DEBUG
    if (encrypt_debug_mode) {
      sprintf(dbgbuf, "(*ep->reply)(%x, %d) returned %s(%d)\n",
	      data, cnt,
	      (ret < 0) ? "FAIL " :
	      (ret == 0) ? "SUCCESS " : "MORE_TO_DO ", ret);
      OutputDebugString(dbgbuf);
    }
#endif
  }
#ifdef DEBUG
  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>encrypt_reply returned %d\n", ret);
    OutputDebugString(dbgbuf);
  }
#endif
  if (ret < 0) {
    autoencrypt = 0;
  } else {
    encrypt_mode = type;
    if (ret == 0 && autoencrypt)
      encrypt_start_output(type);
  }
}

/*
 * Called when a ENCRYPT START command is received.
 */
void
encrypt_start(data, cnt)
     unsigned char *data;
     int cnt;
{
  Encryptions *ep;

  if (!decrypt_mode) {
    /*
     * Something is wrong.  We should not get a START
     * command without having already picked our
     * decryption scheme.  Send a REQUEST-END to
     * attempt to clear the channel...
     */
    /* printf("Warning, Cannot decrypt input stream!!!\n"); */
    encrypt_send_request_end();
    MessageBox(NULL, "Warning, Cannot decrypt input stream!!!", NULL,
	       MB_OK | MB_ICONEXCLAMATION);
    return;
  }

  if (ep = finddecryption(decrypt_mode)) {
	extern BOOL encrypt_flag;

    decrypt_input = ep->input;
	EncryptKSGlobalHack->decrypt = decrypt_ks_stream;
	encrypt_flag = 2;  /* XXX hack */

    if (encrypt_verbose) {
      sprintf(dbgbuf, "[ Input is now decrypted with type %s ]\n",
	      ENCTYPE_NAME(decrypt_mode));
      OutputDebugString(dbgbuf);
    }
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>Start to decrypt input with type %s\n",
	      ENCTYPE_NAME(decrypt_mode));
      OutputDebugString(dbgbuf);
    }
#endif
  } else {
    char buf[1024];
    wsprintf(buf, "Warning, Cannot decrypt type %s (%d)!!!",
	    ENCTYPE_NAME_OK(decrypt_mode)
	    ? ENCTYPE_NAME(decrypt_mode) : "(unknown)",
	    decrypt_mode);
    MessageBox(NULL, buf, NULL, MB_OK | MB_ICONEXCLAMATION);
    encrypt_send_request_end();
  }
}

void
encrypt_session_key(key, server)
     Session_Key *key;
     int server;
{
  Encryptions *ep = encryptions;

  havesessionkey = 1;

  while (ep->type) {
    if (ep->session)
      (*ep->session)(key, server);
#if defined(notdef)
    if (!encrypt_output && autoencrypt && !server)
      encrypt_start_output(ep->type);
    if (!decrypt_input && autodecrypt && !server)
      encrypt_send_request_start();
#endif
    ++ep;
  }
}

/*
 * Called when ENCRYPT END is received.
 */
void
encrypt_end()
{
  decrypt_input = NULL;
  EncryptKSGlobalHack->decrypt = NULL;
#ifdef DEBUG
  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>Input is back to clear text\n");
    OutputDebugString(dbgbuf);
  }
#endif
  if (encrypt_verbose) {
      sprintf(dbgbuf, "[ Input is now clear text ]\n");
      OutputDebugString(dbgbuf);
  }
}

/*
 * Called when ENCRYPT REQUEST-END is received.
 */
void
encrypt_request_end()
{
  encrypt_send_end();
}

/*
 * Called when ENCRYPT REQUEST-START is received.  If we receive
 * this before a type is picked, then that indicates that the
 * other side wants us to start encrypting data as soon as we
 * can.
 */
void
encrypt_request_start(data, cnt)
     unsigned char *data;
     int cnt;
{
  if (encrypt_mode == 0)  {
    return;
  }
  encrypt_start_output(encrypt_mode);
}

static unsigned char str_keyid[(MAXKEYLEN*2)+5] = { IAC, SB, TELOPT_ENCRYPT };

void
encrypt_keyid();

void
encrypt_enc_keyid(keyid, len)
     unsigned char *keyid;
     int len;
{
  encrypt_keyid(&ki[1], keyid, len);
}

void
encrypt_dec_keyid(keyid, len)
     unsigned char *keyid;
     int len;
{
  encrypt_keyid(&ki[0], keyid, len);
}

void
encrypt_keyid(kp, keyid, len)
     struct key_info *kp;
     unsigned char *keyid;
     int len;
{
  Encryptions *ep;
  int dir = kp->dir;
  register int ret = 0;

  if (!(ep = (*kp->getcrypt)(*kp->modep))) {
    if (len == 0)
      return;
    kp->keylen = 0;
  } else if (len == 0) {
    /*
     * Empty option, indicates a failure.
     */
    if (kp->keylen == 0)
      return;
    kp->keylen = 0;
    if (ep->keyid)
      (void)(*ep->keyid)(dir, kp->keyid, &kp->keylen);

  } else if ((len != kp->keylen) || (memcmp(keyid, kp->keyid, len) != 0)) {
    /*
     * Length or contents are different
     */
    kp->keylen = len;
    memcpy(kp->keyid, keyid, len);
    if (ep->keyid)
      (void)(*ep->keyid)(dir, kp->keyid, &kp->keylen);
  } else {
    if (ep->keyid)
      ret = (*ep->keyid)(dir, kp->keyid, &kp->keylen);
    if ((ret == 0) && (dir == DIR_ENCRYPT) && autoencrypt)
      encrypt_start_output(*kp->modep);
    return;
  }

  encrypt_send_keyid(dir, kp->keyid, kp->keylen, 0);
}

void
encrypt_send_keyid(dir, keyid, keylen, saveit)
     int dir;
     unsigned char *keyid;
     int keylen;
     int saveit;
{
  unsigned char *strp;

  str_keyid[3] = (dir == DIR_ENCRYPT)
    ? ENCRYPT_ENC_KEYID : ENCRYPT_DEC_KEYID;
  if (saveit) {
    struct key_info *kp = &ki[(dir == DIR_ENCRYPT) ? 0 : 1];
    memcpy(kp->keyid, keyid, keylen);
    kp->keylen = keylen;
  }

  for (strp = &str_keyid[4]; keylen > 0; --keylen) {
    if ((*strp++ = *keyid++) == IAC)
      *strp++ = IAC;
  }
  *strp++ = IAC;
  *strp++ = SE;
  TelnetSend(EncryptKSGlobalHack, str_keyid, strp - str_keyid, 0);

#ifdef DEBUG
  printsub('>', &str_keyid[2], strp - str_keyid - 2);
#endif

}

void
encrypt_auto(on)
     int on;
{
  if (on < 0)
    autoencrypt ^= 1;
  else
    autoencrypt = on ? 1 : 0;
}

void
decrypt_auto(on)
     int on;
{
  if (on < 0)
    autodecrypt ^= 1;
  else
    autodecrypt = on ? 1 : 0;
}

void
encrypt_start_output(type)
     int type;
{
  Encryptions *ep;
  register unsigned char *p;
  register int i;

  if (!(ep = findencryption(type))) {
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>Can't encrypt with type %s (%d)\n",
	      ENCTYPE_NAME_OK(type)
	      ? ENCTYPE_NAME(type) : "(unknown)",
	      type);
      OutputDebugString(dbgbuf);
    }
#endif
    return;
  }
  if (ep->start) {
    i = (*ep->start)(DIR_ENCRYPT, 0);
#ifdef DEBUG
	if (encrypt_debug_mode) {
      sprintf(dbgbuf, ">>>Encrypt start: %s (%d) %s\n",
	      (i < 0) ? "failed" :
	      "initial negotiation in progress",
	      i, ENCTYPE_NAME(type));
      OutputDebugString(dbgbuf);
    }
#endif
    if (i)
      return;
  }
  p = str_start + 3;
  *p++ = ENCRYPT_START;
  for (i = 0; i < ki[0].keylen; ++i) {
    if ((*p++ = ki[0].keyid[i]) == IAC)
      *p++ = IAC;
  }
  *p++ = IAC;
  *p++ = SE;
  TelnetSend(EncryptKSGlobalHack, str_start, p - str_start, 0);
#ifdef DEBUG
  printsub('>', &str_start[2], p - &str_start[2]);
#endif

  /*
   * If we are already encrypting in some mode, then
   * encrypt the ring (which includes our request) in
   * the old mode, mark it all as "clear text" and then
   * switch to the new mode.
   */
  encrypt_output = ep->output;
  EncryptKSGlobalHack->encrypt = encrypt_ks_stream;
  encrypt_mode = type;
#ifdef DEBUG
  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>Started to encrypt output with type %s\n",
	    ENCTYPE_NAME(type));
    OutputDebugString(dbgbuf);
  }
#endif
  if (encrypt_verbose) {
    sprintf(dbgbuf, "[ Output is now encrypted with type %s ]\n",
	   ENCTYPE_NAME(type));
    OutputDebugString(dbgbuf);
  }
}

void
encrypt_send_end()
{
  if (!encrypt_output)
    return;

  str_end[3] = ENCRYPT_END;
  TelnetSend(EncryptKSGlobalHack, str_end, sizeof(str_end), 0);
#ifdef DEBUG
  printsub('>', &str_end[2], sizeof(str_end) - 2);
#endif

  /*
   * Encrypt the output buffer now because it will not be done by
   * netflush...
   */
  encrypt_output = 0;
  EncryptKSGlobalHack->encrypt = NULL;
#ifdef DEBUG
  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>Output is back to clear text\n");
    OutputDebugString(dbgbuf);
  }
#endif
  if (encrypt_verbose) {
      sprintf(dbgbuf, "[ Output is now clear text ]\n");
      OutputDebugString(dbgbuf);
  }
}

void
encrypt_send_request_start()
{
  register unsigned char *p;
  register int i;

  p = &str_start[3];
  *p++ = ENCRYPT_REQSTART;
  for (i = 0; i < ki[1].keylen; ++i) {
    if ((*p++ = ki[1].keyid[i]) == IAC)
      *p++ = IAC;
  }
  *p++ = IAC;
  *p++ = SE;
  TelnetSend(EncryptKSGlobalHack, str_start, p - str_start, 0);
#ifdef DEBUG
  printsub('>', &str_start[2], p - &str_start[2]);

  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>Request input to be encrypted\n");
    OutputDebugString(dbgbuf);
  }
#endif
}

void
encrypt_send_request_end()
{
  str_end[3] = ENCRYPT_REQEND;
  TelnetSend(EncryptKSGlobalHack, str_end, sizeof(str_end), 0);
#ifdef DEBUG
  printsub('>', &str_end[2], sizeof(str_end) - 2);

  if (encrypt_debug_mode) {
    sprintf(dbgbuf, ">>>Request input to be clear text\n");
    OutputDebugString(dbgbuf);
  }
#endif
}

int encrypt_is_encrypting()
{
  if (encrypt_output && decrypt_input)
    return 1;
  return 0;
}

#ifdef DEBUG
void
encrypt_debug(mode)
     int mode;
{
  encrypt_debug_mode = mode;
}
#endif

#if 0
void
encrypt_gen_printsub(data, cnt, buf, buflen)
     unsigned char *data, *buf;
     int cnt, buflen;
{
  char tbuf[16], *cp;

  cnt -= 2;
  data += 2;
  buf[buflen-1] = '\0';
  buf[buflen-2] = '*';
  buflen -= 2;;
  for (; cnt > 0; cnt--, data++) {
    sprintf(tbuf, " %d", *data);
    for (cp = tbuf; *cp && buflen > 0; --buflen)
      *buf++ = *cp++;
    if (buflen <= 0)
      return;
  }
  *buf = '\0';
}

void
encrypt_printsub(data, cnt, buf, buflen)
     unsigned char *data, *buf;
     int cnt, buflen;
{
  Encryptions *ep;
  register int type = data[1];

  for (ep = encryptions; ep->type && ep->type != type; ep++)
    ;

  if (ep->printsub)
    (*ep->printsub)(data, cnt, buf, buflen);
  else
    encrypt_gen_printsub(data, cnt, buf, buflen);
}
#endif

#endif	/* ENCRYPTION */
