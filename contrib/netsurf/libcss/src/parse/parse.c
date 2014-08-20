/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>

#include <parserutils/input/inputstream.h>
#include <parserutils/utils/stack.h>
#include <parserutils/utils/vector.h>

#include "charset/detect.h"
#include "lex/lex.h"
#include "parse/parse.h"
#include "utils/parserutilserror.h"
#include "utils/utils.h"

#undef DEBUG_STACK
#undef DEBUG_EVENTS

#ifndef NDEBUG
#include <stdio.h>
extern void parserutils_stack_dump(parserutils_stack *stack, const char *prefix,
		void (*printer)(void *item));
extern void parserutils_vector_dump(parserutils_vector *vector,
		const char *prefix, void (*printer)(void *item));
#ifdef DEBUG_STACK
static void printer(void *item);
#endif
#ifdef DEBUG_EVENTS
static void tprinter(void *token);
#endif
#endif

/**
 * Major state numbers
 */
enum {
	sStart = 0,
	sStylesheet = 1,
	sStatement = 2,
	sRuleset = 3,
	sRulesetEnd = 4,
	sAtRule = 5,
	sAtRuleEnd = 6,
	sBlock = 7,
	sBlockContent = 8,
	sSelector = 9,
	sDeclaration = 10,
	sDeclList = 11,
	sDeclListEnd = 12,
	sProperty = 13,
	sValue0 = 14,
	sValue1 = 15,
	sValue = 16,
	sAny0 = 17,
	sAny1 = 18,
	sAny = 19,
	sMalformedDecl = 20,
	sMalformedSelector = 21,
	sMalformedAtRule = 22,
	sInlineStyle = 23,
	sISBody0 = 24,
	sISBody = 25
};

/**
 * Representation of a parser state
 */
typedef struct parser_state
{
	unsigned int state : 16,
	             substate : 16;
} parser_state;

/**
 * CSS parser object
 */
struct css_parser
{
	parserutils_inputstream *stream;	/**< The inputstream */
	css_lexer *lexer;		/**< The lexer to use */

	bool quirks;			/**< Whether to enable parsing quirks */

#define STACK_CHUNK 32
	parserutils_stack *states;	/**< Stack of states */

	parserutils_vector *tokens;	/**< Vector of pending tokens */

	const css_token *pushback;	/**< Push back buffer */

	bool parseError;		/**< A parse error has occurred */
	parserutils_stack *open_items;	/**< Stack of open brackets */

	uint8_t match_char;		/**< Close bracket type for parseAny */

	bool last_was_ws;		/**< Last token was whitespace */

	css_parser_event_handler event;	/**< Client's event handler */
	void *event_pw;			/**< Client data for event handler */
};

static css_error css__parser_create_internal(const char *charset, 
		css_charset_source cs_source, parser_state initial, 
		css_parser **parser);

static css_error transition(css_parser *parser, parser_state to,
		parser_state subsequent);
static css_error transitionNoRet(css_parser *parser, parser_state to);
static css_error done(css_parser *parser);
static css_error expect(css_parser *parser, css_token_type type);
static css_error getToken(css_parser *parser, const css_token **token);
static css_error pushBack(css_parser *parser, const css_token *token);
static css_error eatWS(css_parser *parser);

static css_error parseStart(css_parser *parser);
static css_error parseStylesheet(css_parser *parser);
static css_error parseStatement(css_parser *parser);
static css_error parseRuleset(css_parser *parser);
static css_error parseRulesetEnd(css_parser *parser);
static css_error parseAtRule(css_parser *parser);
static css_error parseAtRuleEnd(css_parser *parser);
static css_error parseBlock(css_parser *parser);
static css_error parseBlockContent(css_parser *parser);
static css_error parseSelector(css_parser *parser);
static css_error parseDeclaration(css_parser *parser);
static css_error parseDeclList(css_parser *parser);
static css_error parseDeclListEnd(css_parser *parser);
static css_error parseProperty(css_parser *parser);
static css_error parseValue0(css_parser *parser);
static css_error parseValue1(css_parser *parser);
static css_error parseValue(css_parser *parser);
static css_error parseAny0(css_parser *parser);
static css_error parseAny1(css_parser *parser);
static css_error parseAny(css_parser *parser);
static css_error parseMalformedDeclaration(css_parser *parser);
static css_error parseMalformedSelector(css_parser *parser);
static css_error parseMalformedAtRule(css_parser *parser);
static css_error parseInlineStyle(css_parser *parser);
static css_error parseISBody0(css_parser *parser);
static css_error parseISBody(css_parser *parser);

static void unref_interned_strings_in_tokens(css_parser *parser);

/**
 * Dispatch table for parsing, indexed by major state number
 */
static css_error (*parseFuncs[])(css_parser *parser) = {
	parseStart,
	parseStylesheet,
	parseStatement,
	parseRuleset,
	parseRulesetEnd,
	parseAtRule,
	parseAtRuleEnd,
	parseBlock,
	parseBlockContent,
	parseSelector,
	parseDeclaration,
	parseDeclList,
	parseDeclListEnd,
	parseProperty,
	parseValue0,
	parseValue1,
	parseValue,
	parseAny0,
	parseAny1,
	parseAny,
	parseMalformedDeclaration,
	parseMalformedSelector,
	parseMalformedAtRule,
	parseInlineStyle,
	parseISBody0,
	parseISBody
};

/**
 * Create a CSS parser
 *
 * \param charset     Charset of data, if known, or NULL
 * \param cs_source   Source of charset information, or CSS_CHARSET_DEFAULT
 * \param parser      Pointer to location to receive parser instance
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters,
 *         CSS_NOMEM on memory exhaustion
 */
css_error css__parser_create(const char *charset, css_charset_source cs_source,
		css_parser **parser)
{
	parser_state initial = { sStart, 0 };

	return css__parser_create_internal(charset, cs_source,
			initial, parser);
}

/**
 * Create a CSS parser for an inline style
 *
 * \param charset     Charset of data, if known, or NULL
 * \param cs_source   Source of charset information, or CSS_CHARSET_DEFAULT
 * \param parser      Pointer to location to receive parser instance
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters,
 *         CSS_NOMEM on memory exhaustion
 */
css_error css__parser_create_for_inline_style(const char *charset,
		css_charset_source cs_source, css_parser **parser)
{
	parser_state initial = { sInlineStyle, 0 };

	return css__parser_create_internal(charset, cs_source,
			initial, parser);
}

/**
 * Destroy a CSS parser
 *
 * \param parser  The parser instance to destroy
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__parser_destroy(css_parser *parser)
{
	if (parser == NULL)
		return CSS_BADPARM;

	parserutils_stack_destroy(parser->open_items);

	parserutils_vector_destroy(parser->tokens);

	parserutils_stack_destroy(parser->states);

	css__lexer_destroy(parser->lexer);

	parserutils_inputstream_destroy(parser->stream);

	free(parser);

	return CSS_OK;
}

/**
 * Configure a CSS parser
 *
 * \param parser  The parser instance to configure
 * \param type    The option to configure
 * \param params  Option-specific data
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__parser_setopt(css_parser *parser, css_parser_opttype type,
		css_parser_optparams *params)
{
	if (parser == NULL || params == NULL)
		return CSS_BADPARM;

	switch (type) {
	case CSS_PARSER_QUIRKS:
		parser->quirks = params->quirks;
		break;
	case CSS_PARSER_EVENT_HANDLER:
		parser->event = params->event_handler.handler;
		parser->event_pw = params->event_handler.pw;
		break;
	}

	return CSS_OK;
}

/**
 * Parse a chunk of data using a CSS parser
 *
 * \param parser  The parser to use
 * \param data    Pointer to the chunk to parse
 * \param len     Length of chunk
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__parser_parse_chunk(css_parser *parser, const uint8_t *data, 
		size_t len)
{
	parserutils_error perror;
	parser_state *state;
	css_error error = CSS_OK;

	if (parser == NULL || data == NULL)
		return CSS_BADPARM;

	perror = parserutils_inputstream_append(parser->stream, data, len);
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

	do {
		state = parserutils_stack_get_current(parser->states);
		if (state == NULL)
			break;

		error = parseFuncs[state->state](parser);
	} while (error == CSS_OK);

	return error;
}

/**
 * Inform a CSS parser that all data has been received.
 *
 * \param parser  The parser to inform
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__parser_completed(css_parser *parser)
{
	parserutils_error perror;
	parser_state *state;
	css_error error = CSS_OK;

	if (parser == NULL)
		return CSS_BADPARM;

	/* Send EOF to input stream */
	perror = parserutils_inputstream_append(parser->stream, NULL, 0);
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

	/* Flush through any remaining data */
	do {
		state = parserutils_stack_get_current(parser->states);
		if (state == NULL)
			break;

		error = parseFuncs[state->state](parser);
	} while (error == CSS_OK);

	return error;
}

/**
 * Retrieve document charset information from a CSS parser
 *
 * \param parser  The parser instance
 * \param source  Pointer to location to receive charset source
 * \return Pointer to charset name (constant; do not free)
 */
const char *css__parser_read_charset(css_parser *parser, 
		css_charset_source *source)
{
	const char *charset;
	uint32_t src;

	if (parser == NULL || source == NULL)
		return NULL;

	charset = parserutils_inputstream_read_charset(parser->stream, &src);

	*source = (css_charset_source) src;

	return charset;
}

/**
 * Quirks permitted when parsing
 *
 * \param parser  Parser to query
 * \return True if quirks permitted, false otherwise
 */
bool css__parser_quirks_permitted(css_parser *parser)
{
	return parser->quirks;
}

/******************************************************************************
 * Parser creation helper                                                     *
 ******************************************************************************/

/**
 * Create a CSS parser (internal)
 *
 * \param charset     Charset of data, if known, or NULL
 * \param cs_source   Source of charset information, or CSS_CHARSET_DEFAULT
 * \param initial     The required initial state of the parser
 * \param parser      Pointer to location to receive parser instance
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters,
 *         CSS_NOMEM on memory exhaustion
 */
css_error css__parser_create_internal(const char *charset, 
		css_charset_source cs_source, parser_state initial, 
		css_parser **parser)
{
	css_parser *p;
	parserutils_error perror;
	css_error error;

	if (parser == NULL)
		return CSS_BADPARM;

	p = malloc(sizeof(css_parser));
	if (p == NULL)
		return CSS_NOMEM;

	perror = parserutils_inputstream_create(charset, cs_source,
			css__charset_extract, &p->stream);
	if (perror != PARSERUTILS_OK) {
		free(p);
		return css_error_from_parserutils_error(perror);
	}

	error = css__lexer_create(p->stream, &p->lexer);
	if (error != CSS_OK) {
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return error;
	}

	perror = parserutils_stack_create(sizeof(parser_state), 
			STACK_CHUNK, &p->states);
	if (perror != PARSERUTILS_OK) {
		css__lexer_destroy(p->lexer);
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return css_error_from_parserutils_error(perror);
	}

	perror = parserutils_vector_create(sizeof(css_token), 
			STACK_CHUNK, &p->tokens);
	if (perror != PARSERUTILS_OK) {
		parserutils_stack_destroy(p->states);
		css__lexer_destroy(p->lexer);
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return css_error_from_parserutils_error(perror);
	}

	perror = parserutils_stack_create(sizeof(char), 
			STACK_CHUNK, &p->open_items);
	if (perror != PARSERUTILS_OK) {
		parserutils_vector_destroy(p->tokens);
		parserutils_stack_destroy(p->states);
		css__lexer_destroy(p->lexer);
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return css_error_from_parserutils_error(perror);
	}

	perror = parserutils_stack_push(p->states, (void *) &initial);
	if (perror != PARSERUTILS_OK) {
		parserutils_stack_destroy(p->open_items);
		parserutils_vector_destroy(p->tokens);
		parserutils_stack_destroy(p->states);
		css__lexer_destroy(p->lexer);
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return css_error_from_parserutils_error(perror);
	}

	p->quirks = false;
	p->pushback = NULL;
	p->parseError = false;
	p->match_char = 0;
	p->event = NULL;
	p->last_was_ws = false;
	p->event_pw = NULL;

	*parser = p;

	return CSS_OK;
}

/******************************************************************************
 * Helper functions                                                           *
 ******************************************************************************/

/**
 * Transition to a new state, ensuring return to the one specified
 *
 * \param parser      The parser instance
 * \param to          Destination state
 * \param subsequent  The state to return to
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error transition(css_parser *parser, parser_state to, 
		parser_state subsequent)
{
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state current = *state;
	parserutils_error perror;

	/* Replace current state on the stack with the subsequent one */
	*state = subsequent;

	/* Push next state on the stack */
	perror = parserutils_stack_push(parser->states, (void *) &to);
	if (perror != PARSERUTILS_OK) {
		*state = current;
		return css_error_from_parserutils_error(perror);
	}

#if !defined(NDEBUG) && defined(DEBUG_STACK)
	parserutils_stack_dump(parser->states, __func__, printer);
#endif

	/* Clear the error flag */
	parser->parseError = false;

	return CSS_OK;
}

/**
 * Transition to a new state, returning to previous state on stack
 *
 * \param parser  The parser instance
 * \param to      Destination state
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error transitionNoRet(css_parser *parser, parser_state to)
{
	parser_state *state = parserutils_stack_get_current(parser->states);

	/* Replace current state on the stack with destination */
	*state = to;

#if !defined(NDEBUG) && defined(DEBUG_STACK)
	parserutils_stack_dump(parser->states, __func__, printer);
#endif

	/* Clear the error flag */
	parser->parseError = false;

	return CSS_OK;
}

/**
 * Return to previous state on the stack
 *
 * \param parser  The parser instance
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error done(css_parser *parser)
{
	parserutils_error perror;

	/* Pop current state from stack */
	perror = parserutils_stack_pop(parser->states, NULL);
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

#if !defined(NDEBUG) && defined(DEBUG_STACK)
	parserutils_stack_dump(parser->states, __func__, printer);
#endif

	return CSS_OK;
}

/**
 * Assert that the expected token is next in the input
 * 
 * \param parser  The parser instance
 * \param type    The expected token type
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error expect(css_parser *parser, css_token_type type)
{
	const css_token *token;
	css_error error;

	error = getToken(parser, &token);
	if (error != CSS_OK)
		return error;

	if (token->type != type) {
		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;
		return CSS_INVALID;
	}

	return CSS_OK;
}

/**
 * Retrieve the next token in the input
 *
 * \param parser  The parser instance
 * \param token   Pointer to location to receive token
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error getToken(css_parser *parser, const css_token **token)
{
	parserutils_error perror;
        lwc_error lerror;
	css_error error;

	/* Use pushback, if it exists */
	if (parser->pushback != NULL) {
		*token = parser->pushback;
		parser->pushback = NULL;
	} else {
		/* Otherwise, ask the lexer */
		css_token *t;

		error = css__lexer_get_token(parser->lexer, &t);
		if (error != CSS_OK)
			return error;

		/* If the last token read was whitespace, keep reading
		 * tokens until we encounter one that isn't whitespace */
		while (parser->last_was_ws && t->type == CSS_TOKEN_S) {
			error = css__lexer_get_token(parser->lexer, &t);
			if (error != CSS_OK)
				return error;
		}

		/* We need only intern for the following token types:
		 *
		 * CSS_TOKEN_IDENT, CSS_TOKEN_ATKEYWORD, CSS_TOKEN_STRING,
		 * CSS_TOKEN_INVALID_STRING, CSS_TOKEN_HASH, CSS_TOKEN_URI,
		 * CSS_TOKEN_UNICODE_RANGE?, CSS_TOKEN_FUNCTION, CSS_TOKEN_CHAR,
		 * CSS_TOKEN_NUMBER, CSS_TOKEN_PERCENTAGE, CSS_TOKEN_DIMENSION
		 *
		 * These token types all appear before CSS_TOKEN_LAST_INTERN.
		 * All other token types appear after this magic value.
		 */

		if (t->type < CSS_TOKEN_LAST_INTERN && t->data.data != NULL) {
			/* Insert token text into the dictionary */
                        lerror = lwc_intern_string((char *)t->data.data,
                                                    t->data.len, &t->idata);
                        if (lerror != lwc_error_ok)
                                return css_error_from_lwc_error(lerror);
		} else {
			t->idata = NULL;
		}

		*token = t;
	}

	/* Append token to vector */
	perror = parserutils_vector_append(parser->tokens, 
			(css_token *) (*token));
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

	parser->last_was_ws = ((*token)->type == CSS_TOKEN_S);

	return CSS_OK;
}

/**
 * Push a token back on the input
 *
 * \param parser  The parser instance
 * \param token   The token to push back
 * \return CSS_OK on success.
 */
css_error pushBack(css_parser *parser, const css_token *token)
{
	parserutils_error perror;

	/* The pushback buffer depth is 1 token. Assert this. */
	assert(parser->pushback == NULL);
        
	perror = parserutils_vector_remove_last(parser->tokens);
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

	parser->pushback = token;

	return CSS_OK;
}

/**
 * Eat whitespace tokens
 *
 * \param parser  The parser instance
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error eatWS(css_parser *parser)
{
	const css_token *token;
	css_error error;

	error = getToken(parser, &token);
	if (error != CSS_OK)
		return error;

	if (token->type != CSS_TOKEN_S) {
		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;
	}

	return CSS_OK;
}

/******************************************************************************
 * Parser stages                                                              *
 ******************************************************************************/

css_error parseStart(css_parser *parser)
{
	enum { Initial = 0, AfterWS = 1, AfterStylesheet = 2 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	css_error error = CSS_OK;

	/* start -> ws stylesheet EOF */

	switch (state->substate) {
	case Initial:
#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
		printf("Begin stylesheet\n");
#endif
		if (parser->event != NULL) {
			parser->event(CSS_PARSER_START_STYLESHEET, NULL, 
					parser->event_pw);
		}

		error = eatWS(parser);
		if (error != CSS_OK)
			return error;
		state->substate = AfterWS;
		/* Fall through */
	case AfterWS:
	{
		parser_state to = { sStylesheet, Initial };
		parser_state subsequent = { sStart, AfterStylesheet };

		return transition(parser, to, subsequent);
	}
	case AfterStylesheet:
		error = expect(parser, CSS_TOKEN_EOF);
		if (error != CSS_OK)
			return error;

		/* Flag completion, just in case */
	}

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
	parserutils_vector_dump(parser->tokens, __func__, tprinter);
	printf("End stylesheet\n");
#endif
	if (parser->event != NULL) {
		parser->event(CSS_PARSER_END_STYLESHEET, NULL, 
				parser->event_pw);
	}
        
        unref_interned_strings_in_tokens(parser);
	parserutils_vector_clear(parser->tokens);

	return done(parser);
}

css_error parseStylesheet(css_parser *parser)
{
	enum { Initial = 0, WS = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* stylesheet -> CDO ws stylesheet
	 *            -> CDC ws stylesheet
	 *            -> statement ws stylesheet
	 *            ->
	 */

	while (1) {
		switch (state->substate) {
		case Initial:
			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			switch (token->type) {
			case CSS_TOKEN_EOF:
				error = pushBack(parser, token);
				if (error != CSS_OK)
					return error;

                                unref_interned_strings_in_tokens(parser);
				parserutils_vector_clear(parser->tokens);

				return done(parser);
			case CSS_TOKEN_CDO:
			case CSS_TOKEN_CDC:
				break;
			default:
			{
				parser_state to = { sStatement, Initial };
				parser_state subsequent = { sStylesheet, WS };

				error = pushBack(parser, token);
				if (error != CSS_OK)
					return error;

				return transition(parser, to, subsequent);
			}
			}

			state->substate = WS;
			/* Fall through */
		case WS:
			error = eatWS(parser);
			if (error != CSS_OK)
				return error;

			state->substate = Initial;
		}
	}
}

css_error parseStatement(css_parser *parser)
{
	enum { Initial = 0 };
	const css_token *token;
	parser_state to = { sRuleset, Initial };
	css_error error;

	/* statement -> ruleset
	 *              at-rule
	 */

	error = getToken(parser, &token);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_ATKEYWORD)
		to.state = sAtRule;

	error = pushBack(parser, token);
	if (error != CSS_OK)
		return error;

	return transitionNoRet(parser, to);
}

css_error parseRuleset(css_parser *parser)
{
	enum { Initial = 0, Brace = 1, WS = 2 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state to = { sRulesetEnd, Initial };
	const css_token *token;
	css_error error;

	/* ruleset -> selector '{' ws ruleset-end 
	 *         -> '{' ws ruleset-end
	 */

	switch (state->substate) {
	case Initial:
                unref_interned_strings_in_tokens(parser);
		parserutils_vector_clear(parser->tokens);

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		/* The grammar's ambiguous here -- selectors may start with a 
		 * brace. We're going to assume that that won't happen, 
		 * however. */
		if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1 && 
				lwc_string_data(token->idata)[0] == '{') {
#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
			printf("Begin ruleset\n");
#endif
			if (parser->event != NULL) {
				if (parser->event(CSS_PARSER_START_RULESET, 
						NULL, parser->event_pw) == 
						CSS_INVALID) {
					parser_state to = 
						{ sMalformedSelector, Initial };

					return transitionNoRet(parser, to);
				}
			}

			state->substate = WS;
			goto ws;
		} else {
			parser_state to = { sSelector, Initial };
			parser_state subsequent = { sRuleset, Brace };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transition(parser, to, subsequent);
		}
		break;
	case Brace:
#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
		printf("Begin ruleset\n");
		parserutils_vector_dump(parser->tokens, __func__, tprinter);
#endif
		if (parser->parseError == false && parser->event != NULL) {
			if (parser->event(CSS_PARSER_START_RULESET, 
					parser->tokens, parser->event_pw) ==
					CSS_INVALID)
				parser->parseError = true;
		}

		if (parser->parseError == true) {
			parser_state to = { sMalformedSelector, Initial };

			return transitionNoRet(parser, to);
		}

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != '{') {
			/* This should never happen, as FOLLOW(selector) 
			 * contains only '{' */
			assert(0 && "Expected {");
		}
		
		state->substate = WS;
		/* Fall through */
	case WS:
	ws:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

	return transitionNoRet(parser, to);
}

css_error parseRulesetEnd(css_parser *parser)
{
	enum { Initial = 0, DeclList = 1, Brace = 2, WS = 3 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* ruleset-end -> declaration decl-list '}' ws 
	 *             -> decl-list '}' ws
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF)
			return done(parser);

		/* If this can't possibly be the start of a decl-list, then
		 * attempt to parse a declaration. This will catch any invalid
		 * input at this point and read to the start of the next
		 * declaration. FIRST(decl-list) = (';', '}') */
		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				(lwc_string_data(token->idata)[0] != '}' &&
				lwc_string_data(token->idata)[0] != ';')) {
			parser_state to = { sDeclaration, Initial };
			parser_state subsequent = { sRulesetEnd, DeclList };

			return transition(parser, to, subsequent);
		}

		state->substate = DeclList;
		/* Fall through */
	case DeclList:
	{
		parser_state to = { sDeclList, Initial };
		parser_state subsequent = { sRulesetEnd, Brace };

		return transition(parser, to, subsequent);
	}
	case Brace:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != '}') {
			/* This should never happen, as FOLLOW(decl-list)
			 * contains only '}' */
			assert(0 && "Expected }");
		}

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
	printf("End ruleset\n");
#endif
	if (parser->event != NULL) {
		parser->event(CSS_PARSER_END_RULESET, NULL, parser->event_pw);
	}

	return done(parser);
}

css_error parseAtRule(css_parser *parser)
{
	enum { Initial = 0, WS = 1, Any = 2, AfterAny = 3 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state to = { sAtRuleEnd, Initial };
	const css_token *token;
	css_error error;

	/* at-rule -> ATKEYWORD ws any0 at-rule-end */

	switch (state->substate) {
	case Initial:
                unref_interned_strings_in_tokens(parser);
		parserutils_vector_clear(parser->tokens);

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		assert(token->type == CSS_TOKEN_ATKEYWORD);

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		state->substate = Any;
		/* Fall through */
	case Any:
	{
		parser_state to = { sAny0, Initial };
		parser_state subsequent = { sAtRule, AfterAny };

		return transition(parser, to, subsequent);
	}
	case AfterAny:
		if (parser->parseError) {
			parser_state to = { sMalformedAtRule, Initial };

			return transitionNoRet(parser, to);
		}

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		/* Grammar ambiguity: any0 can be followed by '{',';',')',']'. 
		 * at-rule can only be followed by '{' and ';'. */
		if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1) {
			if (lwc_string_data(token->idata)[0] == ')' ||
					lwc_string_data(
						token->idata)[0] == ']') {
				parser_state to = { sAny0, Initial };
				parser_state subsequent = { sAtRule, AfterAny };

				return transition(parser, to, subsequent);
			}
		}

		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;

		break;
	}

	return transitionNoRet(parser, to);
}

css_error parseAtRuleEnd(css_parser *parser)
{
	enum { Initial = 0, WS = 1, AfterBlock = 2 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* at-rule-end -> block
	 *             -> ';' ws
	 */

	switch (state->substate) {
	case Initial:
#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
		printf("Begin at-rule\n");
		parserutils_vector_dump(parser->tokens, __func__, tprinter);
#endif
		if (parser->event != NULL) {
			if (parser->event(CSS_PARSER_START_ATRULE, 
					parser->tokens, parser->event_pw) ==
					CSS_INVALID) {
				parser_state to = { sMalformedAtRule, Initial };

				return transitionNoRet(parser, to);
			}
		}

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1) {
			/* Should never happen FOLLOW(at-rule) == '{', ';'*/
			assert(0 && "Expected { or ;");
		}
		
		if (lwc_string_data(token->idata)[0] == '{') {
			parser_state to = { sBlock, Initial };
			parser_state subsequent = { sAtRuleEnd, AfterBlock };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transition(parser, to, subsequent);
		} else if (lwc_string_data(token->idata)[0] != ';') {
			/* Again, should never happen */
			assert(0 && "Expected ;");
		}

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	case AfterBlock:
		break;
	}

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
	printf("End at-rule\n");
#endif
	if (parser->event != NULL) {
		parser->event(CSS_PARSER_END_ATRULE, NULL, parser->event_pw);
	}

	return done(parser);
}

css_error parseBlock(css_parser *parser)
{
	enum { Initial = 0, WS = 1, Content = 2, Brace = 3, WS2 = 4 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* block -> '{' ws block-content '}' ws */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
		printf("Begin block\n");
#endif
		if (parser->event != NULL) {
			parser->event(CSS_PARSER_START_BLOCK, NULL,
					parser->event_pw);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != '{') {
			/* This should never happen, as FIRST(block) == '{' */
			assert(0 && "Expected {");
		}

                unref_interned_strings_in_tokens(parser);
		parserutils_vector_clear(parser->tokens);

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		state->substate = Content;
		/* Fall through */
	case Content:
	{
		parser_state to = { sBlockContent, Initial };
		parser_state subsequent = { sBlock, Brace };

		return transition(parser, to, subsequent);
	}
	case Brace:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != '}') {
			/* This should never happen, as 
			 * FOLLOW(block-content) == '}' */
			assert(0 && "Expected }");
		}

		state->substate = WS2;
		/* Fall through */
	case WS2:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
	printf("End block\n");
#endif
	if (parser->event != NULL) {
		parser->event(CSS_PARSER_END_BLOCK, NULL, parser->event_pw);
	}

        unref_interned_strings_in_tokens(parser);
	parserutils_vector_clear(parser->tokens);

	return done(parser);
}

css_error parseBlockContent(css_parser *parser)
{
	enum { Initial = 0, WS = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* block-content -> any block-content 
	 *               -> block block-content
	 *               -> ATKEYWORD ws block-content
	 *               -> ';' ws block-content
	 *               ->
	 */

	while (1) {
		switch (state->substate) {
		case Initial:
			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_ATKEYWORD) {
				state->substate = WS;
			} else if (token->type == CSS_TOKEN_CHAR) {
				if (lwc_string_length(token->idata) == 1 && 
						lwc_string_data(
						token->idata)[0] == '{') {
					/* Grammar ambiguity. Assume block */
					parser_state to = { sBlock, Initial };
					parser_state subsequent = 
						{ sBlockContent, Initial };

					error = pushBack(parser, token);
					if (error != CSS_OK)
						return error;

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
					parserutils_vector_dump(parser->tokens,
							__func__, tprinter);
#endif
					if (parser->event != NULL) {
						parser->event(
							CSS_PARSER_BLOCK_CONTENT,
							parser->tokens, 
							parser->event_pw);
					}

					unref_interned_strings_in_tokens(
							parser);
					parserutils_vector_clear(
							parser->tokens);

					return transition(parser, to, 
							subsequent);
				} else if (lwc_string_length(
						token->idata) == 1 &&
						lwc_string_data(
						token->idata)[0] == ';') {
					/* Grammar ambiguity. Assume semi */
					error = pushBack(parser, token);
					if (error != CSS_OK)
						return error;

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
					parserutils_vector_dump(parser->tokens,
							__func__, tprinter);
#endif
					if (parser->event != NULL) {
						parser->event(
							CSS_PARSER_BLOCK_CONTENT,
							parser->tokens, 
							parser->event_pw);
					}

					error = getToken(parser, &token);
					if (error != CSS_OK)
						return error;

					unref_interned_strings_in_tokens(
							parser);
					parserutils_vector_clear(
							parser->tokens);

					state->substate = WS;
				} else if (lwc_string_length(
						token->idata) == 1 &&
						lwc_string_data(
						token->idata)[0] == '}') {
					/* Grammar ambiguity. Assume end */
					error = pushBack(parser, token);
					if (error != CSS_OK)
						return error;

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
					parserutils_vector_dump(parser->tokens,
							__func__, tprinter);
#endif
					if (parser->event != NULL) {
						parser->event(
							CSS_PARSER_BLOCK_CONTENT,
							parser->tokens,
							parser->event_pw);
					}

					unref_interned_strings_in_tokens(
							parser);
					parserutils_vector_clear(
							parser->tokens);

					return done(parser);
				}
			} else if (token->type == CSS_TOKEN_EOF) {
				error = pushBack(parser, token);
				if (error != CSS_OK)
					return error;

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
				parserutils_vector_dump(parser->tokens,
						__func__, tprinter);
#endif
				if (parser->event != NULL) {
					parser->event(CSS_PARSER_BLOCK_CONTENT,
							parser->tokens,
							parser->event_pw);
				}

				unref_interned_strings_in_tokens(parser);
				parserutils_vector_clear(parser->tokens);

				return done(parser);
			}

			if (state->substate == Initial) {
				parser_state to = { sAny, Initial };
				parser_state subsequent = 
						{ sBlockContent, Initial };

				error = pushBack(parser, token);
				if (error != CSS_OK)
					return error;

				return transition(parser, to, subsequent);
			}

			break;
		case WS:
			error = eatWS(parser);
			if (error != CSS_OK)
				return error;

			state->substate = Initial;
		}
	}

	 return done(parser);
}

css_error parseSelector(css_parser *parser)
{
	enum { Initial = 0, AfterAny1 = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);

	/* selector -> any1 */

	switch (state->substate) {
	case Initial:
	{
		parser_state to = { sAny1, Initial };
		parser_state subsequent = { sSelector, AfterAny1 };

                unref_interned_strings_in_tokens(parser);
		parserutils_vector_clear(parser->tokens);

		return transition(parser, to, subsequent);
	}
	case AfterAny1:
		break;
	}

	return done(parser);
}

css_error parseDeclaration(css_parser *parser)
{
	enum { Initial = 0, Colon = 1, WS = 2, AfterValue1 = 3 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* declaration -> property ':' ws value1 */

	switch (state->substate) {
	case Initial:
	{
		parser_state to = { sProperty, Initial };
		parser_state subsequent = { sDeclaration, Colon };

                unref_interned_strings_in_tokens(parser);
		parserutils_vector_clear(parser->tokens);

		return transition(parser, to, subsequent);
	}
	case Colon:
		if (parser->parseError) {
			parser_state to = { sMalformedDecl, Initial };

			parser->parseError = false;

			return transitionNoRet(parser, to);
		}

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != ':') {
			/* parse error -- expected : */
			parser_state to = { sMalformedDecl, Initial };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transitionNoRet(parser, to);
		}

		state->substate = WS;
		/* Fall through */
	case WS:
	{
		parser_state to = { sValue1, Initial };
		parser_state subsequent = { sDeclaration, AfterValue1 };

		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		return transition(parser, to, subsequent);
	}
	case AfterValue1:
		if (parser->parseError) {
			parser_state to = { sMalformedDecl, Initial };

			parser->parseError = false;

			return transitionNoRet(parser, to);
		}

#if !defined(NDEBUG) && defined(DEBUG_EVENTS)
		parserutils_vector_dump(parser->tokens, __func__, tprinter);
#endif
		if (parser->event != NULL) {
			parser->event(CSS_PARSER_DECLARATION, parser->tokens,
					parser->event_pw);
		}
		break;
	}

	return done(parser);
}

css_error parseDeclList(css_parser *parser)
{
	enum { Initial = 0, WS = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state to = { sDeclListEnd, Initial };
	const css_token *token;
	css_error error;

	/* decl-list -> ';' ws decl-list-end
	 *           ->
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				(lwc_string_data(token->idata)[0] != '}' && 
				lwc_string_data(token->idata)[0] != ';')) {
			/* Should never happen */
			assert(0 && "Expected ; or  }");
		}

		if (lwc_string_data(token->idata)[0] == '}') {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		} else {
			/* ; */
			state->substate = WS;
		}

		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

	return transitionNoRet(parser, to);
}

css_error parseDeclListEnd(css_parser *parser)
{
	enum { Initial = 0, AfterDeclaration = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state to = { sDeclList, Initial };
	const css_token *token;
	css_error error;

	/* decl-list-end -> declaration decl-list 
	 *               -> decl-list
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 || 
				(lwc_string_data(token->idata)[0] != ';' &&
				lwc_string_data(token->idata)[0] != '}')) {
			parser_state to = { sDeclaration, Initial };
			parser_state subsequent = 
					{ sDeclListEnd, AfterDeclaration };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transition(parser, to, subsequent);
		} else {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;
		}

		state->substate = AfterDeclaration;
		/* Fall through */
	case AfterDeclaration:
		break;
	}

	return transitionNoRet(parser, to);
}

css_error parseProperty(css_parser *parser)
{
	enum { Initial = 0, WS = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* property -> IDENT ws */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_IDENT) {
			/* parse error */
			parser->parseError = true;

			return done(parser);
		}

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

	return done(parser);
}

css_error parseValue1(css_parser *parser)
{
	enum { Initial = 0, AfterValue = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	parser_state to = { sValue0, Initial };
	const css_token *token;
	css_error error;

	/* value1 -> value value0 */

	switch (state->substate) {
	case Initial:
	{
		parser_state to = { sValue, Initial };
		parser_state subsequent = { sValue1, AfterValue };

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;

		/* Grammar ambiguity -- assume ';' or '}' mark end */
		if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1 &&
				(lwc_string_data(token->idata)[0] == ';' ||
				lwc_string_data(token->idata)[0] == '}')) {
			/* Parse error */
			parser->parseError = true;

			return done(parser);
		}

		return transition(parser, to, subsequent);
	}
	case AfterValue:
		if (parser->parseError)
			return done(parser);
		break;
	}

	return transitionNoRet(parser, to);
}

css_error parseValue0(css_parser *parser)
{
	enum { Initial = 0, AfterValue = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* value0 -> value value0
	 *        -> 
	 */

	while (1) {
		switch (state->substate) {
		case Initial:
		{
			parser_state to = { sValue, Initial };
			parser_state subsequent = { sValue0, AfterValue };

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_EOF)
				return done(parser);

			/* Grammar ambiguity -- assume ';' or '}' mark end */
			if (token->type == CSS_TOKEN_CHAR && 
					lwc_string_length(
						token->idata) == 1 &&
					(lwc_string_data(
						token->idata)[0] == ';' ||
					lwc_string_data(
						token->idata)[0] == '}')) {
				return done(parser);
			}

			return transition(parser, to, subsequent);
		}
		case AfterValue:
			if (parser->parseError)
				return done(parser);

			state->substate = Initial;

			break;
		}
	}

	return done(parser);
}

css_error parseValue(css_parser *parser)
{
	enum { Initial = 0, WS = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* value  -> any
	 *        -> block
	 *        -> ATKEYWORD ws
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_ATKEYWORD) {
			state->substate = WS;
		} else if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1 && 
				lwc_string_data(token->idata)[0] == '{') {
			/* Grammar ambiguity. Assume block. */
			parser_state to = { sBlock, Initial };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transitionNoRet(parser, to);
		} else {
			parser_state to = { sAny, Initial };

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return transitionNoRet(parser, to);
		}

		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

	return done(parser);
}

css_error parseAny0(css_parser *parser)
{
	enum { Initial = 0, AfterAny = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* any0 -> any any0
	 *      -> 
	 */

	while (1) {
		switch (state->substate) {
		case Initial:
		{
			parser_state to = { sAny, Initial };
			parser_state subsequent = { sAny0, AfterAny };

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_EOF)
				return done(parser);

			/* Grammar ambiguity: 
			 * assume '{', ';', ')', ']' mark end */
			if (token->type == CSS_TOKEN_CHAR && 
					lwc_string_length(
						token->idata) == 1 &&
					(lwc_string_data(
						token->idata)[0] == '{' ||
					lwc_string_data(
						token->idata)[0] == ';' ||
					lwc_string_data(
						token->idata)[0] == ')' ||
					lwc_string_data(
						token->idata)[0] == ']')) {
				return done(parser);
			}

			return transition(parser, to, subsequent);
		}
		case AfterAny:
			if (parser->parseError)
				return done(parser);

			state->substate = Initial;

			break;
		}
	}

	return done(parser);
}

css_error parseAny1(css_parser *parser)
{
	enum { Initial = 0, AfterAny = 1, AfterAny0 = 2 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* any1 -> any any0 */

	switch (state->substate) {
	case Initial:
	{
		parser_state to = { sAny, Initial };
		parser_state subsequent = { sAny1, AfterAny };

		return transition(parser, to, subsequent);
	}
	case AfterAny:
	{
		parser_state to = { sAny0, Initial };
		parser_state subsequent = { sAny1, AfterAny0 };

		if (parser->parseError)
			return done(parser);

		return transition(parser, to, subsequent);
	}
	case AfterAny0:
		if (parser->parseError)
			return done(parser);

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;

		/* Grammar ambiguity: any0 can be followed by 
		 * '{', ';', ')', ']'. any1 can only be followed by '{'. */
		if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1) {
			if (lwc_string_data(token->idata)[0] == ';' ||
					lwc_string_data(
						token->idata)[0] == ')' ||
					lwc_string_data(
						token->idata)[0] == ']') {
				parser_state to = { sAny, Initial };
				parser_state subsequent = { sAny1, AfterAny };

				return transition(parser, to, subsequent);
			} else if (lwc_string_data(token->idata)[0] != '{') {
				/* parse error */
				parser->parseError = true;
			}
		} else {
			/* parse error */
			parser->parseError = true;
		}
	}

	return done(parser);
}

css_error parseAny(css_parser *parser)
{
	enum { Initial = 0, WS = 1, AfterAny0 = 2, WS2 = 3 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* any -> IDENT ws
	 *     -> NUMBER ws
	 *     -> PERCENTAGE ws
	 *     -> DIMENSION ws
	 *     -> STRING ws
	 *     -> CHAR ws
	 *     -> URI ws
	 *     -> HASH ws
	 *     -> UNICODE-RANGE ws
	 *     -> INCLUDES ws
	 *     -> DASHMATCH ws
	 *     -> PREFIXMATCH ws
	 *     -> SUFFIXMATCH ws
	 *     -> SUBSTRINGMATCH ws
	 *     -> FUNCTION ws any0 ')' ws
	 *     -> '(' ws any0 ')' ws
	 *     -> '[' ws any0 ']' ws
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type != CSS_TOKEN_IDENT && 
				token->type != CSS_TOKEN_NUMBER && 
				token->type != CSS_TOKEN_PERCENTAGE && 
				token->type != CSS_TOKEN_DIMENSION && 
				token->type != CSS_TOKEN_STRING && 
				token->type != CSS_TOKEN_CHAR && 
				token->type != CSS_TOKEN_URI && 
				token->type != CSS_TOKEN_HASH && 
				token->type != CSS_TOKEN_UNICODE_RANGE && 
				token->type != CSS_TOKEN_INCLUDES && 
				token->type != CSS_TOKEN_DASHMATCH && 
				token->type != CSS_TOKEN_PREFIXMATCH && 
				token->type != CSS_TOKEN_SUFFIXMATCH && 
				token->type != CSS_TOKEN_SUBSTRINGMATCH && 
				token->type != CSS_TOKEN_FUNCTION) {
			/* parse error */
			parser->parseError = true;

			return done(parser);
		}

		if (token->type == CSS_TOKEN_FUNCTION) {
			parser->match_char = ')';
			state->substate = WS;
		} else if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1 && 
				(lwc_string_data(token->idata)[0] == '(' || 
				lwc_string_data(token->idata)[0] == '[')) {
			parser->match_char = lwc_string_data(
					token->idata)[0] == '(' ? ')' : ']';
			state->substate = WS;
		} 

		state->substate = WS2;
		/* Fall through */
	case WS:
	case WS2:
	ws2:
	{
		parser_state to = { sAny0, Initial };
		parser_state subsequent = { sAny, AfterAny0 };

		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		if (state->substate == WS2)
			break;

		return transition(parser, to, subsequent);
	}
	case AfterAny0:
	{
		parser_state to = { sAny0, Initial };
		parser_state subsequent = { sAny, AfterAny0 };

		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		/* Match correct close bracket (grammar ambiguity) */
		if (token->type == CSS_TOKEN_CHAR && 
				lwc_string_length(token->idata) == 1 &&
				lwc_string_data(token->idata)[0] == 
				parser->match_char) {
			state->substate = WS2;
			goto ws2;
		}

		return transition(parser, to, subsequent);
	}
	}

	return done(parser);
}

css_error parseMalformedDeclaration(css_parser *parser)
{
	enum { Initial = 0, Go = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token = NULL;
	css_error error;

	/* Malformed declaration: read everything up to the next ; or } 
	 * We must ensure that pairs of {}, (), [], are balanced */

	switch (state->substate) {
	case Initial:
	{
		/* Clear stack of open items */
		while (parserutils_stack_pop(parser->open_items, NULL) == 
				PARSERUTILS_OK)
			;

		state->substate = Go;
		/* Fall through */
	}
	case Go:
		while (1) {
			char want;
			char *match;
			const char *data;
			size_t len;

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_EOF)
				break;

			if (token->type != CSS_TOKEN_CHAR)
				continue;

			len = lwc_string_length(token->idata);
			data = lwc_string_data(token->idata);

			if (len != 1 || (data[0] != '{' && data[0] != '}' &&
					data[0] != '[' && data[0] != ']' &&
					data[0] != '(' && data[0] != ')' &&
					data[0] != ';'))
				continue;

			match = parserutils_stack_get_current(
					parser->open_items);

			/* If the stack is empty, then we're done if we've got
			 * either a ';' or '}' */
			if (match == NULL) {
				if (data[0] == ';' || data[0] == '}')
					break;
			}

			/* Regardless, if we've got a semicolon, ignore it */
			if (data[0] == ';')
				continue;

			/* Get corresponding start tokens for end tokens */
			switch (data[0]) {
			case '}':
				want = '{';
				break;
			case ']':
				want = '[';
				break;
			case ')':
				want = '(';
				break;
			default:
				want = 0;
				break;
			}

			/* Either pop off the stack, if we've matched the 
			 * current item, or push the start token on */
			if (match != NULL && *match == want) {
				parserutils_stack_pop(
					parser->open_items, NULL);
			} else if (want == 0) {
				parserutils_stack_push(parser->open_items, 
						&data[0]);
			}
		}
	}

	/* Push the last token (';', '}' or EOF) back */
	error = pushBack(parser, token);
	if (error != CSS_OK)
		return error;

	/* Discard the tokens we've read */
        unref_interned_strings_in_tokens(parser);
	parserutils_vector_clear(parser->tokens);

	return done(parser);
}

css_error parseMalformedSelector(css_parser *parser)
{
	enum { Initial = 0, Go = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* Malformed selector: discard the entirety of the next block,
	 * ensuring we correctly match pairs of {}, [], and (). */

	switch (state->substate) {
	case Initial:
		/* Clear the stack of open items */
		while (parserutils_stack_pop(parser->open_items, NULL) == 
				PARSERUTILS_OK)
			;

		state->substate = Go;
		/* Fall through */
	case Go:
		while (1) {
			char want;
			char *match;
			const char *data;
			size_t len;

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_EOF)
				break;

			if (token->type != CSS_TOKEN_CHAR)
				continue;

			len = lwc_string_length(token->idata);
			data = lwc_string_data(token->idata);

			if (len != 1 || (data[0] != '{' && data[0] != '}' &&
					data[0] != '[' && data[0] != ']' &&
					data[0] != '(' && data[0] != ')'))
				continue;

			match = parserutils_stack_get_current(
					parser->open_items);

			/* Get corresponding start tokens for end tokens */
			switch (data[0]) {
			case '}':
				want = '{';
				break;
			case ']':
				want = '[';
				break;
			case ')':
				want = '(';
				break;
			default:
				want = 0;
				break;
			}

			/* Either pop off the stack, if we've matched the 
			 * current item, or push the start token on */
			if (match != NULL && *match == want) {
				parserutils_stack_pop(
					parser->open_items, NULL);
			} else if (want == 0) {
				parserutils_stack_push(parser->open_items, 
						&data[0]);
			}

			/* If we encountered a '}', there was data on the stack
			 * before, and the stack's now empty, then we've popped
			 * a '{' off the stack. That means we've found the end 
			 * of the block, so we're done here. */
			if (want == '{' && match != NULL && 
					parserutils_stack_get_current(
					parser->open_items) == NULL)
				break;
		}
	}

	/* Consume any trailing whitespace after the ruleset */
	error = eatWS(parser);
	if (error != CSS_OK)
		return error;

	/* Discard the tokens we've read */
        unref_interned_strings_in_tokens(parser);
	parserutils_vector_clear(parser->tokens);

	return done(parser);
}

css_error parseMalformedAtRule(css_parser *parser)
{
	enum { Initial = 0, Go = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token = NULL;
	css_error error;

	/* Malformed at-rule: read everything up to the next ; or the next
	 * block, whichever is first.
	 * We must ensure that pairs of {}, (), [], are balanced */

	switch (state->substate) {
	case Initial:
	{
		/* Clear stack of open items */
		while (parserutils_stack_pop(parser->open_items, NULL) == 
				PARSERUTILS_OK)
			;

		state->substate = Go;
		/* Fall through */
	}
	case Go:
		while (1) {
			char want;
			char *match;
			const char *data;
			size_t len;

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			if (token->type == CSS_TOKEN_EOF)
				break;

			if (token->type != CSS_TOKEN_CHAR)
				continue;

			len = lwc_string_length(token->idata);
			data = lwc_string_data(token->idata);

			if (len != 1 || (data[0] != '{' && data[0] != '}' &&
					data[0] != '[' && data[0] != ']' &&
					data[0] != '(' && data[0] != ')' &&
					data[0] != ';'))
				continue;

			match = parserutils_stack_get_current(
					parser->open_items);

			/* If we have a semicolon, then we're either done or
			 * need to ignore it */
			if (data[0] == ';') {
				if (match == NULL)
					break;
				else
					continue;
			}

			/* Get corresponding start tokens for end tokens */
			switch (data[0]) {
			case '}':
				want = '{';
				break;
			case ']':
				want = '[';
				break;
			case ')':
				want = '(';
				break;
			default:
				want = 0;
				break;
			}

			/* Either pop off the stack, if we've matched the 
			 * current item, or push the start token on */
			if (match != NULL && *match == want) {
				parserutils_stack_pop(
					parser->open_items, NULL);
			} else if (want == 0) {
				parserutils_stack_push(parser->open_items, 
						&data[0]);
			}

			/* If we encountered a '}', there was data on the stack
			 * before, and the stack's now empty, then we've popped
			 * a '{' off the stack. That means we've found the end 
			 * of the block, so we're done here. */
			if (want == '{' && match != NULL && 
					parserutils_stack_get_current(
					parser->open_items) == NULL)
				break;
		}
	}

	/* Consume any trailing whitespace after the at-rule */
	error = eatWS(parser);
	if (error != CSS_OK)
		return error;

	/* Discard the tokens we've read */
        unref_interned_strings_in_tokens(parser);
	parserutils_vector_clear(parser->tokens);

	return done(parser);
}

css_error parseInlineStyle(css_parser *parser)
{
	enum { Initial = 0, WS = 1, AfterISBody0 = 2 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	css_error error;

	/* inline-style = ws is-body0 */

	switch (state->substate) {
	case Initial:
		/* Emit fake events such that the language parser knows 
		 * no different from a normal parse. */

		if (parser->event != NULL) {
			/* 1) begin stylesheet */
			parser->event(CSS_PARSER_START_STYLESHEET, NULL,
					parser->event_pw);

			/* 2) begin ruleset */
			parser->event(CSS_PARSER_START_RULESET, NULL,
					parser->event_pw);
		}

		/* Fall through */
	case WS:
	{
		parser_state to = { sISBody0, Initial };
		parser_state subsequent = { sInlineStyle, AfterISBody0 };

		state->substate = WS;

		/* Consume leading whitespace */
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		return transition(parser, to, subsequent);
	}
	case AfterISBody0:
		/* Clean up any remaining tokens */
		unref_interned_strings_in_tokens(parser);

		/* Emit remaining fake events to end the parse */
		if (parser->event != NULL) {
			/* 1) end ruleset */
			parser->event(CSS_PARSER_END_RULESET, NULL, 
					parser->event_pw);

			/* 2) end stylesheet */
			parser->event(CSS_PARSER_END_STYLESHEET, NULL,
					parser->event_pw);
		}

		break;
	}

	return done(parser);
}

css_error parseISBody0(css_parser *parser)
{
	enum { Initial = 0, AfterISBody = 1 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* is-body0     -> is-body is-body0
	 *              ->
	 */

	while (1) {
		switch (state->substate) {
		case Initial:
		{
			parser_state to = { sISBody, Initial };
			parser_state subsequent = { sISBody0, AfterISBody };

			error = getToken(parser, &token);
			if (error != CSS_OK)
				return error;

			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			/* EOF is the only way out */
			if (token->type == CSS_TOKEN_EOF)
				return done(parser);

			return transition(parser, to, subsequent);
		}
		case AfterISBody:
			/* Unless there's a parse error */
			if (parser->parseError)
				return done(parser);

			state->substate = Initial;

			break;
		}
	}

	return done(parser);
}

css_error parseISBody(css_parser *parser)
{
	enum { Initial = 0, DeclList = 1, Brace = 2, WS = 3 };
	parser_state *state = parserutils_stack_get_current(parser->states);
	const css_token *token;
	css_error error;

	/* is-body      -> declaration decl-list '}' ws 
	 *              -> decl-list '}' ws
	 *
	 * Note that this looks suspiciously similar to ruleset-end.
	 * The difference here, however, is that we never end the ruleset.
	 */

	switch (state->substate) {
	case Initial:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		error = pushBack(parser, token);
		if (error != CSS_OK)
			return error;

		/* If this can't possibly be the start of a decl-list, then
		 * attempt to parse a declaration. This will catch any invalid
		 * input at this point and read to the start of the next
		 * declaration. FIRST(decl-list) = (';', '}') */
		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				(lwc_string_data(token->idata)[0] != '}' &&
				lwc_string_data(token->idata)[0] != ';')) {
			parser_state to = { sDeclaration, Initial };
			parser_state subsequent = { sISBody, DeclList };

			return transition(parser, to, subsequent);
		}

		state->substate = DeclList;
		/* Fall through */
	case DeclList:
	{
		parser_state to = { sDeclList, Initial };
		parser_state subsequent = { sISBody, Brace };

		return transition(parser, to, subsequent);
	}
	case Brace:
		error = getToken(parser, &token);
		if (error != CSS_OK)
			return error;

		if (token->type == CSS_TOKEN_EOF) {
			error = pushBack(parser, token);
			if (error != CSS_OK)
				return error;

			return done(parser);
		}

		if (token->type != CSS_TOKEN_CHAR || 
				lwc_string_length(token->idata) != 1 ||
				lwc_string_data(token->idata)[0] != '}') {
			/* This should never happen, as FOLLOW(decl-list)
			 * contains only '}' */
			assert(0 && "Expected }");
		}

		state->substate = WS;
		/* Fall through */
	case WS:
		error = eatWS(parser);
		if (error != CSS_OK)
			return error;

		break;
	}

	return done(parser);
}

/**
 * Iterate the token vector and unref any interned strings in the tokens.
 *
 * \param parser The parser whose tokens we are cleaning up.
 */
void unref_interned_strings_in_tokens(css_parser *parser)
{
        int32_t ctx = 0;
        const css_token *tok;
        
        while ((tok = parserutils_vector_iterate(
			parser->tokens, &ctx)) != NULL) {
                if (tok->idata != NULL) {
                        lwc_string_unref(tok->idata);
		}
        }
}

#ifndef NDEBUG
#ifdef DEBUG_STACK
static void printer(void *item)
{
	parser_state *s = item;

	printf("[%d %d]", s->state, s->substate);
}
#endif

#ifdef DEBUG_EVENTS
static void tprinter(void *token)
{
	css_token *t = token;

	if (t->data.data)
		printf("%d: %.*s", t->type, (int) t->data.len, t->data.data);
	else
		printf("%d", t->type);
}
#endif
#endif

