/*	$FreeBSD$	*/
/*	$KAME: rijndael-alg-fst.h,v 1.5 2003/07/15 10:47:16 itojun Exp $	*/
/**
 * rijndael-alg-fst.h
 *
 * @version 3.0 (December 2000)
 *
 * Optimised ANSI C code for the Rijndael cipher (now AES)
 *
 * @author Vincent Rijmen <vincent.rijmen@esat.kuleuven.ac.be>
 * @author Antoon Bosselaers <antoon.bosselaers@esat.kuleuven.ac.be>
 * @author Paulo Barreto <paulo.barreto@terra.com.br>
 *
 * This code is hereby placed in the public domain.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __RIJNDAEL_ALG_FST_H
#define __RIJNDAEL_ALG_FST_H

#define RIJNDAEL_MAXKC	(256/32)
#define RIJNDAEL_MAXKB	(256/8)
#define RIJNDAEL_MAXNR	14

int rijndaelKeySetupEnc(u_int32_t rk[/*4*(Nr + 1)*/], const u_int8_t cipherKey[], int keyBits);
int rijndaelKeySetupDec(u_int32_t rk[/*4*(Nr + 1)*/], const u_int8_t cipherKey[], int keyBits);
void rijndaelEncrypt(const u_int32_t rk[/*4*(Nr + 1)*/], int Nr, const u_int8_t pt[16], u_int8_t ct[16]);
void rijndaelDecrypt(const u_int32_t rk[/*4*(Nr + 1)*/], int Nr, const u_int8_t ct[16], u_int8_t pt[16]);

#endif /* __RIJNDAEL_ALG_FST_H */
