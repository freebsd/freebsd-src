/* Copyright (c) 2013, Vsevolod Stakhov
 * Copyright (c) 2015 Allan Jude <allanjude@freebsd.org>
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
#include "kvec.h"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h> /* for snprintf */

#ifndef _WIN32
#include <glob.h>
#include <sys/param.h>
#else
#ifndef NBBY
#define NBBY 8
#endif
#endif

#ifdef HAVE_LIBGEN_H
#ifndef _WIN32
#include <libgen.h> /* For dirname */
#endif
#endif

typedef kvec_t(ucl_object_t *) ucl_array_t;

#define UCL_ARRAY_GET(ar, obj) ucl_array_t *ar = \
								   (ucl_array_t *) ((obj) != NULL ? (obj)->value.av : NULL)

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#endif

#ifdef CURL_FOUND
/* Seems to be broken */
#define CURL_DISABLE_TYPECHECK 1
#include <curl/curl.h>
#endif
#ifdef HAVE_FETCH_H
#include <fetch.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>

#ifndef PROT_READ
#define PROT_READ 1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 2
#endif
#ifndef PROT_READWRITE
#define PROT_READWRITE 3
#endif
#ifndef MAP_SHARED
#define MAP_SHARED 1
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 2
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif

#define getcwd _getcwd
#define open _open
#define close _close

static void *ucl_mmap(char *addr, size_t length, int prot, int access, int fd, off_t offset)
{
	void *map = NULL;
	HANDLE handle = INVALID_HANDLE_VALUE;

	switch (prot) {
	default:
	case PROT_READ: {
		handle = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, PAGE_READONLY, 0, length, 0);
		if (!handle) break;
		map = (void *) MapViewOfFile(handle, FILE_MAP_READ, 0, 0, length);
		CloseHandle(handle);
		break;
	}
	case PROT_WRITE: {
		handle = CreateFileMapping((HANDLE) _get_osfhandle(fd), 0, PAGE_READWRITE, 0, length, 0);
		if (!handle) break;
		map = (void *) MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, length);
		CloseHandle(handle);
		break;
	}
	case PROT_READWRITE: {
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

static int ucl_munmap(void *map, size_t length)
{
	if (!UnmapViewOfFile(map)) {
		return (-1);
	}
	return (0);
}

static char *ucl_realpath(const char *path, char *resolved_path)
{
	char *p;
	char tmp[MAX_PATH + 1];
	strncpy(tmp, path, sizeof(tmp) - 1);
	p = tmp;
	while (*p) {
		if (*p == '/') *p = '\\';
		p++;
	}
	return _fullpath(resolved_path, tmp, MAX_PATH);
}


char *dirname(char *path)
{
	static char path_buffer[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	_splitpath(path, drive, dir, fname, ext);
	_makepath(path_buffer, drive, dir, NULL, NULL);

	return path_buffer;
}

char *basename(char *path)
{
	static char path_buffer[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	_splitpath(path, drive, dir, fname, ext);
	_makepath(path_buffer, NULL, NULL, fname, ext);

	return path_buffer;
}
#else
#define ucl_mmap mmap
#define ucl_munmap munmap
#define ucl_realpath realpath
#endif

static void ucl_object_dtor_free(ucl_object_t *obj);

static void
ucl_object_dtor_free(ucl_object_t *obj)
{
	if (obj->trash_stack[UCL_TRASH_KEY] != NULL) {
		UCL_FREE(obj->hh.keylen, obj->trash_stack[UCL_TRASH_KEY]);
	}
	if (obj->trash_stack[UCL_TRASH_VALUE] != NULL) {
		UCL_FREE(obj->len, obj->trash_stack[UCL_TRASH_VALUE]);
	}
	/* Do not free ephemeral objects */
	if ((obj->flags & UCL_OBJECT_EPHEMERAL) == 0) {
		if (obj->type != UCL_USERDATA) {
			UCL_FREE(sizeof(ucl_object_t), obj);
		}
		else {
			struct ucl_object_userdata *ud = (struct ucl_object_userdata *) obj;
			if (ud->dtor) {
				ud->dtor(obj->value.ud);
			}
			UCL_FREE(sizeof(*ud), obj);
		}
	}
}

/*
 * Destroying a UCL tree is a graph traversal: releasing a container releases
 * its children, which may be containers themselves. Doing that on the C stack
 * makes cleanup depth proportional to the nesting depth of the document, so a
 * deeply nested (and possibly attacker supplied) input could exhaust the stack
 * while being freed - long after the parser had returned successfully.
 *
 * So the traversal is explicit instead. An object only reaches the worklist
 * once its last reference is gone, which means it is already detached from the
 * tree and nothing else can observe it; its `next` pointer is therefore free
 * for us to reuse as the worklist link. That keeps destruction allocation free
 * and its stack usage constant.
 */
struct ucl_dtor_worklist {
	ucl_object_t *head;
};

static inline void
ucl_dtor_worklist_push(struct ucl_dtor_worklist *wl, ucl_object_t *obj)
{
	obj->next = wl->head;
	wl->head = obj;
}

static inline ucl_object_t *
ucl_dtor_worklist_pop(struct ucl_dtor_worklist *wl)
{
	ucl_object_t *obj = wl->head;

	if (obj != NULL) {
		wl->head = obj->next;
		obj->next = NULL;
	}

	return obj;
}

/*
 * Drop one reference to `obj` and queue it for destruction if that was the
 * last one. `obj` must already be unlinked from whatever contained it.
 */
static void
ucl_dtor_worklist_unref(void *ptr, void *ud)
{
	ucl_object_t *obj = (ucl_object_t *) ptr;
	struct ucl_dtor_worklist *wl = (struct ucl_dtor_worklist *) ud;

	if (obj == NULL) {
		return;
	}

#ifdef HAVE_ATOMIC_BUILTINS
	if (__sync_sub_and_fetch(&obj->ref, 1) == 0) {
#else
	if (--obj->ref == 0) {
#endif
		ucl_dtor_worklist_push(wl, obj);
	}
}

/*
 * Queue every element of an implicit-array chain starting at `obj`. The chain
 * links are read before each element is unreffed, since unreffing may reuse
 * them for the worklist.
 */
static inline void
ucl_dtor_worklist_unref_chain(struct ucl_dtor_worklist *wl, ucl_object_t *obj)
{
	ucl_object_t *tmp;

	while (obj != NULL) {
		tmp = obj->next;
		ucl_dtor_worklist_unref(obj, wl);
		obj = tmp;
	}
}

/*
 * Release a whole subtree. `obj` must have already reached zero references;
 * with `allow_rec` its implicit-array siblings are released as well.
 */
static void
ucl_object_free_internal(ucl_object_t *obj, bool allow_rec)
{
	struct ucl_dtor_worklist wl = {NULL};
	ucl_object_t *cur, *sub, *tmp;

	if (obj == NULL) {
		return;
	}

	/*
	 * The head is dead already, so it goes straight onto the worklist; its
	 * siblings still hold a reference each and have to be unreffed properly.
	 * Save the link first, pushing overwrites it.
	 */
	tmp = obj->next;
	ucl_dtor_worklist_push(&wl, obj);

	if (allow_rec) {
		ucl_dtor_worklist_unref_chain(&wl, tmp);
	}

	while ((cur = ucl_dtor_worklist_pop(&wl)) != NULL) {
		if (cur->type == UCL_ARRAY) {
			UCL_ARRAY_GET(vec, cur);

			if (vec != NULL) {
				unsigned int i;

				for (i = 0; i < vec->n; i++) {
					sub = kv_A(*vec, i);
					ucl_dtor_worklist_unref_chain(&wl, sub);
				}

				kv_destroy(*vec);
				UCL_FREE(sizeof(*vec), vec);
			}

			cur->value.av = NULL;
		}
		else if (cur->type == UCL_OBJECT) {
			if (cur->value.ov != NULL) {
				ucl_hash_destroy(cur->value.ov, ucl_dtor_worklist_unref, &wl);
			}

			cur->value.ov = NULL;
		}

		ucl_object_dtor_free(cur);
	}
}

void ucl_array_detach_elements(ucl_object_t *ar)
{
	if (ar == NULL || ar->type != UCL_ARRAY) {
		return;
	}

	UCL_ARRAY_GET(vec, ar);

	if (vec != NULL) {
		vec->n = 0;
	}

	ar->len = 0;
}

void ucl_object_free(ucl_object_t *obj)
{
	/*
	 * Deprecated: refcount blind, use ucl_object_unref() instead.
	 *
	 * Release the whole subtree recursively (via the worklist based
	 * ucl_object_free_internal) rather than only the direct children.
	 * A shallow free drops the hash tables of nested containers and
	 * leaks their entire subtrees, so it must not be used here.
	 */
	ucl_object_free_internal(obj, true);
}

size_t
ucl_unescape_json_string(char *str, size_t len)
{
	char *t = str, *h = str;
	int i, uval;

	if (len <= 1) {
		return len;
	}
	/* t is target (tortoise), h is source (hare) */

	while (len) {
		if (*h == '\\') {
			h++;

			if (len == 1) {
				/*
				 * If \ is last, then do not try to go further
				 * Issue: #74
				 */
				len--;
				*t++ = '\\';
				continue;
			}

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
				h++; /* u character */
				len--;

				if (len > 3) {
					for (i = 0; i < 4; i++) {
						uval <<= 4;
						if (isdigit(h[i])) {
							uval += h[i] - '0';
						}
						else if (h[i] >= 'a' && h[i] <= 'f') {
							uval += h[i] - 'a' + 10;
						}
						else if (h[i] >= 'A' && h[i] <= 'F') {
							uval += h[i] - 'A' + 10;
						}
						else {
							break;
						}
					}

					/* Encode */
					if (uval < 0x80) {
						t[0] = (char) uval;
						t++;
					}
					else if (uval < 0x800) {
						t[0] = 0xC0 + ((uval & 0x7C0) >> 6);
						t[1] = 0x80 + ((uval & 0x03F));
						t += 2;
					}
					else if (uval < 0x10000) {
						t[0] = 0xE0 + ((uval & 0xF000) >> 12);
						t[1] = 0x80 + ((uval & 0x0FC0) >> 6);
						t[2] = 0x80 + ((uval & 0x003F));
						t += 3;
					}
#if 0
					/* It's not actually supported now */
					else if(uval <= 0x10FFFF) {
						t[0] = 0xF0 + ((uval & 0x1C0000) >> 18);
						t[1] = 0x80 + ((uval & 0x03F000) >> 12);
						t[2] = 0x80 + ((uval & 0x000FC0) >> 6);
						t[3] = 0x80 + ((uval & 0x00003F));
						t += 4;
					}
#endif
					else {
						*t++ = '?';
					}

					/* Consume 4 characters of source */
					h += 4;
					len -= 4;

					if (len > 0) {
						len--; /* for '\' character */
					}
					continue;
				}
				else {
					*t++ = 'u';
				}
				break;
			default:
				*t++ = *h;
				break;
			}
			h++;
			len--;
		}
		else {
			*t++ = *h++;
		}

		if (len > 0) {
			len--;
		}
	}
	*t = '\0';

	return (t - str);
}

size_t
ucl_unescape_squoted_string(char *str, size_t len)
{
	char *t = str, *h = str;

	if (len <= 1) {
		return len;
	}

	/* t is target (tortoise), h is source (hare) */

	while (len) {
		if (*h == '\\') {
			h++;

			if (len == 1) {
				/*
				 * If \ is last, then do not try to go further
				 * Issue: #74
				 */
				len--;
				*t++ = '\\';
				continue;
			}

			switch (*h) {
			case '\'':
				*t++ = '\'';
				break;
			case '\n':
				/* Ignore \<newline> style stuff */
				break;
			case '\r':
				/* Ignore \r and the following \n if needed */
				if (len > 1 && h[1] == '\n') {
					h++;
					len--;
				}
				break;
			default:
				/* Ignore \ */
				*t++ = '\\';
				*t++ = *h;
				break;
			}

			h++;
			len--;
		}
		else {
			*t++ = *h++;
		}

		if (len > 0) {
			len--;
		}
	}

	*t = '\0';

	return (t - str);
}

char *
ucl_copy_key_trash(const ucl_object_t *obj)
{
	ucl_object_t *deconst;
	size_t alloc_len;

	if (obj == NULL) {
		return NULL;
	}
	if (obj->trash_stack[UCL_TRASH_KEY] == NULL && obj->key != NULL) {
		deconst = __DECONST(ucl_object_t *, obj);
		if (ucl_size_add(obj->keylen, 1, &alloc_len)) {
			deconst->trash_stack[UCL_TRASH_KEY] = UCL_ALLOC(alloc_len);
		}
		if (deconst->trash_stack[UCL_TRASH_KEY] == NULL) {
			/* Keep the key as it is: the caller has to fail */
			return NULL;
		}
		memcpy(deconst->trash_stack[UCL_TRASH_KEY], obj->key, obj->keylen);
		deconst->trash_stack[UCL_TRASH_KEY][obj->keylen] = '\0';
		deconst->key = obj->trash_stack[UCL_TRASH_KEY];
		deconst->flags |= UCL_OBJECT_ALLOCATED_KEY;
	}

	return obj->trash_stack[UCL_TRASH_KEY];
}

void ucl_chunk_free(struct ucl_chunk *chunk)
{
	if (chunk) {
		struct ucl_parser_special_handler_chain *chain, *tmp;

		LL_FOREACH_SAFE(chunk->special_handlers, chain, tmp)
		{
			if (chain->special_handler->free_function) {
				chain->special_handler->free_function(
					chain->begin,
					chain->len,
					chain->special_handler->user_data);
			}
			else {
				UCL_FREE(chain->len, chain->begin);
			}

			UCL_FREE(sizeof(*chain), chain);
		}

		chunk->special_handlers = NULL;

		if (chunk->fname) {
			UCL_FREE(strlen(chunk->fname) + 1, chunk->fname);
		}

		UCL_FREE(sizeof(*chunk), chunk);
	}
}

char *
ucl_copy_value_trash(const ucl_object_t *obj)
{
	ucl_object_t *deconst;
	size_t alloc_len;

	if (obj == NULL) {
		return NULL;
	}
	if (obj->trash_stack[UCL_TRASH_VALUE] == NULL) {
		deconst = __DECONST(ucl_object_t *, obj);
		if (obj->type == UCL_STRING) {

			/* Special case for strings */
			if (obj->flags & UCL_OBJECT_BINARY) {
				deconst->trash_stack[UCL_TRASH_VALUE] = UCL_ALLOC(obj->len);
				if (deconst->trash_stack[UCL_TRASH_VALUE] != NULL) {
					memcpy(deconst->trash_stack[UCL_TRASH_VALUE],
						   obj->value.sv,
						   obj->len);
					deconst->value.sv = obj->trash_stack[UCL_TRASH_VALUE];
				}
			}
			else {
				if (ucl_size_add(obj->len, 1, &alloc_len)) {
					deconst->trash_stack[UCL_TRASH_VALUE] = UCL_ALLOC(alloc_len);
				}
				if (deconst->trash_stack[UCL_TRASH_VALUE] != NULL) {
					memcpy(deconst->trash_stack[UCL_TRASH_VALUE],
						   obj->value.sv,
						   obj->len);
					deconst->trash_stack[UCL_TRASH_VALUE][obj->len] = '\0';
					deconst->value.sv = obj->trash_stack[UCL_TRASH_VALUE];
				}
			}
		}
		else {
			/* Just emit value in json notation */
			deconst->trash_stack[UCL_TRASH_VALUE] = ucl_object_emit_single_json(obj);
			deconst->len = strlen(obj->trash_stack[UCL_TRASH_VALUE]);
		}
		deconst->flags |= UCL_OBJECT_ALLOCATED_VALUE;
	}

	return obj->trash_stack[UCL_TRASH_VALUE];
}

ucl_object_t *
ucl_parser_get_object(struct ucl_parser *parser)
{
	if (parser->state != UCL_STATE_ERROR && parser->top_obj != NULL) {
		return ucl_object_ref(parser->top_obj);
	}

	return NULL;
}

void ucl_parser_free(struct ucl_parser *parser)
{
	struct ucl_stack *stack, *stmp;
	struct ucl_macro *macro, *mtmp;
	struct ucl_chunk *chunk, *ctmp;
	struct ucl_pubkey *key, *ktmp;
	struct ucl_variable *var, *vtmp;
	ucl_object_t *tr, *trtmp;

	if (parser == NULL) {
		return;
	}

	if (parser->top_obj != NULL) {
		ucl_object_unref(parser->top_obj);
	}

	if (parser->includepaths != NULL) {
		ucl_object_unref(parser->includepaths);
	}

	LL_FOREACH_SAFE(parser->stack, stack, stmp)
	{
		UCL_FREE(sizeof(struct ucl_stack), stack);
	}
	HASH_ITER(hh, parser->macroes, macro, mtmp)
	{
		UCL_FREE(strlen(macro->name) + 1, macro->name);
		HASH_DEL(parser->macroes, macro);
		UCL_FREE(sizeof(struct ucl_macro), macro);
	}
	LL_FOREACH_SAFE(parser->chunks, chunk, ctmp)
	{
		ucl_chunk_free(chunk);
	}
	LL_FOREACH_SAFE(parser->keys, key, ktmp)
	{
		UCL_FREE(sizeof(struct ucl_pubkey), key);
	}
	LL_FOREACH_SAFE(parser->variables, var, vtmp)
	{
		UCL_FREE(var->value_len + 1, var->value);
		UCL_FREE(var->var_len + 1, var->var);
		UCL_FREE(sizeof(struct ucl_variable), var);
	}
	LL_FOREACH_SAFE(parser->trash_objs, tr, trtmp)
	{
		/*
		 * A discarded value may hold a whole subtree. The trash list uses
		 * its implicit-array links, so detach it before releasing it.
		 */
		tr->next = NULL;
		tr->prev = tr;
		ucl_object_unref(tr);
	}

	if (parser->err != NULL) {
		utstring_free(parser->err);
	}

	if (parser->cur_file) {
		UCL_FREE(strlen(parser->cur_file) + 1, parser->cur_file);
	}

	if (parser->comments) {
		ucl_object_unref(parser->comments);
	}

	if (parser->last_comment) {
		/* saved but never attached: parsing stopped with an error */
		ucl_object_unref(parser->last_comment);
	}

	if (parser->pending_merge) {
		/* the value of its key never came */
		ucl_object_unref(parser->pending_merge);
	}

	UCL_FREE(sizeof(struct ucl_parser), parser);
}

const char *
ucl_parser_get_error(struct ucl_parser *parser)
{
	if (parser == NULL) {
		return NULL;
	}

	if (parser->err == NULL) {
		return NULL;
	}

	return utstring_body(parser->err);
}

int ucl_parser_get_error_code(struct ucl_parser *parser)
{
	if (parser == NULL) {
		return 0;
	}

	return parser->err_code;
}

unsigned
ucl_parser_get_column(struct ucl_parser *parser)
{
	if (parser == NULL || parser->chunks == NULL) {
		return 0;
	}

	return parser->chunks->column;
}

unsigned
ucl_parser_get_linenum(struct ucl_parser *parser)
{
	if (parser == NULL || parser->chunks == NULL) {
		return 0;
	}

	return parser->chunks->line;
}

void ucl_parser_clear_error(struct ucl_parser *parser)
{
	if (parser != NULL && parser->err != NULL) {
		utstring_free(parser->err);
		parser->err = NULL;
		parser->err_code = 0;
	}
}

bool ucl_pubkey_add(struct ucl_parser *parser, const unsigned char *key, size_t len)
{
#ifndef HAVE_OPENSSL
	ucl_create_err(&parser->err, "cannot check signatures without openssl");
	return false;
#else
#if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	ucl_create_err(&parser->err, "cannot check signatures, openssl version is unsupported");
	return false;
#else
	struct ucl_pubkey *nkey;
	BIO *mem;

	mem = BIO_new_mem_buf((void *) key, len);
	nkey = UCL_ALLOC(sizeof(struct ucl_pubkey));
	if (nkey == NULL) {
		ucl_create_err(&parser->err, "cannot allocate memory for key");
		return false;
	}
	nkey->key = PEM_read_bio_PUBKEY(mem, &nkey->key, NULL, NULL);
	BIO_free(mem);
	if (nkey->key == NULL) {
		UCL_FREE(sizeof(struct ucl_pubkey), nkey);
		ucl_create_err(&parser->err, "%s",
					   ERR_error_string(ERR_get_error(), NULL));
		return false;
	}
	LL_PREPEND(parser->keys, nkey);
#endif
#endif
	return true;
}

void ucl_parser_add_special_handler(struct ucl_parser *parser,
									struct ucl_parser_special_handler *handler)
{
	LL_APPEND(parser->special_handlers, handler);
}

#ifdef CURL_FOUND
struct ucl_curl_cbdata {
	unsigned char *buf;
	size_t buflen;
};

static size_t
ucl_curl_write_callback(void *contents, size_t size, size_t nmemb, void *ud)
{
	struct ucl_curl_cbdata *cbdata = ud;
	size_t realsize, newlen;
	unsigned char *nbuf;

	if (!ucl_size_mul(size, nmemb, &realsize) ||
		!ucl_size_add(cbdata->buflen, realsize, &newlen) ||
		!ucl_size_add(newlen, 1, &newlen)) {
		return 0;
	}

	/* On failure the old buffer stays in cbdata and is freed by the caller */
	nbuf = UCL_REALLOC(cbdata->buf, newlen);
	if (nbuf == NULL) {
		return 0;
	}
	cbdata->buf = nbuf;

	memcpy(&(cbdata->buf[cbdata->buflen]), contents, realsize);
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
bool ucl_fetch_url(const unsigned char *url, unsigned char **buf, size_t *buflen,
				   UT_string **err, bool must_exist)
{

#ifdef HAVE_FETCH_H
	struct url *fetch_url;
	struct url_stat us;
	FILE *in;

	fetch_url = fetchParseURL(url);
	if (fetch_url == NULL) {
		ucl_create_err(err, "invalid URL %s: %s",
					   url, strerror(errno));
		return false;
	}
	if ((in = fetchXGet(fetch_url, &us, "")) == NULL) {
		if (must_exist) {
			ucl_create_err(err, "cannot fetch URL %s: %s",
						   url, strerror(errno));
		}
		fetchFreeURL(fetch_url);
		return false;
	}

	*buflen = us.size;
	*buf = UCL_ALLOC(*buflen);
	if (*buf == NULL) {
		ucl_create_err(err, "cannot allocate buffer for URL %s: %s",
					   url, strerror(errno));
		fclose(in);
		fetchFreeURL(fetch_url);
		return false;
	}

	if (fread(*buf, *buflen, 1, in) != 1) {
		ucl_create_err(err, "cannot read URL %s: %s",
					   url, strerror(errno));
		UCL_FREE(*buflen, *buf);
		*buf = NULL;
		fclose(in);
		fetchFreeURL(fetch_url);
		return false;
	}

	fclose(in);
	fetchFreeURL(fetch_url);
	return true;
#elif defined(CURL_FOUND)
	CURL *curl;
	int r;
	struct ucl_curl_cbdata cbdata;

	curl = curl_easy_init();
	if (curl == NULL) {
		ucl_create_err(err, "CURL interface is broken");
		return false;
	}
	if ((r = curl_easy_setopt(curl, CURLOPT_URL, url)) != CURLE_OK) {
		ucl_create_err(err, "invalid URL %s: %s",
					   url, curl_easy_strerror(r));
		curl_easy_cleanup(curl);
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ucl_curl_write_callback);
	cbdata.buf = NULL;
	cbdata.buflen = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cbdata);

	if ((r = curl_easy_perform(curl)) != CURLE_OK) {
		if (must_exist) {
			ucl_create_err(err, "error fetching URL %s: %s",
						   url, curl_easy_strerror(r));
		}
		curl_easy_cleanup(curl);
		if (cbdata.buf) {
			UCL_FREE(cbdata.buflen, cbdata.buf);
		}
		return false;
	}
	*buf = cbdata.buf;
	*buflen = cbdata.buflen;

	curl_easy_cleanup(curl);

	return true;
#else
	ucl_create_err(err, "URL support is disabled");
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
bool ucl_fetch_file(const unsigned char *filename, unsigned char **buf, size_t *buflen,
					UT_string **err, bool must_exist)
{
	int fd;
	struct stat st;
	if ((fd = open(filename, O_RDONLY)) == -1) {
		ucl_create_err(err, "cannot open file %s: %s",
					   filename, strerror(errno));
		return false;
	}

	if (fstat(fd, &st) == -1) {
		if (must_exist || errno == EPERM) {
			ucl_create_err(err, "cannot stat file %s: %s",
						   filename, strerror(errno));
		}
		close(fd);

		return false;
	}
	if (!S_ISREG(st.st_mode)) {
		if (must_exist) {
			ucl_create_err(err, "file %s is not a regular file", filename);
		}
		close(fd);

		return false;
	}

	if (st.st_size == 0) {
		/* Do not map empty files */
		*buf = NULL;
		*buflen = 0;
	}
	else {
		if ((*buf = ucl_mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
			close(fd);
			ucl_create_err(err, "cannot mmap file %s: %s",
						   filename, strerror(errno));
			*buf = NULL;

			return false;
		}
		*buflen = st.st_size;
	}

	close(fd);

	return true;
}


#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
static inline bool
ucl_sig_check(const unsigned char *data, size_t datalen,
			  const unsigned char *sig, size_t siglen, struct ucl_parser *parser)
{
	struct ucl_pubkey *key;
	char dig[EVP_MAX_MD_SIZE];
	unsigned int diglen;
	EVP_PKEY_CTX *key_ctx;
	EVP_MD_CTX *sign_ctx = NULL;

	sign_ctx = EVP_MD_CTX_create();

	LL_FOREACH(parser->keys, key)
	{
		key_ctx = EVP_PKEY_CTX_new(key->key, NULL);
		if (key_ctx != NULL) {
			if (EVP_PKEY_verify_init(key_ctx) <= 0) {
				EVP_PKEY_CTX_free(key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_rsa_padding(key_ctx, RSA_PKCS1_PADDING) <= 0) {
				EVP_PKEY_CTX_free(key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_signature_md(key_ctx, EVP_sha256()) <= 0) {
				EVP_PKEY_CTX_free(key_ctx);
				continue;
			}
			EVP_DigestInit(sign_ctx, EVP_sha256());
			EVP_DigestUpdate(sign_ctx, data, datalen);
			EVP_DigestFinal(sign_ctx, dig, &diglen);

			if (EVP_PKEY_verify(key_ctx, sig, siglen, dig, diglen) == 1) {
				EVP_MD_CTX_destroy(sign_ctx);
				EVP_PKEY_CTX_free(key_ctx);
				return true;
			}

			EVP_PKEY_CTX_free(key_ctx);
		}
	}

	EVP_MD_CTX_destroy(sign_ctx);

	return false;
}
#endif

struct ucl_include_params {
	bool check_signature;
	bool must_exist;
	bool use_glob;
	bool use_prefix;
	bool soft_fail;
	bool allow_glob;
	/* An included file has been read, but has failed to parse */
	bool parse_failed;
	unsigned priority;
	enum ucl_duplicate_strategy strat;
	enum ucl_parse_type parse_type;
	const char *prefix;
	const char *target;
};

/**
 * Include an url to configuration
 * @param data
 * @param len
 * @param parser
 * @param err
 * @return
 */
static bool
ucl_include_url(const unsigned char *data, size_t len,
				struct ucl_parser *parser,
				struct ucl_include_params *params)
{

	bool res;
	unsigned char *buf = NULL;
	size_t buflen = 0;
	struct ucl_chunk *chunk;
	char urlbuf[PATH_MAX];
	int prev_state;

	snprintf(urlbuf, sizeof(urlbuf), "%.*s", (int) len, data);

	if (!ucl_fetch_url(urlbuf, &buf, &buflen, &parser->err, params->must_exist)) {
		if (!params->must_exist) {
			ucl_parser_clear_error(parser);
		}
		return !params->must_exist;
	}

	if (params->check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		unsigned char *sigbuf = NULL;
		size_t siglen = 0;
		/* We need to check signature first */
		snprintf(urlbuf, sizeof(urlbuf), "%.*s.sig", (int) len, data);
		if (!ucl_fetch_url(urlbuf, &sigbuf, &siglen, &parser->err, true)) {
			UCL_FREE(buflen, buf);
			return false;
		}
		if (!ucl_sig_check(buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err(&parser->err, "cannot verify url %s: %s",
						   urlbuf,
						   ERR_error_string(ERR_get_error(), NULL));
			if (siglen > 0) {
				UCL_FREE(siglen, sigbuf);
			}
			UCL_FREE(buflen, buf);
			return false;
		}
		if (siglen > 0) {
			UCL_FREE(siglen, sigbuf);
		}
#else
		ucl_create_err(&parser->err, "cannot verify url %.*s: "
									 "libucl is built without signatures support",
					   (int) len, data);
		UCL_FREE(buflen, buf);
		return false;
#endif
	}

	prev_state = parser->state;
	parser->state = UCL_STATE_INIT;

	res = ucl_parser_add_chunk_full(parser, buf, buflen, params->priority,
									params->strat, params->parse_type);
	if (res == true) {
		/* Remove chunk from the stack */
		chunk = parser->chunks;
		if (chunk != NULL) {
			parser->chunks = chunk->next;
			ucl_chunk_free(chunk);
			parser->recursion--;
		}
	}

	parser->state = prev_state;
	UCL_FREE(buflen, buf);

	return res;
}

/*
 * Hand the file context of the including document back to the parser once an
 * included file is done with, whether it has been parsed or not
 */
static void
ucl_include_restore_filevars(struct ucl_parser *parser, char *old_curfile,
							 struct ucl_variable *old_curdir,
							 struct ucl_variable *old_filename)
{
	struct ucl_variable *cur_var, *tmp_var;

	if (parser->cur_file) {
		UCL_FREE(strlen(parser->cur_file) + 1, parser->cur_file);
	}

	parser->cur_file = old_curfile;
	DL_FOREACH_SAFE(parser->variables, cur_var, tmp_var)
	{
		if (strcmp(cur_var->var, "CURDIR") == 0 && old_curdir) {
			DL_DELETE(parser->variables, cur_var);
			UCL_FREE(cur_var->var_len + 1, cur_var->var);
			UCL_FREE(cur_var->value_len + 1, cur_var->value);
			UCL_FREE(sizeof(struct ucl_variable), cur_var);
		}
		else if (strcmp(cur_var->var, "FILENAME") == 0 && old_filename) {
			DL_DELETE(parser->variables, cur_var);
			UCL_FREE(cur_var->var_len + 1, cur_var->var);
			UCL_FREE(cur_var->value_len + 1, cur_var->value);
			UCL_FREE(sizeof(struct ucl_variable), cur_var);
		}
	}
	if (old_filename) {
		DL_APPEND(parser->variables, old_filename);
	}
	if (old_curdir) {
		DL_APPEND(parser->variables, old_curdir);
	}
}

/*
 * Fail a prefixed include on an allocation failure, once what it has
 * allocated for the prefix has been released
 */
static bool
ucl_include_enomem(struct ucl_parser *parser, unsigned char *buf,
				   size_t buflen, char *old_curfile,
				   struct ucl_variable *old_curdir,
				   struct ucl_variable *old_filename)
{
	ucl_create_err(&parser->err, "cannot allocate memory for an object");
	if (buf) {
		ucl_munmap(buf, buflen);
	}

	ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

	return false;
}

/**
 * Include a single file to the parser
 * @param data
 * @param len
 * @param parser
 * @param check_signature
 * @param must_exist
 * @param allow_glob
 * @param priority
 * @return
 */
static bool
ucl_include_file_single(const unsigned char *data, size_t len,
						struct ucl_parser *parser, struct ucl_include_params *params)
{
	bool res;
	struct ucl_chunk *chunk, *outer;
	unsigned char *buf = NULL;
	char *old_curfile, *ext;
	size_t buflen = 0;
	char filebuf[PATH_MAX], realbuf[PATH_MAX];
	int prev_state;
	struct ucl_variable *cur_var, *tmp_var, *old_curdir = NULL,
											*old_filename = NULL;
	ucl_object_t *nest_obj = NULL, *old_obj = NULL, *new_obj = NULL;
	ucl_hash_t *container = NULL, *nhash;
	struct ucl_stack *st = NULL, *cur_st;

	if (parser->state == UCL_STATE_ERROR) {
		/* Return immediately if we are in the error state... */
		return false;
	}

	snprintf(filebuf, sizeof(filebuf), "%.*s", (int) len, data);
	if (ucl_realpath(filebuf, realbuf) == NULL) {
		if (params->soft_fail) {
			return false;
		}
		if (!params->must_exist && errno != EPERM) {
			return true;
		}

		ucl_create_err(&parser->err, "cannot open file %s: %s",
					   filebuf,
					   strerror(errno));
		return false;
	}

	if (parser->cur_file && strcmp(realbuf, parser->cur_file) == 0) {
		/* We are likely including the file itself */
		if (params->soft_fail) {
			return false;
		}

		ucl_create_err(&parser->err, "trying to include the file %s from itself",
					   realbuf);
		return false;
	}

	if (!ucl_fetch_file(realbuf, &buf, &buflen, &parser->err, params->must_exist)) {
		if (params->soft_fail) {
			return false;
		}

		if (params->must_exist || parser->err != NULL) {
			/* The case of fatal errors */
			return false;
		}

		ucl_parser_clear_error(parser);

		return true;
	}

	if (params->check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		unsigned char *sigbuf = NULL;
		size_t siglen = 0;
		/* We need to check signature first */
		snprintf(filebuf, sizeof(filebuf), "%s.sig", realbuf);
		if (!ucl_fetch_file(filebuf, &sigbuf, &siglen, &parser->err, true)) {
			if (buf) {
				ucl_munmap(buf, buflen);
			}

			return false;
		}
		if (!ucl_sig_check(buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err(&parser->err, "cannot verify file %s: %s",
						   filebuf,
						   ERR_error_string(ERR_get_error(), NULL));
			if (sigbuf) {
				ucl_munmap(sigbuf, siglen);
			}
			if (buf) {
				ucl_munmap(buf, buflen);
			}

			return false;
		}

		if (sigbuf) {
			ucl_munmap(sigbuf, siglen);
		}
#else
		ucl_create_err(&parser->err, "cannot verify file %s: "
									 "libucl is built without signatures support",
					   realbuf);
		if (buf) {
			ucl_munmap(buf, buflen);
		}

		return false;
#endif
	}

	old_curfile = parser->cur_file;
	parser->cur_file = NULL;

	/* Store old file vars */
	DL_FOREACH_SAFE(parser->variables, cur_var, tmp_var)
	{
		if (strcmp(cur_var->var, "CURDIR") == 0) {
			old_curdir = cur_var;
			DL_DELETE(parser->variables, cur_var);
		}
		else if (strcmp(cur_var->var, "FILENAME") == 0) {
			old_filename = cur_var;
			DL_DELETE(parser->variables, cur_var);
		}
	}

	ucl_parser_set_filevars(parser, realbuf, false);

	prev_state = parser->state;
	parser->state = UCL_STATE_INIT;

	if (params->use_prefix && params->prefix == NULL) {
		/* Auto generate a key name based on the included filename */
		params->prefix = basename(realbuf);
		ext = strrchr(params->prefix, '.');
		if (ext != NULL && (strcmp(ext, ".conf") == 0 || strcmp(ext, ".ucl") == 0)) {
			/* Strip off .conf or .ucl */
			*ext = '\0';
		}
	}
	if (params->prefix != NULL) {
		/* This is a prefixed include */
		container = parser->stack->obj->value.ov;

		old_obj = __DECONST(ucl_object_t *, ucl_hash_search(container,
															params->prefix, strlen(params->prefix)));

		if (strcasecmp(params->target, "array") == 0) {
			if (old_obj == NULL) {
				/* Create an array with key: prefix */
				old_obj = ucl_object_new_full(UCL_ARRAY, params->priority);
				nest_obj = ucl_object_new_full(UCL_OBJECT, params->priority);
				if (old_obj != NULL) {
					old_obj->key = params->prefix;
					old_obj->keylen = strlen(params->prefix);
				}

				if (old_obj == NULL || nest_obj == NULL ||
					ucl_copy_key_trash(old_obj) == NULL) {
					ucl_create_err(&parser->err,
								   "cannot allocate memory for an object");
					if (buf) {
						ucl_munmap(buf, buflen);
					}

					ucl_object_unref(old_obj);
					ucl_object_unref(nest_obj);
					ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

					return false;
				}

				old_obj->prev = old_obj;
				old_obj->next = NULL;
				nest_obj->prev = nest_obj;
				nest_obj->next = NULL;

				if (!ucl_array_append(old_obj, nest_obj)) {
					ucl_object_unref(nest_obj);
					ucl_object_unref(old_obj);

					return ucl_include_enomem(parser, buf, buflen, old_curfile,
											  old_curdir, old_filename);
				}

				nhash = ucl_hash_insert_object(container, old_obj,
											   parser->flags & UCL_PARSER_KEY_LOWERCASE);
				if (nhash == NULL) {
					/* nest_obj goes along with it */
					ucl_object_unref(old_obj);

					return ucl_include_enomem(parser, buf, buflen, old_curfile,
											  old_curdir, old_filename);
				}

				container = nhash;
				parser->stack->obj->len++;
			}
			else {
				if (ucl_object_type(old_obj) == UCL_ARRAY) {
					/* Append to the existing array */
					nest_obj = ucl_object_new_full(UCL_OBJECT,
												   params->priority);
					if (nest_obj == NULL) {
						ucl_create_err(&parser->err,
									   "cannot allocate memory for an object");
						if (buf) {
							ucl_munmap(buf, buflen);
						}

						ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

						return false;
					}
					nest_obj->prev = nest_obj;
					nest_obj->next = NULL;

					if (!ucl_array_append(old_obj, nest_obj)) {
						ucl_object_unref(nest_obj);

						return ucl_include_enomem(parser, buf, buflen, old_curfile,
												  old_curdir, old_filename);
					}
				}
				else {
					/* Convert the object to an array */
					new_obj = ucl_object_typed_new(UCL_ARRAY);
					if (new_obj == NULL) {
						ucl_create_err(&parser->err,
									   "cannot allocate memory for an object");
						if (buf) {
							ucl_munmap(buf, buflen);
						}

						ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

						return false;
					}
					new_obj->key = old_obj->key;
					new_obj->keylen = old_obj->keylen;
					new_obj->flags |= UCL_OBJECT_MULTIVALUE;
					new_obj->prev = new_obj;
					new_obj->next = NULL;

					nest_obj = ucl_object_new_full(UCL_OBJECT,
												   params->priority);
					if (nest_obj == NULL) {
						ucl_create_err(&parser->err,
									   "cannot allocate memory for an object");
						if (buf) {
							ucl_munmap(buf, buflen);
						}

						ucl_object_unref(new_obj);

						ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

						return false;
					}
					nest_obj->prev = nest_obj;
					nest_obj->next = NULL;

					if (!ucl_array_append(new_obj, old_obj) ||
						!ucl_array_append(new_obj, nest_obj)) {
						/* old_obj stays in the container */
						ucl_array_detach_elements(new_obj);
						ucl_object_unref(new_obj);
						ucl_object_unref(nest_obj);

						return ucl_include_enomem(parser, buf, buflen, old_curfile,
												  old_curdir, old_filename);
					}

					ucl_hash_replace(container, old_obj, new_obj);
				}
			}
		}
		else {
			/* Case of object */
			if (old_obj == NULL) {
				/* Create an object with key: prefix */
				nest_obj = ucl_object_new_full(UCL_OBJECT, params->priority);
				if (nest_obj != NULL) {
					nest_obj->key = params->prefix;
					nest_obj->keylen = strlen(params->prefix);
				}

				if (nest_obj == NULL || ucl_copy_key_trash(nest_obj) == NULL) {
					ucl_create_err(&parser->err, "cannot allocate memory for an object");
					if (buf) {
						ucl_munmap(buf, buflen);
					}

					ucl_object_unref(nest_obj);
					ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

					return false;
				}

				nest_obj->prev = nest_obj;
				nest_obj->next = NULL;

				nhash = ucl_hash_insert_object(container, nest_obj,
											   parser->flags & UCL_PARSER_KEY_LOWERCASE);
				if (nhash == NULL) {
					ucl_object_unref(nest_obj);

					return ucl_include_enomem(parser, buf, buflen, old_curfile,
											  old_curdir, old_filename);
				}

				container = nhash;
				parser->stack->obj->len++;
			}
			else {
				if (ucl_object_type(old_obj) == UCL_OBJECT) {
					/* Append to existing Object*/
					nest_obj = old_obj;
				}
				else {
					/* The key is not an object */
					ucl_create_err(&parser->err,
								   "Conflicting type for key: %s, asked %s, has %s",
								   params->prefix, params->target,
								   ucl_object_type_to_string(ucl_object_type(old_obj)));
					if (buf) {
						ucl_munmap(buf, buflen);
					}

					ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

					return false;
				}
			}
		}


		/* Put all of the content of the include inside that object */
		parser->stack->obj->value.ov = container;

		st = UCL_ALLOC(sizeof(struct ucl_stack));
		if (st == NULL) {
			/* nest_obj is in the tree already, which owns it */
			ucl_create_err(&parser->err, "cannot allocate memory for an object");

			if (buf) {
				ucl_munmap(buf, buflen);
			}

			ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

			return false;
		}
		st->obj = nest_obj;
		st->e.params.level = parser->stack->e.params.level;
		/*
		 * No brace opens this frame, so none may close it: with the flags of
		 * the enclosing frame, a } in the included file popped and freed it,
		 * and the pop below then used it after free.
		 */
		st->e.params.flags = 0;
		st->e.params.line = parser->stack->e.params.line;
		st->chunk = parser->chunks;
		LL_PREPEND(parser->stack, st);
		parser->cur_depth++;
		parser->cur_obj = nest_obj;
	}

	outer = parser->chunks;
	res = ucl_parser_add_chunk_full(parser, buf, buflen, params->priority,
									params->strat, params->parse_type);

	if (res) {
		/* Stop nesting the include, take 1 level off the stack */
		if (params->prefix != NULL && nest_obj != NULL) {
			ucl_parser_pop_container(parser, st);
		}

		parser->state = prev_state;
	}
	else if (parser->chunks != outer) {
		/* Its chunk is there: the file has been read, but does not parse */
		params->parse_failed = true;
	}

	/*
	 * Remove the chunk of the included file, which points to the buffer
	 * unmapped below, whether the file has parsed or not. A failed parse
	 * leaves it on top, along with the chunks of includes that failed in it:
	 * the including file read the unmapped buffer of the failed one when a
	 * .try_include glob went on with the parse, and a successful include
	 * after it removed that chunk instead of its own. Frames left open by a
	 * removed chunk now belong to the including one, as for an inserted chunk.
	 */
	while (parser->chunks != NULL && parser->chunks != outer) {
		chunk = parser->chunks;
		parser->chunks = chunk->next;
		LL_FOREACH(parser->stack, cur_st)
		{
			if (cur_st->chunk == chunk) {
				cur_st->chunk = outer;
			}
		}
		ucl_chunk_free(chunk);
		parser->recursion--;
	}

	/* Restore old file vars */
	ucl_include_restore_filevars(parser, old_curfile, old_curdir, old_filename);

	if (buflen > 0) {
		ucl_munmap(buf, buflen);
	}

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
ucl_include_file(const unsigned char *data, size_t len,
				 struct ucl_parser *parser,
				 struct ucl_include_params *params,
				 const ucl_object_t *args)
{
	const unsigned char *p = data, *end = data + len;
	bool need_glob = false;
	int cnt = 0;
	char glob_pattern[PATH_MAX];
	size_t i;

#ifndef _WIN32
	if (!params->allow_glob) {
		return ucl_include_file_single(data, len, parser, params);
	}
	else {
		/* Check for special symbols in a filename */
		while (p != end) {
			if (*p == '*' || *p == '?') {
				need_glob = true;
				break;
			}
			p++;
		}
		if (need_glob) {
			glob_t globbuf;
			memset(&globbuf, 0, sizeof(globbuf));
			ucl_strlcpy(glob_pattern, (const char *) data,
						(len + 1 < sizeof(glob_pattern) ? len + 1 : sizeof(glob_pattern)));
			if (glob(glob_pattern, 0, NULL, &globbuf) != 0) {
				return (!params->must_exist || false);
			}
			for (i = 0; i < globbuf.gl_pathc; i++) {

				if (parser->include_trace_func) {
					const ucl_object_t *parent = NULL;

					if (parser->stack) {
						parent = parser->stack->obj;
					}

					parser->include_trace_func(parser, parent, NULL,
											   globbuf.gl_pathv[i],
											   strlen(globbuf.gl_pathv[i]),
											   parser->include_trace_ud);
				}

				if (!ucl_include_file_single((unsigned char *) globbuf.gl_pathv[i],
											 strlen(globbuf.gl_pathv[i]), parser, params)) {
					if (params->soft_fail && !params->parse_failed) {
						/*
						 * Skip a file that cannot be included, but not one
						 * that fails to parse, as for .try_include of that
						 * file alone: the parse cannot go on from where the
						 * file has left it
						 */
						continue;
					}
					globfree(&globbuf);
					return false;
				}
				cnt++;
			}
			globfree(&globbuf);

			if (cnt == 0 && params->must_exist) {
				ucl_create_err(&parser->err, "cannot match any files for pattern %s",
							   glob_pattern);
				return false;
			}
		}
		else {
			return ucl_include_file_single(data, len, parser, params);
		}
	}
#else
	/* Win32 compilers do not support globbing. Therefore, for Win32,
	   treat allow_glob/need_glob as a NOOP and just return */
	return ucl_include_file_single(data, len, parser, params);
#endif

	return true;
}

/**
 * Common function to handle .*include* macros
 * @param data
 * @param len
 * @param args
 * @param parser
 * @param default_try
 * @param default_sign
 * @return
 */
static bool
ucl_include_common(const unsigned char *data, size_t len,
				   const ucl_object_t *args, struct ucl_parser *parser,
				   bool default_try,
				   bool default_sign)
{
	bool allow_url = false, search = false;
	const char *duplicate;
	const ucl_object_t *param;
	ucl_object_iter_t it = NULL, ip = NULL;
	int args_err = 0;
	char ipath[PATH_MAX];
	struct ucl_include_params params;

	/* Default values */
	params.soft_fail = default_try;
	params.allow_glob = false;
	params.parse_failed = false;
	params.check_signature = default_sign;
	params.use_prefix = false;
	params.target = "object";
	params.prefix = NULL;
	params.priority = 0;
	params.parse_type = UCL_PARSE_UCL;
	params.strat = UCL_DUPLICATE_APPEND;
	params.must_exist = !default_try;

	if (parser->include_trace_func) {
		const ucl_object_t *parent = NULL;

		if (parser->stack) {
			parent = parser->stack->obj;
		}

		parser->include_trace_func(parser, parent, args,
								   data, len, parser->include_trace_ud);
	}

	/* Process arguments */
	if (args != NULL && args->type == UCL_OBJECT) {
		while ((param = ucl_object_iterate_with_error(args, &it, true,
														  &args_err)) != NULL) {
			if (param->type == UCL_BOOLEAN) {
				if (strncmp(param->key, "try", param->keylen) == 0) {
					params.must_exist = !ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "sign", param->keylen) == 0) {
					params.check_signature = ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "glob", param->keylen) == 0) {
					params.allow_glob = ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "url", param->keylen) == 0) {
					allow_url = ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "prefix", param->keylen) == 0) {
					params.use_prefix = ucl_object_toboolean(param);
				}
			}
			else if (param->type == UCL_STRING) {
				if (strncmp(param->key, "key", param->keylen) == 0) {
					params.prefix = ucl_object_tostring(param);
				}
				else if (strncmp(param->key, "target", param->keylen) == 0) {
					params.target = ucl_object_tostring(param);
				}
				else if (strncmp(param->key, "duplicate", param->keylen) == 0) {
					duplicate = ucl_object_tostring(param);

					if (strcmp(duplicate, "append") == 0) {
						params.strat = UCL_DUPLICATE_APPEND;
					}
					else if (strcmp(duplicate, "merge") == 0) {
						params.strat = UCL_DUPLICATE_MERGE;
					}
					else if (strcmp(duplicate, "rewrite") == 0) {
						params.strat = UCL_DUPLICATE_REWRITE;
					}
					else if (strcmp(duplicate, "error") == 0) {
						params.strat = UCL_DUPLICATE_ERROR;
					}
				}
			}
			else if (param->type == UCL_ARRAY) {
				if (strncmp(param->key, "path", param->keylen) == 0) {
					ucl_set_include_path(parser, __DECONST(ucl_object_t *, param));
				}
			}
			else if (param->type == UCL_INT) {
				if (strncmp(param->key, "priority", param->keylen) == 0) {
					params.priority = ucl_object_toint(param);
				}
			}
		}

		if (args_err == ENOMEM) {
			/* The arguments must not be dropped silently */
			ucl_create_err(&parser->err, "cannot allocate memory for macro arguments");
			return false;
		}
	}

	if (parser->includepaths == NULL) {
		if (allow_url && ucl_strnstr(data, "://", len) != NULL) {
			/* Globbing is not used for URL's */
			return ucl_include_url(data, len, parser, &params);
		}
		else if (data != NULL) {
			/* Try to load a file */
			if (ucl_include_file(data, len, parser, &params, args)) {
				return true;
			}

			/*
			 * .try_include of a file that cannot be included goes on with
			 * the parse: only a file that has been read and does not parse
			 * stops it
			 */
			return params.soft_fail && !params.parse_failed &&
				   parser->err == NULL;
		}
	}
	else {
		if (allow_url && ucl_strnstr(data, "://", len) != NULL) {
			/* Globbing is not used for URL's */
			return ucl_include_url(data, len, parser, &params);
		}

		ip = ucl_object_iterate_new(parser->includepaths);
		while ((param = ucl_object_iterate_safe(ip, true)) != NULL) {
			if (ucl_object_type(param) == UCL_STRING) {
				snprintf(ipath, sizeof(ipath), "%s/%.*s", ucl_object_tostring(param),
						 (int) len, data);
				if ((search = ucl_include_file(ipath, strlen(ipath),
											   parser, &params, args))) {
					if (!params.allow_glob) {
						break;
					}
				}
				else if (params.parse_failed) {
					/* The file is found, so the search ends with its error */
					break;
				}
			}
		}
		ucl_object_iterate_free(ip);
		if (search == true) {
			return true;
		}
		else {
			ucl_create_err(&parser->err,
						   "cannot find file: %.*s in search path",
						   (int) len, data);
			return false;
		}
	}

	return false;
}

/**
 * Handle include macro
 * @param data include data
 * @param len length of data
 * @param args UCL object representing arguments to the macro
 * @param ud user data
 * @return
 */
bool ucl_include_handler(const unsigned char *data, size_t len,
						 const ucl_object_t *args, void *ud)
{
	struct ucl_parser *parser = ud;

	return ucl_include_common(data, len, args, parser, false, false);
}

/**
 * Handle includes macro
 * @param data include data
 * @param len length of data
 * @param args UCL object representing arguments to the macro
 * @param ud user data
 * @return
 */
bool ucl_includes_handler(const unsigned char *data, size_t len,
						  const ucl_object_t *args, void *ud)
{
	struct ucl_parser *parser = ud;

	return ucl_include_common(data, len, args, parser, false, true);
}

/**
 * Handle tryinclude macro
 * @param data include data
 * @param len length of data
 * @param args UCL object representing arguments to the macro
 * @param ud user data
 * @return
 */
bool ucl_try_include_handler(const unsigned char *data, size_t len,
							 const ucl_object_t *args, void *ud)
{
	struct ucl_parser *parser = ud;

	return ucl_include_common(data, len, args, parser, true, false);
}

/**
 * Handle priority macro
 * @param data include data
 * @param len length of data
 * @param args UCL object representing arguments to the macro
 * @param ud user data
 * @return
 */
bool ucl_priority_handler(const unsigned char *data, size_t len,
						  const ucl_object_t *args, void *ud)
{
	struct ucl_parser *parser = ud;
	unsigned priority = 255;
	const ucl_object_t *param;
	bool found = false;
	char *value = NULL, *leftover = NULL;
	ucl_object_iter_t it = NULL;
	int args_err = 0;
	size_t alloc_len;

	if (parser == NULL) {
		return false;
	}

	/* Process arguments */
	if (args != NULL && args->type == UCL_OBJECT) {
		while ((param = ucl_object_iterate_with_error(args, &it, true,
														  &args_err)) != NULL) {
			if (param->type == UCL_INT) {
				if (strncmp(param->key, "priority", param->keylen) == 0) {
					priority = ucl_object_toint(param);
					found = true;
				}
			}
		}

		if (args_err == ENOMEM) {
			/* The arguments must not be dropped silently */
			ucl_create_err(&parser->err, "cannot allocate memory for macro arguments");
			return false;
		}
	}

	if (len > 0) {
		if (ucl_size_add(len, 1, &alloc_len)) {
			value = malloc(alloc_len);
		}
		if (value == NULL) {
			ucl_create_err(&parser->err, "cannot allocate memory for priority");

			return false;
		}
		ucl_strlcpy(value, (const char *) data, alloc_len);
		priority = strtol(value, &leftover, 10);
		if (*leftover != '\0') {
			ucl_create_err(&parser->err, "Invalid priority value in macro: %s",
						   value);
			free(value);
			return false;
		}
		free(value);
		found = true;
	}

	if (found == true) {
		parser->chunks->priority = priority;
		return true;
	}

	ucl_create_err(&parser->err, "Unable to parse priority macro");
	return false;
}

/**
 * Handle load macro
 * @param data include data
 * @param len length of data
 * @param args UCL object representing arguments to the macro
 * @param ud user data
 * @return
 */
bool ucl_load_handler(const unsigned char *data, size_t len,
					  const ucl_object_t *args, void *ud)
{
	struct ucl_parser *parser = ud;
	const ucl_object_t *param;
	ucl_object_t *obj, *old_obj;
	ucl_object_iter_t it = NULL;
	int args_err = 0;
	bool try_load, multiline, test;
	const char *target, *prefix;
	char *load_file, *tmp = NULL;
	unsigned char *buf;
	size_t buflen, alloc_len;
	unsigned priority;
	int64_t iv;
	ucl_object_t *container = NULL;
	enum ucl_string_flags flags;

	/* Default values */
	try_load = false;
	multiline = false;
	test = false;
	target = "string";
	prefix = NULL;
	load_file = NULL;
	buf = NULL;
	buflen = 0;
	priority = 0;
	obj = NULL;
	old_obj = NULL;
	flags = 0;

	if (parser == NULL) {
		return false;
	}

	/* Process arguments */
	if (args != NULL && args->type == UCL_OBJECT) {
		while ((param = ucl_object_iterate_with_error(args, &it, true,
														  &args_err)) != NULL) {
			if (param->type == UCL_BOOLEAN) {
				if (strncmp(param->key, "try", param->keylen) == 0) {
					try_load = ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "multiline", param->keylen) == 0) {
					multiline = ucl_object_toboolean(param);
				}
				else if (strncmp(param->key, "escape", param->keylen) == 0) {
					test = ucl_object_toboolean(param);
					if (test) {
						flags |= UCL_STRING_ESCAPE;
					}
				}
				else if (strncmp(param->key, "trim", param->keylen) == 0) {
					test = ucl_object_toboolean(param);
					if (test) {
						flags |= UCL_STRING_TRIM;
					}
				}
			}
			else if (param->type == UCL_STRING) {
				if (strncmp(param->key, "key", param->keylen) == 0) {
					prefix = ucl_object_tostring(param);
				}
				else if (strncmp(param->key, "target", param->keylen) == 0) {
					target = ucl_object_tostring(param);
				}
			}
			else if (param->type == UCL_INT) {
				if (strncmp(param->key, "priority", param->keylen) == 0) {
					priority = ucl_object_toint(param);
				}
			}
		}

		if (args_err == ENOMEM) {
			/* The arguments must not be dropped silently */
			ucl_create_err(&parser->err, "cannot allocate memory for macro arguments");
			return false;
		}
	}

	if (prefix == NULL || strlen(prefix) == 0) {
		ucl_create_err(&parser->err, "No Key specified in load macro");
		return false;
	}

	if (len > 0) {
		if (ucl_size_add(len, 1, &alloc_len)) {
			load_file = malloc(alloc_len);
		}
		if (!load_file) {
			ucl_create_err(&parser->err, "cannot allocate memory for suffix");

			return false;
		}

		snprintf(load_file, alloc_len, "%.*s", (int) len, data);

		if (!ucl_fetch_file(load_file, &buf, &buflen, &parser->err,
							!try_load)) {
			free(load_file);

			if (try_load) {
				ucl_parser_clear_error(parser);
			}

			return (try_load || false);
		}

		free(load_file);
		container = parser->stack->obj;
		old_obj = __DECONST(ucl_object_t *, ucl_object_lookup(container,
															  prefix));

		if (old_obj != NULL) {
			ucl_create_err(&parser->err, "Key %s already exists", prefix);
			if (buf) {
				ucl_munmap(buf, buflen);
			}

			return false;
		}

		if (strcasecmp(target, "string") == 0) {
			obj = ucl_object_fromstring_common(buf, buflen, flags);
			ucl_copy_value_trash(obj);
			if (multiline) {
				obj->flags |= UCL_OBJECT_MULTILINE;
			}
		}
		else if (strcasecmp(target, "int") == 0) {
			if (ucl_size_add(buflen, 1, &alloc_len)) {
				tmp = malloc(alloc_len);
			}

			if (tmp == NULL) {
				ucl_create_err(&parser->err, "Memory allocation failed");
				if (buf) {
					ucl_munmap(buf, buflen);
				}

				return false;
			}

			snprintf(tmp, alloc_len, "%.*s", (int) buflen, buf);
			iv = strtoll(tmp, NULL, 10);
			obj = ucl_object_fromint(iv);
			free(tmp);
		}

		if (buf) {
			ucl_munmap(buf, buflen);
		}

		if (obj != NULL) {
			obj->key = prefix;
			obj->keylen = strlen(prefix);
			obj->prev = obj;
			obj->next = NULL;
			ucl_object_set_priority(obj, priority);
			/* The prefix belongs to the macro arguments: copy it */
			if (ucl_copy_key_trash(obj) == NULL ||
				!ucl_object_insert_key(container, obj, obj->key, obj->keylen, false)) {
				ucl_object_unref(obj);
				ucl_create_err(&parser->err, "cannot insert key %s of load macro",
							   prefix);
				return false;
			}
		}

		return true;
	}

	ucl_create_err(&parser->err, "Unable to parse load macro");
	return false;
}

bool ucl_inherit_handler(const unsigned char *data, size_t len,
						 const ucl_object_t *args, const ucl_object_t *ctx, void *ud)
{
	const ucl_object_t *parent, *cur;
	ucl_object_t *target, *copy;
	ucl_object_iter_t it = NULL;
	bool replace = false;
	struct ucl_parser *parser = ud;

	parent = ucl_object_lookup_len(ctx, data, len);

	/* Some sanity checks */
	if (parent == NULL || ucl_object_type(parent) != UCL_OBJECT) {
		ucl_create_err(&parser->err, "Unable to find inherited object %.*s",
					   (int) len, data);
		return false;
	}

	if (parser->stack == NULL || parser->stack->obj == NULL ||
		ucl_object_type(parser->stack->obj) != UCL_OBJECT) {
		ucl_create_err(&parser->err, "Invalid inherit context");
		return false;
	}

	target = parser->stack->obj;

	if (args && (cur = ucl_object_lookup(args, "replace")) != NULL) {
		replace = ucl_object_toboolean(cur);
	}

	while ((cur = ucl_object_iterate(parent, &it, true))) {
		/* We do not replace existing keys */
		if (!replace && ucl_object_lookup_len(target, cur->key, cur->keylen)) {
			continue;
		}

		copy = ucl_object_copy(cur);

		if (!replace) {
			copy->flags |= UCL_OBJECT_INHERITED;
		}

		ucl_object_insert_key(target, copy, copy->key,
							  copy->keylen, false);
	}

	return true;
}

bool ucl_parser_set_filevars(struct ucl_parser *parser, const char *filename, bool need_expand)
{
	char realbuf[PATH_MAX], *curdir;

	if (filename != NULL) {
		if (need_expand) {
			if (ucl_realpath(filename, realbuf) == NULL) {
				return false;
			}
		}
		else {
			ucl_strlcpy(realbuf, filename, sizeof(realbuf));
		}

		if (parser->cur_file) {
			UCL_FREE(strlen(parser->cur_file) + 1, parser->cur_file);
		}

		parser->cur_file = UCL_STRDUP(realbuf);

		/* Define variables */
		ucl_parser_register_variable(parser, "FILENAME", realbuf);
		curdir = dirname(realbuf);
		ucl_parser_register_variable(parser, "CURDIR", curdir);
	}
	else {
		/* Set everything from the current dir */
		curdir = getcwd(realbuf, sizeof(realbuf));
		ucl_parser_register_variable(parser, "FILENAME", "undef");
		ucl_parser_register_variable(parser, "CURDIR", curdir);
	}

	return true;
}

/*
 * Parse a mapped file and release the mapping. Nothing may point into it
 * afterwards: the input is parsed without zero-copy, and the chunk, which
 * stays in the parser's list, forgets its data.
 */
static bool
ucl_parser_add_mapped_chunk(struct ucl_parser *parser, unsigned char *buf,
							size_t len, unsigned priority,
							enum ucl_duplicate_strategy strat,
							enum ucl_parse_type parse_type)
{
	struct ucl_chunk *chunk;
	int zerocopy = parser->flags & UCL_PARSER_ZEROCOPY;
	bool ret;

	parser->flags &= ~UCL_PARSER_ZEROCOPY;
	ret = ucl_parser_add_chunk_full(parser, buf, len, priority, strat,
									parse_type);
	parser->flags |= zerocopy;

	if (len > 0) {
		ucl_munmap(buf, len);

		LL_FOREACH(parser->chunks, chunk)
		{
			if (chunk->begin == buf) {
				chunk->begin = NULL;
				chunk->pos = NULL;
				chunk->end = NULL;
				chunk->remain = 0;
			}
		}
	}

	return ret;
}

bool ucl_parser_add_file_full(struct ucl_parser *parser, const char *filename,
							  unsigned priority, enum ucl_duplicate_strategy strat,
							  enum ucl_parse_type parse_type)
{
	unsigned char *buf;
	size_t len;
	bool ret;
	char realbuf[PATH_MAX];

	if (ucl_realpath(filename, realbuf) == NULL) {
		ucl_create_err(&parser->err, "cannot open file %s: %s",
					   filename,
					   strerror(errno));
		return false;
	}

	if (!ucl_fetch_file(realbuf, &buf, &len, &parser->err, true)) {
		return false;
	}

	ucl_parser_set_filevars(parser, realbuf, false);
	ret = ucl_parser_add_mapped_chunk(parser, buf, len, priority, strat,
									  parse_type);

	return ret;
}

bool ucl_parser_add_file_priority(struct ucl_parser *parser, const char *filename,
								  unsigned priority)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_file_full(parser, filename, priority,
									UCL_DUPLICATE_APPEND, UCL_PARSE_UCL);
}

bool ucl_parser_add_file(struct ucl_parser *parser, const char *filename)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_file_full(parser, filename,
									parser->default_priority, UCL_DUPLICATE_APPEND,
									UCL_PARSE_UCL);
}


bool ucl_parser_add_fd_full(struct ucl_parser *parser, int fd,
							unsigned priority, enum ucl_duplicate_strategy strat,
							enum ucl_parse_type parse_type)
{
	unsigned char *buf;
	size_t len;
	bool ret;
	struct stat st;

	if (fstat(fd, &st) == -1) {
		ucl_create_err(&parser->err, "cannot stat fd %d: %s",
					   fd, strerror(errno));
		return false;
	}
	if (st.st_size == 0) {
		return true;
	}
	if ((buf = ucl_mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		ucl_create_err(&parser->err, "cannot mmap fd %d: %s",
					   fd, strerror(errno));
		return false;
	}

	if (parser->cur_file) {
		UCL_FREE(strlen(parser->cur_file) + 1, parser->cur_file);
	}
	parser->cur_file = NULL;
	len = st.st_size;
	ret = ucl_parser_add_mapped_chunk(parser, buf, len, priority, strat,
									  parse_type);

	return ret;
}

bool ucl_parser_add_fd_priority(struct ucl_parser *parser, int fd,
								unsigned priority)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_fd_full(parser, fd, parser->default_priority,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_UCL);
}

bool ucl_parser_add_fd(struct ucl_parser *parser, int fd)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_fd_priority(parser, fd, parser->default_priority);
}

size_t
ucl_strlcpy(char *dst, const char *src, size_t siz)
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

	return (s - src - 1); /* count does not include NUL */
}

size_t
ucl_strlcpy_unsafe(char *dst, const char *src, size_t siz)
{
    if (siz == 0) {
        return 0;
    }

    memcpy(dst, src, siz - 1);
    dst[siz - 1] = '\0';

    return siz - 1;
}

size_t
ucl_strlcpy_tolower(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = tolower(*s++)) == '\0') {
				break;
			}
		}
	}

	if (n == 0 && siz != 0) {
		*d = '\0';
	}

	return (s - src); /* count does not include NUL */
}

/*
 * Find the first occurrence of find in s
 */
char *
ucl_strnstr(const char *s, const char *find, int len)
{
	char c, sc;
	int mlen;

	if ((c = *find++) != 0) {
		mlen = strlen(find);
		do {
			do {
				if (len <= mlen)
					return (NULL);

				sc = *s++;
				len--;

				if (sc == '\0')
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, mlen) != 0);
		s--;
	}
	return ((char *) s);
}

/*
 * Find the first occurrence of find in s, ignore case.
 */
char *
ucl_strncasestr(const char *s, const char *find, int len)
{
	char c, sc;
	int mlen;

	if ((c = *find++) != 0) {
		c = tolower(c);
		mlen = strlen(find);
		do {
			do {
				if (len <= mlen)
					return (NULL);

				sc = *s++;
				len--;

				if (sc == '\0')
					return (NULL);
			} while (tolower(sc) != c);
		} while (strncasecmp(s, find, mlen) != 0);
		s--;
	}
	return ((char *) s);
}

/* Count one escaped byte without allowing the running total to wrap. */
static bool
ucl_escaped_len_add(size_t *len, unsigned char c)
{
	size_t increment = 1;

	if (ucl_test_character(c, UCL_CHARACTER_JSON_UNSAFE | UCL_CHARACTER_WHITESPACE_UNSAFE)) {
		if (c == '\v' || c == '\0') {
			increment = 6;
		}
		else if (c != ' ') {
			increment = 2;
		}
	}

	return ucl_size_add(*len, increment, len);
}

ucl_object_t *
ucl_object_fromstring_common(const char *str, size_t len, enum ucl_string_flags flags)
{
	ucl_object_t *obj;
	const char *start, *end, *p, *pos;
	char *dst = NULL, *d;
	size_t escaped_len, alloc_len;

	if (str == NULL) {
		return NULL;
	}

	obj = ucl_object_new();
	if (obj) {
		if (len == 0) {
			len = strlen(str);
		}
		if (flags & UCL_STRING_TRIM) {
			/* Skip leading spaces */
			for (start = str; (size_t) (start - str) < len; start++) {
				if (!ucl_test_character(*start, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
					break;
				}
			}
			/* Skip trailing spaces */
			for (end = str + len - 1; end > start; end--) {
				if (!ucl_test_character(*end, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
					break;
				}
			}
			end++;
		}
		else {
			start = str;
			end = str + len;
		}

		obj->type = UCL_STRING;
		if (flags & UCL_STRING_ESCAPE) {
			for (p = start, escaped_len = 0; p < end; p++) {
				if (!ucl_escaped_len_add(&escaped_len, (unsigned char) *p)) {
					ucl_object_unref(obj);
					return NULL;
				}
			}
			if (ucl_size_add(escaped_len, 1, &alloc_len)) {
				dst = malloc(alloc_len);
			}
			if (dst != NULL) {
				for (p = start, d = dst; p < end; p++, d++) {
					if (ucl_test_character(*p, UCL_CHARACTER_JSON_UNSAFE | UCL_CHARACTER_WHITESPACE_UNSAFE)) {
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
						case '\0':
							*d++ = '\\';
							*d++ = 'u';
							*d++ = '0';
							*d++ = '0';
							*d++ = '0';
							*d = '0';
							break;
						case '\v':
							*d++ = '\\';
							*d++ = 'u';
							*d++ = '0';
							*d++ = '0';
							*d++ = '0';
							*d = 'B';
							break;
						case '\\':
							*d++ = '\\';
							*d = '\\';
							break;
						case ' ':
							*d = ' ';
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
			if (ucl_size_add((size_t) (end - start), 1, &alloc_len)) {
				dst = malloc(alloc_len);
			}
			if (dst != NULL) {
				ucl_strlcpy_unsafe(dst, start, alloc_len);
				obj->value.sv = dst;
				obj->trash_stack[UCL_TRASH_VALUE] = dst;
				obj->len = end - start;
			}
		}
		if (dst == NULL) {
			ucl_object_unref(obj);
			return NULL;
		}
		if (flags & UCL_STRING_PARSE) {
			/* Parse what we have */
			if (flags & UCL_STRING_PARSE_BOOLEAN) {
				if (!ucl_maybe_parse_boolean(obj, dst, obj->len) && (flags & UCL_STRING_PARSE_NUMBER)) {
					ucl_maybe_parse_number(obj, dst, dst + obj->len, &pos,
										   flags & UCL_STRING_PARSE_DOUBLE,
										   flags & UCL_STRING_PARSE_BYTES,
										   flags & UCL_STRING_PARSE_TIME);
				}
			}
			else {
				ucl_maybe_parse_number(obj, dst, dst + obj->len, &pos,
									   flags & UCL_STRING_PARSE_DOUBLE,
									   flags & UCL_STRING_PARSE_BYTES,
									   flags & UCL_STRING_PARSE_TIME);
			}
		}
	}

	return obj;
}

static bool
ucl_object_insert_key_common(ucl_object_t *top, ucl_object_t *elt,
							 const char *key, size_t keylen, bool copy_key, bool merge, bool replace)
{
	ucl_object_t *found, *tmp;
	const ucl_object_t *cur;
	ucl_object_iter_t it = NULL;
	ucl_hash_t *nhash;
	const char *p;
	int ret = true;

	if (elt == NULL || key == NULL) {
		return false;
	}

	if (top == NULL) {
		return false;
	}

	if (top->type != UCL_OBJECT) {
		/* It is possible to convert NULL type to an object */
		if (top->type == UCL_NULL) {
			top->type = UCL_OBJECT;
		}
		else {
			/* Refuse converting of other object types */
			return false;
		}
	}

	if (top->value.ov == NULL) {
		top->value.ov = ucl_hash_create(false);
		if (top->value.ov == NULL) {
			return false;
		}
	}

	if (keylen == 0) {
		keylen = strlen(key);
	}

	for (p = key; p < key + keylen; p++) {
		if (ucl_test_character(*p, UCL_CHARACTER_UCL_UNSAFE)) {
			elt->flags |= UCL_OBJECT_NEED_KEY_ESCAPE;
			break;
		}
	}

	/* workaround for some use cases */
	if (elt->trash_stack[UCL_TRASH_KEY] != NULL &&
		key != (const char *) elt->trash_stack[UCL_TRASH_KEY]) {
		/* Remove copied key */
		UCL_FREE(elt->keylen + 1, elt->trash_stack[UCL_TRASH_KEY]);
		elt->trash_stack[UCL_TRASH_KEY] = NULL;
		elt->flags &= ~UCL_OBJECT_ALLOCATED_KEY;
	}

	elt->key = key;
	elt->keylen = keylen;

	if (copy_key && ucl_copy_key_trash(elt) == NULL) {
		return false;
	}

	found = __DECONST(ucl_object_t *, ucl_hash_search_obj(top->value.ov, elt));

	if (found == NULL) {
		nhash = ucl_hash_insert_object(top->value.ov, elt, false);
		if (nhash == NULL) {
			return false;
		}
		top->value.ov = nhash;
		top->len++;
		/* Key was inserted - return true regardless of replace flag */
	}
	else {
		if (replace) {
			ucl_hash_replace(top->value.ov, found, elt);
			ucl_object_unref(found);
		}
		else if (merge) {
			if (found->type != UCL_OBJECT && elt->type == UCL_OBJECT) {
				/* Insert old elt to new one */
				if (!ucl_object_insert_key_common(elt, found, found->key,
												  found->keylen, copy_key, false, false)) {
					return false;
				}
				ucl_hash_replace(top->value.ov, found, elt);
			}
			else if (found->type == UCL_OBJECT && elt->type != UCL_OBJECT) {
				/* Insert new to old */
				ret = ucl_object_insert_key_common(found, elt, elt->key,
												   elt->keylen, copy_key, false, false);
			}
			else if (found->type == UCL_OBJECT && elt->type == UCL_OBJECT) {
				/* Mix two hashes */
				while ((cur = ucl_object_iterate(elt, &it, true)) != NULL) {
					tmp = ucl_object_ref(cur);
					if (!ucl_object_insert_key_common(found, tmp, cur->key,
													  cur->keylen, copy_key, true, false)) {
						ucl_object_unref(tmp);
					}
				}
				ucl_object_unref(elt);
			}
			else {
				/* Just make a list of scalars */
				DL_CONCAT(found, elt);
			}
		}
		else {
			DL_CONCAT(found, elt);
		}
	}

	return ret;
}

bool ucl_object_delete_keyl(ucl_object_t *top, const char *key, size_t keylen)
{
	ucl_object_t *found;

	if (top == NULL || key == NULL) {
		return false;
	}

	found = __DECONST(ucl_object_t *, ucl_object_lookup_len(top, key, keylen));

	if (found == NULL) {
		return false;
	}

	ucl_hash_delete(top->value.ov, found);
	ucl_object_unref(found);
	top->len--;

	return true;
}

bool ucl_object_delete_key(ucl_object_t *top, const char *key)
{
	return ucl_object_delete_keyl(top, key, strlen(key));
}

ucl_object_t *
ucl_object_pop_keyl(ucl_object_t *top, const char *key, size_t keylen)
{
	const ucl_object_t *found;

	if (top == NULL || key == NULL) {
		return NULL;
	}
	found = ucl_object_lookup_len(top, key, keylen);

	if (found == NULL) {
		return NULL;
	}
	ucl_hash_delete(top->value.ov, found);
	top->len--;

	return __DECONST(ucl_object_t *, found);
}

ucl_object_t *
ucl_object_pop_key(ucl_object_t *top, const char *key)
{
	return ucl_object_pop_keyl(top, key, strlen(key));
}

bool ucl_object_insert_key(ucl_object_t *top, ucl_object_t *elt,
						   const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common(top, elt, key, keylen, copy_key, false, false);
}

bool ucl_object_insert_key_merged(ucl_object_t *top, ucl_object_t *elt,
								  const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common(top, elt, key, keylen, copy_key, true, false);
}

bool ucl_object_replace_key(ucl_object_t *top, ucl_object_t *elt,
							const char *key, size_t keylen, bool copy_key)
{
	return ucl_object_insert_key_common(top, elt, key, keylen, copy_key, false, true);
}

bool ucl_object_merge(ucl_object_t *top, ucl_object_t *elt, bool copy)
{
	ucl_object_t *cur = NULL, *cp = NULL, *found = NULL;
	ucl_object_iter_t iter = NULL;
	ucl_hash_t *nhash;
	int iter_err = 0;

	if (top == NULL || elt == NULL) {
		return false;
	}

	if (top->type == UCL_ARRAY) {
		if (elt->type == UCL_ARRAY) {
			/* Merge two arrays */
			return ucl_array_merge(top, elt, copy);
		}
		else {
			cp = copy ? ucl_object_copy(elt) : ucl_object_ref(elt);

			if (cp == NULL || !ucl_array_append(top, cp)) {
				ucl_object_unref(cp);

				return false;
			}

			return true;
		}
	}
	else if (top->type == UCL_OBJECT) {
		if (elt->type == UCL_OBJECT) {
			/* Mix two hashes */
			while ((cur = (ucl_object_t *) ucl_hash_iterate2(elt->value.ov,
															 &iter, &iter_err))) {

				if (copy) {
					cp = ucl_object_copy(cur);
				}
				else {
					cp = ucl_object_ref(cur);
				}

				if (cp == NULL) {
					break;
				}

				found = __DECONST(ucl_object_t *,
								  ucl_hash_search(top->value.ov, cp->key, cp->keylen));

				if (found == NULL) {
					/* The key does not exist */
					nhash = ucl_hash_insert_object(top->value.ov, cp, false);
					if (nhash == NULL) {
						ucl_object_unref(cp);
						break;
					}
					top->value.ov = nhash;
					top->len++;
				}
				else {
					/* The key already exists, merge it recursively */
					if (found->type == UCL_OBJECT || found->type == UCL_ARRAY) {
						if (!ucl_object_merge(found, cp, copy)) {
							ucl_object_unref(cp);
							break;
						}
						ucl_object_unref(cp);
					}
					else {
						ucl_hash_replace(top->value.ov, found, cp);
						ucl_object_unref(found);
					}
				}
			}

			if (iter != NULL) {
				/* Stopped on a failure, before the end that frees it */
				ucl_hash_iterate_free(iter);
				return false;
			}
			if (iter_err == ENOMEM) {
				return false;
			}
		}
		else {
			if (copy) {
				cp = ucl_object_copy(elt);
			}
			else {
				cp = ucl_object_ref(elt);
			}

			if (cp == NULL) {
				return false;
			}

			found = __DECONST(ucl_object_t *,
							  ucl_hash_search(top->value.ov, cp->key, cp->keylen));

			if (found == NULL) {
				/* The key does not exist */
				nhash = ucl_hash_insert_object(top->value.ov, cp, false);
				if (nhash == NULL) {
					ucl_object_unref(cp);

					return false;
				}
				top->value.ov = nhash;
				top->len++;
			}
			else {
				/* The key already exists, merge it recursively */
				if (found->type == UCL_OBJECT || found->type == UCL_ARRAY) {
					if (!ucl_object_merge(found, cp, copy)) {
						return false;
					}
					ucl_object_unref(cp);
				}
				else {
					ucl_hash_replace(top->value.ov, found, cp);
					ucl_object_unref(found);
				}
			}
		}
	}
	else {
		/* Cannot merge trivial objects */
		return false;
	}

	return true;
}

const ucl_object_t *
ucl_object_lookup_len(const ucl_object_t *obj, const char *key, size_t klen)
{
	const ucl_object_t *ret;
	ucl_object_t srch;

	if (obj == NULL || obj->type != UCL_OBJECT || key == NULL) {
		return NULL;
	}

	srch.key = key;
	srch.keylen = klen;
	ret = ucl_hash_search_obj(obj->value.ov, &srch);

	return ret;
}

const ucl_object_t *
ucl_object_lookup(const ucl_object_t *obj, const char *key)
{
	if (key == NULL) {
		return NULL;
	}

	return ucl_object_lookup_len(obj, key, strlen(key));
}

const ucl_object_t *
ucl_object_lookup_any(const ucl_object_t *obj,
					  const char *key, ...)
{
	va_list ap;
	const ucl_object_t *ret = NULL;
	const char *nk = NULL;

	if (obj == NULL || key == NULL) {
		return NULL;
	}

	ret = ucl_object_lookup_len(obj, key, strlen(key));

	if (ret == NULL) {
		va_start(ap, key);

		while (ret == NULL) {
			nk = va_arg(ap, const char *);

			if (nk == NULL) {
				break;
			}
			else {
				ret = ucl_object_lookup_len(obj, nk, strlen(nk));
			}
		}

		va_end(ap);
	}

	return ret;
}

const ucl_object_t *
ucl_object_iterate_with_error(const ucl_object_t *obj, ucl_object_iter_t *iter, bool expand_values,
							  int *ep)
{
	const ucl_object_t *elt = NULL;

	if (obj == NULL || iter == NULL) {
		return NULL;
	}

	if (expand_values) {
		switch (obj->type) {
		case UCL_OBJECT:
			return (const ucl_object_t *) ucl_hash_iterate2(obj->value.ov, iter, ep);
			break;
		case UCL_ARRAY: {
			unsigned int idx;
			UCL_ARRAY_GET(vec, obj);
			idx = (unsigned int) (uintptr_t) (*iter);

			if (vec != NULL) {
				while (idx < kv_size(*vec)) {
					if ((elt = kv_A(*vec, idx)) != NULL) {
						idx++;
						break;
					}
					idx++;
				}
				*iter = (void *) (uintptr_t) idx;
			}

			return elt;
			break;
		}
		default:
			/* Go to linear iteration */
			break;
		}
	}
	/* Treat everything as a linear list */
	elt = *iter;
	if (elt == NULL) {
		elt = obj;
	}
	else if (elt == obj) {
		return NULL;
	}
	*iter = __DECONST(void *, elt->next ? elt->next : obj);
	return elt;

	/* Not reached */
	return NULL;
}

void
ucl_object_iterate_end(const ucl_object_t *obj, ucl_object_iter_t *iter)
{
	if (iter == NULL || *iter == NULL) {
		return;
	}

	if (obj != NULL && obj->type == UCL_OBJECT) {
		const ucl_object_t *cur;

		/*
		 * Without expand_values, the iterator is a position in obj's own
		 * list of objects, not a hash iterator: nothing to free.
		 */
		for (cur = obj; cur != NULL; cur = cur->next) {
			if (cur == *iter) {
				*iter = NULL;
				return;
			}
		}

		ucl_hash_iterate_free(*iter);
	}

	*iter = NULL;
}

enum ucl_safe_iter_flags {
	UCL_ITERATE_FLAG_UNDEFINED = 0,
	UCL_ITERATE_FLAG_INSIDE_ARRAY,
	UCL_ITERATE_FLAG_INSIDE_OBJECT,
	UCL_ITERATE_FLAG_IMPLICIT,
	UCL_ITERATE_FLAG_EXCEPTION
};

static const char safe_iter_magic[4] = {'u', 'i', 't', 'e'};
struct ucl_object_safe_iter {
	char magic[4]; /* safety check */
	uint32_t flags;
	const ucl_object_t *impl_it; /* implicit object iteration */
	ucl_object_iter_t expl_it;   /* explicit iteration */
};

#define UCL_SAFE_ITER(ptr) (struct ucl_object_safe_iter *) (ptr)
#define UCL_SAFE_ITER_CHECK(it)                                             \
	do {                                                                    \
		assert(it != NULL);                                                 \
		assert(memcmp(it->magic, safe_iter_magic, sizeof(it->magic)) == 0); \
	} while (0)

ucl_object_iter_t
ucl_object_iterate_new(const ucl_object_t *obj)
{
	struct ucl_object_safe_iter *it;

	it = UCL_ALLOC(sizeof(*it));
	if (it != NULL) {
		memcpy(it->magic, safe_iter_magic, sizeof(it->magic));
		it->flags = UCL_ITERATE_FLAG_UNDEFINED;
		it->expl_it = NULL;
		it->impl_it = obj;
	}

	return (ucl_object_iter_t) it;
}

bool ucl_object_iter_chk_excpn(ucl_object_iter_t it)
{
	struct ucl_object_safe_iter *rit = UCL_SAFE_ITER(it);

	UCL_SAFE_ITER_CHECK(rit);

	return (rit->flags == UCL_ITERATE_FLAG_EXCEPTION);
}

ucl_object_iter_t
ucl_object_iterate_reset(ucl_object_iter_t it, const ucl_object_t *obj)
{
	struct ucl_object_safe_iter *rit = UCL_SAFE_ITER(it);

	UCL_SAFE_ITER_CHECK(rit);

	if (rit->expl_it != NULL) {
		if (rit->flags == UCL_ITERATE_FLAG_INSIDE_OBJECT) {
			ucl_hash_iterate_free(rit->expl_it);
		}
	}

	rit->impl_it = obj;
	rit->expl_it = NULL;
	rit->flags = UCL_ITERATE_FLAG_UNDEFINED;

	return it;
}

const ucl_object_t *
ucl_object_iterate_safe(ucl_object_iter_t it, bool expand_values)
{
	return ucl_object_iterate_full(it, expand_values ? UCL_ITERATE_BOTH : UCL_ITERATE_IMPLICIT);
}

const ucl_object_t *
ucl_object_iterate_full(ucl_object_iter_t it, enum ucl_iterate_type type)
{
	struct ucl_object_safe_iter *rit = UCL_SAFE_ITER(it);
	const ucl_object_t *ret = NULL;
	int ern;

	UCL_SAFE_ITER_CHECK(rit);

	if (rit->impl_it == NULL) {
		return NULL;
	}

	if (rit->impl_it->type == UCL_OBJECT) {
		rit->flags = UCL_ITERATE_FLAG_INSIDE_OBJECT;
		ret = ucl_object_iterate_with_error(rit->impl_it, &rit->expl_it, true, &ern);

		if (ret == NULL && ern != 0) {
			rit->flags = UCL_ITERATE_FLAG_EXCEPTION;
			return NULL;
		}

		if (ret == NULL && (type & UCL_ITERATE_IMPLICIT)) {
			/* Need to switch to another implicit object in chain */
			rit->impl_it = rit->impl_it->next;
			rit->expl_it = NULL;

			return ucl_object_iterate_safe(it, type);
		}
	}
	else if (rit->impl_it->type == UCL_ARRAY) {
		rit->flags = UCL_ITERATE_FLAG_INSIDE_ARRAY;
		ret = ucl_object_iterate(rit->impl_it, &rit->expl_it, true);

		if (ret == NULL && (type & UCL_ITERATE_IMPLICIT)) {
			/* Need to switch to another implicit object in chain */
			rit->impl_it = rit->impl_it->next;
			rit->expl_it = NULL;

			return ucl_object_iterate_safe(it, type);
		}
	}
	else {
		/* Just iterate over the implicit array */
		rit->flags = UCL_ITERATE_FLAG_IMPLICIT;
		ret = rit->impl_it;
		rit->impl_it = rit->impl_it->next;

		if (type & UCL_ITERATE_EXPLICIT) {
			/* We flatten objects if need to expand values */
			if (ret->type == UCL_OBJECT || ret->type == UCL_ARRAY) {
				return ucl_object_iterate_safe(it, type);
			}
		}
	}

	return ret;
}

void ucl_object_iterate_free(ucl_object_iter_t it)
{
	struct ucl_object_safe_iter *rit = UCL_SAFE_ITER(it);

	UCL_SAFE_ITER_CHECK(rit);

	if (rit->expl_it != NULL) {
		if (rit->flags == UCL_ITERATE_FLAG_INSIDE_OBJECT) {
			ucl_hash_iterate_free(rit->expl_it);
		}
	}

	UCL_FREE(sizeof(*rit), it);
}

const ucl_object_t *
ucl_object_lookup_path(const ucl_object_t *top, const char *path_in)
{
	return ucl_object_lookup_path_char(top, path_in, '.');
}


const ucl_object_t *
ucl_object_lookup_path_char(const ucl_object_t *top, const char *path_in, const char sep)
{
	const ucl_object_t *o = NULL, *found;
	const char *p, *c;
	char *err_str;
	unsigned index;

	if (path_in == NULL || top == NULL) {
		return NULL;
	}

	found = NULL;
	p = path_in;

	/* Skip leading dots (a NUL separator must not skip the terminator) */
	while (*p != '\0' && *p == sep) {
		p++;
	}

	c = p;
	while (*p != '\0') {
		p++;
		if (*p == sep || *p == '\0') {
			if (p > c) {
				switch (top->type) {
				case UCL_ARRAY:
					/* Key should be an int */
					index = strtoul(c, &err_str, 10);
					if (err_str != NULL && (*err_str != sep && *err_str != '\0')) {
						return NULL;
					}
					o = ucl_array_find_index(top, index);
					break;
				default:
					o = ucl_object_lookup_len(top, c, p - c);
					break;
				}
				if (o == NULL) {
					return NULL;
				}
				top = o;
			}
			if (*p != '\0') {
				c = p + 1;
			}
		}
	}
	found = o;

	return found;
}


ucl_object_t *
ucl_object_new(void)
{
	return ucl_object_typed_new(UCL_NULL);
}

ucl_object_t *
ucl_object_typed_new(ucl_type_t type)
{
	return ucl_object_new_full(type, 0);
}

ucl_object_t *
ucl_object_new_full(ucl_type_t type, unsigned priority)
{
	ucl_object_t *new;

	if (type != UCL_USERDATA) {
		new = UCL_ALLOC(sizeof(ucl_object_t));
		if (new != NULL) {
			memset(new, 0, sizeof(ucl_object_t));
			new->ref = 1;
			new->type = (type <= UCL_NULL ? type : UCL_NULL);
			new->next = NULL;
			new->prev = new;
			ucl_object_set_priority(new, priority);

			if (type == UCL_ARRAY) {
				new->value.av = UCL_ALLOC(sizeof(ucl_array_t));
				if (new->value.av) {
					memset(new->value.av, 0, sizeof(ucl_array_t));
					UCL_ARRAY_GET(vec, new);

					/* Preallocate some space for arrays */
					kv_resize_safe(ucl_object_t *, *vec, 8, enomem);
				}
			}
		}
	}
	else {
		new = ucl_object_new_userdata(NULL, NULL, NULL);
		ucl_object_set_priority(new, priority);
	}
enomem:
	return new;
}

bool ucl_object_reserve(ucl_object_t *obj, size_t reserved)
{
	if (obj->type == UCL_ARRAY) {
		UCL_ARRAY_GET(vec, obj);

		if (vec == NULL) {
			/* Allocate array storage if not present (e.g., copied empty array) */
			vec = UCL_ALLOC(sizeof(*vec));
			if (vec == NULL) {
				return false;
			}
			kv_init(*vec);
			obj->value.av = (void *)vec;
		}

		if (vec->m < reserved) {
			/* Preallocate some space for arrays */
			kv_resize_safe(ucl_object_t *, *vec, reserved, e0);
		}
	}
	else if (obj->type == UCL_OBJECT) {
		if (obj->value.ov == NULL) {
			obj->value.ov = ucl_hash_create(false);
			if (obj->value.ov == NULL) {
				return false;
			}
		}
		return ucl_hash_reserve(obj->value.ov, reserved);
	}
	return true;
e0:
	return false;
}

ucl_object_t *
ucl_object_new_userdata(ucl_userdata_dtor dtor,
						ucl_userdata_emitter emitter,
						void *ptr)
{
	struct ucl_object_userdata *new;
	size_t nsize = sizeof(*new);

	new = UCL_ALLOC(nsize);
	if (new != NULL) {
		memset(new, 0, nsize);
		new->obj.ref = 1;
		new->obj.type = UCL_USERDATA;
		new->obj.next = NULL;
		new->obj.prev = (ucl_object_t *) new;
		new->dtor = dtor;
		new->emitter = emitter;
		new->obj.value.ud = ptr;
	}

	return (ucl_object_t *) new;
}

ucl_type_t
ucl_object_type(const ucl_object_t *obj)
{
	if (obj == NULL) {
		return UCL_NULL;
	}

	return obj->type;
}

ucl_object_t *
ucl_object_fromstring(const char *str)
{
	return ucl_object_fromstring_common(str, 0, UCL_STRING_RAW);
}

ucl_object_t *
ucl_object_fromlstring(const char *str, size_t len)
{
	return ucl_object_fromstring_common(str, len, UCL_STRING_RAW);
}

ucl_object_t *
ucl_object_fromint(int64_t iv)
{
	ucl_object_t *obj;

	obj = ucl_object_new();
	if (obj != NULL) {
		obj->type = UCL_INT;
		obj->value.iv = iv;
	}

	return obj;
}

ucl_object_t *
ucl_object_fromdouble(double dv)
{
	ucl_object_t *obj;

	obj = ucl_object_new();
	if (obj != NULL) {
		obj->type = UCL_FLOAT;
		obj->value.dv = dv;
	}

	return obj;
}

ucl_object_t *
ucl_object_frombool(bool bv)
{
	ucl_object_t *obj;

	obj = ucl_object_new();
	if (obj != NULL) {
		obj->type = UCL_BOOLEAN;
		obj->value.iv = bv;
	}

	return obj;
}

bool ucl_array_append(ucl_object_t *top, ucl_object_t *elt)
{
	if (top->type != UCL_ARRAY) {
		return false;
	}

	UCL_ARRAY_GET(vec, top);

	if (elt == NULL || top == NULL) {
		return false;
	}

	if (vec == NULL) {
		vec = UCL_ALLOC(sizeof(*vec));

		if (vec == NULL) {
			return false;
		}

		kv_init(*vec);
		top->value.av = (void *) vec;
	}

	kv_push_safe(ucl_object_t *, *vec, elt, e0);

	top->len++;

	return true;
e0:
	return false;
}

bool ucl_array_prepend(ucl_object_t *top, ucl_object_t *elt)
{
	if (top->type != UCL_ARRAY) {
		return false;
	}

	UCL_ARRAY_GET(vec, top);

	if (elt == NULL || top == NULL) {
		return false;
	}

	if (vec == NULL) {
		vec = UCL_ALLOC(sizeof(*vec));
		kv_init(*vec);
		top->value.av = (void *) vec;
		kv_push_safe(ucl_object_t *, *vec, elt, e0);
	}
	else {
		/* Slow O(n) algorithm */
		kv_prepend_safe(ucl_object_t *, *vec, elt, e0);
	}

	top->len++;

	return true;
e0:
	return false;
}

bool ucl_array_merge(ucl_object_t *top, ucl_object_t *elt, bool copy)
{
	unsigned i;
	ucl_object_t *cp = NULL;
	ucl_object_t **obj;

	if (elt == NULL || top == NULL || top->type != UCL_ARRAY || elt->type != UCL_ARRAY) {
		return false;
	}

	if (copy) {
		cp = ucl_object_copy(elt);
	}
	else {
		cp = ucl_object_ref(elt);
	}

	UCL_ARRAY_GET(v1, top);
	UCL_ARRAY_GET(v2, cp);

	if (v1 && v2) {
		size_t old_n = v1->n;

		kv_concat_safe(ucl_object_t *, *v1, *v2, e0);

		for (i = old_n; i < v1->n; i++) {
			obj = &kv_A(*v1, i);
			if (*obj == NULL) {
				continue;
			}
			if (!copy) {
				/* Now held by both arrays */
				ucl_object_ref(*obj);
			}
			top->len++;
		}

		if (copy) {
			/* The copied elements now belong to top */
			v2->n = 0;
		}
	}

	ucl_object_unref(cp);

	return true;
e0:
	ucl_object_unref(cp);
	return false;
}

ucl_object_t *
ucl_array_delete(ucl_object_t *top, ucl_object_t *elt)
{
	if (top->type != UCL_ARRAY) {
		return NULL;
	}

	UCL_ARRAY_GET(vec, top);
	ucl_object_t *ret = NULL;
	unsigned i;

	if (vec == NULL) {
		return NULL;
	}

	for (i = 0; i < vec->n; i++) {
		if (kv_A(*vec, i) == elt) {
			kv_del(ucl_object_t *, *vec, i);
			ret = elt;
			top->len--;
			break;
		}
	}

	return ret;
}

const ucl_object_t *
ucl_array_head(const ucl_object_t *top)
{
	UCL_ARRAY_GET(vec, top);

	if (vec == NULL || top == NULL || top->type != UCL_ARRAY ||
		top->value.av == NULL) {
		return NULL;
	}

	return (vec->n > 0 ? vec->a[0] : NULL);
}

const ucl_object_t *
ucl_array_tail(const ucl_object_t *top)
{
	UCL_ARRAY_GET(vec, top);

	if (top == NULL || top->type != UCL_ARRAY || top->value.av == NULL) {
		return NULL;
	}

	return (vec->n > 0 ? vec->a[vec->n - 1] : NULL);
}

ucl_object_t *
ucl_array_pop_last(ucl_object_t *top)
{
	if (top->type != UCL_ARRAY) {
		return NULL;
	}

	UCL_ARRAY_GET(vec, top);
	ucl_object_t **obj, *ret = NULL;

	if (vec != NULL && vec->n > 0) {
		obj = &kv_A(*vec, vec->n - 1);
		ret = *obj;
		kv_del(ucl_object_t *, *vec, vec->n - 1);
		top->len--;
	}

	return ret;
}

ucl_object_t *
ucl_array_pop_first(ucl_object_t *top)
{
	if (top->type != UCL_ARRAY) {
		return NULL;
	}

	UCL_ARRAY_GET(vec, top);
	ucl_object_t **obj, *ret = NULL;

	if (vec != NULL && vec->n > 0) {
		obj = &kv_A(*vec, 0);
		ret = *obj;
		kv_del(ucl_object_t *, *vec, 0);
		top->len--;
	}

	return ret;
}

unsigned int
ucl_array_size(const ucl_object_t *top)
{
	if (top == NULL || top->type != UCL_ARRAY) {
		return 0;
	}

	UCL_ARRAY_GET(vec, top);

	if (vec != NULL) {
		return kv_size(*vec);
	}

	return 0;
}

const ucl_object_t *
ucl_array_find_index(const ucl_object_t *top, unsigned int index)
{
	if (top->type != UCL_ARRAY) {
		return NULL;
	}

	UCL_ARRAY_GET(vec, top);

	if (vec != NULL && vec->n > 0 && index < vec->n) {
		return kv_A(*vec, index);
	}

	return NULL;
}

unsigned int
ucl_array_index_of(ucl_object_t *top, ucl_object_t *elt)
{
	if (top->type != UCL_ARRAY) {
		return (unsigned int) (-1);
	}

	UCL_ARRAY_GET(vec, top);
	unsigned i;

	if (vec == NULL) {
		return (unsigned int) (-1);
	}

	for (i = 0; i < vec->n; i++) {
		if (kv_A(*vec, i) == elt) {
			return i;
		}
	}

	return (unsigned int) (-1);
}

ucl_object_t *
ucl_array_replace_index(ucl_object_t *top, ucl_object_t *elt,
						unsigned int index)
{
	if (top->type != UCL_ARRAY) {
		return NULL;
	}

	UCL_ARRAY_GET(vec, top);
	ucl_object_t *ret = NULL;

	if (vec != NULL && vec->n > 0 && index < vec->n) {
		ret = kv_A(*vec, index);
		kv_A(*vec, index) = elt;
	}

	return ret;
}

ucl_object_t *
ucl_elt_append(ucl_object_t *head, ucl_object_t *elt)
{

	if (head == NULL) {
		elt->next = NULL;
		elt->prev = elt;
		head = elt;
	}
	else {
		if (head->type == UCL_USERDATA) {
			/* Userdata objects are VERY special! */
			struct ucl_object_userdata *ud = (struct ucl_object_userdata *) head;
			elt->prev = ud->obj.prev;
			ud->obj.prev->next = elt;
			ud->obj.prev = elt;
			elt->next = NULL;
		}
		else {
			elt->prev = head->prev;
			head->prev->next = elt;
			head->prev = elt;
			elt->next = NULL;
		}
	}

	return head;
}

bool ucl_object_todouble_safe(const ucl_object_t *obj, double *target)
{
	if (obj == NULL || target == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_INT:
		*target = obj->value.iv; /* Probably could cause overflow */
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		*target = obj->value.dv;
		break;
	default:
		return false;
	}

	return true;
}

double
ucl_object_todouble(const ucl_object_t *obj)
{
	double result = 0.;

	ucl_object_todouble_safe(obj, &result);
	return result;
}

bool ucl_object_toint_safe(const ucl_object_t *obj, int64_t *target)
{
	if (obj == NULL || target == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_INT:
		*target = obj->value.iv;
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		/* The cast is undefined for NaN and out of range values */
		if (isnan(obj->value.dv)) {
			return false;
		}
		else if (obj->value.dv >= 9223372036854775808.0) {
			*target = INT64_MAX;
		}
		else if (obj->value.dv < -9223372036854775808.0) {
			*target = INT64_MIN;
		}
		else {
			*target = obj->value.dv; /* Losing of decimal points */
		}
		break;
	default:
		return false;
	}

	return true;
}

int64_t
ucl_object_toint(const ucl_object_t *obj)
{
	int64_t result = 0;

	ucl_object_toint_safe(obj, &result);
	return result;
}

bool ucl_object_toboolean_safe(const ucl_object_t *obj, bool *target)
{
	if (obj == NULL || target == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_BOOLEAN:
		*target = (obj->value.iv == true);
		break;
	default:
		return false;
	}

	return true;
}

bool ucl_object_toboolean(const ucl_object_t *obj)
{
	bool result = false;

	ucl_object_toboolean_safe(obj, &result);
	return result;
}

bool ucl_object_tostring_safe(const ucl_object_t *obj, const char **target)
{
	if (obj == NULL || target == NULL) {
		return false;
	}

	switch (obj->type) {
	case UCL_STRING:
		if (!(obj->flags & UCL_OBJECT_BINARY)) {
			*target = ucl_copy_value_trash(obj);
		}
		break;
	default:
		return false;
	}

	return true;
}

const char *
ucl_object_tostring(const ucl_object_t *obj)
{
	const char *result = NULL;

	ucl_object_tostring_safe(obj, &result);
	return result;
}

const char *
ucl_object_tostring_forced(const ucl_object_t *obj)
{
	/* TODO: For binary strings we might encode string here */
	if (!(obj->flags & UCL_OBJECT_BINARY)) {
		return ucl_copy_value_trash(obj);
	}

	return NULL;
}

bool ucl_object_tolstring_safe(const ucl_object_t *obj, const char **target, size_t *tlen)
{
	if (obj == NULL || target == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_STRING:
		*target = obj->value.sv;
		if (tlen != NULL) {
			*tlen = obj->len;
		}
		break;
	default:
		return false;
	}

	return true;
}

const char *
ucl_object_tolstring(const ucl_object_t *obj, size_t *tlen)
{
	const char *result = NULL;

	ucl_object_tolstring_safe(obj, &result, tlen);
	return result;
}

const char *
ucl_object_key(const ucl_object_t *obj)
{
	return ucl_copy_key_trash(obj);
}

const char *
ucl_object_keyl(const ucl_object_t *obj, size_t *len)
{
	if (len == NULL || obj == NULL) {
		return NULL;
	}
	*len = obj->keylen;
	return obj->key;
}

ucl_object_t *
ucl_object_ref(const ucl_object_t *obj)
{
	ucl_object_t *res = NULL;

	if (obj != NULL) {
		if (obj->flags & UCL_OBJECT_EPHEMERAL) {
			/*
			 * Use deep copy for ephemeral objects, note that its refcount
			 * is NOT increased, since ephemeral objects does not need refcount
			 * at all
			 */
			res = ucl_object_copy(obj);
		}
		else {
			res = __DECONST(ucl_object_t *, obj);
#ifdef HAVE_ATOMIC_BUILTINS
			(void) __sync_add_and_fetch(&res->ref, 1);
#else
			res->ref++;
#endif
		}
	}
	return res;
}

static ucl_object_t *
ucl_object_copy_internal(const ucl_object_t *other, bool allow_array)
{

	ucl_object_t *new;
	ucl_object_iter_t it = NULL;
	const ucl_object_t *cur;
	int iter_error = 0;
	size_t sz = sizeof(*new), alloc_len;

	if (other->type == UCL_USERDATA) {
		sz = sizeof(struct ucl_object_userdata);
	}
	new = UCL_ALLOC(sz);

	if (new != NULL) {
		memcpy(new, other, sz);
		if (other->flags & UCL_OBJECT_EPHEMERAL) {
			/* Copied object is always non ephemeral */
			new->flags &= ~UCL_OBJECT_EPHEMERAL;
		}
		new->ref = 1;
		/* Unlink from others */
		new->next = NULL;
		new->prev = new;
		new->trash_stack[UCL_TRASH_KEY] = NULL;
		new->trash_stack[UCL_TRASH_VALUE] = NULL;
		if (other->type == UCL_ARRAY || other->type == UCL_OBJECT) {
			memset(&new->value, 0, sizeof(new->value));
			new->len = 0;
		}

		/* deep copy of values stored */
		if (other->trash_stack[UCL_TRASH_KEY] != NULL) {
			if (other->key == (const char *) other->trash_stack[UCL_TRASH_KEY]) {
				if (ucl_size_add(other->keylen, 1, &alloc_len)) {
					new->trash_stack[UCL_TRASH_KEY] = UCL_ALLOC(alloc_len);
				}
				if (new->trash_stack[UCL_TRASH_KEY] == NULL) {
					goto fail;
				}
				memcpy(new->trash_stack[UCL_TRASH_KEY], other->trash_stack[UCL_TRASH_KEY], other->keylen);
				new->trash_stack[UCL_TRASH_KEY][other->keylen] = '\0';
				new->key = new->trash_stack[UCL_TRASH_KEY];
			}
		}
		if (other->trash_stack[UCL_TRASH_VALUE] != NULL) {
			if (new->type == UCL_STRING) {
				/*
				 * A string value can carry embedded NUL bytes when it was
				 * built with ucl_object_fromlstring(); its real extent is
				 * tracked by new->len (copied above from other->len), not
				 * by a terminating NUL. UCL_STRDUP stops at the first NUL
				 * it sees, so on such a value it can hand back a shorter
				 * allocation than the length ucl_object_emit() will later
				 * read against. Duplicate the exact byte range instead,
				 * the same way the key is duplicated a few lines up.
				 */
				if (ucl_size_add(new->len, 1, &alloc_len)) {
					new->trash_stack[UCL_TRASH_VALUE] = UCL_ALLOC(alloc_len);
				}
				if (new->trash_stack[UCL_TRASH_VALUE] == NULL) {
					goto fail;
				}
				memcpy(new->trash_stack[UCL_TRASH_VALUE],
					   other->trash_stack[UCL_TRASH_VALUE], new->len);
				new->trash_stack[UCL_TRASH_VALUE][new->len] = '\0';
				new->value.sv = new->trash_stack[UCL_TRASH_VALUE];
			}
			else {
				new->trash_stack[UCL_TRASH_VALUE] =
					UCL_STRDUP(other->trash_stack[UCL_TRASH_VALUE]);
				if (new->trash_stack[UCL_TRASH_VALUE] == NULL) {
					goto fail;
				}
			}
		}

		if (other->type == UCL_ARRAY || other->type == UCL_OBJECT) {
			while ((other->type != UCL_OBJECT || other->value.ov != NULL) &&
				   (cur = ucl_object_iterate_with_error(other, &it, true, &iter_error)) != NULL) {
				ucl_object_t *cp = ucl_object_copy_internal(cur, other->type == UCL_OBJECT);

				if (cp == NULL) {
					goto fail;
				}
				if (!(other->type == UCL_ARRAY ? ucl_array_append(new, cp) :
					  ucl_object_insert_key(new, cp, cp->key, cp->keylen, false))) {
					ucl_object_unref(cp);
					goto fail;
				}
			}
			if (iter_error != 0) {
				goto fail;
			}
		}
		else if (allow_array && other->next != NULL) {
			LL_FOREACH(other->next, cur)
			{
				ucl_object_t *cp = ucl_object_copy_internal(cur, false);
				if (cp == NULL) {
					goto fail;
				}
				DL_APPEND(new, cp);
			}
		}
	}

	return new;

fail:
	ucl_object_iterate_end(other, &it);
	/* A failed userdata copy must not run the source's destructor. */
	if (new->type == UCL_USERDATA) {
		((struct ucl_object_userdata *) new)->dtor = NULL;
	}
	ucl_object_unref(new);
	return NULL;
}

ucl_object_t *
ucl_object_copy(const ucl_object_t *other)
{
	return ucl_object_copy_internal(other, true);
}

void ucl_object_unref(ucl_object_t *obj)
{
	if (obj != NULL) {
#ifdef HAVE_ATOMIC_BUILTINS
		unsigned int rc = __sync_sub_and_fetch(&obj->ref, 1);
		if (rc == 0) {
#else
		if (--obj->ref == 0) {
#endif
			ucl_object_free_internal(obj, true);
		}
	}
}

int ucl_object_compare(const ucl_object_t *o1, const ucl_object_t *o2)
{
	const ucl_object_t *it1, *it2;
	ucl_object_iter_t iter = NULL;
	int ret = 0;

	if (o1->type != o2->type) {
		return (o1->type) - (o2->type);
	}

	switch (o1->type) {
	case UCL_STRING:
		if (o1->len == o2->len && o1->len > 0) {
			/* Binary strings have no C string form, so compare the bytes */
			ret = memcmp(o1->value.sv, o2->value.sv, o1->len);
		}
		else {
			ret = (o1->len > o2->len) - (o1->len < o2->len);
		}
		break;
	case UCL_INT:
		/*
		 * Compare rather than subtract: a difference of less than one, or
		 * one too large for an int, would be wrong once stored in ret, and
		 * a double cannot hold every int64 value
		 */
		ret = (ucl_object_toint(o1) > ucl_object_toint(o2)) -
			  (ucl_object_toint(o1) < ucl_object_toint(o2));
		break;
	case UCL_FLOAT:
	case UCL_TIME: {
		double d1 = ucl_object_todouble(o1), d2 = ucl_object_todouble(o2);

		if (isnan(d1) || isnan(d2)) {
			/*
			 * NaN is neither less nor greater than anything, which would
			 * make it equal to every number: put it after all of them
			 */
			ret = (isnan(d1) != 0) - (isnan(d2) != 0);
		}
		else {
			ret = (d1 > d2) - (d1 < d2);
		}
		break;
	}
	case UCL_BOOLEAN:
		ret = ucl_object_toboolean(o1) - ucl_object_toboolean(o2);
		break;
	case UCL_ARRAY:
		if (o1->len == o2->len && o1->len > 0) {
			UCL_ARRAY_GET(vec1, o1);
			UCL_ARRAY_GET(vec2, o2);
			unsigned i;

			/* Compare all elements in both arrays */
			for (i = 0; i < vec1->n; i++) {
				it1 = kv_A(*vec1, i);
				it2 = kv_A(*vec2, i);

				if (it1 == NULL && it2 != NULL) {
					return -1;
				}
				else if (it2 == NULL && it1 != NULL) {
					return 1;
				}
				else if (it1 != NULL && it2 != NULL) {
					ret = ucl_object_compare(it1, it2);
					if (ret != 0) {
						break;
					}
				}
			}
		}
		else {
			ret = (o1->len > o2->len) - (o1->len < o2->len);
		}
		break;
	case UCL_OBJECT:
		if (o1->len == o2->len && o1->len > 0) {
			while ((it1 = ucl_object_iterate(o1, &iter, true)) != NULL) {
				it2 = ucl_object_lookup(o2, ucl_object_key(it1));
				if (it2 == NULL) {
					ret = 1;
					break;
				}
				ret = ucl_object_compare(it1, it2);
				if (ret != 0) {
					break;
				}
			}
			ucl_object_iterate_end(o1, &iter);
		}
		else {
			ret = (o1->len > o2->len) - (o1->len < o2->len);
		}
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int ucl_object_compare_qsort(const ucl_object_t **o1,
							 const ucl_object_t **o2)
{
	return ucl_object_compare(*o1, *o2);
}

void ucl_object_array_sort(ucl_object_t *ar,
						   int (*cmp)(const ucl_object_t **o1, const ucl_object_t **o2))
{
	UCL_ARRAY_GET(vec, ar);

	if (cmp == NULL || ar == NULL || ar->type != UCL_ARRAY) {
		return;
	}

	qsort(vec->a, vec->n, sizeof(ucl_object_t *),
		  (int (*)(const void *, const void *)) cmp);
}

void ucl_object_sort_keys(ucl_object_t *obj,
						  enum ucl_object_keys_sort_flags how)
{
	if (obj != NULL && obj->type == UCL_OBJECT) {
		ucl_hash_sort(obj->value.ov, how);
	}
}

#define PRIOBITS 4

unsigned int
ucl_object_get_priority(const ucl_object_t *obj)
{
	if (obj == NULL) {
		return 0;
	}

	return (obj->flags >> ((sizeof(obj->flags) * NBBY) - PRIOBITS));
}

void ucl_object_set_priority(ucl_object_t *obj,
							 unsigned int priority)
{
	if (obj != NULL) {
		priority &= (0x1 << PRIOBITS) - 1;
		priority <<= ((sizeof(obj->flags) * NBBY) - PRIOBITS);
		priority |= obj->flags & ((1 << ((sizeof(obj->flags) * NBBY) -
										 PRIOBITS)) -
								  1);
		obj->flags = priority;
	}
}

bool ucl_object_string_to_type(const char *input, ucl_type_t *res)
{
	if (strcasecmp(input, "object") == 0) {
		*res = UCL_OBJECT;
	}
	else if (strcasecmp(input, "array") == 0) {
		*res = UCL_ARRAY;
	}
	else if (strcasecmp(input, "integer") == 0) {
		*res = UCL_INT;
	}
	else if (strcasecmp(input, "number") == 0) {
		*res = UCL_FLOAT;
	}
	else if (strcasecmp(input, "string") == 0) {
		*res = UCL_STRING;
	}
	else if (strcasecmp(input, "boolean") == 0) {
		*res = UCL_BOOLEAN;
	}
	else if (strcasecmp(input, "null") == 0) {
		*res = UCL_NULL;
	}
	else if (strcasecmp(input, "userdata") == 0) {
		*res = UCL_USERDATA;
	}
	else {
		return false;
	}

	return true;
}

const char *
ucl_object_type_to_string(ucl_type_t type)
{
	const char *res = "unknown";

	switch (type) {
	case UCL_OBJECT:
		res = "object";
		break;
	case UCL_ARRAY:
		res = "array";
		break;
	case UCL_INT:
		res = "integer";
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		res = "number";
		break;
	case UCL_STRING:
		res = "string";
		break;
	case UCL_BOOLEAN:
		res = "boolean";
		break;
	case UCL_USERDATA:
		res = "userdata";
		break;
	case UCL_NULL:
		res = "null";
		break;
	}

	return res;
}

const ucl_object_t *
ucl_parser_get_comments(struct ucl_parser *parser)
{
	if (parser && parser->comments) {
		return parser->comments;
	}

	return NULL;
}

const ucl_object_t *
ucl_comments_find(const ucl_object_t *comments,
				  const ucl_object_t *srch)
{
	if (comments && srch) {
		return ucl_object_lookup_len(comments, (const char *) &srch,
									 sizeof(void *));
	}

	return NULL;
}

bool ucl_comments_move(ucl_object_t *comments,
					   const ucl_object_t *from, const ucl_object_t *to)
{
	const ucl_object_t *found;
	ucl_object_t *obj;

	if (comments && from && to) {
		found = ucl_object_lookup_len(comments,
									  (const char *) &from, sizeof(void *));

		if (found) {
			/* Replace key */
			obj = ucl_object_ref(found);
			ucl_object_delete_keyl(comments, (const char *) &from,
								   sizeof(void *));
			ucl_object_insert_key(comments, obj, (const char *) &to,
								  sizeof(void *), true);

			return true;
		}
	}

	return false;
}

void ucl_comments_add(ucl_object_t *comments, const ucl_object_t *obj,
					  const char *comment)
{
	if (comments && obj && comment) {
		ucl_object_t *nobj = ucl_object_fromstring(comment);

		if (nobj != NULL &&
			!ucl_object_insert_key(comments, nobj, (const char *) &obj,
								   sizeof(void *), true)) {
			/* comments is not an object: do not leak the new comment */
			ucl_object_unref(nobj);
		}
	}
}

void ucl_comments_forget(ucl_object_t *comments, const ucl_object_t *obj)
{
	kvec_t(const ucl_object_t *) todo;
	const ucl_object_t *cur = NULL, *elt, *sib;
	ucl_object_iter_t it = NULL;

	/* Released only with its last reference, and its siblings with it */
	if (comments == NULL || obj == NULL || obj->ref != 1) {
		return;
	}

	kv_init(todo);

	LL_FOREACH(obj, sib)
	{
		kv_push_safe(const ucl_object_t *, todo, sib, enomem);
	}

	while (kv_size(todo) > 0) {
		cur = kv_pop(todo);

		/* Shared, so it outlives the release along with all it holds */
		if (cur->ref != 1) {
			continue;
		}

		ucl_object_delete_keyl(comments, (const char *) &cur, sizeof(void *));

		if (cur->type == UCL_OBJECT || cur->type == UCL_ARRAY) {
			it = NULL;

			while ((elt = ucl_object_iterate(cur, &it, true)) != NULL) {
				LL_FOREACH(elt, sib)
				{
					kv_push_safe(const ucl_object_t *, todo, sib, enomem);
				}
			}
		}
	}

	kv_destroy(todo);

	return;

enomem:
	/* Comments left behind may only go to the wrong objects */
	ucl_object_iterate_end(cur, &it);
	kv_destroy(todo);
}

void ucl_parser_set_include_tracer(struct ucl_parser *parser,
								   ucl_include_trace_func_t func,
								   void *user_data)
{
	parser->include_trace_func = func;
	parser->include_trace_ud = user_data;
}

const char *
ucl_parser_get_cur_file(struct ucl_parser *parser)
{
	return parser->cur_file;
}
