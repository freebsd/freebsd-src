/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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

/**
 *
 */

/*! @mainpage Heimdal crypto library
 *
 * @section intro Introduction
 *
 * Heimdal libhcrypto library is a implementation many crypto
 * algorithms, among others: AES, SHA, DES, RSA, Camellia and many
 * help function.
 *
 * hcrypto provies a OpenSSL compatible interface libcrypto interface
 * and is licensed under a 3 clause BSD license (GPL compatible).
 *
 * The project web page: http://www.h5l.org/
 *
 * Sections of this manual:
 *
 * - @subpage page_evp, @ref hcrypto_evp
 * - @subpage page_rand, @ref hcrypto_rand
 * - @subpage page_dh, @ref hcrypto_dh
 * - @subpage page_rsa, @ref hcrypto_rsa
 * - @ref hcrypto_misc
 *
 * Older interfaces that you should not use:
 *
 * - @subpage page_des, @ref hcrypto_des
 *
 * @subsection control_functions Control functions
 *
 * Functions controlling general behavior, like adding algorithms, are
 * documented in this module: @ref hcrypto_core .
 *
 * @subsection return_values Return values
 *
 * Return values are diffrent in this module to be compatible with
 * OpenSSL interface. The diffrence is that on success 1 is returned
 * instead of the customary 0.

 * @subsection History
 *
 * Eric Young implemented DES in the library libdes, that grew into
 * libcrypto in the ssleay package. ssleay went into recession and
 * then got picked up by the OpenSSL (htp://www.openssl.org/)
 * project.
 *
 * libhcrypto is an independent implementation with no code decended
 * from ssleay/openssl. Both includes some common imported code, for
 * example the AES implementation.
 */

/** @defgroup hcrypto_dh Diffie-Hellman functions
 * See the @ref page_dh for description and examples.
 */
/** @defgroup hcrypto_rsa RSA functions
 * See the @ref page_rsa for description and examples.
 */
/** @defgroup hcrypto_evp EVP generic crypto functions
 * See the @ref page_evp for description and examples.
 */
/** @defgroup hcrypto_rand RAND crypto functions
 * See the @ref page_rand for description and examples.
 */
/** @defgroup hcrypto_des DES crypto functions
 * See the @ref page_des for description and examples.
 */
/** @defgroup hcrypto_core hcrypto function controlling behavior */
/** @defgroup hcrypto_misc hcrypto miscellaneous functions */
