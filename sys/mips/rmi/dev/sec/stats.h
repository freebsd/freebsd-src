/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RMI_BSD */

#ifndef _STATS_H_
#define _STATS_H_

typedef struct hmac_stats
{
  unsigned long md5_count;
  unsigned long long md5_bytes;
  unsigned long sha1_count;
  unsigned long long sha1_bytes;
  unsigned long sha256_count;
  unsigned long long sha256_bytes;
  unsigned long sha384_count;
  unsigned long long sha384_bytes;
  unsigned long sha512_count;
  unsigned long long sha512_bytes;
  unsigned long gcm_count;
  unsigned long long gcm_bytes;
  unsigned long kasumi_f9_count;
  unsigned long long kasumi_f9_bytes;
  unsigned long reverts;
  unsigned long long reverts_bytes;
} hmac_stats_t, *hmac_stats_pt;

typedef struct cipher_stats
{
  unsigned long des_encrypts;
  unsigned long long des_encrypt_bytes;
  unsigned long des_decrypts;
  unsigned long long des_decrypt_bytes;
  unsigned long des3_encrypts;
  unsigned long long des3_encrypt_bytes;
  unsigned long des3_decrypts;
  unsigned long long des3_decrypt_bytes;
  unsigned long aes_encrypts;
  unsigned long long aes_encrypt_bytes;
  unsigned long aes_decrypts;
  unsigned long long aes_decrypt_bytes;
  unsigned long arc4_encrypts;
  unsigned long long arc4_encrypt_bytes;
  unsigned long arc4_decrypts;
  unsigned long long arc4_decrypt_bytes;
  unsigned long kasumi_f8_encrypts;
  unsigned long long kasumi_f8_encrypt_bytes;
  unsigned long kasumi_f8_decrypts;
  unsigned long long kasumi_f8_decrypt_bytes;
  unsigned long reverts;
  unsigned long long reverts_bytes;
} cipher_stats_t, *cipher_stats_pt;


typedef struct modexp_stats
{
  unsigned long modexp_512s;
  unsigned long modexp_1024s;
} modexp_stats_t, *modexp_stats_pt;

typedef struct ecc_stats
{
  unsigned long ecc_mul;
  unsigned long ecc_add;
  unsigned long ecc_dbl;
  unsigned long ecc_vfy;
  unsigned long ecc_bin_mul;
  unsigned long ecc_field_bin_inv;
  unsigned long ecc_field_bin_mul;
  unsigned long ecc_field_bin_add;
  unsigned long ecc_field_add;
  unsigned long ecc_field_sub;
  unsigned long ecc_field_mul;
  unsigned long ecc_field_inv;
  unsigned long ecc_field_div;
  unsigned long ecc_field_red;
} ecc_stats_t, *ecc_stats_pt;


typedef struct opt_stats
{
  unsigned long combined;
  unsigned long unaligned_auth_dest;
  unsigned long sym_failed;
  unsigned long modexp_failed;
  unsigned long ecc_failed;
} opt_stats_t, *opt_stats_pt;

typedef struct rmisec_stats
{
  uint32_t sent;
  uint32_t received;
  uint32_t stats_mask;
  uint32_t control_mask;
  rwlock_t rmisec_control_lock;
  rwlock_t rmisec_stats_lock;
  char clear_start[0];
  uint64_t wait_time;
  uint32_t max_wait_time;
  uint32_t maxsnd_wait_time;
  uint32_t wait_count;
  hmac_stats_t hmac;
  cipher_stats_t cipher;
  modexp_stats_t modexp;
  ecc_stats_t ecc;
  opt_stats_t opt;
} rmisec_stats_t, *rmisec_stats_pt;


/* stats routines */

static void inline phxdrv_record_sent(rmisec_stats_pt stats)
{
  write_lock(&stats->rmisec_stats_lock);
  stats->sent++;
  write_unlock(&stats->rmisec_stats_lock);
}

static void inline phxdrv_record_received(rmisec_stats_pt stats)
{
  write_lock(&stats->rmisec_stats_lock);
  stats->received++;
  write_unlock(&stats->rmisec_stats_lock);
}


static void inline phxdrv_record_des(rmisec_stats_pt stats, int enc,
                     int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_DES) {
    write_lock(&stats->rmisec_stats_lock);
    if (enc) {
      stats->cipher.des_encrypts++;
      stats->cipher.des_encrypt_bytes += nbytes;
    }
    else {
      stats->cipher.des_decrypts++;
      stats->cipher.des_decrypt_bytes += nbytes;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_3des(rmisec_stats_pt stats, int enc,
                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_3DES) {
    write_lock(&stats->rmisec_stats_lock);
    if (enc) {
      stats->cipher.des3_encrypts++;
      stats->cipher.des3_encrypt_bytes += nbytes;
    }
    else {
      stats->cipher.des3_decrypts++;
      stats->cipher.des3_decrypt_bytes += nbytes;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_aes(rmisec_stats_pt stats, int enc,
                     int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_AES) {
    write_lock(&stats->rmisec_stats_lock);
    if (enc) {
      stats->cipher.aes_encrypts++;
      stats->cipher.aes_encrypt_bytes += nbytes;
    }
    else {
      stats->cipher.aes_decrypts++;
      stats->cipher.aes_decrypt_bytes += nbytes;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_arc4(rmisec_stats_pt stats, int enc,
                     int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_ARC4) {
    write_lock(&stats->rmisec_stats_lock);
    if (enc) {
      stats->cipher.arc4_encrypts++;
      stats->cipher.arc4_encrypt_bytes += nbytes;
    }
    else {
      stats->cipher.arc4_decrypts++;
      stats->cipher.arc4_decrypt_bytes += nbytes;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_kasumi_f8(rmisec_stats_pt stats, int enc,
                                     int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_KASUMI_F8) {
    write_lock(&stats->rmisec_stats_lock);
    if (enc) {
      stats->cipher.kasumi_f8_encrypts++;
      stats->cipher.kasumi_f8_encrypt_bytes += nbytes;
    }
    else {
      stats->cipher.kasumi_f8_decrypts++;
      stats->cipher.kasumi_f8_decrypt_bytes += nbytes;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_modexp(rmisec_stats_pt stats,
                        int blksize)
{
  if (stats->stats_mask & PHXDRV_PROFILE_MODEXP) {
    write_lock(&stats->rmisec_stats_lock);
    if (blksize == 512) {
      stats->modexp.modexp_512s++;
    }
    if (blksize == 1024) {
      stats->modexp.modexp_1024s++;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_ecc(rmisec_stats_pt stats, PHX_ECC_OP op)
{
  if (stats->stats_mask & PHXDRV_PROFILE_ECC) {
    write_lock(&stats->rmisec_stats_lock);
    switch (op) {
    case PHX_ECC_NOP:
        break;
    case PHX_ECC_MUL:
        stats->ecc.ecc_mul++;
        break;
    case PHX_ECC_BIN_MUL:
        stats->ecc.ecc_bin_mul++;
        break;
    case PHX_ECC_ADD:
        stats->ecc.ecc_add++;
        break;
    case PHX_ECC_DBL:
        stats->ecc.ecc_dbl++;
        break;
    case PHX_ECC_VFY:
        stats->ecc.ecc_vfy++;
        break;
    case PHX_ECC_FIELD_BIN_INV:
        stats->ecc.ecc_field_bin_inv++;
        break;
    case PHX_ECC_FIELD_BIN_MUL:
        stats->ecc.ecc_field_bin_mul++;
        break;
    case PHX_ECC_FIELD_BIN_ADD:
        stats->ecc.ecc_field_bin_add++;
        break;
    case PHX_ECC_FIELD_ADD:
        stats->ecc.ecc_field_add++;
        break;
    case PHX_ECC_FIELD_SUB:
        stats->ecc.ecc_field_sub++;
        break;
    case PHX_ECC_FIELD_MUL:
        stats->ecc.ecc_field_mul++;
        break;
    case PHX_ECC_FIELD_INV:
        stats->ecc.ecc_field_inv++;
        break;
    case PHX_ECC_FIELD_DIV:
        stats->ecc.ecc_field_div++;
        break;
    case PHX_ECC_FIELD_RED:
        stats->ecc.ecc_field_red++;
        break;
    case PHX_ECC_FIELD:
    case PHX_ECC_BIN:
        break;
    }
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_cipher_revert(rmisec_stats_pt stats,
                           int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_CPHR_REVERTS) {
    write_lock(&stats->rmisec_stats_lock);
    stats->cipher.reverts++;
    stats->cipher.reverts_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_hmac_revert(rmisec_stats_pt stats,
                         int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_HMAC_REVERTS) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.reverts++;
    stats->hmac.reverts_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_md5(rmisec_stats_pt stats,
                     int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_MD5) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.md5_count++;
    stats->hmac.md5_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_sha1(rmisec_stats_pt stats,
                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_SHA1) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.sha1_count++;
    stats->hmac.sha1_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_sha256(rmisec_stats_pt stats,
                    int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_SHA256) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.sha256_count++;
    stats->hmac.sha256_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_sha384(rmisec_stats_pt stats,
                                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_SHA384) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.sha384_count++;
    stats->hmac.sha384_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_sha512(rmisec_stats_pt stats,
                                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_SHA512) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.sha512_count++;
    stats->hmac.sha512_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_gcm(rmisec_stats_pt stats,
                                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_GCM) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.gcm_count++;
    stats->hmac.gcm_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_kasumi_f9(rmisec_stats_pt stats,
                                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_KASUMI_F9) {
    write_lock(&stats->rmisec_stats_lock);
    stats->hmac.kasumi_f9_count++;
    stats->hmac.kasumi_f9_bytes += nbytes;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_unaligned_auth_dest(rmisec_stats_pt stats,
                             int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_UNALIGNED_AUTH_DEST) {
    write_lock(&stats->rmisec_stats_lock);
    stats->opt.unaligned_auth_dest++;
    write_unlock(&stats->rmisec_stats_lock);
  }
}


static void inline phxdrv_record_combined(rmisec_stats_pt stats,
                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_COMBINED) {
    write_lock(&stats->rmisec_stats_lock);
    stats->opt.combined++;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_sym_failed(rmisec_stats_pt stats,
                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_COMBINED) {
    write_lock(&stats->rmisec_stats_lock);
    stats->opt.sym_failed++;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_modexp_failed(rmisec_stats_pt stats,
                      int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_COMBINED) {
    write_lock(&stats->rmisec_stats_lock);
    stats->opt.modexp_failed++;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

static void inline phxdrv_record_ecc_failed(rmisec_stats_pt stats,
                                          int nbytes)
{
  if (stats->stats_mask & PHXDRV_PROFILE_COMBINED) {
    write_lock(&stats->rmisec_stats_lock);
    stats->opt.ecc_failed++;
    write_unlock(&stats->rmisec_stats_lock);
  }
}

#endif
