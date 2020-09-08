/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * Copyright (c) 2018 iXsystems, Inc
 * All rights reserved.
 *
 * This software was developed by John-Mark Gurney under
 * the sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$FreeBSD$
 *
 * This file implements AES-CCM+CBC-MAC, as described
 * at https://tools.ietf.org/html/rfc3610, using Intel's
 * AES-NI instructions.
 *
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>

#include <sys/systm.h>
#include <crypto/aesni/aesni.h>
#include <crypto/aesni/aesni_os.h>
#include <crypto/aesni/aesencdec.h>
#define AESNI_ENC(d, k, nr)	aesni_enc(nr-1, (const __m128i*)k, d)

#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

/*
 * Encrypt a single 128-bit block after
 * doing an xor.  This is also used to
 * decrypt (yay symmetric encryption).
 */
static inline __m128i
xor_and_encrypt(__m128i a, __m128i b, const unsigned char *k, int nr)
{
	__m128i retval = _mm_xor_si128(a, b);

	retval = AESNI_ENC(retval, k, nr);
	return (retval);
}

/*
 * Put value at the end of block, starting at offset.
 * (This goes backwards, putting bytes in *until* it
 * reaches offset.)
 */
static void
append_int(size_t value, __m128i *block, size_t offset)
{
	int indx = sizeof(*block) - 1;
	uint8_t *bp = (uint8_t*)block;

	while (indx > (sizeof(*block) - offset)) {
		bp[indx] = value & 0xff;
		indx--;
		value >>= 8;
	}
}

/*
 * Start the CBC-MAC process.  This handles the auth data.
 */
static __m128i
cbc_mac_start(const unsigned char *auth_data, size_t auth_len,
	     const unsigned char *nonce, size_t nonce_len,
	     const unsigned char *key, int nr,
	     size_t data_len, size_t tag_len)
{
	__m128i cbc_block, staging_block;
	uint8_t *byte_ptr;
	/* This defines where the message length goes */
	int L = sizeof(__m128i) - 1 - nonce_len;

	/*
	 * Set up B0 here.  This has the flags byte,
	 * followed by the nonce, followed by the
	 * length of the message.
	 */
	cbc_block = _mm_setzero_si128();
	byte_ptr = (uint8_t*)&cbc_block;
	byte_ptr[0] = ((auth_len > 0) ? 1 : 0) * 64 |
		(((tag_len - 2) / 2) * 8) |
		(L - 1);
	bcopy(nonce, byte_ptr + 1, nonce_len);
	append_int(data_len, &cbc_block, L+1);
	cbc_block = AESNI_ENC(cbc_block, key, nr);

	if (auth_len != 0) {
		/*
		 * We need to start by appending the length descriptor.
		 */
		uint32_t auth_amt;
		size_t copy_amt;
		const uint8_t *auth_ptr = auth_data;

		staging_block = _mm_setzero_si128();

		/*
		 * The current OCF calling convention means that
		 * there can never be more than 4g of authentication
		 * data, so we don't handle the 0xffff case.
		 */
		KASSERT(auth_len < (1ULL << 32),
		    ("%s: auth_len (%zu) larger than 4GB",
			__FUNCTION__, auth_len));

		if (auth_len < ((1 << 16) - (1 << 8))) {
			/*
			 * If the auth data length is less than
			 * 0xff00, we don't need to encode a length
			 * specifier, just the length of the auth
			 * data.
			 */
			be16enc(&staging_block, auth_len);
			auth_amt = 2;
		} else if (auth_len < (1ULL << 32)) {
			/*
			 * Two bytes for the length prefix, and then
			 * four bytes for the length.  This makes a total
			 * of 6 bytes to describe the auth data length.
			 */
			be16enc(&staging_block, 0xfffe);
			be32enc((char*)&staging_block + 2, auth_len);
			auth_amt = 6;
		} else
			panic("%s: auth len too large", __FUNCTION__);

		/*
		 * Need to copy abytes into blocks.  The first block is
		 * already partially filled, by auth_amt, so we need
		 * to handle that.  The last block needs to be zero padded.
		 */
		copy_amt = MIN(auth_len,
		    sizeof(staging_block) - auth_amt);
		byte_ptr = (uint8_t*)&staging_block;
		bcopy(auth_ptr, &byte_ptr[auth_amt], copy_amt);
		auth_ptr += copy_amt;

		cbc_block = xor_and_encrypt(cbc_block, staging_block, key, nr);
		
		while (auth_ptr < auth_data + auth_len) {
			copy_amt = MIN((auth_data + auth_len) - auth_ptr,
			    sizeof(staging_block));
			if (copy_amt < sizeof(staging_block))
				bzero(&staging_block, sizeof(staging_block));
			bcopy(auth_ptr, &staging_block, copy_amt);
			cbc_block = xor_and_encrypt(cbc_block, staging_block,
			    key, nr);
			auth_ptr += copy_amt;
		}
	}
	return (cbc_block);
}

/*
 * Implement AES CCM+CBC-MAC encryption and authentication.
 *
 * A couple of notes:
 * The specification allows for a different number of tag lengths;
 * however, they're always truncated from 16 bytes, and the tag
 * length isn't passed in.  (This could be fixed by changing the
 * code in aesni.c:aesni_cipher_crypt().)
 * Similarly, although the nonce length is passed in, the
 * OpenCrypto API that calls us doesn't have a way to set the nonce
 * other than by having different crypto algorithm types.  As a result,
 * this is currently always called with nlen=12; this means that we
 * also have a maximum message length of 16 megabytes.  And similarly,
 * since abytes is limited to a 32 bit value here, the AAD is
 * limited to 4 gigabytes or less.
 */
void
AES_CCM_encrypt(const unsigned char *in, unsigned char *out,
		const unsigned char *addt, const unsigned char *nonce,
		unsigned char *tag, uint32_t nbytes, uint32_t abytes, int nlen,
		const unsigned char *key, int nr)
{
	static const int tag_length = 16;	/* 128 bits */
	int L;
	int counter = 1;	/* S0 has 0, S1 has 1 */
	size_t copy_amt, total = 0;
	uint8_t *byte_ptr;
	__m128i s0, rolling_mac, s_x, staging_block;

	if (nbytes == 0 && abytes == 0)
		return;

	/* NIST 800-38c section A.1 says n is [7, 13]. */
	if (nlen < 7 || nlen > 13)
		panic("%s: bad nonce length %d", __FUNCTION__, nlen);

	/*
	 * We need to know how many bytes to use to describe
	 * the length of the data.  Normally, nlen should be
	 * 12, which leaves us 3 bytes to do that -- 16mbytes of
	 * data to encrypt.  But it can be longer or shorter;
	 * this impacts the length of the message.
	 */
	L = sizeof(__m128i) - 1 - nlen;

	/*
	 * Now, this shouldn't happen, but let's make sure that
	 * the data length isn't too big.
	 */
	KASSERT(nbytes <= ((1 << (8 * L)) - 1),
	    ("%s: nbytes is %u, but length field is %d bytes",
		__FUNCTION__, nbytes, L));

	/*
	 * Clear out the blocks
	 */
	s0 = _mm_setzero_si128();

	rolling_mac = cbc_mac_start(addt, abytes, nonce, nlen,
	    key, nr, nbytes, tag_length);

	/* s0 has flags, nonce, and then 0 */
	byte_ptr = (uint8_t*)&s0;
	byte_ptr[0] = L - 1;	/* but the flags byte only has L' */
	bcopy(nonce, &byte_ptr[1], nlen);

	/*
	 * Now to cycle through the rest of the data.
	 */
	bcopy(&s0, &s_x, sizeof(s0));

	while (total < nbytes) {
		/*
		 * Copy the plain-text data into staging_block.
		 * This may need to be zero-padded.
		 */
		copy_amt = MIN(nbytes - total, sizeof(staging_block));
		bcopy(in+total, &staging_block, copy_amt);
		if (copy_amt < sizeof(staging_block)) {
			byte_ptr = (uint8_t*)&staging_block;
			bzero(&byte_ptr[copy_amt],
			    sizeof(staging_block) - copy_amt);
		}
		rolling_mac = xor_and_encrypt(rolling_mac, staging_block,
		    key, nr);
		/* Put the counter into the s_x block */
		append_int(counter++, &s_x, L+1);
		/* Encrypt that */
		__m128i X = AESNI_ENC(s_x, key, nr);
		/* XOR the plain-text with the encrypted counter block */
		staging_block = _mm_xor_si128(staging_block, X);
		/* And copy it out */
		bcopy(&staging_block, out+total, copy_amt);
		total += copy_amt;
	}
	/*
	 * Allegedly done with it!  Except for the tag.
	 */
	s0 = AESNI_ENC(s0, key, nr);
	staging_block = _mm_xor_si128(s0, rolling_mac);
	bcopy(&staging_block, tag, tag_length);
	explicit_bzero(&s0, sizeof(s0));
	explicit_bzero(&staging_block, sizeof(staging_block));
	explicit_bzero(&s_x, sizeof(s_x));
	explicit_bzero(&rolling_mac, sizeof(rolling_mac));
}

/*
 * Implement AES CCM+CBC-MAC decryption and authentication.
 * Returns 0 on failure, 1 on success.
 *
 * The primary difference here is that each encrypted block
 * needs to be hashed&encrypted after it is decrypted (since
 * the CBC-MAC is based on the plain text).  This means that
 * we do the decryption twice -- first to verify the tag,
 * and second to decrypt and copy it out.
 *
 * To avoid annoying code copying, we implement the main
 * loop as a separate function.
 *
 * Call with out as NULL to not store the decrypted results;
 * call with hashp as NULL to not run the authentication.
 * Calling with neither as NULL does the decryption and
 * authentication as a single pass (which is not allowed
 * per the specification, really).
 *
 * If hashp is non-NULL, it points to the post-AAD computed
 * checksum.
 */
static void
decrypt_loop(const unsigned char *in, unsigned char *out, size_t nbytes,
    __m128i s0, size_t nonce_length, __m128i *macp,
    const unsigned char *key, int nr)
{
	size_t total = 0;
	__m128i s_x = s0, mac_block;
	int counter = 1;
	const size_t L = sizeof(__m128i) - 1 - nonce_length;
	__m128i pad_block, staging_block;

	/*
	 * The starting mac (post AAD, if any).
	 */
	if (macp != NULL)
		mac_block = *macp;
	
	while (total < nbytes) {
		size_t copy_amt = MIN(nbytes - total, sizeof(staging_block));

		if (copy_amt < sizeof(staging_block)) {
			staging_block = _mm_setzero_si128();
		}
		bcopy(in+total, &staging_block, copy_amt);

		/*
		 * staging_block has the current block of input data,
		 * zero-padded if necessary.  This is used in computing
		 * both the decrypted data, and the authentication tag.
		 */
		append_int(counter++, &s_x, L+1);
		/*
		 * The tag is computed based on the decrypted data.
		 */
		pad_block = AESNI_ENC(s_x, key, nr);
		if (copy_amt < sizeof(staging_block)) {
			/*
			 * Need to pad out pad_block with 0.
			 * (staging_block was set to 0's above.)
			 */
			uint8_t *end_of_buffer = (uint8_t*)&pad_block;
			bzero(end_of_buffer + copy_amt,
			    sizeof(pad_block) - copy_amt);
		}
		staging_block = _mm_xor_si128(staging_block, pad_block);

		if (out)
			bcopy(&staging_block, out+total, copy_amt);

		if (macp)
			mac_block = xor_and_encrypt(mac_block, staging_block,
			    key, nr);
		total += copy_amt;
	}

	if (macp)
		*macp = mac_block;

	explicit_bzero(&pad_block, sizeof(pad_block));
	explicit_bzero(&staging_block, sizeof(staging_block));
	explicit_bzero(&mac_block, sizeof(mac_block));
}

/*
 * The exposed decryption routine.  This is practically a
 * copy of the encryption routine, except that the order
 * in which the tag is created is changed.
 * XXX combine the two functions at some point!
 */
int
AES_CCM_decrypt(const unsigned char *in, unsigned char *out,
		const unsigned char *addt, const unsigned char *nonce,
		const unsigned char *tag, uint32_t nbytes, uint32_t abytes, int nlen,
		const unsigned char *key, int nr)
{
	static const int tag_length = 16;	/* 128 bits */
	int L;
	__m128i s0, rolling_mac, staging_block;
	uint8_t *byte_ptr;

	if (nbytes == 0 && abytes == 0)
		return (1);	// No message means no decryption!
	if (nlen < 0 || nlen > 15)
		panic("%s: bad nonce length %d", __FUNCTION__, nlen);

	/*
	 * We need to know how many bytes to use to describe
	 * the length of the data.  Normally, nlen should be
	 * 12, which leaves us 3 bytes to do that -- 16mbytes of
	 * data to encrypt.  But it can be longer or shorter.
	 */
	L = sizeof(__m128i) - 1 - nlen;

	/*
	 * Now, this shouldn't happen, but let's make sure that
	 * the data length isn't too big.
	 */
	if (nbytes > ((1 << (8 * L)) - 1))
		panic("%s: nbytes is %u, but length field is %d bytes",
		      __FUNCTION__, nbytes, L);
	/*
	 * Clear out the blocks
	 */
	s0 = _mm_setzero_si128();

	rolling_mac = cbc_mac_start(addt, abytes, nonce, nlen,
	    key, nr, nbytes, tag_length);
	/* s0 has flags, nonce, and then 0 */
	byte_ptr = (uint8_t*)&s0;
	byte_ptr[0] = L-1;	/* but the flags byte only has L' */
	bcopy(nonce, &byte_ptr[1], nlen);

	/*
	 * Now to cycle through the rest of the data.
	 */
	decrypt_loop(in, NULL, nbytes, s0, nlen, &rolling_mac, key, nr);

	/*
	 * Compare the tag.
	 */
	staging_block = _mm_xor_si128(AESNI_ENC(s0, key, nr), rolling_mac);
	if (timingsafe_bcmp(&staging_block, tag, tag_length) != 0) {
		return (0);
	}

	/*
	 * Push out the decryption results this time.
	 */
	decrypt_loop(in, out, nbytes, s0, nlen, NULL, key, nr);
	return (1);
}
