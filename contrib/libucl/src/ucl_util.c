/* Copyright (c) 2013, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl.h"
#include "ucl_internal.h"
#include "ucl_chartable.h"

#include <libgen.h> /* For dirname */

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#endif

#ifdef _WIN32
#include <windows.h>

#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_READWRITE  3
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_FAILED      ((void *) -1)

static void *mmap(char *addr, size_t length, int prot, int access, int fd, off_t offset)
{
	void *map = NULL;
	HANDLE handle = INVALID_HANDLE_VALUE;

	switch (prot) {
	default:
	case PROT_READ:
		{
			handle = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, PAGE_READONLY, 0, length, 0);
			if (!handle) break;
			map = (void *) MapViewOfFile(handle, FILE_MAP_READ, 0, 0, length);
			CloseHandle(handle);
			break;
		}
	case PROT_WRITE:
		{
			handle = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, PAGE_READWRITE, 0, length, 0);
			if (!handle) break;
			map = (void *) MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, length);
			CloseHandle(handle);
			break;
		}
	case PROT_READWRITE:
		{
			handle = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, PAGE_READWRITE, 0, length, 0);
			if (!handle) break;
			map = (void *) MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, length);
			CloseHandle(handle);
			break;
		}
	}
	if (map == (void *) NULL) {
		return (void *) MAP_FAILED;
	}
	return (void *) ((char *) map + offset);
}

static int munmap(void *map,size_t length)
{
	if (!UnmapViewOfFile(map)) {
		return(-1);
	}
	return(0);
}

static char* realpath(const char *path, char *resolved_path) {
    char *p;
    char tmp[MAX_PATH + 1];
    strncpy(tmp, path, sizeof(tmp)-1);
    p = tmp;
    while(*p) {
        if (*p == '/') *p = '\\';
        p++;
    }
    return _fullpath(resolved_path, tmp, MAX_PATH);
}
#endif

/**
 * @file rcl_util.c
 * Utilities for rcl parsing
 */


static void
ucl_object_free_internal (ucl_object_t *obj, bool allow_rec)
{
	ucl_object_t *sub, *tmp;

	while (obj != NULL) {
		if (obj->trash_stack[UCL_TRASH_KEY] != NULL) {
			UCL_FREE (obj->hh.keylen, obj->trash_stack[UCL_TRASH_KEY]);
		}
		if (obj->trash_stack[UCL_TRASH_VALUE] != NULL) {
			UCL_FREE (obj->len, obj->trash_stack[UCL_TRASH_VALUE]);
		}

		if (obj->type == UCL_ARRAY) {
			sub = obj->value.av;
			while (sub != NULL) {
				tmp = sub->next;
				ucl_object_free_internal (sub, false);
				sub = tmp;
			}
		}
		else if (obj->type == UCL_OBJECT) {
			if (obj->value.ov != NULL) {
				ucl_hash_destroy (obj->value.ov, (ucl_hash_free_func *)ucl_object_unref);
			}
		}
		tmp = obj->next;
		UCL_FREE (sizeof (ucl_object_t), obj);
		obj = tmp;

		if (!allow_rec) {
			break;
		}
	}
}

void
ucl_object_free (ucl_object_t *obj)
{
	ucl_object_free_internal (obj, true);
}

size_t
ucl_unescape_json_string (char *str, size_t len)
{
	char *t = str, *h = str;
	int i, uval;

	/* t is target (tortoise), h is source (hare) */

	while (len) {
		if (*h == '\\') {
			h ++;
			switch (*h) {
			case 'n':
				*t++ = '\n';
				break;
			case 'r':
				*t++ = '\r';
				break;
			case 'b':
				*t++ = '\b';
				break;
			case 't':
				*t++ = '\t';
				break;
			case 'f':
				*t++ = '\f';
				break;
			case '\\':
				*t++ = '\\';
				break;
			case '"':
				*t++ = '"';
				break;
			case 'u':
				/* Unicode escape */
				uval = 0;
				for (i = 0; i < 4; i++) {
					uval <<= 4;
					if (isdigit (h[i])) {
						uval += h[i] - '0';
					}
					else if (h[i] >= 'a' && h[i] <= 'f') {
						uval += h[i] - 'a' + 10;
					}
					else if (h[i] >= 'A' && h[i] <= 'F') {
						uval += h[i] - 'A' + 10;
					}
				}
				h += 3;
				len -= 3;
				/* Encode */
				if(uval < 0x80) {
					t[0] = (char)uval;
					t ++;
				}
				else if(uval < 0x800) {
					t[0] = 0xC0 + ((uval & 0x7C0) >> 6);
					t[1] = 0x80 + ((uval & 0x03F));
					t += 2;
				}
				else if(uval < 0x10000) {
					t[0] = 0xE0 + ((uval & 0xF000) >> 12);
					t[1] = 0x80 + ((uval & 0x0FC0) >> 6);
					t[2] = 0x80 + ((uval & 0x003F));
					t += 3;
				}
				else if(uval <= 0x10FFFF) {
					t[0] = 0xF0 + ((uval & 0x1C0000) >> 18);
					t[1] = 0x80 + ((uval & 0x03F000) >> 12);
					t[2] = 0x80 + ((uval & 0x000FC0) >> 6);
					t[3] = 0x80 + ((uval & 0x00003F));
					t += 4;
				}
				else {
					*t++ = '?';
				}
				break;
			default:
				*t++ = *h;
				break;
			}
			h ++;
			len --;
		}
		else {
			*t++ = *h++;
		}
		len --;
	}
	*t = '\0';

	return (t - str);
}

UCL_EXTERN char *
ucl_copy_key_trash (ucl_object_t *obj)
{
	if (obj->trash_stack[UCL_TRASH_KEY] == NULL && obj->key != NULL) {
		obj->trash_stack[UCL_TRASH_KEY] = malloc (obj->keylen + 1);
		if (obj->trash_stack[UCL_TRASH_KEY] != NULL) {
			memcpy (obj->trash_stack[UCL_TRASH_KEY], obj->key, obj->keylen);
			obj->trash_stack[UCL_TRASH_KEY][obj->keylen] = '\0';
		}
		obj->key = obj->trash_stack[UCL_TRASH_KEY];
		obj->flags |= UCL_OBJECT_ALLOCATED_KEY;
	}

	return obj->trash_stack[UCL_TRASH_KEY];
}

UCL_EXTERN char *
ucl_copy_value_trash (ucl_object_t *obj)
{
	if (obj->trash_stack[UCL_TRASH_VALUE] == NULL) {
		if (obj->type == UCL_STRING) {
			/* Special case for strings */
			obj->trash_stack[UCL_TRASH_VALUE] = malloc (obj->len + 1);
			if (obj->trash_stack[UCL_TRASH_VALUE] != NULL) {
				memcpy (obj->trash_stack[UCL_TRASH_VALUE], obj->value.sv, obj->len);
				obj->trash_stack[UCL_TRASH_VALUE][obj->len] = '\0';
				obj->value.sv = obj->trash_stack[UCL_TRASH_VALUE];
			}
		}
		else {
			/* Just emit value in json notation */
			obj->trash_stack[UCL_TRASH_VALUE] = ucl_object_emit_single_json (obj);
			obj->len = strlen (obj->trash_stack[UCL_TRASH_VALUE]);
		}
		obj->flags |= UCL_OBJECT_ALLOCATED_VALUE;
	}
	return obj->trash_stack[UCL_TRASH_VALUE];
}

UCL_EXTERN ucl_object_t*
ucl_parser_get_object (struct ucl_parser *parser)
{
	if (parser->state != UCL_STATE_ERROR && parser->top_obj != NULL) {
		return ucl_object_ref (parser->top_obj);
	}

	return NULL;
}

UCL_EXTERN void
ucl_parser_free (struct ucl_parser *parser)
{
	struct ucl_stack *stack, *stmp;
	struct ucl_macro *macro, *mtmp;
	struct ucl_chunk *chunk, *ctmp;
	struct ucl_pubkey *key, *ktmp;
	struct ucl_variable *var, *vtmp;

	if (parser->top_obj != NULL) {
		ucl_object_unref (parser->top_obj);
	}

	LL_FOREACH_SAFE (parser->stack, stack, stmp) {
		free (stack);
	}
	HASH_ITER (hh, parser->macroes, macro, mtmp) {
		free (macro->name);
		HASH_DEL (parser->macroes, macro);
		UCL_FREE (sizeof (struct ucl_macro), macro);
	}
	LL_FOREACH_SAFE (parser->chunks, chunk, ctmp) {
		UCL_FREE (sizeof (struct ucl_chunk), chunk);
	}
	LL_FOREACH_SAFE (parser->keys, key, ktmp) {
		UCL_FREE (sizeof (struct ucl_pubkey), key);
	}
	LL_FOREACH_SAFE (parser->variables, var, vtmp) {
		free (var->value);
		free (var->var);
		UCL_FREE (sizeof (struct ucl_variable), var);
	}

	if (parser->err != NULL) {
		utstring_free(parser->err);
	}

	UCL_FREE (sizeof (struct ucl_parser), parser);
}

UCL_EXTERN const char *
ucl_parser_get_error(struct ucl_parser *parser)
{
	if (parser->err == NULL)
		return NULL;

	return utstring_body(parser->err);
}

UCL_EXTERN bool
ucl_pubkey_add (struct ucl_parser *parser, const unsigned char *key, size_t len)
{
#ifndef HAVE_OPENSSL
	ucl_create_err (&parser->err, "cannot check signatures without openssl");
	return false;
#else
# if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	ucl_create_err (&parser->err, "cannot check signatures, openssl version is unsupported");
	return EXIT_FAILURE;
# else
	struct ucl_pubkey *nkey;
	BIO *mem;

	mem = BIO_new_mem_buf ((void *)key, len);
	nkey = UCL_ALLOC (sizeof (struct ucl_pubkey));
	nkey->key = PEM_read_bio_PUBKEY (mem, &nkey->key, NULL, NULL);
	BIO_free (mem);
	if (nkey->key == NULL) {
		UCL_FREE (sizeof (struct ucl_pubkey), nkey);
		ucl_create_err (&parser->err, "%s",
				ERR_error_string (ERR_get_error (), NULL));
		return false;
	}
	LL_PREPEND (parser->keys, nkey);
# endif
#endif
	return true;
}

#ifdef CURL_FOUND
struct ucl_curl_cbdata {
	unsigned char *buf;
	size_t buflen;
};

static size_t
ucl_curl_write_callback (void* contents, size_t size, size_t nmemb, void* ud)
{
	struct ucl_curl_cbdata *cbdata = ud;
	size_t realsize = size * nmemb;

	cbdata->buf = realloc (cbdata->buf, cbdata->buflen + realsize + 1);
	if (cbdata->buf == NULL) {
		return 0;
	}

	memcpy (&(cbdata->buf[cbdata->buflen]), contents, realsize);
	cbdata->buflen += realsize;
	cbdata->buf[cbdata->buflen] = 0;

	return realsize;
}
#endif

/**
 * Fetch a url and save results to the memory buffer
 * @param url url to fetch
 * @param len length of url
 * @param buf target buffer
 * @param buflen target length
 * @return
 */
static bool
ucl_fetch_url (const unsigned char *url, unsigned char **buf, size_t *buflen,
		UT_string **err, bool must_exist)
{

#ifdef HAVE_FETCH_H
	struct url *fetch_url;
	struct url_stat us;
	FILE *in;

	fetch_url = fetchParseURL (url);
	if (fetch_url == NULL) {
		ucl_create_err (err, "invalid URL %s: %s",
				url, strerror (errno));
		return false;
	}
	if ((in = fetchXGet (fetch_url, &us, "")) == NULL) {
		if (!must_exist) {
			ucl_create_err (err, "cannot fetch URL %s: %s",
				url, strerror (errno));
		}
		fetchFreeURL (fetch_url);
		return false;
	}

	*buflen = us.size;
	*buf = malloc (*buflen);
	if (*buf == NULL) {
		ucl_create_err (err, "cannot allocate buffer for URL %s: %s",
				url, strerror (errno));
		fclose (in);
		fetchFreeURL (fetch_url);
		return false;
	}

	if (fread (*buf, *buflen, 1, in) != 1) {
		ucl_create_err (err, "cannot read URL %s: %s",
				url, strerror (errno));
		fclose (in);
		fetchFreeURL (fetch_url);
		return false;
	}

	fetchFreeURL (fetch_url);
	return true;
#elif defined(CURL_FOUND)
	CURL *curl;
	int r;
	struct ucl_curl_cbdata cbdata;

	curl = curl_easy_init ();
	if (curl == NULL) {
		ucl_create_err (err, "CURL interface is broken");
		return false;
	}
	if ((r = curl_easy_setopt (curl, CURLOPT_URL, url)) != CURLE_OK) {
		ucl_create_err (err, "invalid URL %s: %s",
				url, curl_easy_strerror (r));
		curl_easy_cleanup (curl);
		return false;
	}
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ucl_curl_write_callback);
	cbdata.buf = *buf;
	cbdata.buflen = *buflen;
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &cbdata);

	if ((r = curl_easy_perform (curl)) != CURLE_OK) {
		if (!must_exist) {
			ucl_create_err (err, "error fetching URL %s: %s",
				url, curl_easy_strerror (r));
		}
		curl_easy_cleanup (curl);
		if (cbdata.buf) {
			free (cbdata.buf);
		}
		return false;
	}
	*buf = cbdata.buf;
	*buflen = cbdata.buflen;

	return true;
#else
	ucl_create_err (err, "URL support is disabled");
	return false;
#endif
}

/**
 * Fetch a file and save results to the memory buffer
 * @param filename filename to fetch
 * @param len length of filename
 * @param buf target buffer
 * @param buflen target length
 * @return
 */
static bool
ucl_fetch_file (const unsigned char *filename, unsigned char **buf, size_t *buflen,
		UT_string **err, bool must_exist)
{
	int fd;
	struct stat st;

	if (stat (filename, &st) == -1 || !S_ISREG (st.st_mode)) {
		if (must_exist) {
			ucl_create_err (err, "cannot stat file %s: %s",
					filename, strerror (errno));
		}
		return false;
	}
	if (st.st_size == 0) {
		/* Do not map empty files */
		*buf = "";
		*buflen = 0;
	}
	else {
		if ((fd = open (filename, O_RDONLY)) == -1) {
			ucl_create_err (err, "cannot open file %s: %s",
					filename, strerror (errno));
			return false;
		}
		if ((*buf = mmap (NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
			close (fd);
			ucl_create_err (err, "cannot mmap file %s: %s",
					filename, strerror (errno));
			return false;
		}
		*buflen = st.st_size;
		close (fd);
	}

	return true;
}


#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
static inline bool
ucl_sig_check (const unsigned char *data, size_t datalen,
		const unsigned char *sig, size_t siglen, struct ucl_parser *parser)
{
	struct ucl_pubkey *key;
	char dig[EVP_MAX_MD_SIZE];
	unsigned int diglen;
	EVP_PKEY_CTX *key_ctx;
	EVP_MD_CTX *sign_ctx = NULL;

	sign_ctx = EVP_MD_CTX_create ();

	LL_FOREACH (parser->keys, key) {
		key_ctx = EVP_PKEY_CTX_new (key->key, NULL);
		if (key_ctx != NULL) {
			if (EVP_PKEY_verify_init (key_ctx) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_rsa_padding (key_ctx, RSA_PKCS1_PADDING) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_signature_md (key_ctx, EVP_sha256 ()) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			EVP_DigestInit (sign_ctx, EVP_sha256 ());
			EVP_DigestUpdate (sign_ctx, data, datalen);
			EVP_DigestFinal (sign_ctx, dig, &diglen);

			if (EVP_PKEY_verify (key_ctx, sig, siglen, dig, diglen) == 1) {
				EVP_MD_CTX_destroy (sign_ctx);
				EVP_PKEY_CTX_free (key_ctx);
				return true;
			}

			EVP_PKEY_CTX_free (key_ctx);
		}
	}

	EVP_MD_CTX_destroy (sign_ctx);

	return false;
}
#endif

/**
 * Include an url to configuration
 * @param data
 * @param len
 * @param parser
 * @param err
 * @return
 */
static bool
ucl_include_url (const unsigned char *data, size_t len,
		struct ucl_parser *parser, bool check_signature, bool must_exist)
{

	bool res;
	unsigned char *buf = NULL;
	size_t buflen = 0;
	struct ucl_chunk *chunk;
	char urlbuf[PATH_MAX];
	int prev_state;

	snprintf (urlbuf, sizeof (urlbuf), "%.*s", (int)len, data);

	if (!ucl_fetch_url (urlbuf, &buf, &buflen, &parser->err, must_exist)) {
		return (!must_exist || false);
	}

	if (check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		unsigned char *sigbuf = NULL;
		size_t siglen = 0;
		/* We need to check signature first */
		snprintf (urlbuf, sizeof (urlbuf), "%.*s.sig", (int)len, data);
		if (!ucl_fetch_url (urlbuf, &sigbuf, &siglen, &parser->err, true)) {
			return false;
		}
		if (!ucl_sig_check (buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err (&parser->err, "cannot verify url %s: %s",
							urlbuf,
							ERR_error_string (ERR_get_error (), NULL));
			if (siglen > 0) {
				munmap (sigbuf, siglen);
			}
			return false;
		}
		if (siglen > 0) {
			munmap (sigbuf, siglen);
		}
#endif
	}

	prev_state = parser->state;
	parser->state = UCL_STATE_INIT;

	res = ucl_parser_add_chunk (parser, buf, buflen);
	if (res == true) {
		/* Remove chunk from the stack */
		chunk = parser->chunks;
		if (chunk != NULL) {
			parser->chunks = chunk->next;
			UCL_FREE (sizeof (struct ucl_chunk), chunk);
		}
	}

	parser->state = prev_state;
	free (buf);

	return res;
}

/**
 * Include a file to configuration
 * @param data
 * @param len
 * @param parser
 * @param err
 * @return
 */
static bool
ucl_include_file (const unsigned char *data, size_t len,
		struct ucl_parser *parser, bool check_signature, bool must_exist)
{
	bool res;
	struct ucl_chunk *chunk;
	unsigned char *buf = NULL;
	size_t buflen;
	char filebuf[PATH_MAX], realbuf[PATH_MAX];
	int prev_state;

	snprintf (filebuf, sizeof (filebuf), "%.*s", (int)len, data);
	if (realpath (filebuf, realbuf) == NULL) {
		if (!must_exist) {
			return true;
		}
		ucl_create_err (&parser->err, "cannot open file %s: %s",
									filebuf,
									strerror (errno));
		return false;
	}

	if (!ucl_fetch_file (realbuf, &buf, &buflen, &parser->err, must_exist)) {
		return (!must_exist || false);
	}

	if (check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		unsigned char *sigbuf = NULL;
		size_t siglen = 0;
		/* We need to check signature first */
		snprintf (filebuf, sizeof (filebuf), "%s.sig", realbuf);
		if (!ucl_fetch_file (filebuf, &sigbuf, &siglen, &parser->err, true)) {
			return false;
		}
		if (!ucl_sig_check (buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err (&parser->err, "cannot verify file %s: %s",
							filebuf,
							ERR_error_string (ERR_get_error (), NULL));
			if (siglen > 0) {
				munmap (sigbuf, siglen);
			}
			return false;
		}
		if (siglen > 0) {
			munmap (sigbuf, siglen);
		}
#endif
	}

	ucl_parser_set_filevars (parser, realbuf, false);

	prev_state = parser->state;
	parser->state = UCL_STATE_INIT;

	res = ucl_parser_add_chunk (parser, buf, buflen);
	if (res == true) {
		/* Remove chunk from the stack */
		chunk = parser->chunks;
		if (chunk != NULL) {
			parser->chunks = chunk->next;
			UCL_FREE (sizeof (struct ucl_chunk), chunk);
		}
	}

	parser->state = prev_state;

	if (buflen > 0) {
		munmap (buf, buflen);
	}

	return res;
}

/**
 * Handle include macro
 * @param data include data
 * @param len length of data
 * @param ud user data
 * @param err error ptr
 * @return
 */
UCL_EXTERN bool
ucl_include_handler (const unsigned char *data, size_t len, void* ud)
{
	struct ucl_parser *parser = ud;

	if (*data == '/' || *data == '.') {
		/* Try to load a file */
		return ucl_include_file (data, len, parser, false, true);
	}

	return ucl_include_url (data, len, parser, false, true);
}

/**
 * Handle includes macro
 * @param data include data
 * @param len length of data
 * @param ud user data
 * @param err error ptr
 * @return
 */
UCL_EXTERN bool
ucl_includes_handler (const unsigned char *data, size_t len, void* ud)
{
	struct ucl_parser *parser = ud;

	if (*data == '/' || *data == '.') {
		/* Try to load a file */
		return ucl_include_file (data, len, parser, true, true);
	}

	return ucl_include_url (data, len, parser, true, true);
}


UCL_EXTERN bool
ucl_try_include_handler (const unsigned char *data, size_t len, void* ud)
{
	struct ucl_parser *parser = ud;

	if (*data == '/' || *data == '.') {
		/* Try to load a file */
		return ucl_include_file (data, len, parser, false, false);
	}

	return ucl_include_url (data, len, parser, false, false);
}

UCL_EXTERN bool
ucl_parser_set_filevars (struct ucl_parser *parser, const char *filename, bool need_expand)
{
	char realbuf[PATH_MAX], *curdir;

	if (filename != NULL) {
		if (need_expand) {
			if (realpath (filename, realbuf) == NULL) {
				return false;
			}
		}
		else {
			ucl_strlcpy (realbuf, filename, sizeof (realbuf));
		}

		/* Define variables */
		ucl_parser_register_variable (parser, "FILENAME", realbuf);
		curdir = dirname (realbuf);
		ucl_parser_register_variable (parser, "CURDIR", curdir);
	}
	else {
		/* Set everything from the current dir */
		curdir = getcwd (realbuf, sizeof (realbuf));
		ucl_parser_register_variable (parser, "FILENAME", "undef");
		ucl_parser_register_variable (parser, "CURDIR", curdir);
	}

	return true;
}

UCL_EXTERN bool
ucl_parser_add_file (struct ucl_parser *parser, const char *filename)
{
	unsigned char *buf;
	size_t len;
	bool ret;
	char realbuf[PATH_MAX];

	if (realpath (filename, realbuf) == NULL) {
		ucl_create_err (&parser->err, "cannot open file %s: %s",
				filename,
				strerror (errno));
		return false;
	}

	if (!ucl_fetch_file (realbuf, &buf, &len, &parser->err, true)) {
		return false;
	}

	ucl_parser_set_filevars (parser, realbuf, false);
	ret = ucl_parser_add_chunk (parser, buf, len);

	if (len > 0) {
		munmap (buf, len);
	}

	return ret;
}

size_t
ucl_strlcpy (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0') {
				break;
			}
		}
	}

	if (n == 0 && siz != 0) {
		*d = '\0';
	}

	return (s - src - 1);    /* count does not include NUL */
}

size_t
ucl_strlcpy_unsafe (char *dst, const char *src, size_t siz)
{
	memcpy (dst, src, siz - 1);
	dst[siz - 1] = '\0';

	return siz - 1;
}

size_t
ucl_strlcpy_tolower (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = tolower (*s++)) == '\0') {
				break;
			}
		}
	}

	if (n == 0 && siz != 0) {
		*d = '\0';
	}

	return (s - src);    /* count does not include NUL */
}

ucl_object_t *
ucl_object_fromstring_common (const char *str, size_t len, enum ucl_string_flags flags)
{
	ucl_object_t *obj;
	const char *start, *end, *p, *pos;
	char *dst, *d;
	size_t escaped_len;

	if (str == NULL) {
		return NULL;
	}

	obj = ucl_object_new ();
	if (obj) {
		if (len == 0) {
			len = strlen (str);
		}
		if (flags & UCL_STRING_TRIM) {
			/* Skip leading spaces */
			for (start = str; (size_t)(start - str) < len; start ++) {
				if (!ucl_test_character (*start, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
					break;
				}
			}
			/* Skip trailing spaces */
			for (end = str + len - 1; end > start; end --) {
				if (!ucl_test_character (*end, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
					break;
				}
			}
			end ++;
		}
		else {
			start = str;
			end = str + len;
		}

		obj->type = UCL_STRING;
		if (flags & UCL_STRING_ESCAPE) {
			for (p = start, escaped_len = 0; p < end; p ++, escaped_len ++) {
				if (ucl_test_character (*p, UCL_CHARACTER_JSON_UNSAFE)) {
					escaped_len ++;
				}
			}
			dst = malloc (escaped_len + 1);
			if (dst != NULL) {
				for (p = start, d = dst; p < end; p ++, d ++) {
					if (ucl_test_character (*p, UCL_CHARACTER_JSON_UNSAFE)) {
						switch (*p) {
						case '\n':
							*d++ = '\\';
							*d = 'n';
							break;
						case '\r':
							*d++ = '\\';
							*d = 'r';
							break;
						case '\b':
							*d++ = '\\';
							*d = 'b';
							break;
						case '\t':
							*d++ = '\\';
							*d = 't';
							break;
						case '\f':
							*d++ = '\\';
							*d = 'f';
							break;
						case '\\':
							*d++ = '\\';
							*d = '\\';
							break;
						case '"':
							*d++ = '\\';
							*d = '"';
							break;
						}
					}
					else {
						*d = *p;
					}
				}
				*d = '\0';
				obj->value.sv = dst;
				obj->trash_stack[UCL_TRASH_VALUE] = dst;
				obj->len = escaped_len;
			}
		}
		else {
			dst = malloc (end - start + 1);
			if (dst != NULL) {
				ucl_strlcpy_unsafe (dst, start, end - start + 1);
				obj->value.sv = dst;
				obj->trash_stack[UCL_TRASH_VALUE] = dst;
				obj->len = end - start;
			}
		}
		if ((flags & UCL_STRING_PARSE) && dst != NULL) {
			/* Parse what we have */
			if (flags & UCL_STRING_PARSE_BOOLEAN) {
				if (!ucl_maybe_parse_boolean (obj, dst, obj->len) && (flags & UCL_STRING_PARSE_NUMBER)) {
					ucl_maybe_parse_number (obj, dst, dst + obj->len, &pos,
							flags & UCL_STRING_PARSE_DOUBLE,
							flags & UCL_STRING_PARSE_BYTES);
				}
			}
			else {
				ucl_maybe_parse_number (obj, dst, dst + obj->len, &pos,
						flags & UCL_STRING_PARSE_DOUBLE,
						flags & UCL_STRING_PARSE_BYTES);
			}
		}
	}

	return obj;
}

static ucl_object_t *
ucl_object_insert_key_common (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key, bool merge, bool replace)
{
	ucl_object_t *found, *cur;
	ucl_object_iter_t it = NULL;
	const char *p;

	if (elt == NULL || key == NULL) {
		return NULL;
	}

	if (top == NULL) {
		top = ucl_object_new ();
		top->type = UCL_OBJECT;
	}

	if (top->type != UCL_OBJECT) {
		/* It is possible to convert NULL type to an object */
		if (top->type == UCL_NULL) {
			top->type = UCL_OBJECT;
		}
		else {
			/* Refuse converting of other object types */
			return top;
		}
	}

	if (top->value.ov == NULL) {
		top->value.ov = ucl_hash_create ();
	}

	if (keylen == 0) {
		keylen = strlen (key);
	}

	for (p = key; p < key + keylen; p ++) {
		if (ucl_test_character (*p, UCL_CHARACTER_UCL_UNSAFE)) {
			elt->flags |= UCL_OBJECT_NEED_KEY_ESCAPE;
			break;
		}
	}

	elt->key = key;
	elt->keylen = keylen;

	if (copy_key) {
		ucl_copy_key_trash (elt);
	}

	found = ucl_hash_search_obj (top->value.ov, elt);

	if (!found) {
		top->value.ov = ucl_hash_insert_object (top->value.ov, elt);
		DL_APPEND (found, elt);
	}
	else {
		if (replace) {
			ucl_hash_delete (top->value.ov, found);
			ucl_object_unref (found);
			top->value.ov = ucl_hash_insert_object (top->value.ov, elt);
			found = NULL;
			DL_APPEND (found, elt);
		}
		else if (merge) {
			if (found->type != UCL_OBJECT && elt->type == UCL_OBJECT) {
				/* Insert old elt to new one */
				elt = ucl_object_insert_key_common (elt, found, found->key, found->keylen, copy_key, false, false);
				ucl_hash_delete (top->value.ov, found);
				top->value.ov = ucl_hash_insert_object (top->value.ov, elt);
			}
			else if (found->type == UCL_OBJECT && elt->type != UCL_OBJECT) {
				/* Insert new to old */
				found = ucl_object_insert_key_common (found, elt, elt->key, elt->keylen, copy_key, false, false);
			}
			else if (found->type == UCL_OBJECT && elt->type == UCL_OBJECT) {
				/* Mix two hashes */
				while ((cur = ucl_iterate_object (elt, &it, true)) != NULL) {
					ucl_object_ref (cur);
					found = ucl_object_insert_key_common (found, cur, cur->key, cur->keylen, copy_key, false, false);
				}
				ucl_object_unref (elt);
			}
			else {
				/* Just make a list of scalars */
				DL_APPEND (found, elt);
			}
		}
		else {
			DL_APPEND (found, elt);
		}
	}

	return top;
}

bool
ucl_object_delete_keyl(ucl_object_t *top, const char *key, size_t keylen)
{
	ucl_object_t *found;

	found = ucl_object_find_keyl(top, key, keylen);

	if (found == NULL)
		return false;

	ucl_hash_delete(top->value.ov, found);
	ucl_object_unref (found);
	top->len --;

	return true;
}

bool
ucl_object_delete_key(ucl_object_t *top, const char *key)
{
	return ucl_object_delete_keyl(top, key, 0);
}

ucl_object_t *
ucl_object_insert_key (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common (top, elt, key, keylen, copy_key, false, false);
}

ucl_object_t *
ucl_object_insert_key_merged (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common (top, elt, key, keylen, copy_key, true, false);
}

ucl_object_t *
ucl_object_replace_key (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common (top, elt, key, keylen, copy_key, false, true);
}

ucl_object_t *
ucl_object_find_keyl (ucl_object_t *obj, const char *key, size_t klen)
{
	ucl_object_t *ret, srch;

	if (obj == NULL || obj->type != UCL_OBJECT || key == NULL) {
		return NULL;
	}

	srch.key = key;
	srch.keylen = klen;
	ret = ucl_hash_search_obj (obj->value.ov, &srch);

	return ret;
}

ucl_object_t *
ucl_object_find_key (ucl_object_t *obj, const char *key)
{
	size_t klen;
	ucl_object_t *ret, srch;

	if (obj == NULL || obj->type != UCL_OBJECT || key == NULL) {
		return NULL;
	}

	klen = strlen (key);
	srch.key = key;
	srch.keylen = klen;
	ret = ucl_hash_search_obj (obj->value.ov, &srch);

	return ret;
}

ucl_object_t*
ucl_iterate_object (ucl_object_t *obj, ucl_object_iter_t *iter, bool expand_values)
{
	ucl_object_t *elt;

	if (expand_values) {
		switch (obj->type) {
		case UCL_OBJECT:
			return (ucl_object_t*)ucl_hash_iterate (obj->value.ov, iter);
			break;
		case UCL_ARRAY:
			elt = *iter;
			if (elt == NULL) {
				elt = obj->value.av;
				if (elt == NULL) {
					return NULL;
				}
			}
			else if (elt == obj->value.av) {
				return NULL;
			}
			*iter = elt->next ? elt->next : obj->value.av;
			return elt;
		default:
			/* Go to linear iteration */
			break;
		}
	}
	/* Treat everything as a linear list */
	elt = *iter;
	if (elt == NULL) {
		elt = obj;
		if (elt == NULL) {
			return NULL;
		}
	}
	else if (elt == obj) {
		return NULL;
	}
	*iter = elt->next ? elt->next : obj;
	return elt;

	/* Not reached */
	return NULL;
}
