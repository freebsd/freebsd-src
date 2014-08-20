/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file CSS lexer
 * 
 * See docs/Tokens for the production rules used by this lexer.
 *
 * See docs/Lexer for the inferred first characters for each token.
 *
 * See also CSS3 Syntax module and CSS2.1 $4.1.1 + errata
 *
 * The lexer assumes that all invalid Unicode codepoints have been converted
 * to U+FFFD by the input stream.
 *
 * The lexer comprises a state machine, the top-level of which is derived from
 * the First sets in docs/Lexer. Each top-level state may contain a number of
 * sub states. These enable restarting of the parser.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <parserutils/charset/utf8.h>
#include <parserutils/input/inputstream.h>
#include <parserutils/utils/buffer.h>

#include <libcss/errors.h>

#include "lex/lex.h"
#include "utils/parserutilserror.h"
#include "utils/utils.h"

/** \todo Optimisation -- we're currently revisiting a bunch of input 
 *	  characters (Currently, we're calling parserutils_inputstream_peek 
 *	  about 1.5x the number of characters in the input stream). Ideally, 
 *	  we'll visit each character in the input exactly once. In reality, 
 *	  the upper bound is twice, due to the need, in some cases, to read 
 *	  one character beyond the end of a token's input to detect the end 
 *	  of the token. Resumability adds a little overhead here, unless 
 *	  we're somewhat more clever when it comes to having support for 
 *	  restarting mid-escape sequence. Currently, we rewind back to the 
 *	  start of the sequence and process the whole thing again.
 */

enum {
	sSTART		=  0,
	sATKEYWORD	=  1,
	sSTRING		=  2,
	sHASH		=  3,
	sNUMBER		=  4, 
	sCDO		=  5,
	sCDC		=  6,
	sS		=  7,
	sCOMMENT	=  8,
	sMATCH		=  9, 
	sURI		= 10,
	sIDENT		= 11,
	sESCAPEDIDENT	= 12,
	sURL		= 13,
	sUCR		= 14 
};

/**
 * CSS lexer object
 */
struct css_lexer
{
	parserutils_inputstream *input;	/**< Inputstream containing CSS */

	size_t bytesReadForToken;	/**< Total bytes read from the 
					 * inputstream for the current token */

	css_token token;		/**< The current token */

	bool escapeSeen;		/**< Whether an escape sequence has 
					 * been seen while processing the input
					 * for the current token */
	parserutils_buffer *unescapedTokenData;	/**< Buffer containing 
					 	 * unescaped token data 
						 * (used iff escapeSeen == true)
						 */

	unsigned int state    : 4,	/**< Current state */
		     substate : 4;	/**< Current substate */

	struct {
		uint8_t first;		/**< First character read for token */
		size_t origBytes;	/**< Storage of current number of 
					 * bytes read, for rewinding */
		bool lastWasStar;	/**< Whether the previous character 
					 * was an asterisk */
		bool lastWasCR;		/**< Whether the previous character
					 * was CR */
		size_t bytesForURL;	/**< Input bytes read for "url(", for 
					 * rewinding */
		size_t dataLenForURL;	/**< Output length for "url(", for
					 * rewinding */
		int hexCount;		/**< Counter for reading hex digits */
	} context;			/**< Context for the current state */

	bool emit_comments;		/**< Whether to emit comment tokens */

	uint32_t currentCol;		/**< Current column in source */
	uint32_t currentLine;		/**< Current line in source */
};

#define APPEND(lexer, data, len)					\
do {									\
	css_error error;						\
	error = appendToTokenData((lexer), 				\
			(const uint8_t *) (data), (len));		\
	if (error != CSS_OK)						\
		return error;						\
	(lexer)->bytesReadForToken += (len);				\
	(lexer)->currentCol += (len);					\
} while(0)								\

static css_error appendToTokenData(css_lexer *lexer, 
		const uint8_t *data, size_t len);
static css_error emitToken(css_lexer *lexer, css_token_type type,
		css_token **token);

static css_error AtKeyword(css_lexer *lexer, css_token **token);
static css_error CDCOrIdentOrFunctionOrNPD(css_lexer *lexer,
		css_token **token);
static css_error CDO(css_lexer *lexer, css_token **token);
static css_error Comment(css_lexer *lexer, css_token **token);
static css_error EscapedIdentOrFunction(css_lexer *lexer,
		css_token **token);
static css_error Hash(css_lexer *lexer, css_token **token);
static css_error IdentOrFunction(css_lexer *lexer,
		css_token **token);
static css_error Match(css_lexer *lexer, css_token **token);
static css_error NumberOrPercentageOrDimension(css_lexer *lexer,
		css_token **token);
static css_error S(css_lexer *lexer, css_token **token);
static css_error Start(css_lexer *lexer, css_token **token);
static css_error String(css_lexer *lexer, css_token **token);
static css_error URIOrUnicodeRangeOrIdentOrFunction(
		css_lexer *lexer, css_token **token);
static css_error URI(css_lexer *lexer, css_token **token);
static css_error UnicodeRange(css_lexer *lexer, css_token **token);

static css_error consumeDigits(css_lexer *lexer);
static css_error consumeEscape(css_lexer *lexer, bool nl);
static css_error consumeNMChars(css_lexer *lexer);
static css_error consumeString(css_lexer *lexer);
static css_error consumeStringChars(css_lexer *lexer);
static css_error consumeUnicode(css_lexer *lexer, uint32_t ucs);
static css_error consumeURLChars(css_lexer *lexer);
static css_error consumeWChars(css_lexer *lexer);

static inline bool startNMChar(uint8_t c);
static inline bool startNMStart(uint8_t c);
static inline bool startStringChar(uint8_t c);
static inline bool startURLChar(uint8_t c);
static inline bool isSpace(uint8_t c);

/**
 * Create a lexer instance
 *
 * \param input  The inputstream to read from
 * \param lexer  Pointer to location to receive lexer instance
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters,
 *         CSS_NOMEM on memory exhaustion
 */
css_error css__lexer_create(parserutils_inputstream *input, css_lexer **lexer)
{
	css_lexer *lex;

	if (input == NULL || lexer == NULL)
		return CSS_BADPARM;

	lex = malloc(sizeof(css_lexer));
	if (lex == NULL)
		return CSS_NOMEM;

	lex->input = input;
	lex->bytesReadForToken = 0;
	lex->token.type = CSS_TOKEN_EOF;
	lex->token.data.data = NULL;
	lex->token.data.len = 0;
	lex->escapeSeen = false;
	lex->unescapedTokenData = NULL;
	lex->state = sSTART;
	lex->substate = 0;
	lex->emit_comments = false;
	lex->currentCol = 1;
	lex->currentLine = 1;

	*lexer = lex;

	return CSS_OK;
}

/**
 * Destroy a lexer instance
 *
 * \param lexer  The instance to destroy
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__lexer_destroy(css_lexer *lexer)
{
	if (lexer == NULL)
		return CSS_BADPARM;

	if (lexer->unescapedTokenData != NULL)
		parserutils_buffer_destroy(lexer->unescapedTokenData);

	free(lexer);

	return CSS_OK;
}

/**
 * Configure a lexer instance
 *
 * \param lexer   The lexer to configure
 * \param type    The option type to modify
 * \param params  Option-specific parameters
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__lexer_setopt(css_lexer *lexer, css_lexer_opttype type,
		css_lexer_optparams *params)
{
	if (lexer == NULL || params == NULL)
		return CSS_BADPARM;

	switch (type) {
	case CSS_LEXER_EMIT_COMMENTS:
		lexer->emit_comments = params->emit_comments;
		break;
	default:
		return CSS_BADPARM;
	}

	return CSS_OK;
}

/**
 * Retrieve a token from a lexer
 *
 * \param lexer  The lexer instance to read from
 * \param token  Pointer to location to receive pointer to token
 * \return CSS_OK on success, appropriate error otherwise
 *
 * The returned token object is owned by the lexer. However, the client is
 * permitted to modify the data members of the token. The token must not be
 * freed by the client (it may not have been allocated in the first place),
 * nor may any of the pointers contained within it. The client may, if they
 * wish, overwrite any data member of the returned token object -- the lexer
 * does not depend on these remaining constant. This allows the client code
 * to efficiently implement a push-back buffer with interned string data.
 */
css_error css__lexer_get_token(css_lexer *lexer, css_token **token)
{
	css_error error;

	if (lexer == NULL || token == NULL)
		return CSS_BADPARM;

	switch (lexer->state)
	{
	case sSTART:
	start:
		return Start(lexer, token);
	case sATKEYWORD:
		return AtKeyword(lexer, token);
	case sSTRING:
		return String(lexer, token);
	case sHASH:
		return Hash(lexer, token);
	case sNUMBER:
		return NumberOrPercentageOrDimension(lexer, token);
	case sCDO:
		return CDO(lexer, token);
	case sCDC:
		return CDCOrIdentOrFunctionOrNPD(lexer, token);
	case sS:
		return S(lexer, token);
	case sCOMMENT:
		error = Comment(lexer, token);
		if (!lexer->emit_comments && error == CSS_OK && 
				(*token)->type == CSS_TOKEN_COMMENT)
			goto start;
		return error;
	case sMATCH:
		return Match(lexer, token);
	case sURI:
		return URI(lexer, token);
	case sIDENT:
		return IdentOrFunction(lexer, token);
	case sESCAPEDIDENT:
		return EscapedIdentOrFunction(lexer, token);
	case sURL:
		return URI(lexer, token);
	case sUCR:
		return UnicodeRange(lexer, token);
	}

	/* Should never be reached */
	assert(0);

	return CSS_OK;
}

/******************************************************************************
 * Utility routines                                                           *
 ******************************************************************************/

/**
 * Append some data to the current token
 *
 * \param lexer  The lexer instance
 * \param data   Pointer to data to append
 * \param len    Length, in bytes, of data
 * \return CSS_OK on success, appropriate error otherwise
 *
 * This should not be called directly without good reason. Use the APPEND()
 * macro instead. 
 */
css_error appendToTokenData(css_lexer *lexer, const uint8_t *data, size_t len)
{
	css_token *token = &lexer->token;

	if (lexer->escapeSeen) {
		css_error error = css_error_from_parserutils_error(
				parserutils_buffer_append(
					lexer->unescapedTokenData, data, len));
		if (error != CSS_OK)
			return error;
	}

	token->data.len += len;

	return CSS_OK;
}

/**
 * Prepare a token for consumption and emit it to the client
 *
 * \param lexer  The lexer instance
 * \param type   The type of token to emit
 * \param token  Pointer to location to receive pointer to token
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error emitToken(css_lexer *lexer, css_token_type type,
		css_token **token)
{
	css_token *t = &lexer->token;

	t->type = type;

	/* Calculate token data start pointer. We have to do this here as 
	 * the inputstream's buffer may have moved under us. */
	if (lexer->escapeSeen) {
		t->data.data = lexer->unescapedTokenData->data;
	} else {
		size_t clen;
		const uint8_t *data;
		parserutils_error error; 

		error = parserutils_inputstream_peek(lexer->input, 0, 
				&data, &clen);

#ifndef NDEBUG
		assert(type == CSS_TOKEN_EOF || error == PARSERUTILS_OK);
#else
		(void) error;
#endif

		t->data.data = (type == CSS_TOKEN_EOF) ? NULL : (uint8_t *) data;
	}

	switch (type) {
	case CSS_TOKEN_ATKEYWORD:
		/* Strip the '@' from the front */
		t->data.data += 1;
		t->data.len -= 1;
		break;
	case CSS_TOKEN_STRING:
		/* Strip the leading quote */
		t->data.data += 1;
		t->data.len -= 1;

		/* Strip the trailing quote, iff it exists (may have hit EOF) */
		if (t->data.len > 0 && (t->data.data[t->data.len - 1] == '"' ||
				t->data.data[t->data.len - 1] == '\'')) {
			t->data.len -= 1;
		}
		break;
	case CSS_TOKEN_INVALID_STRING:
		/* Strip the leading quote */
		t->data.data += 1;
		t->data.len -= 1;
		break;
	case CSS_TOKEN_HASH:
		/* Strip the '#' from the front */
		t->data.data += 1;
		t->data.len -= 1;
		break;
	case CSS_TOKEN_PERCENTAGE:
		/* Strip the '%' from the end */
		t->data.len -= 1;
		break;
	case CSS_TOKEN_DIMENSION:
		break;
	case CSS_TOKEN_URI:
		/* Strip the "url(" from the start */
		t->data.data += SLEN("url(");
		t->data.len -= SLEN("url(");

		/* Strip any leading whitespace */
		while (isSpace(t->data.data[0])) {
			t->data.data++;
			t->data.len--;
		}

		/* Strip any leading quote */
		if (t->data.data[0] == '"' || t->data.data[0] == '\'') {
			t->data.data += 1;
			t->data.len -= 1;
		}

		/* Strip the trailing ')' */
		t->data.len -= 1;

		/* Strip any trailing whitespace */
		while (t->data.len > 0 &&
				isSpace(t->data.data[t->data.len - 1])) {
			t->data.len--;
		}

		/* Strip any trailing quote */
		if (t->data.len > 0 && (t->data.data[t->data.len - 1] == '"' || 
				t->data.data[t->data.len - 1] == '\'')) {
			t->data.len -= 1;
		}
		break;
	case CSS_TOKEN_UNICODE_RANGE:
		/* Remove "U+" from the start */
		t->data.data += SLEN("U+");
		t->data.len -= SLEN("U+");
		break;
	case CSS_TOKEN_COMMENT:
		/* Strip the leading '/' and '*' */
		t->data.data += SLEN("/*");
		t->data.len -= SLEN("/*");

		/* Strip the trailing '*' and '/' */
		t->data.len -= SLEN("*/");
		break;
	case CSS_TOKEN_FUNCTION:
		/* Strip the trailing '(' */
		t->data.len -= 1;
		break;
	default:
		break;
	}

	*token = t;

	/* Reset the lexer's state */
	lexer->state = sSTART;
	lexer->substate = 0;

	return CSS_OK;
}

/******************************************************************************
 * State machine components                                                   *
 ******************************************************************************/

css_error AtKeyword(css_lexer *lexer, css_token **token)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;
	enum { Initial = 0, Escape = 1, NMChar = 2 };

	/* ATKEYWORD = '@' ident 
	 * 
	 * The '@' has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		c = *cptr;

		if (!startNMStart(c))
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->bytesReadForToken += clen;
			goto escape;
		}

		/* Fall through */
	case NMChar:
	nmchar:
		lexer->substate = NMChar;
		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;
		break;

	case Escape:
	escape:
		lexer->substate = Escape;
		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			}

			return error;
		}

		goto nmchar;
	}

	return emitToken(lexer, CSS_TOKEN_ATKEYWORD, token);
}

css_error CDCOrIdentOrFunctionOrNPD(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;
	enum { Initial = 0, Escape = 1, Gt = 2 };

	/* CDC = "-->"
	 * IDENT = [-]? nmstart nmchar*
	 * FUNCTION = [-]? nmstart nmchar* '('
	 * NUMBER = num = [-+]? ([0-9]+ | [0-9]* '.' [0-9]+)
	 * PERCENTAGE = num '%'
	 * DIMENSION = num ident
	 *
	 * The first dash has been consumed. Thus, we must consume the next 
	 * character in the stream. If it's a dash, then we're dealing with 
	 * CDC. If it's a digit or dot, then we're dealing with NPD. 
	 * Otherwise, we're dealing with IDENT/FUNCTION.
	 */

	switch (lexer->substate) {
	case Initial:
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* We can only match char with what we've read so far */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *cptr;

		if (isDigit(c) || c == '.') {
			/* NPD */
			APPEND(lexer, cptr, clen);
			lexer->state = sNUMBER;
			lexer->substate = 0;
			/* Abuse "first" to store first non-sign character */
			lexer->context.first = c;
			return NumberOrPercentageOrDimension(lexer, token);
		}

		if (c != '-' && !startNMStart(c)) {
			/* Can only be CHAR */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}


		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c != '-') {
			if (c == '\\') {
				lexer->bytesReadForToken += clen;
				goto escape;
			}

			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Fall through */
	case Gt:
		lexer->substate = Gt;

		/* Ok, so we're dealing with CDC. Expect a '>' */
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* CHAR is the only match here */
			/* Remove the '-' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *cptr;

		if (c == '>') {
			APPEND(lexer, cptr, clen);

			t->type = CSS_TOKEN_CDC;
		} else {
			/* Remove the '-' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			t->type = CSS_TOKEN_CHAR;
		}
		break;

	case Escape:
	escape:
		lexer->substate = Escape;
		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			}

			return error;
		}

		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	}

	return emitToken(lexer, t->type, token);
}

css_error CDO(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;
	enum { Initial = 0, Dash1 = 1, Dash2 = 2 };

	/* CDO = "<!--"
	 * 
	 * The '<' has been consumed
	 */

	switch (lexer->substate) {
	case Initial:
		/* Expect '!' */
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* CHAR is the only match here */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *cptr;

		if (c == '!') {
			APPEND(lexer, cptr, clen);
		} else {
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		/* Fall Through */
	case Dash1:
		lexer->substate = Dash1;

		/* Expect '-' */
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* CHAR is the only match here */
			/* Remove the '!' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *cptr;

		if (c == '-') {
			APPEND(lexer, cptr, clen);
		} else {
			/* Remove the '!' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		/* Fall through */
	case Dash2:
		lexer->substate = Dash2;

		/* Expect '-' */
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* CHAR is the only match here */
			/* Remove the '-' and the '!' we read above */
			lexer->bytesReadForToken -= 2;
			t->data.len -= 2;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *cptr;

		if (c == '-') {
			APPEND(lexer, cptr, clen);
		} else {
			/* Remove the '-' and the '!' we read above */
			lexer->bytesReadForToken -= 2;
			t->data.len -= 2;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}
	}

	return emitToken(lexer, CSS_TOKEN_CDO, token);
}

css_error Comment(css_lexer *lexer, css_token **token)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;
	enum { Initial = 0, InComment = 1 };

	/* COMMENT = '/' '*' [^*]* '*'+ ([^/] [^*]* '*'+)* '/'
	 *
	 * The '/' has been consumed.
	 */
	switch (lexer->substate) {
	case Initial:
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		c = *cptr;

		if (c != '*')
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		APPEND(lexer, cptr, clen);
	
		/* Fall through */
	case InComment:
		lexer->substate = InComment;

		while (1) {
			perror = parserutils_inputstream_peek(lexer->input,
					lexer->bytesReadForToken, &cptr, &clen);
			if (perror != PARSERUTILS_OK && 
					perror != PARSERUTILS_EOF)
				return css_error_from_parserutils_error(perror);

			if (perror == PARSERUTILS_EOF) {
				/* As per unterminated strings, 
				 * we ignore unterminated comments. */
				return emitToken(lexer, CSS_TOKEN_EOF, token);
			}

			c = *cptr;

			APPEND(lexer, cptr, clen);

			if (lexer->context.lastWasStar && c == '/')
				break;

			lexer->context.lastWasStar = (c == '*');

			if (c == '\n' || c == '\f') {
				lexer->currentCol = 1;
				lexer->currentLine++;
			}

			if (lexer->context.lastWasCR && c != '\n') {
				lexer->currentCol = 1;
				lexer->currentLine++;
			}
			lexer->context.lastWasCR = (c == '\r');
		}
	}

	return emitToken(lexer, CSS_TOKEN_COMMENT, token);
}

css_error EscapedIdentOrFunction(css_lexer *lexer, css_token **token)
{
	css_error error;

	/* IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * In this case, nmstart is an escape sequence and no '-' is present.
	 *
	 * The '\\' has been consumed.
	 */

	error = consumeEscape(lexer, false);
	if (error != CSS_OK) {
		if (error == CSS_EOF || error == CSS_INVALID) {
			/* The '\\' is a token of its own */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		return error;
	}

	lexer->state = sIDENT;
	lexer->substate = 0;
	return IdentOrFunction(lexer, token);
}

css_error Hash(css_lexer *lexer, css_token **token)
{	
	css_error error;
	
	/* HASH = '#' name  = '#' nmchar+ 
	 *
	 * The '#' has been consumed.
	 */

	error = consumeNMChars(lexer);
	if (error != CSS_OK)
		return error;

	/* Require at least one NMChar otherwise, we're just a raw '#' */
	if (lexer->bytesReadForToken - lexer->context.origBytes > 0)
		return emitToken(lexer, CSS_TOKEN_HASH, token);

	return emitToken(lexer, CSS_TOKEN_CHAR, token);
}

css_error IdentOrFunction(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;
	enum { Initial = 0, Bracket = 1 };

	/* IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * The optional dash and nmstart are already consumed
	 */

	switch (lexer->substate) {
	case Initial:
		/* Consume all subsequent nmchars (if any exist) */
		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Bracket:
		lexer->substate = Bracket;

		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* IDENT, rather than CHAR */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		}

		c = *cptr;

		if (c == '(') {
			APPEND(lexer, cptr, clen);

			t->type = CSS_TOKEN_FUNCTION;
		} else {
			t->type = CSS_TOKEN_IDENT;
		}
	}

	return emitToken(lexer, t->type, token);
}

css_error Match(css_lexer *lexer, css_token **token)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;
	css_token_type type = CSS_TOKEN_EOF; /* GCC's braindead */

	/* INCLUDES       = "~="
	 * DASHMATCH      = "|="
	 * PREFIXMATCH    = "^="
	 * SUFFIXMATCH    = "$="
	 * SUBSTRINGMATCH = "*="
	 *
	 * The first character has been consumed.
	 */

	perror = parserutils_inputstream_peek(lexer->input,
			lexer->bytesReadForToken, &cptr, &clen);
	if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
		return css_error_from_parserutils_error(perror);

	if (perror == PARSERUTILS_EOF)
		return emitToken(lexer, CSS_TOKEN_CHAR, token);

	c = *cptr;

	if (c != '=')
		return emitToken(lexer, CSS_TOKEN_CHAR, token);

	APPEND(lexer, cptr, clen);

	switch (lexer->context.first) {
	case '~':
		type = CSS_TOKEN_INCLUDES;
		break;
	case '|':
		type = CSS_TOKEN_DASHMATCH;
		break;
	case '^':
		type = CSS_TOKEN_PREFIXMATCH;
		break;
	case '$':
		type = CSS_TOKEN_SUFFIXMATCH;
		break;
	case '*':
		type = CSS_TOKEN_SUBSTRINGMATCH;
		break;
	default:
		assert(0);
	}

	return emitToken(lexer, type, token);
}

css_error NumberOrPercentageOrDimension(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;
	enum { Initial = 0, Dot = 1, MoreDigits = 2, 
		Suffix = 3, NMChars = 4, Escape = 5 };

	/* NUMBER = num = [-+]? ([0-9]+ | [0-9]* '.' [0-9]+)
	 * PERCENTAGE = num '%'
	 * DIMENSION = num ident
	 *
	 * The sign, or sign and first digit or dot, 
	 * or first digit, or '.' has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		error = consumeDigits(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Dot:
		lexer->substate = Dot;

		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			if (t->data.len == 1 && (lexer->context.first == '.' ||
					lexer->context.first == '+'))
				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			else
				return emitToken(lexer, CSS_TOKEN_NUMBER, 
						token);
		}

		c = *cptr;

		/* Bail if we've not got a '.' or we've seen one already */
		if (c != '.' || lexer->context.first == '.')
			goto suffix;

		/* Save the token length up to the end of the digits */
		lexer->context.origBytes = lexer->bytesReadForToken;

		/* Append the '.' to the token */
		APPEND(lexer, cptr, clen);

		/* Fall through */
	case MoreDigits:
		lexer->substate = MoreDigits;

		error = consumeDigits(lexer);
		if (error != CSS_OK)
			return error;

		if (lexer->bytesReadForToken - lexer->context.origBytes == 1) {
			/* No digits after dot => dot isn't part of number */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
		}

		/* Fall through */
	case Suffix:
	suffix:
		lexer->substate = Suffix;

		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			if (t->data.len == 1 && (lexer->context.first == '.' ||
					lexer->context.first == '+'))
				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			else
				return emitToken(lexer, CSS_TOKEN_NUMBER, 
						token);
		}

		c = *cptr;

		/* A solitary '.' or '+' is a CHAR, not numeric */
		if (t->data.len == 1 && (lexer->context.first == '.' ||
				lexer->context.first == '+'))
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		if (c == '%') {
			APPEND(lexer, cptr, clen);

			return emitToken(lexer, CSS_TOKEN_PERCENTAGE, token);
		} else if (!startNMStart(c)) {
			return emitToken(lexer, CSS_TOKEN_NUMBER, token);
		}

		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->bytesReadForToken += clen;
			goto escape;
		}

		/* Fall through */
	case NMChars:
	nmchars:
		lexer->substate = NMChars;

		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;

		break;
	case Escape:
	escape:
		lexer->substate = Escape;

		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				/* This can only be a number */
				return emitToken(lexer, 
						CSS_TOKEN_NUMBER, token);
			}

			return error;
		}

		goto nmchars;
	}

	return emitToken(lexer, CSS_TOKEN_DIMENSION, token);
}

css_error S(css_lexer *lexer, css_token **token)
{
	css_error error;

	/* S = wc*
	 * 
	 * The first whitespace character has been consumed.
	 */

	error = consumeWChars(lexer);
	if (error != CSS_OK)
		return error;

	return emitToken(lexer, CSS_TOKEN_S, token);
}

css_error Start(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;

start:

	/* Advance past the input read for the previous token */
	if (lexer->bytesReadForToken > 0) {
		parserutils_inputstream_advance(
				lexer->input, lexer->bytesReadForToken);
		lexer->bytesReadForToken = 0;
	}

	/* Reset in preparation for the next token */
	t->type = CSS_TOKEN_EOF;
	t->data.data = NULL;
	t->data.len = 0;
	t->idata = NULL;
	t->col = lexer->currentCol;
	t->line = lexer->currentLine;
	lexer->escapeSeen = false;
	if (lexer->unescapedTokenData != NULL)
		lexer->unescapedTokenData->length = 0;

	perror = parserutils_inputstream_peek(lexer->input, 0, &cptr, &clen);
	if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
		return css_error_from_parserutils_error(perror);

	if (perror == PARSERUTILS_EOF)
		return emitToken(lexer, CSS_TOKEN_EOF, token);

	APPEND(lexer, cptr, clen);

	c = *cptr;

	if (clen > 1 || c >= 0x80) {
		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	}

	switch (c) {
	case '@':
		lexer->state = sATKEYWORD;
		lexer->substate = 0;
		return AtKeyword(lexer, token);
	case '"': case '\'':
		lexer->state = sSTRING;
		lexer->substate = 0;
		lexer->context.first = c;
		return String(lexer, token);
	case '#':
		lexer->state = sHASH;
		lexer->substate = 0;
		lexer->context.origBytes = lexer->bytesReadForToken;
		return Hash(lexer, token);
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9':
	case '.': case '+':
		lexer->state = sNUMBER;
		lexer->substate = 0;
		lexer->context.first = c;
		return NumberOrPercentageOrDimension(lexer, token);
	case '<':
		lexer->state = sCDO;
		lexer->substate = 0;
		return CDO(lexer, token);
	case '-':
		lexer->state = sCDC;
		lexer->substate = 0;
		return CDCOrIdentOrFunctionOrNPD(lexer, token);
	case ' ': case '\t': case '\r': case '\n': case '\f':
		lexer->state = sS;
		lexer->substate = 0;
		if (c == '\n' || c == '\f') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}
		lexer->context.lastWasCR = (c == '\r');
		return S(lexer, token);
	case '/':
		lexer->state = sCOMMENT;
		lexer->substate = 0;
		lexer->context.lastWasStar = false;
		lexer->context.lastWasCR = false;
		error = Comment(lexer, token);
		if (!lexer->emit_comments && error == CSS_OK &&
				(*token)->type == CSS_TOKEN_COMMENT)
			goto start;
		return error;
	case '~': case '|': case '^': case '$': case '*':
		lexer->state = sMATCH;
		lexer->substate = 0;
		lexer->context.first = c;
		return Match(lexer, token);
	case 'u': case 'U':
		lexer->state = sURI;
		lexer->substate = 0;
		return URIOrUnicodeRangeOrIdentOrFunction(lexer, token);
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': 
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': 
	case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
	case 's': case 't': /*  'u'*/ case 'v': case 'w': case 'x': 
	case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': /*  'U'*/ case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
	case '_': 
		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	case '\\':
		lexer->state = sESCAPEDIDENT;
		lexer->substate = 0;
		return EscapedIdentOrFunction(lexer, token);
	default:
		return emitToken(lexer, CSS_TOKEN_CHAR, token);
	}
}

css_error String(css_lexer *lexer, css_token **token)
{
	css_error error;

	/* STRING = string
	 *
	 * The open quote has been consumed.
	 */

	error = consumeString(lexer);
	if (error != CSS_OK && error != CSS_EOF && error != CSS_INVALID)
		return error;

	/* EOF will be reprocessed in Start() */
	return emitToken(lexer, 
			error == CSS_INVALID ? CSS_TOKEN_INVALID_STRING 
					     : CSS_TOKEN_STRING, 
			token);
}

css_error URIOrUnicodeRangeOrIdentOrFunction(css_lexer *lexer, 
		css_token **token)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;

	/* URI = "url(" w (string | urlchar*) w ')' 
	 * UNICODE-RANGE = [Uu] '+' [0-9a-fA-F?]{1,6}(-[0-9a-fA-F]{1,6})?
	 * IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * The 'u' (or 'U') has been consumed.
	 */

	perror = parserutils_inputstream_peek(lexer->input, 
			lexer->bytesReadForToken, &cptr, &clen);
	if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
		return css_error_from_parserutils_error(perror);

	if (perror == PARSERUTILS_EOF) {
		/* IDENT, rather than CHAR */
		return emitToken(lexer, CSS_TOKEN_IDENT, token);
	}

	c = *cptr;

	if (c == 'r' || c == 'R') {
		APPEND(lexer, cptr, clen);

		lexer->state = sURL;
		lexer->substate = 0;
		return URI(lexer, token);
	} else if (c == '+') {
		APPEND(lexer, cptr, clen);

		lexer->state = sUCR;
		lexer->substate = 0;
		lexer->context.hexCount = 0;
		return UnicodeRange(lexer, token);
	}

	/* Can only be IDENT or FUNCTION. Reprocess current character */
	lexer->state = sIDENT;
	lexer->substate = 0;
	return IdentOrFunction(lexer, token);
}

css_error URI(css_lexer *lexer, css_token **token)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;
	enum { Initial = 0, LParen = 1, W1 = 2, Quote = 3, 
		URL = 4, W2 = 5, RParen = 6, String = 7 };

	/* URI = "url(" w (string | urlchar*) w ')' 
	 *
	 * 'u' and 'r' have been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* IDENT */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		}

		c = *cptr;

		if (c == 'l' || c == 'L') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Fall through */
	case LParen:
		lexer->substate = LParen;

		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return emitToken(lexer, CSS_TOKEN_IDENT, token);

		c = *cptr;

		if (c == '(') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Save the number of input bytes read for "url(" */
		lexer->context.bytesForURL = lexer->bytesReadForToken;
		/* And the length of the token data at the same point */
		lexer->context.dataLenForURL = lexer->token.data.len;

		lexer->context.lastWasCR = false;

		/* Fall through */
	case W1:
		lexer->substate = W1;

		error = consumeWChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Quote:
		lexer->substate = Quote;

		perror = parserutils_inputstream_peek(lexer->input,
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		c = *cptr;

		if (c == '"' || c == '\'') {
			APPEND(lexer, cptr, clen);

			lexer->context.first = c;

			goto string;
		}

		/* Potential minor optimisation: If string is more common, 
		 * then fall through to that state and branch for the URL 
		 * state. Need to investigate a reasonably large corpus of 
		 * real-world data to determine if this is worthwhile. */

		/* Fall through */
	case URL:
		lexer->substate = URL;

		error = consumeURLChars(lexer);
		if (error != CSS_OK)
			return error;

		lexer->context.lastWasCR = false;

		/* Fall through */
	case W2:
	w2:
		lexer->substate = W2;

		error = consumeWChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case RParen:
		lexer->substate = RParen;

		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		c = *cptr;

		if (c != ')') {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		APPEND(lexer, cptr, clen);
		break;
	case String:
	string:
		lexer->substate = String;

		error = consumeString(lexer);
		if (error == CSS_INVALID) {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		} else if (error != CSS_OK && error != CSS_EOF) {
			return error;
		}

		/* EOF gets handled in RParen */

		lexer->context.lastWasCR = false;

		goto w2;
	}

	return emitToken(lexer, CSS_TOKEN_URI, token);
}

css_error UnicodeRange(css_lexer *lexer, css_token **token)
{
	css_token *t = &lexer->token;
	const uint8_t *cptr;
	uint8_t c = 0; /* GCC: shush */
	size_t clen;
	parserutils_error perror;
	enum { Initial = 0, MoreDigits = 1 };

	/* UNICODE-RANGE = [Uu] '+' [0-9a-fA-F?]{1,6}(-[0-9a-fA-F]{1,6})?
	 * 
	 * "U+" has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		/* Attempt to consume 6 hex digits (or question marks) */
		for (; lexer->context.hexCount < 6; lexer->context.hexCount++) {
			perror = parserutils_inputstream_peek(lexer->input,
					lexer->bytesReadForToken, &cptr, &clen);
			if (perror != PARSERUTILS_OK && 
					perror != PARSERUTILS_EOF)
				return css_error_from_parserutils_error(perror);

			if (perror == PARSERUTILS_EOF) {
				if (lexer->context.hexCount == 0) {
					/* Remove '+' */
					lexer->bytesReadForToken -= 1;
					t->data.len -= 1;
					/* u == IDENT */
					return emitToken(lexer, 
							CSS_TOKEN_IDENT, token);
				} else {
					return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);
				}
			}

			c = *cptr;

			if (isHex(c) || c == '?') {
				APPEND(lexer, cptr, clen);
			} else {
				break;
			}
		}

		if (lexer->context.hexCount == 0) {
			/* We didn't consume any valid Unicode Range digits */
			/* Remove the '+' */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			/* 'u' == IDENT */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		} 

		if (lexer->context.hexCount == 6) {
			/* Consumed 6 valid characters. Look for '-' */
			perror = parserutils_inputstream_peek(lexer->input, 
					lexer->bytesReadForToken, &cptr, &clen);
			if (perror != PARSERUTILS_OK && 
					perror != PARSERUTILS_EOF)
				return css_error_from_parserutils_error(perror);

			if (perror == PARSERUTILS_EOF)
				return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);

			c = *cptr;
		}

		/* If we've got a '-', then we may have a 
		 * second range component */
		if (c != '-') {
			/* Reached the end of the range */
			return emitToken(lexer, CSS_TOKEN_UNICODE_RANGE, token);
		}

		APPEND(lexer, cptr, clen);

		/* Reset count for next set of digits */
		lexer->context.hexCount = 0;

		/* Fall through */
	case MoreDigits:
		lexer->substate = MoreDigits;

		/* Consume up to 6 hex digits */
		for (; lexer->context.hexCount < 6; lexer->context.hexCount++) {
			perror = parserutils_inputstream_peek(lexer->input, 
					lexer->bytesReadForToken, &cptr, &clen);
			if (perror != PARSERUTILS_OK &&
					perror != PARSERUTILS_EOF)
				return css_error_from_parserutils_error(perror);

			if (perror == PARSERUTILS_EOF) {
				if (lexer->context.hexCount == 0) {
					/* Remove '-' */
					lexer->bytesReadForToken -= 1;
					t->data.len -= 1;
				}

				return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);
			}

			c = *cptr;

			if (isHex(c)) {
				APPEND(lexer, cptr, clen);
			} else {
				break;
			}
		}

		if (lexer->context.hexCount == 0) {
			/* No hex digits consumed. Remove '-' */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
		}
	}

	return emitToken(lexer, CSS_TOKEN_UNICODE_RANGE, token);
}

/******************************************************************************
 * Input consumers                                                            *
 ******************************************************************************/

css_error consumeDigits(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;

	/* digit = [0-9] */

	/* Consume all digits */
	do {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_OK;

		c = *cptr;

		if (isDigit(c)) {
			APPEND(lexer, cptr, clen);
		}
	} while (isDigit(c));

	return CSS_OK;
}

css_error consumeEscape(css_lexer *lexer, bool nl)
{
	const uint8_t *cptr, *sdata;
	uint8_t c;
	size_t clen, slen;
	css_error error;
	parserutils_error perror;

	/* escape = unicode | '\' [^\n\r\f0-9a-fA-F] 
	 * 
	 * The '\' has been consumed.
	 */

	perror = parserutils_inputstream_peek(lexer->input, 
			lexer->bytesReadForToken, &cptr, &clen);
	if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
		return css_error_from_parserutils_error(perror);

	if (perror == PARSERUTILS_EOF)
		return CSS_EOF;

	c = *cptr;

	if (!nl && (c == '\n' || c == '\r' || c == '\f')) {
		/* These are not permitted */
		return CSS_INVALID;
	}

	/* Create unescaped buffer, if it doesn't already exist */
	if (lexer->unescapedTokenData == NULL) {
		perror = parserutils_buffer_create(&lexer->unescapedTokenData);
		if (perror != PARSERUTILS_OK)
			return css_error_from_parserutils_error(perror);
	}

	/* If this is the first escaped character we've seen for this token,
	 * we must copy the characters we've read to the unescaped buffer */
	if (!lexer->escapeSeen) {
		if (lexer->bytesReadForToken > 1) {
			perror = parserutils_inputstream_peek(
					lexer->input, 0, &sdata, &slen);

			assert(perror == PARSERUTILS_OK);

			/* -1 to skip '\\' */
			perror = parserutils_buffer_append(
					lexer->unescapedTokenData, 
					sdata, lexer->bytesReadForToken - 1);
			if (perror != PARSERUTILS_OK)
				return css_error_from_parserutils_error(perror);
		}

		lexer->token.data.len = lexer->bytesReadForToken - 1;
		lexer->escapeSeen = true;
	}

	if (isHex(c)) {
		lexer->bytesReadForToken += clen;

		error = consumeUnicode(lexer, charToHex(c));
		if (error != CSS_OK) {
			/* Rewind for next time */
			lexer->bytesReadForToken -= clen;
		}

		return error;
	}

	/* If we're handling escaped newlines, convert CR(LF)? to LF */
	if (nl && c == '\r') {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken + clen, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF) {
			c = '\n';
			APPEND(lexer, &c, 1);

			lexer->currentCol = 1;
			lexer->currentLine++;

			return CSS_OK;
		}

		c = *cptr;

		if (c == '\n') {
			APPEND(lexer, &c, 1);
			/* And skip the '\r' in the input */
			lexer->bytesReadForToken += clen;

			lexer->currentCol = 1;
			lexer->currentLine++;

			return CSS_OK;
		}
	} else if (nl && (c == '\n' || c == '\f')) {
		/* APPEND will increment this appropriately */
		lexer->currentCol = 0;
		lexer->currentLine++;
	} else if (c != '\n' && c != '\r' && c != '\f') {
		lexer->currentCol++;
	}

	/* Append the unescaped character */
	APPEND(lexer, cptr, clen);

	return CSS_OK;
}

css_error consumeNMChars(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;

	/* nmchar = [a-zA-Z] | '-' | '_' | nonascii | escape */

	do {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_OK;

		c = *cptr;

		if (startNMChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, false);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the 
				 * right thing next time */
				lexer->bytesReadForToken -= clen;

				/* Convert either EOF or INVALID into OK.
				 * This will cause the caller to believe that
				 * all NMChars in the sequence have been 
				 * processed (and thus proceed to the next
				 * state). Eventually, the '\\' will be output
				 * as a CHAR. */
				if (error == CSS_EOF || error == CSS_INVALID)
					return CSS_OK;

				return error;
			}
		}
	} while (startNMChar(c));

	return CSS_OK;
}

css_error consumeString(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	uint8_t quote = lexer->context.first;
	uint8_t permittedquote = (quote == '"') ? '\'' : '"';
	css_error error;
	parserutils_error perror;

	/* string = '"' (stringchar | "'")* '"' | "'" (stringchar | '"')* "'"
	 *
	 * The open quote has been consumed.
	 */

	do {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_EOF;

		c = *cptr;

		if (c == permittedquote) {
			APPEND(lexer, cptr, clen);
		} else if (startStringChar(c)) {
			error = consumeStringChars(lexer);
			if (error != CSS_OK)
				return error;
		} else if (c != quote) {
			/* Invalid character in string */
			return CSS_INVALID;
		}
	} while(c != quote);

	/* Append closing quote to token data */
	APPEND(lexer, cptr, clen);

	return CSS_OK;
}

css_error consumeStringChars(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;

	/* stringchar = urlchar | ' ' | ')' | '\' nl */

	do {
		perror = parserutils_inputstream_peek(lexer->input,
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_OK;

		c = *cptr;

		if (startStringChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, true);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the 
				 * right thing next time. */
				lexer->bytesReadForToken -= clen;

				/* Convert EOF to OK. This causes the caller
				 * to believe that all StringChars have been
				 * processed. Eventually, the '\\' will be
				 * output as a CHAR. */
				if (error == CSS_EOF)
					return CSS_OK;

				return error;
			}
		}
	} while (startStringChar(c));

	return CSS_OK;

}

css_error consumeUnicode(css_lexer *lexer, uint32_t ucs)
{
	const uint8_t *cptr;
	uint8_t c = 0;
	size_t clen;
	uint8_t utf8[6];
	uint8_t *utf8data = utf8;
	size_t utf8len = sizeof(utf8);
	size_t bytesReadInit = lexer->bytesReadForToken;
	int count;
	css_error error;
	parserutils_error perror;

	/* unicode = '\' [0-9a-fA-F]{1,6} wc? 
	 *
	 * The '\' and the first digit have been consumed.
	 */

	/* Attempt to consume a further five hex digits */
	for (count = 0; count < 5; count++) {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF) {
			/* Rewind what we've read */
			lexer->bytesReadForToken = bytesReadInit;
			return css_error_from_parserutils_error(perror);
		}

		if (perror == PARSERUTILS_EOF)
			break;

		c = *cptr;

		if (isHex(c)) {
			lexer->bytesReadForToken += clen;

			ucs = (ucs << 4) | charToHex(c);
		} else {
			break;
		}
	}

	/* Sanitise UCS4 character */
	if (ucs > 0x10FFFF || ucs <= 0x0008 || ucs == 0x000B ||
			(0x000E <= ucs && ucs <= 0x001F) ||
			(0x007F <= ucs && ucs <= 0x009F) ||
			(0xD800 <= ucs && ucs <= 0xDFFF) ||
			(0xFDD0 <= ucs && ucs <= 0xFDEF) ||
			(ucs & 0xFFFE) == 0xFFFE) {
		ucs = 0xFFFD;
	} else if (ucs == 0x000D) {
		ucs = 0x000A;
	}

	/* Convert our UCS4 character to UTF-8 */
	perror = parserutils_charset_utf8_from_ucs4(ucs, &utf8data, &utf8len);
	assert(perror == PARSERUTILS_OK);

	/* Attempt to read a trailing whitespace character */
	perror = parserutils_inputstream_peek(lexer->input,
			lexer->bytesReadForToken, &cptr, &clen);
	if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF) {
		/* Rewind what we've read */
		lexer->bytesReadForToken = bytesReadInit;
		return css_error_from_parserutils_error(perror);
	}

	if (perror == PARSERUTILS_OK && *cptr == '\r') {
		/* Potential CRLF */
		const uint8_t *pCR = cptr;

		perror = parserutils_inputstream_peek(lexer->input,
				lexer->bytesReadForToken + 1, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF) {
			/* Rewind what we've read */
			lexer->bytesReadForToken = bytesReadInit;
			return css_error_from_parserutils_error(perror);
		}

		if (perror == PARSERUTILS_OK && *cptr == '\n') {
			/* CRLF -- account for CR */
			lexer->bytesReadForToken += 1;
		} else {
			/* Stray CR -- restore for later */
			cptr = pCR;
			clen = 1;
			perror = PARSERUTILS_OK;
		}
	}

	/* Append char. to the token data (unescaped buffer already set up) */
	/* We can't use the APPEND() macro here as we want to rewind correctly
	 * on error. Additionally, lexer->bytesReadForToken has already been
	 * advanced */
	error = appendToTokenData(lexer, (const uint8_t *) utf8, 
			sizeof(utf8) - utf8len);
	if (error != CSS_OK) {
		/* Rewind what we've read */
		lexer->bytesReadForToken = bytesReadInit;
		return error;
	}

	/* Deal with the whitespace character */
	if (perror == PARSERUTILS_EOF)
		return CSS_OK;

	if (isSpace(*cptr)) {
		lexer->bytesReadForToken += clen;
	}

	/* Fixup cursor position */
	if (*cptr == '\r' || *cptr == '\n' || *cptr == '\f') {
		lexer->currentCol = 1;
		lexer->currentLine++;
	} else {
		/* +2 for '\' and first digit */
		lexer->currentCol += lexer->bytesReadForToken - 
				bytesReadInit + 2;
	}

	return CSS_OK;
}

css_error consumeURLChars(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	parserutils_error perror;

	/* urlchar = [\t!#-&(*-~] | nonascii | escape */

	do {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_OK;

		c = *cptr;

		if (startURLChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, false);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the
				 * right thing next time */
				lexer->bytesReadForToken -= clen;

				/* Convert either EOF or INVALID into OK.
				 * This will cause the caller to believe that
				 * all URLChars in the sequence have been 
				 * processed (and thus proceed to the next
				 * state). Eventually, the '\\' will be output
				 * as a CHAR. */
				if (error == CSS_EOF || error == CSS_INVALID)
					return CSS_OK;

				return error;
			}
		}
	} while (startURLChar(c));

	return CSS_OK;
}

css_error consumeWChars(css_lexer *lexer)
{
	const uint8_t *cptr;
	uint8_t c;
	size_t clen;
	parserutils_error perror;

	do {
		perror = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &cptr, &clen);
		if (perror != PARSERUTILS_OK && perror != PARSERUTILS_EOF)
			return css_error_from_parserutils_error(perror);

		if (perror == PARSERUTILS_EOF)
			return CSS_OK;

		c = *cptr;

		if (isSpace(c)) {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\n' || c == '\f') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}

		if (lexer->context.lastWasCR && c != '\n') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}
		lexer->context.lastWasCR = (c == '\r');
	} while (isSpace(c));

	if (lexer->context.lastWasCR) {
		lexer->currentCol = 1;
		lexer->currentLine++;
	}

	return CSS_OK;
}

/******************************************************************************
 * More utility routines                                                      *
 ******************************************************************************/

bool startNMChar(uint8_t c)
{
	return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || 
		('0' <= c && c <= '9') || c == '-' || c >= 0x80 || c == '\\';
}

bool startNMStart(uint8_t c)
{
	return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
		c >= 0x80 || c == '\\';
}

bool startStringChar(uint8_t c)
{
	return startURLChar(c) || c == ' ' || c == ')';
}

bool startURLChar(uint8_t c)
{
	return c == '\t' || c == '!' || ('#' <= c && c <= '&') || c == '(' ||
		('*' <= c && c <= '~') || c >= 0x80 || c == '\\';
}

bool isSpace(uint8_t c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\f' || c == '\t';
}

