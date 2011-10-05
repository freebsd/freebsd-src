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
RCSID("$Id$");
#endif

#include "otp_locl.h"

extern const char *const std_dict[];

unsigned
otp_checksum (OtpKey key)
{
  int i;
  unsigned sum = 0;

  for (i = 0; i < OTPKEYSIZE; ++i)
    sum += ((key[i] >> 0) & 0x03)
      + ((key[i] >> 2) & 0x03)
      + ((key[i] >> 4) & 0x03)
      + ((key[i] >> 6) & 0x03);
  sum &= 0x03;
  return sum;
}

void
otp_print_stddict (OtpKey key, char *str, size_t sz)
{
  unsigned sum;

  sum = otp_checksum (key);
  snprintf (str, sz,
	    "%s %s %s %s %s %s",
	    std_dict[(key[0] << 3) | (key[1] >> 5)],
	    std_dict[((key[1] & 0x1F) << 6) | (key[2] >> 2)],
	    std_dict[((key[2] & 0x03) << 9) | (key[3] << 1) | (key[4] >> 7)],
	    std_dict[((key[4] & 0x7F) << 4) | (key[5] >> 4)],
	    std_dict[((key[5] & 0x0F) << 7) | (key[6] >> 1)],
	    std_dict[((key[6] & 0x01) << 10) | (key[7] << 2) | sum]);
}

void
otp_print_hex (OtpKey key, char *str, size_t sz)
{
  snprintf (str, sz,
	    "%02x%02x%02x%02x%02x%02x%02x%02x",
	    key[0], key[1], key[2], key[3],
	    key[4], key[5], key[6], key[7]);
}

void
otp_print_hex_extended (OtpKey key, char *str, size_t sz)
{
  strlcpy (str, OTP_HEXPREFIX, sz);
  otp_print_hex (key,
		 str + strlen(OTP_HEXPREFIX),
		 sz - strlen(OTP_HEXPREFIX));
}

void
otp_print_stddict_extended (OtpKey key, char *str, size_t sz)
{
  strlcpy (str, OTP_WORDPREFIX, sz);
  otp_print_stddict (key,
		     str + strlen(OTP_WORDPREFIX),
		     sz - strlen(OTP_WORDPREFIX));
}
