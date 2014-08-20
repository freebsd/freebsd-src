/* libwapcaplet.c
 *
 * String internment and management tools.
 *
 * Copyright 2009 The NetSurf Browser Project.
 *		  Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libwapcaplet/libwapcaplet.h"

#ifndef UNUSED
#define UNUSED(x) ((x) = (x))
#endif

static inline lwc_hash
lwc__calculate_hash(const char *str, size_t len)
{
	lwc_hash z = 0x811c9dc5;
	

	while (len > 0) {
		z *= 0x01000193;
		z ^= *str++;
                len--;
	}

	return z;
}

#define STR_OF(str) ((char *)(str + 1))
#define CSTR_OF(str) ((const char *)(str + 1))

#define NR_BUCKETS_DEFAULT	(4091)

typedef struct lwc_context_s {
        lwc_string **		buckets;
        lwc_hash		bucketcount;
} lwc_context;

static lwc_context *ctx = NULL;

#define LWC_ALLOC(s) malloc(s)
#define LWC_FREE(p) free(p)

typedef lwc_hash (*lwc_hasher)(const char *, size_t);
typedef int (*lwc_strncmp)(const char *, const char *, size_t);
typedef void (*lwc_memcpy)(char *, const char *, size_t);

static lwc_error
lwc__initialise(void)
{
        if (ctx != NULL)
                return lwc_error_ok;
        
        ctx = LWC_ALLOC(sizeof(lwc_context));
        
        if (ctx == NULL)
                return lwc_error_oom;
        
        memset(ctx, 0, sizeof(lwc_context));
        
        ctx->bucketcount = NR_BUCKETS_DEFAULT;
        ctx->buckets = LWC_ALLOC(sizeof(lwc_string *) * ctx->bucketcount);
        
        if (ctx->buckets == NULL) {
                LWC_FREE(ctx);
		ctx = NULL;
                return lwc_error_oom;
        }
        
        memset(ctx->buckets, 0, sizeof(lwc_string *) * ctx->bucketcount);
        
        return lwc_error_ok;
}

static lwc_error
lwc__intern(const char *s, size_t slen,
           lwc_string **ret,
           lwc_hasher hasher,
           lwc_strncmp compare,
           lwc_memcpy copy)
{
        lwc_hash h;
        lwc_hash bucket;
        lwc_string *str;
        lwc_error eret;
        
        assert((s != NULL) || (slen == 0));
        assert(ret);
        
        if (ctx == NULL) {
                eret = lwc__initialise();
                if (eret != lwc_error_ok)
                        return eret;
                if (ctx == NULL)
                        return lwc_error_oom;
        }
        
        h = hasher(s, slen);
        bucket = h % ctx->bucketcount;
        str = ctx->buckets[bucket];
        
        while (str != NULL) {
                if ((str->hash == h) && (str->len == slen)) {
                        if (compare(CSTR_OF(str), s, slen) == 0) {
                                str->refcnt++;
                                *ret = str;
                                return lwc_error_ok;
                        }
                }
                str = str->next;
        }
        
        /* Add one for the additional NUL. */
        *ret = str = LWC_ALLOC(sizeof(lwc_string) + slen + 1);
        
        if (str == NULL)
                return lwc_error_oom;
        
        str->prevptr = &(ctx->buckets[bucket]);
        str->next = ctx->buckets[bucket];
        if (str->next != NULL)
                str->next->prevptr = &(str->next);
        ctx->buckets[bucket] = str;

        str->len = slen;
        str->hash = h;
        str->refcnt = 1;
        str->insensitive = NULL;
        
        copy(STR_OF(str), s, slen);

        /* Guarantee NUL termination */
        STR_OF(str)[slen] = '\0';
        
        return lwc_error_ok;
}

lwc_error
lwc_intern_string(const char *s, size_t slen,
                  lwc_string **ret)
{
        return lwc__intern(s, slen, ret,
                           lwc__calculate_hash,
                           strncmp, (lwc_memcpy)memcpy);
}

lwc_error
lwc_intern_substring(lwc_string *str,
                     size_t ssoffset, size_t sslen,
                     lwc_string **ret)
{
        assert(str);
        assert(ret);
        
        if (ssoffset >= str->len)
                return lwc_error_range;
        if ((ssoffset + sslen) > str->len)
                return lwc_error_range;
        
        return lwc_intern_string(CSTR_OF(str) + ssoffset, sslen, ret);
}

void
lwc_string_destroy(lwc_string *str)
{
        assert(str);
        
        *(str->prevptr) = str->next;
        
        if (str->next != NULL)
                str->next->prevptr = str->prevptr;

        if (str->insensitive != NULL && str->refcnt == 0)
                lwc_string_unref(str->insensitive);

#ifndef NDEBUG
        memset(str, 0xA5, sizeof(*str) + str->len);
#endif
        
        LWC_FREE(str);
}

/**** Shonky caseless bits ****/

static inline char
lwc__dolower(const char c)
{
        if (c >= 'A' && c <= 'Z')
                return c + 'a' - 'A';
        return c;
}

static inline lwc_hash
lwc__calculate_lcase_hash(const char *str, size_t len)
{
	lwc_hash z = 0x811c9dc5;
	

	while (len > 0) {
		z *= 0x01000193;
		z ^= lwc__dolower(*str++);
                len--;
	}

	return z;
}

static int
lwc__lcase_strncmp(const char *s1, const char *s2, size_t n)
{
        while (n--) {
                if (*s1++ != lwc__dolower(*s2++))
                        /** @todo Test this somehow? */
                        return 1;
        }
        return 0;
}

static void
lwc__lcase_memcpy(char *target, const char *source, size_t n)
{
        while (n--) {
                *target++ = lwc__dolower(*source++);
        }
}

lwc_error
lwc__intern_caseless_string(lwc_string *str)
{
        assert(str);
        assert(str->insensitive == NULL);
        
        return lwc__intern(CSTR_OF(str),
                           str->len, &(str->insensitive),
                           lwc__calculate_lcase_hash,
                           lwc__lcase_strncmp,
                           lwc__lcase_memcpy);
}

/**** Iteration ****/

void
lwc_iterate_strings(lwc_iteration_callback_fn cb, void *pw)
{
        lwc_hash n;
        lwc_string *str;
       
	if (ctx == NULL)
		return;
 
        for (n = 0; n < ctx->bucketcount; ++n) {
                for (str = ctx->buckets[n]; str != NULL; str = str->next)
                        cb(str, pw);
        }
}
