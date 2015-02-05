
/* ntp_scanner.c
 *
 * The source code for a simple lexical analyzer. 
 *
 * Written By:	Sachin Kamboj
 *		University of Delaware
 *		Newark, DE 19711
 * Copyright (c) 2006
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "ntpd.h"
#include "ntp_config.h"
#include "ntpsim.h"
#include "ntp_scanner.h"
#include "ntp_parser.h"

/* ntp_keyword.h declares finite state machine and token text */
#include "ntp_keyword.h"



/* SCANNER GLOBAL VARIABLES 
 * ------------------------
 */

#define MAX_LEXEME (1024 + 1)	/* The maximum size of a lexeme */
char yytext[MAX_LEXEME];	/* Buffer for storing the input text/lexeme */
u_int32 conf_file_sum;		/* Simple sum of characters read */




/* CONSTANTS 
 * ---------
 */


/* SCANNER GLOBAL VARIABLES 
 * ------------------------
 */
const char special_chars[] = "{}(),;|=";


/* FUNCTIONS
 * ---------
 */

static int is_keyword(char *lexeme, follby *pfollowedby);


/*
 * keyword() - Return the keyword associated with token T_ identifier.
 *	       See also token_name() for the string-ized T_ identifier.
 *	       Example: keyword(T_Server) returns "server"
 *			token_name(T_Server) returns "T_Server"
 */
const char *
keyword(
	int token
	)
{
	size_t i;
	const char *text;

	i = token - LOWEST_KEYWORD_ID;

	if (i < COUNTOF(keyword_text))
		text = keyword_text[i];
	else
		text = NULL;

	return (text != NULL)
		   ? text
		   : "(keyword not found)";
}


/* FILE INTERFACE
 * --------------
 * We define a couple of wrapper functions around the standard C fgetc
 * and ungetc functions in order to include positional bookkeeping
 */

struct FILE_INFO *
F_OPEN(
	const char *path,
	const char *mode
	)
{
	struct FILE_INFO *my_info;

	my_info = emalloc(sizeof *my_info);

	my_info->line_no = 1;
	my_info->col_no = 0;
	my_info->prev_line_col_no = 0;
	my_info->prev_token_col_no = 0;
	my_info->fname = path;

	my_info->fd = fopen(path, mode);
	if (NULL == my_info->fd) {
		free(my_info);
		return NULL;
	}
	return my_info;
}

int
FGETC(
	struct FILE_INFO *stream
	)
{
	int ch;
	
	do 
		ch = fgetc(stream->fd);
	while (EOF != ch && (CHAR_MIN > ch || ch > CHAR_MAX));

	if (EOF != ch) {
		if (input_from_file)
			conf_file_sum += (u_char)ch;
		++stream->col_no;
		if (ch == '\n') {
			stream->prev_line_col_no = stream->col_no;
			++stream->line_no;
			stream->col_no = 1;
		}
	}

	return ch;
}

/* BUGS: 1. Function will fail on more than one line of pushback
 *       2. No error checking is done to see if ungetc fails
 * SK: I don't think its worth fixing these bugs for our purposes ;-)
 */
int
UNGETC(
	int ch,
	struct FILE_INFO *stream
	)
{
	if (input_from_file)
		conf_file_sum -= (u_char)ch;
	if (ch == '\n') {
		stream->col_no = stream->prev_line_col_no;
		stream->prev_line_col_no = -1;
		--stream->line_no;
	}
	--stream->col_no;
	return ungetc(ch, stream->fd);
}

int
FCLOSE(
	struct FILE_INFO *stream
	)
{
	int ret_val = fclose(stream->fd);

	if (!ret_val)
		free(stream);
	return ret_val;
}

/* STREAM INTERFACE 
 * ----------------
 * Provide a wrapper for the stream functions so that the
 * stream can either read from a file or from a character
 * array. 
 * NOTE: This is not very efficient for reading from character
 * arrays, but needed to allow remote configuration where the
 * configuration command is provided through ntpq.
 * 
 * The behavior of there two functions is determined by the 
 * input_from_file flag.
 */

static int
get_next_char(
	struct FILE_INFO *ip_file
	)
{
	char ch;

	if (input_from_file)
		return FGETC(ip_file);
	else {
		if (remote_config.buffer[remote_config.pos] == '\0') 
			return EOF;
		else {
			ip_file->col_no++;
			ch = remote_config.buffer[remote_config.pos++];
			if (ch == '\n') {
				ip_file->prev_line_col_no = ip_file->col_no;
				++ip_file->line_no;
				ip_file->col_no = 1;
			}
			return ch;
		}
	}
}

static void
push_back_char(
	struct FILE_INFO *ip_file,
	int ch
	)
{
	if (input_from_file)
		UNGETC(ch, ip_file);
	else {
		if (ch == '\n') {
			ip_file->col_no = ip_file->prev_line_col_no;
			ip_file->prev_line_col_no = -1;
			--ip_file->line_no;
		}
		--ip_file->col_no;

		remote_config.pos--;
	}
}

 

/* STATE MACHINES 
 * --------------
 */

/* Keywords */
static int
is_keyword(
	char *lexeme,
	follby *pfollowedby
	)
{
	follby fb;
	int curr_s;		/* current state index */
	int token;
	int i;

	curr_s = SCANNER_INIT_S;
	token = 0;

	for (i = 0; lexeme[i]; i++) {
		while (curr_s && (lexeme[i] != SS_CH(sst[curr_s])))
			curr_s = SS_OTHER_N(sst[curr_s]);

		if (curr_s && (lexeme[i] == SS_CH(sst[curr_s]))) {
			if ('\0' == lexeme[i + 1]
			    && FOLLBY_NON_ACCEPTING 
			       != SS_FB(sst[curr_s])) {
				fb = SS_FB(sst[curr_s]);
				*pfollowedby = fb;
				token = curr_s;
				break;
			}
			curr_s = SS_MATCH_N(sst[curr_s]);
		} else
			break;
	}

	return token;
}


/* Integer */
static int
is_integer(
	char *lexeme
	)
{
	int	i;
	int	is_neg;
	u_int	u_val;
	
	i = 0;

	/* Allow a leading minus sign */
	if (lexeme[i] == '-') {
		i++;
		is_neg = TRUE;
	} else {
		is_neg = FALSE;
	}

	/* Check that all the remaining characters are digits */
	for (; lexeme[i] != '\0'; i++) {
		if (!isdigit((unsigned char)lexeme[i]))
			return FALSE;
	}

	if (is_neg)
		return TRUE;

	/* Reject numbers that fit in unsigned but not in signed int */
	if (1 == sscanf(lexeme, "%u", &u_val))
		return (u_val <= INT_MAX);
	else
		return FALSE;
}


/* U_int -- assumes is_integer() has returned FALSE */
static int
is_u_int(
	char *lexeme
	)
{
	int	i;
	int	is_hex;
	
	i = 0;
	if ('0' == lexeme[i] && 'x' == tolower((unsigned char)lexeme[i + 1])) {
		i += 2;
		is_hex = TRUE;
	} else {
		is_hex = FALSE;
	}

	/* Check that all the remaining characters are digits */
	for (; lexeme[i] != '\0'; i++) {
		if (is_hex && !isxdigit((unsigned char)lexeme[i]))
			return FALSE;
		if (!is_hex && !isdigit((unsigned char)lexeme[i]))
			return FALSE;
	}

	return TRUE;
}


/* Double */
static int
is_double(
	char *lexeme
	)
{
	u_int num_digits = 0;  /* Number of digits read */
	u_int i;

	i = 0;

	/* Check for an optional '+' or '-' */
	if ('+' == lexeme[i] || '-' == lexeme[i])
		i++;

	/* Read the integer part */
	for (; lexeme[i] && isdigit((unsigned char)lexeme[i]); i++)
		num_digits++;

	/* Check for the optional decimal point */
	if ('.' == lexeme[i]) {
		i++;
		/* Check for any digits after the decimal point */
		for (; lexeme[i] && isdigit((unsigned char)lexeme[i]); i++)
			num_digits++;
	}

	/*
	 * The number of digits in both the decimal part and the
	 * fraction part must not be zero at this point 
	 */
	if (!num_digits)
		return 0;

	/* Check if we are done */
	if (!lexeme[i])
		return 1;

	/* There is still more input, read the exponent */
	if ('e' == tolower((unsigned char)lexeme[i]))
		i++;
	else
		return 0;

	/* Read an optional Sign */
	if ('+' == lexeme[i] || '-' == lexeme[i])
		i++;

	/* Now read the exponent part */
	while (lexeme[i] && isdigit((unsigned char)lexeme[i]))
		i++;

	/* Check if we are done */
	if (!lexeme[i])
		return 1;
	else
		return 0;
}


/* is_special() - Test whether a character is a token */
static inline int
is_special(
	int ch
	)
{
	return strchr(special_chars, ch) != NULL;
}


static int
is_EOC(
	int ch
	)
{
	if ((old_config_style && (ch == '\n')) ||
	    (!old_config_style && (ch == ';')))
		return 1;
	return 0;
}


char *
quote_if_needed(char *str)
{
	char *ret;
	size_t len;
	size_t octets;

	len = strlen(str);
	octets = len + 2 + 1;
	ret = emalloc(octets);
	if ('"' != str[0] 
	    && (strcspn(str, special_chars) < len 
		|| strchr(str, ' ') != NULL)) {
		snprintf(ret, octets, "\"%s\"", str);
	} else
		strlcpy(ret, str, octets);

	return ret;
}


static int
create_string_token(
	char *lexeme
	)
{
	char *pch;

	/*
	 * ignore end of line whitespace
	 */
	pch = lexeme;
	while (*pch && isspace((unsigned char)*pch))
		pch++;

	if (!*pch) {
		yylval.Integer = T_EOC;
		return yylval.Integer;
	}

	yylval.String = estrdup(lexeme);
	return T_String;
}


/*
 * yylex() - function that does the actual scanning.
 * Bison expects this function to be called yylex and for it to take no
 * input and return an int.
 * Conceptually yylex "returns" yylval as well as the actual return
 * value representing the token or type.
 */
int
yylex(
	struct FILE_INFO *ip_file
	)
{
	static follby	followedby = FOLLBY_TOKEN;
	size_t		i;
	int		instring;
	int		yylval_was_set;
	int		converted;
	int		token;		/* The return value */
	int		ch;

	if (input_from_file)
		ip_file = fp[curr_include_level];
	instring = FALSE;
	yylval_was_set = FALSE;

	do {
		/* Ignore whitespace at the beginning */
		while (EOF != (ch = get_next_char(ip_file)) &&
		       isspace(ch) &&
		       !is_EOC(ch))
			; /* Null Statement */

		if (EOF == ch) {

			if (!input_from_file || curr_include_level <= 0) 
				return 0;

			FCLOSE(fp[curr_include_level]);
			ip_file = fp[--curr_include_level];
			token = T_EOC;
			goto normal_return;

		} else if (is_EOC(ch)) {

			/* end FOLLBY_STRINGS_TO_EOC effect */
			followedby = FOLLBY_TOKEN;
			token = T_EOC;
			goto normal_return;

		} else if (is_special(ch) && FOLLBY_TOKEN == followedby) {
			/* special chars are their own token values */
			token = ch;
			/*
			 * '=' outside simulator configuration implies
			 * a single string following as in:
			 * setvar Owner = "The Boss" default
			 */
			if ('=' == ch && old_config_style)
				followedby = FOLLBY_STRING;
			yytext[0] = (char)ch;
			yytext[1] = '\0';
			goto normal_return;
		} else
			push_back_char(ip_file, ch);

		/* save the position of start of the token */
		ip_file->prev_token_line_no = ip_file->line_no;
		ip_file->prev_token_col_no = ip_file->col_no;

		/* Read in the lexeme */
		i = 0;
		while (EOF != (ch = get_next_char(ip_file))) {

			yytext[i] = (char)ch;

			/* Break on whitespace or a special character */
			if (isspace(ch) || is_EOC(ch) 
			    || '"' == ch
			    || (FOLLBY_TOKEN == followedby
				&& is_special(ch)))
				break;

			/* Read the rest of the line on reading a start
			   of comment character */
			if ('#' == ch) {
				while (EOF != (ch = get_next_char(ip_file))
				       && '\n' != ch)
					; /* Null Statement */
				break;
			}

			i++;
			if (i >= COUNTOF(yytext))
				goto lex_too_long;
		}
		/* Pick up all of the string inside between " marks, to
		 * end of line.  If we make it to EOL without a
		 * terminating " assume it for them.
		 *
		 * XXX - HMS: I'm not sure we want to assume the closing "
		 */
		if ('"' == ch) {
			instring = TRUE;
			while (EOF != (ch = get_next_char(ip_file)) &&
			       ch != '"' && ch != '\n') {
				yytext[i++] = (char)ch;
				if (i >= COUNTOF(yytext))
					goto lex_too_long;
			}
			/*
			 * yytext[i] will be pushed back as not part of
			 * this lexeme, but any closing quote should
			 * not be pushed back, so we read another char.
			 */
			if ('"' == ch)
				ch = get_next_char(ip_file);
		}
		/* Pushback the last character read that is not a part
		 * of this lexeme.
		 * If the last character read was an EOF, pushback a
		 * newline character. This is to prevent a parse error
		 * when there is no newline at the end of a file.
		 */
		if (EOF == ch)
			push_back_char(ip_file, '\n');
		else
			push_back_char(ip_file, ch); 
		yytext[i] = '\0';
	} while (i == 0);

	/* Now return the desired token */
	
	/* First make sure that the parser is *not* expecting a string
	 * as the next token (based on the previous token that was
	 * returned) and that we haven't read a string.
	 */
	
	if (followedby == FOLLBY_TOKEN && !instring) {
		token = is_keyword(yytext, &followedby);
		if (token) {
			/*
			 * T_Server is exceptional as it forces the
			 * following token to be a string in the
			 * non-simulator parts of the configuration,
			 * but in the simulator configuration section,
			 * "server" is followed by "=" which must be
			 * recognized as a token not a string.
			 */
			if (T_Server == token && !old_config_style)
				followedby = FOLLBY_TOKEN;
			goto normal_return;
		} else if (is_integer(yytext)) {
			yylval_was_set = TRUE;
			errno = 0;
			if ((yylval.Integer = strtol(yytext, NULL, 10)) == 0
			    && ((errno == EINVAL) || (errno == ERANGE))) {
				msyslog(LOG_ERR, 
					"Integer cannot be represented: %s",
					yytext);
				if (input_from_file) {
					exit(1);
				} else {
					/* force end of parsing */
					yylval.Integer = 0;
					return 0;
				}
			}
			token = T_Integer;
			goto normal_return;
		} else if (is_u_int(yytext)) {
			yylval_was_set = TRUE;
			if ('0' == yytext[0] &&
			    'x' == tolower((unsigned char)yytext[1]))
				converted = sscanf(&yytext[2], "%x",
						   &yylval.U_int);
			else
				converted = sscanf(yytext, "%u",
						   &yylval.U_int);
			if (1 != converted) {
				msyslog(LOG_ERR, 
					"U_int cannot be represented: %s",
					yytext);
				if (input_from_file) {
					exit(1);
				} else {
					/* force end of parsing */
					yylval.Integer = 0;
					return 0;
				}
			}
			token = T_U_int;
			goto normal_return;
		} else if (is_double(yytext)) {
			yylval_was_set = TRUE;
			errno = 0;
			if ((yylval.Double = atof(yytext)) == 0 && errno == ERANGE) {
				msyslog(LOG_ERR,
					"Double too large to represent: %s",
					yytext);
				exit(1);
			} else {
				token = T_Double;
				goto normal_return;
			}
		} else {
			/* Default: Everything is a string */
			yylval_was_set = TRUE;
			token = create_string_token(yytext);
			goto normal_return;
		}
	}

	/*
	 * Either followedby is not FOLLBY_TOKEN or this lexeme is part
	 * of a string.  Hence, we need to return T_String.
	 * 
	 * _Except_ we might have a -4 or -6 flag on a an association
	 * configuration line (server, peer, pool, etc.).
	 *
	 * This is a terrible hack, but the grammar is ambiguous so we
	 * don't have a choice.  [SK]
	 *
	 * The ambiguity is in the keyword scanner, not ntp_parser.y.
	 * We do not require server addresses be quoted in ntp.conf,
	 * complicating the scanner's job.  To avoid trying (and
	 * failing) to match an IP address or DNS name to a keyword,
	 * the association keywords use FOLLBY_STRING in the keyword
	 * table, which tells the scanner to force the next token to be
	 * a T_String, so it does not try to match a keyword but rather
	 * expects a string when -4/-6 modifiers to server, peer, etc.
	 * are encountered.
	 * restrict -4 and restrict -6 parsing works correctly without
	 * this hack, as restrict uses FOLLBY_TOKEN.  [DH]
	 */
	if ('-' == yytext[0]) {
		if ('4' == yytext[1]) {
			token = T_Ipv4_flag;
			goto normal_return;
		} else if ('6' == yytext[1]) {
			token = T_Ipv6_flag;
			goto normal_return;
		}
	}

	instring = FALSE;
	if (FOLLBY_STRING == followedby)
		followedby = FOLLBY_TOKEN;

	yylval_was_set = TRUE;
	token = create_string_token(yytext);

normal_return:
	if (T_EOC == token)
		DPRINTF(4,("\t<end of command>\n"));
	else
		DPRINTF(4, ("yylex: lexeme '%s' -> %s\n", yytext,
			    token_name(token)));

	if (!yylval_was_set)
		yylval.Integer = token;

	return token;

lex_too_long:
	yytext[min(sizeof(yytext) - 1, 50)] = 0;
	msyslog(LOG_ERR, 
		"configuration item on line %d longer than limit of %lu, began with '%s'",
		ip_file->line_no, (u_long)min(sizeof(yytext) - 1, 50),
		yytext);

	/*
	 * If we hit the length limit reading the startup configuration
	 * file, abort.
	 */
	if (input_from_file)
		exit(sizeof(yytext) - 1);

	/*
	 * If it's runtime configuration via ntpq :config treat it as
	 * if the configuration text ended before the too-long lexeme,
	 * hostname, or string.
	 */
	yylval.Integer = 0;
	return 0;
}
