/*
 * parse.h 
 *
 * a Net::DNS like library for C
 * LibDNS Team @ NLnet Labs
 * (c) NLnet Labs, 2005-2006
 * See the file LICENSE for the license
 */

#ifndef LDNS_PARSE_H
#define LDNS_PARSE_H

#include <ldns/common.h>
#include <ldns/buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDNS_PARSE_SKIP_SPACE		"\f\n\r\v"
#define LDNS_PARSE_NORMAL		" \f\n\r\t\v"
#define LDNS_PARSE_NO_NL		" \t"
#define LDNS_MAX_LINELEN		10230
#define LDNS_MAX_KEYWORDLEN		32


/**
 * \file
 *
 * Contains some low-level parsing functions, mostly used in the _frm_str
 * family of functions.
 */
 
/**
 * different type of directives in zone files
 * We now deal with $TTL, $ORIGIN and $INCLUDE.
 * The latter is not implemented in ldns (yet)
 */
enum ldns_enum_directive
{
	LDNS_DIR_TTL,
	LDNS_DIR_ORIGIN,
	LDNS_DIR_INCLUDE
};
typedef enum ldns_enum_directive ldns_directive;

/** 
 * returns a token/char from the stream F.
 * This function deals with ( and ) in the stream,
 * and ignores them when encountered
 * \param[in] *f the file to read from
 * \param[out] *token the read token is put here
 * \param[in] *delim chars at which the parsing should stop
 * \param[in] *limit how much to read. If 0 the builtin maximum is used
 * \return 0 on error of EOF of the stream F.  Otherwise return the length of what is read
 */
ssize_t ldns_fget_token(FILE *f, char *token, const char *delim, size_t limit);

/** 
 * returns a token/char from the stream F.
 * This function deals with ( and ) in the stream,
 * and ignores when it finds them.
 * \param[in] *f the file to read from
 * \param[out] *token the token is put here
 * \param[in] *delim chars at which the parsing should stop
 * \param[in] *limit how much to read. If 0 use builtin maximum
 * \param[in] line_nr pointer to an integer containing the current line number (for debugging purposes)
 * \return 0 on error of EOF of F otherwise return the length of what is read
 */
ssize_t ldns_fget_token_l(FILE *f, char *token, const char *delim, size_t limit, int *line_nr);

/** 
 * returns a token/char from the stream f.
 * This function deals with ( and ) in the stream,
 * and ignores when it finds them.
 * \param[in] *f the file to read from
 * \param[out] **token this should be a reference to a string buffer in which
 *                     the token is put. A new buffer will be allocated when
 *                     *token is NULL and fixed is false. If the buffer is too
 *                     small to hold the token, the buffer is reallocated with
 *                     double the size (of limit).
 *                     If fixed is true, the string buffer may not be NULL
 *                     and limit must be set to the buffer size. In that case
 *                     no reallocations will be done.
 * \param[in,out] *limit reference to the size of the token buffer. Will be
 *                       reset to the new limit of the token buffer if the
 *                       buffer is reallocated.
 * \param [in] fixed If fixed is false, the token buffer is allowed to grow
 *                   when needed (by way of reallocation). If true, the token
 *                   buffer will not be resized.
 * \param[in] *delim chars at which the parsing should stop
 * \param[in] line_nr pointer to an integer containing the current line number (for debugging purposes)
 * \return LDNS_STATUS_OK on success, LDNS_STATUS_SYNTAX_EMPTY when no token
 *         was read and an error otherwise.
 */
ldns_status ldns_fget_token_l_st(FILE *f, char **token, size_t *limit, bool fixed, const char *delim, int *line_nr);

ssize_t ldns_fget_token_l_resolv_conf(FILE *f, char *token, const char *delim, size_t limit, int *line_nr);

/**
 * returns a token/char from the buffer b.
 * This function deals with ( and ) in the buffer,
 * and ignores when it finds them.
 * \param[in] *b the buffer to read from
 * \param[out] *token the token is put here
 * \param[in] *delim chars at which the parsing should stop
 * \param[in] *limit how much to read. If 0 the builtin maximum is used
 * \returns 0 on error of EOF of b. Otherwise return the length of what is read
 */
ssize_t ldns_bget_token(ldns_buffer *b, char *token, const char *delim, size_t limit);

/*
 * searches for keyword and delim in a file. Gives everything back
 * after the keyword + k_del until we hit d_del
 * \param[in] f file pointer to read from
 * \param[in] keyword keyword to look for
 * \param[in] k_del keyword delimiter 
 * \param[out] data the data found 
 * \param[in] d_del the data delimiter
 * \param[in] data_limit maximum size the the data buffer
 * \return the number of character read
 */
ssize_t ldns_fget_keyword_data(FILE *f, const char *keyword, const char *k_del, char *data, const char *d_del, size_t data_limit);

/*
 * searches for keyword and delim. Gives everything back
 * after the keyword + k_del until we hit d_del
 * \param[in] f file pointer to read from
 * \param[in] keyword keyword to look for
 * \param[in] k_del keyword delimiter 
 * \param[out] data the data found 
 * \param[in] d_del the data delimiter
 * \param[in] data_limit maximum size the the data buffer
 * \param[in] line_nr pointer to an integer containing the current line number (for
debugging purposes)
 * \return the number of character read
 */
ssize_t ldns_fget_keyword_data_l(FILE *f, const char *keyword, const char *k_del, char *data, const char *d_del, size_t data_limit, int *line_nr);

/*
 * searches for keyword and delim in a buffer. Gives everything back
 * after the keyword + k_del until we hit d_del
 * \param[in] b buffer pointer to read from
 * \param[in] keyword keyword to look for
 * \param[in] k_del keyword delimiter 
 * \param[out] data the data found 
 * \param[in] d_del the data delimiter
 * \param[in] data_limit maximum size the the data buffer
 * \return the number of character read
 */
ssize_t ldns_bget_keyword_data(ldns_buffer *b, const char *keyword, const char *k_del, char *data, const char *d_del, size_t data_limit);

/**
 * returns the next character from a buffer. Advances the position pointer with 1.
 * When end of buffer is reached returns EOF. This is the buffer's equivalent
 * for getc().
 * \param[in] *buffer buffer to read from
 * \return EOF on failure otherwise return the character
 */
int ldns_bgetc(ldns_buffer *buffer);

/**
 * skips all of the characters in the given string in the buffer, moving
 * the position to the first character that is not in *s.
 * \param[in] *buffer buffer to use
 * \param[in] *s characters to skip
 * \return void
 */
void ldns_bskipcs(ldns_buffer *buffer, const char *s);

/**
 * skips all of the characters in the given string in the fp, moving
 * the position to the first character that is not in *s.
 * \param[in] *fp file to use
 * \param[in] *s characters to skip
 * \return void
 */
void ldns_fskipcs(FILE *fp, const char *s);


/**
 * skips all of the characters in the given string in the fp, moving
 * the position to the first character that is not in *s.
 * \param[in] *fp file to use
 * \param[in] *s characters to skip
 * \param[in] line_nr pointer to an integer containing the current line number (for debugging purposes)
 * \return void
 */
void ldns_fskipcs_l(FILE *fp, const char *s, int *line_nr);

#ifdef __cplusplus
}
#endif

#endif /* LDNS_PARSE_H */
