/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
RCSID("$Id: otptest.c,v 1.6 1999/12/02 16:58:45 joda Exp $");
#endif

#include <stdio.h>
#include <string.h>
#include <otp.h>

static int
test_one(OtpKey key1, char *name, char *val,
	 void (*print)(OtpKey,char*, size_t),
	 OtpAlgorithm *alg)
{
  char buf[256];
  OtpKey key2;

  (*print)(key1, buf, sizeof(buf));
  printf ("%s: %s, ", name, buf);
  if (strcmp (buf, val) != 0) {
    printf ("failed(*%s* != *%s*)\n", buf, val);
    return 1;
  }
  if (otp_parse (key2, buf, alg)) {
    printf ("parse of %s failed\n", name);
    return 1;
  }
  if (memcmp (key1, key2, OTPKEYSIZE) != 0) {
    printf ("key1 != key2, ");
  }
  printf ("success\n");
  return 0;
}

static int
test (void)
{
  struct test {
    char *alg;
    char *passphrase;
    char *seed;
    int count;
    char *hex;
    char *word;
  } tests[] = {

    /* md4 */
    {"md4", "This is a test.", "TeSt", 0, "d1854218ebbb0b51", "ROME MUG FRED SCAN LIVE LACE"},
    {"md4", "This is a test.", "TeSt", 1, "63473ef01cd0b444", "CARD SAD MINI RYE COL KIN"},
    {"md4", "This is a test.", "TeSt", 99, "c5e612776e6c237a", "NOTE OUT IBIS SINK NAVE MODE"},
    {"md4", "AbCdEfGhIjK", "alpha1", 0, "50076f47eb1ade4e", "AWAY SEN ROOK SALT LICE MAP"},
    {"md4", "AbCdEfGhIjK", "alpha1", 1, "65d20d1949b5f7ab", "CHEW GRIM WU HANG BUCK SAID"},
    {"md4", "AbCdEfGhIjK", "alpha1", 99, "d150c82cce6f62d1", "ROIL FREE COG HUNK WAIT COCA"},
    {"md4", "OTP's are good", "correct", 0, "849c79d4f6f55388", "FOOL STEM DONE TOOL BECK NILE"},
    {"md4", "OTP's are good", "correct", 1, "8c0992fb250847b1", "GIST AMOS MOOT AIDS FOOD SEEM"},
    {"md4", "OTP's are good", "correct",99, "3f3bf4b4145fd74b", "TAG SLOW NOV MIN WOOL KENO"},


    /* md5 */
    {"md5", "This is a test.", "TeSt", 0, "9e876134d90499dd", "INCH SEA ANNE LONG AHEM TOUR"},
    {"md5", "This is a test.", "TeSt", 1, "7965e05436f5029f", "EASE OIL FUM CURE AWRY AVIS"},
    {"md5", "This is a test.", "TeSt", 99, "50fe1962c4965880", "BAIL TUFT BITS GANG CHEF THY"},
    {"md5", "AbCdEfGhIjK", "alpha1",  0, "87066dd9644bf206", "FULL PEW DOWN ONCE MORT ARC"},
    {"md5", "AbCdEfGhIjK", "alpha1",   1, "7cd34c1040add14b", "FACT HOOF AT FIST SITE KENT"},
    {"md5", "AbCdEfGhIjK", "alpha1",  99, "5aa37a81f212146c", "BODE HOP JAKE STOW JUT RAP"},
    {"md5", "OTP's are good", "correct", 0,  "f205753943de4cf9", "ULAN NEW ARMY FUSE SUIT EYED"},
    {"md5", "OTP's are good", "correct", 1, "ddcdac956f234937", "SKIM CULT LOB SLAM POE HOWL"},
    {"md5", "OTP's are good", "correct",99, "b203e28fa525be47", "LONG IVY JULY AJAR BOND LEE"},

    /* sha */
    {"sha", "This is a test.", "TeSt", 0, "bb9e6ae1979d8ff4", "MILT VARY MAST OK SEES WENT"},
    {"sha", "This is a test.", "TeSt", 1, "63d936639734385b", "CART OTTO HIVE ODE VAT NUT"},
    {"sha", "This is a test.", "TeSt", 99, "87fec7768b73ccf9", "GAFF WAIT SKID GIG SKY EYED"},
    {"sha", "AbCdEfGhIjK", "alpha1", 0, "ad85f658ebe383c9", "LEST OR HEEL SCOT ROB SUIT"},
    {"sha", "AbCdEfGhIjK", "alpha1", 1, "d07ce229b5cf119b", "RITE TAKE GELD COST TUNE RECK"},
    {"sha", "AbCdEfGhIjK", "alpha1", 99, "27bc71035aaf3dc6", "MAY STAR TIN LYON VEDA STAN"},
    {"sha", "OTP's are good", "correct", 0, "d51f3e99bf8e6f0b", "RUST WELT KICK FELL TAIL FRAU"},
    {"sha", "OTP's are good", "correct", 1, "82aeb52d943774e4", "FLIT DOSE ALSO MEW DRUM DEFY"},
    {"sha", "OTP's are good", "correct", 99, "4f296a74fe1567ec", "AURA ALOE HURL WING BERG WAIT"},
    {NULL}
  };

  struct test *t;
  int sum = 0;

  for(t = tests; t->alg; ++t) {
    int i;
    OtpAlgorithm *alg = otp_find_alg (t->alg);
    OtpKey key;

    if (alg == NULL) {
      printf ("Could not find alg %s\n", t->alg);
      return 1;
    }
    if(alg->init (key, t->passphrase, t->seed))
      return 1;
    for (i = 0; i < t->count; ++i) {
      if (alg->next (key))
	return 1;
    }
    sum += test_one (key, "hexadecimal", t->hex, otp_print_hex,
		     alg) +
      test_one (key, "standard_word", t->word, otp_print_stddict, alg);
  }
  return sum;
}

int
main (void)
{
  return test ();
}
