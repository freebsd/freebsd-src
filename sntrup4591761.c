/*  $OpenBSD: sntrup4591761.c,v 1.3 2019/01/30 19:51:15 markus Exp $ */

/*
 * Public Domain, Authors:
 * - Daniel J. Bernstein
 * - Chitchanok Chuengsatiansup
 * - Tanja Lange
 * - Christine van Vredendaal
 */

#include "includes.h"

#include <string.h>
#include "crypto_api.h"

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/int32_sort.h */
#ifndef int32_sort_h
#define int32_sort_h


static void int32_sort(crypto_int32 *,int);

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/int32_sort.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void minmax(crypto_int32 *x,crypto_int32 *y)
{
  crypto_uint32 xi = *x;
  crypto_uint32 yi = *y;
  crypto_uint32 xy = xi ^ yi;
  crypto_uint32 c = yi - xi;
  c ^= xy & (c ^ yi);
  c >>= 31;
  c = -c;
  c &= xy;
  *x = xi ^ c;
  *y = yi ^ c;
}

static void int32_sort(crypto_int32 *x,int n)
{
  int top,p,q,i;

  if (n < 2) return;
  top = 1;
  while (top < n - top) top += top;

  for (p = top;p > 0;p >>= 1) {
    for (i = 0;i < n - p;++i)
      if (!(i & p))
        minmax(x + i,x + i + p);
    for (q = top;q > p;q >>= 1)
      for (i = 0;i < n - q;++i)
        if (!(i & p))
          minmax(x + i + p,x + i + q);
  }
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/small.h */
#ifndef small_h
#define small_h


typedef crypto_int8 small;

static void small_encode(unsigned char *,const small *);

static void small_decode(small *,const unsigned char *);


static void small_random(small *);

static void small_random_weightw(small *);

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/mod3.h */
#ifndef mod3_h
#define mod3_h


/* -1 if x is nonzero, 0 otherwise */
static inline int mod3_nonzero_mask(small x)
{
  return -x*x;
}

/* input between -100000 and 100000 */
/* output between -1 and 1 */
static inline small mod3_freeze(crypto_int32 a)
{
  a -= 3 * ((10923 * a) >> 15);
  a -= 3 * ((89478485 * a + 134217728) >> 28);
  return a;
}

static inline small mod3_minusproduct(small a,small b,small c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return mod3_freeze(A - B * C);
}

static inline small mod3_plusproduct(small a,small b,small c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return mod3_freeze(A + B * C);
}

static inline small mod3_product(small a,small b)
{
  return a * b;
}

static inline small mod3_sum(small a,small b)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  return mod3_freeze(A + B);
}

static inline small mod3_reciprocal(small a1)
{
  return a1;
}

static inline small mod3_quotient(small num,small den)
{
  return mod3_product(num,mod3_reciprocal(den));
}

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/modq.h */
#ifndef modq_h
#define modq_h


typedef crypto_int16 modq;

/* -1 if x is nonzero, 0 otherwise */
static inline int modq_nonzero_mask(modq x)
{
  crypto_int32 r = (crypto_uint16) x;
  r = -r;
  r >>= 30;
  return r;
}

/* input between -9000000 and 9000000 */
/* output between -2295 and 2295 */
static inline modq modq_freeze(crypto_int32 a)
{
  a -= 4591 * ((228 * a) >> 20);
  a -= 4591 * ((58470 * a + 134217728) >> 28);
  return a;
}

static inline modq modq_minusproduct(modq a,modq b,modq c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return modq_freeze(A - B * C);
}

static inline modq modq_plusproduct(modq a,modq b,modq c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return modq_freeze(A + B * C);
}

static inline modq modq_product(modq a,modq b)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  return modq_freeze(A * B);
}

static inline modq modq_square(modq a)
{
  crypto_int32 A = a;
  return modq_freeze(A * A);
}

static inline modq modq_sum(modq a,modq b)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  return modq_freeze(A + B);
}

static inline modq modq_reciprocal(modq a1)
{
  modq a2 = modq_square(a1);
  modq a3 = modq_product(a2,a1);
  modq a4 = modq_square(a2);
  modq a8 = modq_square(a4);
  modq a16 = modq_square(a8);
  modq a32 = modq_square(a16);
  modq a35 = modq_product(a32,a3);
  modq a70 = modq_square(a35);
  modq a140 = modq_square(a70);
  modq a143 = modq_product(a140,a3);
  modq a286 = modq_square(a143);
  modq a572 = modq_square(a286);
  modq a1144 = modq_square(a572);
  modq a1147 = modq_product(a1144,a3);
  modq a2294 = modq_square(a1147);
  modq a4588 = modq_square(a2294);
  modq a4589 = modq_product(a4588,a1);
  return a4589;
}

static inline modq modq_quotient(modq num,modq den)
{
  return modq_product(num,modq_reciprocal(den));
}

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/params.h */
#ifndef params_h
#define params_h

#define q 4591
/* XXX: also built into modq in various ways */

#define qshift 2295
#define p 761
#define w 286

#define rq_encode_len 1218
#define small_encode_len 191

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3.h */
#ifndef r3_h
#define r3_h


static void r3_mult(small *,const small *,const small *);

extern int r3_recip(small *,const small *);

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq.h */
#ifndef rq_h
#define rq_h


static void rq_encode(unsigned char *,const modq *);

static void rq_decode(modq *,const unsigned char *);

static void rq_encoderounded(unsigned char *,const modq *);

static void rq_decoderounded(modq *,const unsigned char *);

static void rq_round3(modq *,const modq *);

static void rq_mult(modq *,const modq *,const small *);

int rq_recip3(modq *,const small *);

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/swap.h */
#ifndef swap_h
#define swap_h

static void swap(void *,void *,int,int);

#endif

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/dec.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */

#ifdef KAT
#endif


int crypto_kem_sntrup4591761_dec(
  unsigned char *k,
  const unsigned char *cstr,
  const unsigned char *sk
)
{
  small f[p];
  modq h[p];
  small grecip[p];
  modq c[p];
  modq t[p];
  small t3[p];
  small r[p];
  modq hr[p];
  unsigned char rstr[small_encode_len];
  unsigned char hash[64];
  int i;
  int result = 0;
  int weight;

  small_decode(f,sk);
  small_decode(grecip,sk + small_encode_len);
  rq_decode(h,sk + 2 * small_encode_len);

  rq_decoderounded(c,cstr + 32);

  rq_mult(t,c,f);
  for (i = 0;i < p;++i) t3[i] = mod3_freeze(modq_freeze(3*t[i]));

  r3_mult(r,t3,grecip);

#ifdef KAT
  {
    int j;
    printf("decrypt r:");
    for (j = 0;j < p;++j)
      if (r[j] == 1) printf(" +%d",j);
      else if (r[j] == -1) printf(" -%d",j);
    printf("\n");
  }
#endif

  weight = 0;
  for (i = 0;i < p;++i) weight += (1 & r[i]);
  weight -= w;
  result |= modq_nonzero_mask(weight); /* XXX: puts limit on p */

  rq_mult(hr,h,r);
  rq_round3(hr,hr);
  for (i = 0;i < p;++i) result |= modq_nonzero_mask(hr[i] - c[i]);

  small_encode(rstr,r);
  crypto_hash_sha512(hash,rstr,sizeof rstr);
  result |= crypto_verify_32(hash,cstr);

  for (i = 0;i < 32;++i) k[i] = (hash[32 + i] & ~result);
  return result;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/enc.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */

#ifdef KAT
#endif


int crypto_kem_sntrup4591761_enc(
  unsigned char *cstr,
  unsigned char *k,
  const unsigned char *pk
)
{
  small r[p];
  modq h[p];
  modq c[p];
  unsigned char rstr[small_encode_len];
  unsigned char hash[64];

  small_random_weightw(r);

#ifdef KAT
  {
    int i;
    printf("encrypt r:");
    for (i = 0;i < p;++i)
      if (r[i] == 1) printf(" +%d",i);
      else if (r[i] == -1) printf(" -%d",i);
    printf("\n");
  }
#endif

  small_encode(rstr,r);
  crypto_hash_sha512(hash,rstr,sizeof rstr);

  rq_decode(h,pk);
  rq_mult(c,h,r);
  rq_round3(c,c);

  memcpy(k,hash + 32,32);
  memcpy(cstr,hash,32);
  rq_encoderounded(cstr + 32,c);

  return 0;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/keypair.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


#if crypto_kem_sntrup4591761_PUBLICKEYBYTES != rq_encode_len
#error "crypto_kem_sntrup4591761_PUBLICKEYBYTES must match rq_encode_len"
#endif
#if crypto_kem_sntrup4591761_SECRETKEYBYTES != rq_encode_len + 2 * small_encode_len
#error "crypto_kem_sntrup4591761_SECRETKEYBYTES must match rq_encode_len + 2 * small_encode_len"
#endif

int crypto_kem_sntrup4591761_keypair(unsigned char *pk,unsigned char *sk)
{
  small g[p];
  small grecip[p];
  small f[p];
  modq f3recip[p];
  modq h[p];

  do
    small_random(g);
  while (r3_recip(grecip,g) != 0);

  small_random_weightw(f);
  rq_recip3(f3recip,f);

  rq_mult(h,f3recip,g);

  rq_encode(pk,h);
  small_encode(sk,f);
  small_encode(sk + small_encode_len,grecip);
  memcpy(sk + 2 * small_encode_len,pk,rq_encode_len);

  return 0;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3_mult.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void r3_mult(small *h,const small *f,const small *g)
{
  small fg[p + p - 1];
  small result;
  int i, j;

  for (i = 0;i < p;++i) {
    result = 0;
    for (j = 0;j <= i;++j)
      result = mod3_plusproduct(result,f[j],g[i - j]);
    fg[i] = result;
  }
  for (i = p;i < p + p - 1;++i) {
    result = 0;
    for (j = i - p + 1;j < p;++j)
      result = mod3_plusproduct(result,f[j],g[i - j]);
    fg[i] = result;
  }

  for (i = p + p - 2;i >= p;--i) {
    fg[i - p] = mod3_sum(fg[i - p],fg[i]);
    fg[i - p + 1] = mod3_sum(fg[i - p + 1],fg[i]);
  }

  for (i = 0;i < p;++i)
    h[i] = fg[i];
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/r3_recip.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


/* caller must ensure that x-y does not overflow */
static int smaller_mask_r3_recip(int x,int y)
{
  return (x - y) >> 31;
}

static void vectormod3_product(small *z,int len,const small *x,const small c)
{
  int i;
  for (i = 0;i < len;++i) z[i] = mod3_product(x[i],c);
}

static void vectormod3_minusproduct(small *z,int len,const small *x,const small *y,const small c)
{
  int i;
  for (i = 0;i < len;++i) z[i] = mod3_minusproduct(x[i],y[i],c);
}

static void vectormod3_shift(small *z,int len)
{
  int i;
  for (i = len - 1;i > 0;--i) z[i] = z[i - 1];
  z[0] = 0;
}

/*
r = s^(-1) mod m, returning 0, if s is invertible mod m
or returning -1 if s is not invertible mod m
r,s are polys of degree <p
m is x^p-x-1
*/
int r3_recip(small *r,const small *s)
{
  const int loops = 2*p + 1;
  int loop;
  small f[p + 1]; 
  small g[p + 1]; 
  small u[2*p + 2];
  small v[2*p + 2];
  small c;
  int i;
  int d = p;
  int e = p;
  int swapmask;

  for (i = 2;i < p;++i) f[i] = 0;
  f[0] = -1;
  f[1] = -1;
  f[p] = 1;
  /* generalization: can initialize f to any polynomial m */
  /* requirements: m has degree exactly p, nonzero constant coefficient */

  for (i = 0;i < p;++i) g[i] = s[i];
  g[p] = 0;

  for (i = 0;i <= loops;++i) u[i] = 0;

  v[0] = 1;
  for (i = 1;i <= loops;++i) v[i] = 0;

  loop = 0;
  for (;;) {
    /* e == -1 or d + e + loop <= 2*p */

    /* f has degree p: i.e., f[p]!=0 */
    /* f[i]==0 for i < p-d */

    /* g has degree <=p (so it fits in p+1 coefficients) */
    /* g[i]==0 for i < p-e */

    /* u has degree <=loop (so it fits in loop+1 coefficients) */
    /* u[i]==0 for i < p-d */
    /* if invertible: u[i]==0 for i < loop-p (so can look at just p+1 coefficients) */

    /* v has degree <=loop (so it fits in loop+1 coefficients) */
    /* v[i]==0 for i < p-e */
    /* v[i]==0 for i < loop-p (so can look at just p+1 coefficients) */

    if (loop >= loops) break;

    c = mod3_quotient(g[p],f[p]);

    vectormod3_minusproduct(g,p + 1,g,f,c);
    vectormod3_shift(g,p + 1);

#ifdef SIMPLER
    vectormod3_minusproduct(v,loops + 1,v,u,c);
    vectormod3_shift(v,loops + 1);
#else
    if (loop < p) {
      vectormod3_minusproduct(v,loop + 1,v,u,c);
      vectormod3_shift(v,loop + 2);
    } else {
      vectormod3_minusproduct(v + loop - p,p + 1,v + loop - p,u + loop - p,c);
      vectormod3_shift(v + loop - p,p + 2);
    }
#endif

    e -= 1;

    ++loop;

    swapmask = smaller_mask_r3_recip(e,d) & mod3_nonzero_mask(g[p]);
    swap(&e,&d,sizeof e,swapmask);
    swap(f,g,(p + 1) * sizeof(small),swapmask);

#ifdef SIMPLER
    swap(u,v,(loops + 1) * sizeof(small),swapmask);
#else
    if (loop < p) {
      swap(u,v,(loop + 1) * sizeof(small),swapmask);
    } else {
      swap(u + loop - p,v + loop - p,(p + 1) * sizeof(small),swapmask);
    }
#endif
  }

  c = mod3_reciprocal(f[p]);
  vectormod3_product(r,p,u + p,c);
  return smaller_mask_r3_recip(0,d);
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/randomsmall.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void small_random(small *g)
{
  int i;

  for (i = 0;i < p;++i) {
    crypto_uint32 r = small_random32();
    g[i] = (small) (((1073741823 & r) * 3) >> 30) - 1;
  }
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/randomweightw.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void small_random_weightw(small *f)
{
  crypto_int32 r[p];
  int i;

  for (i = 0;i < p;++i) r[i] = small_random32();
  for (i = 0;i < w;++i) r[i] &= -2;
  for (i = w;i < p;++i) r[i] = (r[i] & -3) | 1;
  int32_sort(r,p);
  for (i = 0;i < p;++i) f[i] = ((small) (r[i] & 3)) - 1;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void rq_encode(unsigned char *c,const modq *f)
{
  crypto_int32 f0, f1, f2, f3, f4;
  int i;

  for (i = 0;i < p/5;++i) {
    f0 = *f++ + qshift;
    f1 = *f++ + qshift;
    f2 = *f++ + qshift;
    f3 = *f++ + qshift;
    f4 = *f++ + qshift;
    /* now want f0 + 6144*f1 + ... as a 64-bit integer */
    f1 *= 3;
    f2 *= 9;
    f3 *= 27;
    f4 *= 81;
    /* now want f0 + f1<<11 + f2<<22 + f3<<33 + f4<<44 */
    f0 += f1 << 11;
    *c++ = f0; f0 >>= 8;
    *c++ = f0; f0 >>= 8;
    f0 += f2 << 6;
    *c++ = f0; f0 >>= 8;
    *c++ = f0; f0 >>= 8;
    f0 += f3 << 1;
    *c++ = f0; f0 >>= 8;
    f0 += f4 << 4;
    *c++ = f0; f0 >>= 8;
    *c++ = f0; f0 >>= 8;
    *c++ = f0;
  }
  /* XXX: using p mod 5 = 1 */
  f0 = *f++ + qshift;
  *c++ = f0; f0 >>= 8;
  *c++ = f0;
}

static void rq_decode(modq *f,const unsigned char *c)
{
  crypto_uint32 c0, c1, c2, c3, c4, c5, c6, c7;
  crypto_uint32 f0, f1, f2, f3, f4;
  int i;

  for (i = 0;i < p/5;++i) {
    c0 = *c++;
    c1 = *c++;
    c2 = *c++;
    c3 = *c++;
    c4 = *c++;
    c5 = *c++;
    c6 = *c++;
    c7 = *c++;

    /* f0 + f1*6144 + f2*6144^2 + f3*6144^3 + f4*6144^4 */
    /* = c0 + c1*256 + ... + c6*256^6 + c7*256^7 */
    /* with each f between 0 and 4590 */

    c6 += c7 << 8;
    /* c6 <= 23241 = floor(4591*6144^4/2^48) */
    /* f4 = (16/81)c6 + (1/1296)(c5+[0,1]) - [0,0.75] */
    /* claim: 2^19 f4 < x < 2^19(f4+1) */
    /* where x = 103564 c6 + 405(c5+1) */
    /* proof: x - 2^19 f4 = (76/81)c6 + (37/81)c5 + 405 - (32768/81)[0,1] + 2^19[0,0.75] */
    /* at least 405 - 32768/81 > 0 */
    /* at most (76/81)23241 + (37/81)255 + 405 + 2^19 0.75 < 2^19 */
    f4 = (103564*c6 + 405*(c5+1)) >> 19;

    c5 += c6 << 8;
    c5 -= (f4 * 81) << 4;
    c4 += c5 << 8;

    /* f0 + f1*6144 + f2*6144^2 + f3*6144^3 */
    /* = c0 + c1*256 + c2*256^2 + c3*256^3 + c4*256^4 */
    /* c4 <= 247914 = floor(4591*6144^3/2^32) */
    /* f3 = (1/54)(c4+[0,1]) - [0,0.75] */
    /* claim: 2^19 f3 < x < 2^19(f3+1) */
    /* where x = 9709(c4+2) */
    /* proof: x - 2^19 f3 = 19418 - (1/27)c4 - (262144/27)[0,1] + 2^19[0,0.75] */
    /* at least 19418 - 247914/27 - 262144/27 > 0 */
    /* at most 19418 + 2^19 0.75 < 2^19 */
    f3 = (9709*(c4+2)) >> 19;

    c4 -= (f3 * 27) << 1;
    c3 += c4 << 8;
    /* f0 + f1*6144 + f2*6144^2 */
    /* = c0 + c1*256 + c2*256^2 + c3*256^3 */
    /* c3 <= 10329 = floor(4591*6144^2/2^24) */
    /* f2 = (4/9)c3 + (1/576)c2 + (1/147456)c1 + (1/37748736)c0 - [0,0.75] */
    /* claim: 2^19 f2 < x < 2^19(f2+1) */
    /* where x = 233017 c3 + 910(c2+2) */
    /* proof: x - 2^19 f2 = 1820 + (1/9)c3 - (2/9)c2 - (32/9)c1 - (1/72)c0 + 2^19[0,0.75] */
    /* at least 1820 - (2/9)255 - (32/9)255 - (1/72)255 > 0 */
    /* at most 1820 + (1/9)10329 + 2^19 0.75 < 2^19 */
    f2 = (233017*c3 + 910*(c2+2)) >> 19;

    c2 += c3 << 8;
    c2 -= (f2 * 9) << 6;
    c1 += c2 << 8;
    /* f0 + f1*6144 */
    /* = c0 + c1*256 */
    /* c1 <= 110184 = floor(4591*6144/2^8) */
    /* f1 = (1/24)c1 + (1/6144)c0 - (1/6144)f0 */
    /* claim: 2^19 f1 < x < 2^19(f1+1) */
    /* where x = 21845(c1+2) + 85 c0 */
    /* proof: x - 2^19 f1 = 43690 - (1/3)c1 - (1/3)c0 + 2^19 [0,0.75] */
    /* at least 43690 - (1/3)110184 - (1/3)255 > 0 */
    /* at most 43690 + 2^19 0.75 < 2^19 */
    f1 = (21845*(c1+2) + 85*c0) >> 19;

    c1 -= (f1 * 3) << 3;
    c0 += c1 << 8;
    f0 = c0;

    *f++ = modq_freeze(f0 + q - qshift);
    *f++ = modq_freeze(f1 + q - qshift);
    *f++ = modq_freeze(f2 + q - qshift);
    *f++ = modq_freeze(f3 + q - qshift);
    *f++ = modq_freeze(f4 + q - qshift);
  }

  c0 = *c++;
  c1 = *c++;
  c0 += c1 << 8;
  *f++ = modq_freeze(c0 + q - qshift);
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_mult.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void rq_mult(modq *h,const modq *f,const small *g)
{
  modq fg[p + p - 1];
  modq result;
  int i, j;

  for (i = 0;i < p;++i) {
    result = 0;
    for (j = 0;j <= i;++j)
      result = modq_plusproduct(result,f[j],g[i - j]);
    fg[i] = result;
  }
  for (i = p;i < p + p - 1;++i) {
    result = 0;
    for (j = i - p + 1;j < p;++j)
      result = modq_plusproduct(result,f[j],g[i - j]);
    fg[i] = result;
  }

  for (i = p + p - 2;i >= p;--i) {
    fg[i - p] = modq_sum(fg[i - p],fg[i]);
    fg[i - p + 1] = modq_sum(fg[i - p + 1],fg[i]);
  }

  for (i = 0;i < p;++i)
    h[i] = fg[i];
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_recip3.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


/* caller must ensure that x-y does not overflow */
static int smaller_mask_rq_recip3(int x,int y)
{
  return (x - y) >> 31;
}

static void vectormodq_product(modq *z,int len,const modq *x,const modq c)
{
  int i;
  for (i = 0;i < len;++i) z[i] = modq_product(x[i],c);
}

static void vectormodq_minusproduct(modq *z,int len,const modq *x,const modq *y,const modq c)
{
  int i;
  for (i = 0;i < len;++i) z[i] = modq_minusproduct(x[i],y[i],c);
}

static void vectormodq_shift(modq *z,int len)
{
  int i;
  for (i = len - 1;i > 0;--i) z[i] = z[i - 1];
  z[0] = 0;
}

/*
r = (3s)^(-1) mod m, returning 0, if s is invertible mod m
or returning -1 if s is not invertible mod m
r,s are polys of degree <p
m is x^p-x-1
*/
int rq_recip3(modq *r,const small *s)
{
  const int loops = 2*p + 1;
  int loop;
  modq f[p + 1]; 
  modq g[p + 1]; 
  modq u[2*p + 2];
  modq v[2*p + 2];
  modq c;
  int i;
  int d = p;
  int e = p;
  int swapmask;

  for (i = 2;i < p;++i) f[i] = 0;
  f[0] = -1;
  f[1] = -1;
  f[p] = 1;
  /* generalization: can initialize f to any polynomial m */
  /* requirements: m has degree exactly p, nonzero constant coefficient */

  for (i = 0;i < p;++i) g[i] = 3 * s[i];
  g[p] = 0;

  for (i = 0;i <= loops;++i) u[i] = 0;

  v[0] = 1;
  for (i = 1;i <= loops;++i) v[i] = 0;

  loop = 0;
  for (;;) {
    /* e == -1 or d + e + loop <= 2*p */

    /* f has degree p: i.e., f[p]!=0 */
    /* f[i]==0 for i < p-d */

    /* g has degree <=p (so it fits in p+1 coefficients) */
    /* g[i]==0 for i < p-e */

    /* u has degree <=loop (so it fits in loop+1 coefficients) */
    /* u[i]==0 for i < p-d */
    /* if invertible: u[i]==0 for i < loop-p (so can look at just p+1 coefficients) */

    /* v has degree <=loop (so it fits in loop+1 coefficients) */
    /* v[i]==0 for i < p-e */
    /* v[i]==0 for i < loop-p (so can look at just p+1 coefficients) */

    if (loop >= loops) break;

    c = modq_quotient(g[p],f[p]);

    vectormodq_minusproduct(g,p + 1,g,f,c);
    vectormodq_shift(g,p + 1);

#ifdef SIMPLER
    vectormodq_minusproduct(v,loops + 1,v,u,c);
    vectormodq_shift(v,loops + 1);
#else
    if (loop < p) {
      vectormodq_minusproduct(v,loop + 1,v,u,c);
      vectormodq_shift(v,loop + 2);
    } else {
      vectormodq_minusproduct(v + loop - p,p + 1,v + loop - p,u + loop - p,c);
      vectormodq_shift(v + loop - p,p + 2);
    }
#endif

    e -= 1;

    ++loop;

    swapmask = smaller_mask_rq_recip3(e,d) & modq_nonzero_mask(g[p]);
    swap(&e,&d,sizeof e,swapmask);
    swap(f,g,(p + 1) * sizeof(modq),swapmask);

#ifdef SIMPLER
    swap(u,v,(loops + 1) * sizeof(modq),swapmask);
#else
    if (loop < p) {
      swap(u,v,(loop + 1) * sizeof(modq),swapmask);
    } else {
      swap(u + loop - p,v + loop - p,(p + 1) * sizeof(modq),swapmask);
    }
#endif
  }

  c = modq_reciprocal(f[p]);
  vectormodq_product(r,p,u + p,c);
  return smaller_mask_rq_recip3(0,d);
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_round3.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void rq_round3(modq *h,const modq *f)
{
  int i;

  for (i = 0;i < p;++i)
    h[i] = ((21846 * (f[i] + 2295) + 32768) >> 16) * 3 - 2295;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/rq_rounded.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void rq_encoderounded(unsigned char *c,const modq *f)
{
  crypto_int32 f0, f1, f2;
  int i;

  for (i = 0;i < p/3;++i) {
    f0 = *f++ + qshift;
    f1 = *f++ + qshift;
    f2 = *f++ + qshift;
    f0 = (21846 * f0) >> 16;
    f1 = (21846 * f1) >> 16;
    f2 = (21846 * f2) >> 16;
    /* now want f0 + f1*1536 + f2*1536^2 as a 32-bit integer */
    f2 *= 3;
    f1 += f2 << 9;
    f1 *= 3;
    f0 += f1 << 9;
    *c++ = f0; f0 >>= 8;
    *c++ = f0; f0 >>= 8;
    *c++ = f0; f0 >>= 8;
    *c++ = f0;
  }
  /* XXX: using p mod 3 = 2 */
  f0 = *f++ + qshift;
  f1 = *f++ + qshift;
  f0 = (21846 * f0) >> 16;
  f1 = (21846 * f1) >> 16;
  f1 *= 3;
  f0 += f1 << 9;
  *c++ = f0; f0 >>= 8;
  *c++ = f0; f0 >>= 8;
  *c++ = f0;
}

static void rq_decoderounded(modq *f,const unsigned char *c)
{
  crypto_uint32 c0, c1, c2, c3;
  crypto_uint32 f0, f1, f2;
  int i;

  for (i = 0;i < p/3;++i) {
    c0 = *c++;
    c1 = *c++;
    c2 = *c++;
    c3 = *c++;

    /* f0 + f1*1536 + f2*1536^2 */
    /* = c0 + c1*256 + c2*256^2 + c3*256^3 */
    /* with each f between 0 and 1530 */

    /* f2 = (64/9)c3 + (1/36)c2 + (1/9216)c1 + (1/2359296)c0 - [0,0.99675] */
    /* claim: 2^21 f2 < x < 2^21(f2+1) */
    /* where x = 14913081*c3 + 58254*c2 + 228*(c1+2) */
    /* proof: x - 2^21 f2 = 456 - (8/9)c0 + (4/9)c1 - (2/9)c2 + (1/9)c3 + 2^21 [0,0.99675] */
    /* at least 456 - (8/9)255 - (2/9)255 > 0 */
    /* at most 456 + (4/9)255 + (1/9)255 + 2^21 0.99675 < 2^21 */
    f2 = (14913081*c3 + 58254*c2 + 228*(c1+2)) >> 21;

    c2 += c3 << 8;
    c2 -= (f2 * 9) << 2;
    /* f0 + f1*1536 */
    /* = c0 + c1*256 + c2*256^2 */
    /* c2 <= 35 = floor((1530+1530*1536)/256^2) */
    /* f1 = (128/3)c2 + (1/6)c1 + (1/1536)c0 - (1/1536)f0 */
    /* claim: 2^21 f1 < x < 2^21(f1+1) */
    /* where x = 89478485*c2 + 349525*c1 + 1365*(c0+1) */
    /* proof: x - 2^21 f1 = 1365 - (1/3)c2 - (1/3)c1 - (1/3)c0 + (4096/3)f0 */
    /* at least 1365 - (1/3)35 - (1/3)255 - (1/3)255 > 0 */
    /* at most 1365 + (4096/3)1530 < 2^21 */
    f1 = (89478485*c2 + 349525*c1 + 1365*(c0+1)) >> 21;

    c1 += c2 << 8;
    c1 -= (f1 * 3) << 1;

    c0 += c1 << 8;
    f0 = c0;

    *f++ = modq_freeze(f0 * 3 + q - qshift);
    *f++ = modq_freeze(f1 * 3 + q - qshift);
    *f++ = modq_freeze(f2 * 3 + q - qshift);
  }

  c0 = *c++;
  c1 = *c++;
  c2 = *c++;

  f1 = (89478485*c2 + 349525*c1 + 1365*(c0+1)) >> 21;

  c1 += c2 << 8;
  c1 -= (f1 * 3) << 1;

  c0 += c1 << 8;
  f0 = c0;

  *f++ = modq_freeze(f0 * 3 + q - qshift);
  *f++ = modq_freeze(f1 * 3 + q - qshift);
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/small.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


/* XXX: these functions rely on p mod 4 = 1 */

/* all coefficients in -1, 0, 1 */
static void small_encode(unsigned char *c,const small *f)
{
  small c0;
  int i;

  for (i = 0;i < p/4;++i) {
    c0 = *f++ + 1;
    c0 += (*f++ + 1) << 2;
    c0 += (*f++ + 1) << 4;
    c0 += (*f++ + 1) << 6;
    *c++ = c0;
  }
  c0 = *f++ + 1;
  *c++ = c0;
}

static void small_decode(small *f,const unsigned char *c)
{
  unsigned char c0;
  int i;

  for (i = 0;i < p/4;++i) {
    c0 = *c++;
    *f++ = ((small) (c0 & 3)) - 1; c0 >>= 2;
    *f++ = ((small) (c0 & 3)) - 1; c0 >>= 2;
    *f++ = ((small) (c0 & 3)) - 1; c0 >>= 2;
    *f++ = ((small) (c0 & 3)) - 1;
  }
  c0 = *c++;
  *f++ = ((small) (c0 & 3)) - 1;
}

/* from libpqcrypto-20180314/crypto_kem/sntrup4591761/ref/swap.c */
/* See https://ntruprime.cr.yp.to/software.html for detailed documentation. */


static void swap(void *x,void *y,int bytes,int mask)
{
  int i;
  char xi, yi, c, t;

  c = mask;
  
  for (i = 0;i < bytes;++i) {
    xi = i[(char *) x];
    yi = i[(char *) y];
    t = c & (xi ^ yi);
    xi ^= t;
    yi ^= t;
    i[(char *) x] = xi;
    i[(char *) y] = yi;
  }
}

