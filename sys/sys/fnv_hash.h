/*
 * Fowler / Noll / Vo Hash (FNV Hash)
 * http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * This is an implementation of the algorithms posted above.
 * This file is placed in the public domain by Peter Wemm.
 *
 * $FreeBSD$
 */

#define FNV_32_PRIME ((u_int32_t) 0x01000193UL)
#define FNV1_32_INIT ((u_int32_t) 33554467UL)

#define FNV_64_PRIME ((u_int64_t) 0x100000001b3ULL)
#define FNV1_64_INIT ((u_int64_t) 0xcbf29ce484222325ULL)

static __inline u_int32_t
fnv32_hashbuf(const void *buf, size_t len)
{
	const u_int8_t *s = (const u_int8_t *)buf;
	u_int32_t hval;

	hval = FNV1_32_INIT;
	while (len-- != 0) {
		hval *= FNV_32_PRIME;
		hval ^= *s++;
	}
	return hval;
}

static __inline u_int32_t
fnv32_hashstr(const char *str)
{
	const u_int8_t *s = (const u_int8_t *)str;
	u_int32_t hval, c;

	hval = FNV1_32_INIT;
	while ((c = *s++) != 0) {
		hval *= FNV_32_PRIME;
		hval ^= c;
	}
	return hval;
}

static __inline u_int64_t
fnv64_hashbuf(const void *buf, size_t len)
{
	const u_int8_t *s = (const u_int8_t *)buf;
	u_int64_t hval;

	hval = FNV1_64_INIT;
	while (len-- != 0) {
		hval *= FNV_64_PRIME;
		hval ^= *s++;
	}
	return hval;
}

static __inline u_int64_t
fnv64_hashstr(const char *str)
{
	const u_int8_t *s = (const u_int8_t *)str;
	u_int64_t hval;
	u_register_t c;		 /* 32 bit on i386, 64 bit on alpha,ia64 */

	hval = FNV1_64_INIT;
	while ((c = *s++) != 0) {
		hval *= FNV_64_PRIME;
		hval ^= c;
	}
	return hval;
}
