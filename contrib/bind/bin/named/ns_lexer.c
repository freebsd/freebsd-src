#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_lexer.c,v 8.12 1997/12/04 08:11:52 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <syslog.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"
#include "ns_parser.h"
#include "ns_parseutil.h"
#include "ns_lexer.h"

typedef enum lexer_state {
		scan, number, identifier, ipv4, quoted_string
} LexerState;

#define LEX_EOF			0x01

typedef struct lexer_file_context {
	const char *	name;
	FILE *		stream;
	int		line_number;
	LexerState	state;
	u_int		flags;
        int		warnings;
        int		errors;
	struct lexer_file_context *
			next;
} *LexerFileContext;

LexerFileContext current_file = NULL;

#define LEX_LAST_WAS_DOT	0x01
#define LEX_CONSECUTIVE_DOTS	0x02

typedef struct lexer_identifier {
	char buffer[LEX_MAX_IDENT_SIZE+1];
	int index;
	int num_dots;
	unsigned int flags;
} *LexerIdentifier;

static LexerIdentifier id;

static char special_chars[256];

#define whitespace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')
#define domain_char(c) (isalnum((c)) || (c) == '.' || (c) == '-')
#define special_char(c) (special_chars[(c)] == 1)
#define identifier_char(c) (!whitespace(c) && !special_char(c))

static int last_token;
static YYSTYPE last_yylval;

static int lexer_initialized = 0;

/*
 * Problem Reporting
 */

static char *
token_to_text(int token, YYSTYPE lval) {
	static char buffer[LEX_MAX_IDENT_SIZE+50];

	if (token < 128) {
		if (token == 0)
			strcpy(buffer, "<end of file>");
		else
			sprintf(buffer, "'%c'", token);
	} else {
		switch (token) {
		case L_EOS:
			strcpy(buffer, ";");
			break;
		case L_STRING:
			sprintf(buffer, "'%s'", lval.cp);
			break;
		case L_QSTRING:
			sprintf(buffer, "\"%s\"", lval.cp);
			break;
		case L_IPADDR:
			sprintf(buffer, "%s", inet_ntoa(lval.ip_addr));
			break;
		case L_NUMBER:
			sprintf(buffer, "%ld", lval.num);
			break;	
		case L_END_INCLUDE:
			sprintf(buffer, "<end of include>");
			break;
		default:
			sprintf(buffer, "%s", lval.cp);
		}
	}

	return (buffer);
}

static char where[MAXPATHLEN + 100];
static char message[20480];

static void
parser_complain(int is_warning, int print_last_token, const char *format,
		va_list args)
{
	LexerFileContext lf;
	int severity;

	if (is_warning) {
		severity = log_warning;
	} else {
		severity = log_error;
	}

	INSIST(current_file != NULL);
	if (current_file->next != NULL) {
		for (lf = current_file; lf != NULL; lf = lf->next) {
			log_write(log_ctx, ns_log_parser, severity,
				  "%s '%s' line %d", 
				  (lf == current_file) ? 
				  "In" : "included from",
				  lf->name, lf->line_number);
		}
	}
	sprintf(where, "%s:%d: ", current_file->name,
		current_file->line_number);
	vsprintf(message, format, args);
	if (print_last_token)
		log_write(log_ctx, ns_log_parser, severity, "%s%s near %s",
			  where, message,
			  token_to_text(last_token, last_yylval));
	else
		log_write(log_ctx, ns_log_parser, severity,
			  "%s%s", where, message);
}

int
parser_warning(int print_last_token, const char *format, ...) {
	va_list args;

	va_start(args, format);
	parser_complain(1, print_last_token, format, args);
	va_end(args);
	current_file->warnings++;
	return (1);
}

int
parser_error(int print_last_token, const char *format, ...) {
	va_list args;

	va_start(args, format);
	parser_complain(0, print_last_token, format, args);
	va_end(args);
	current_file->errors++;
	return (1);
}

void
yyerror(const char *message) {
	parser_error(1, message);
}

/*
 * Keywords
 */

struct keyword {
        char *name;
	int token;
};

/*
 * "keywords" is an array of the keywords which are the fixed syntactic
 * elements of the configuration file.  Each keyword has a string version
 * of the keyword and a token id, which should be an identifier which
 * matches that in a %token statement inside the parser.y file.
 */
static struct keyword keywords[] = {
	{"acl", T_ACL}, 
	{"address", T_ADDRESS},
	{"algorithm", T_ALGID},
	{"allow-query", T_ALLOW_QUERY}, 
	{"allow-transfer", T_ALLOW_TRANSFER},
	{"allow-update", T_ALLOW_UPDATE},
	{"also-notify", T_ALSO_NOTIFY},
	{"auth-nxdomain", T_AUTH_NXDOMAIN},
	{"bogus", T_BOGUS},
	{"category", T_CATEGORY},
	{"channel", T_CHANNEL},
	{"check-names", T_CHECK_NAMES},
	{"cleaning-interval", T_CLEAN_INTERVAL},
	{"coresize", T_CORESIZE},
	{"datasize", T_DATASIZE},
	{"deallocate-on-exit", T_DEALLOC_ON_EXIT},
	{"debug", T_DEBUG},
	{"default", T_DEFAULT},
	{"directory", T_DIRECTORY}, 
	{"dump-file", T_DUMP_FILE},
	{"dynamic", T_DYNAMIC},
	{"fail", T_FAIL},
	{"fake-iquery", T_FAKE_IQUERY},
	{"false", T_FALSE},
	{"fetch-glue", T_FETCH_GLUE},
	{"file", T_FILE}, 
	{"files", T_FILES}, 
	{"first", T_FIRST}, 
	{"forward", T_FORWARD},
	{"forwarders", T_FORWARDERS},
	{"hint", T_HINT},
	{"host-statistics", T_HOSTSTATS},
	{"if-no-answer", T_IF_NO_ANSWER},
	{"if-no-domain", T_IF_NO_DOMAIN},
	{"ignore", T_IGNORE},
	{"include", T_INCLUDE},
	{"interface-interval", T_INTERFACE_INTERVAL},
	{"key", T_SEC_KEY},
	{"keys", T_KEYS},
	{"listen-on", T_LISTEN_ON},
	{"logging", T_LOGGING},
	{"many-answers", T_MANY_ANSWERS},
	{"master", T_MASTER},
	{"masters", T_MASTERS},
	{"max-transfer-time-in", T_MAX_TRANSFER_TIME_IN},
	{"memstatistics-file", T_MEMSTATS_FILE},
	{"multiple-cnames", T_MULTIPLE_CNAMES},
	{"named-xfer", T_NAMED_XFER},
	{"no", T_NO},
	{"notify", T_NOTIFY},
	{"null", T_NULL_OUTPUT},
	{"one-answer", T_ONE_ANSWER},
	{"only", T_ONLY},
	{"options", T_OPTIONS},
	{"pid-file", T_PIDFILE},
	{"port", T_PORT},
	{"print-category", T_PRINT_CATEGORY},
	{"print-severity", T_PRINT_SEVERITY},
	{"print-time", T_PRINT_TIME},
	{"query-source", T_QUERY_SOURCE},
	{"recursion", T_RECURSION},
	{"response", T_RESPONSE},
	{"secret", T_SECRET},
	{"server", T_SERVER}, 
	{"severity", T_SEVERITY}, 
	{"size", T_SIZE}, 
	{"slave", T_SLAVE},
	{"stacksize", T_STACKSIZE},
	{"statistics-file", T_STATS_FILE},
	{"statistics-interval", T_STATS_INTERVAL},
	{"stub", T_STUB},
	{"syslog", T_SYSLOG}, 
	{"topology", T_TOPOLOGY},
	{"transfer-format", T_TRANSFER_FORMAT}, 
	{"transfer-source", T_TRANSFER_SOURCE},
	{"transfers", T_TRANSFERS}, 
	{"transfers-in", T_TRANSFERS_IN}, 
	{"transfers-out", T_TRANSFERS_OUT}, 
	{"transfers-per-ns", T_TRANSFERS_PER_NS}, 
	{"true", T_TRUE}, 
	{"type", T_TYPE},
	{"unlimited", T_UNLIMITED},
	{"versions", T_VERSIONS}, 
	{"warn", T_WARN},
	{"yes", T_YES}, 
	{"zone", T_ZONE},
	{(char *) NULL, 0},
};

/*
 * The table size should be a prime chosen to minimize collisions.
 */
#define KEYWORD_TABLE_SIZE 461

static symbol_table keyword_table = NULL;

static void
init_keywords() {
	struct keyword *k;
	symbol_value value;

	if (keyword_table != NULL)
		free_symbol_table(keyword_table);
	keyword_table = new_symbol_table(KEYWORD_TABLE_SIZE, NULL);
	for (k = keywords; k->name != NULL; k++) {
		value.integer = k->token;
		define_symbol(keyword_table, k->name, 0, value, 0);
	}
	dprint_symbol_table(99, keyword_table);
}

/*
 * File Contexts
 */

void
lexer_begin_file(const char *filename, FILE *stream) {
	LexerFileContext lf;

	if (stream == NULL) {
		stream = fopen(filename, "r");
		if (stream == NULL) {
			parser_error(0, "couldn't open include file '%s'",
				     filename);
			return;
		}
	}
	lf = (LexerFileContext)memget(sizeof (struct lexer_file_context));
	if (lf == NULL)
		panic("memget failed in lexer_begin_file", NULL);
	INSIST(stream != NULL);
	lf->stream = stream;
	lf->name = filename;  /* note copy by reference */
	lf->line_number = 1;
	lf->state = scan;
	lf->flags = 0;
	lf->warnings = 0;
	lf->errors = 0;
	lf->next = current_file;
	current_file = lf;
}

void
lexer_end_file(void) {
	LexerFileContext lf;

	INSIST(current_file != NULL);
	lf = current_file;
	current_file = lf->next;
	fclose(lf->stream);
	memput(lf, sizeof *lf);
}

/*
 * Character Input
 */

static void
scan_to_comment_end(int c_plus_plus_style) {
	int c, nc;
	int done = 0;
	int prev_was_star = 0;

	while (!done) {
		c = getc(current_file->stream);
		switch (c) {
		case EOF:
			if (!c_plus_plus_style)
				parser_error(0, "EOF in comment");
			current_file->flags |= LEX_EOF;
			done = 1;
			break;
		case '*':
			prev_was_star = 1;
			break;
		case '/':
			if (prev_was_star && !c_plus_plus_style)
				done = 1;
			prev_was_star = 0;
			break;
		case '\n':
			if (c_plus_plus_style) {
				/* don't consume the newline because
				   we want it to be a delimiter for
				   anything before the comment
				   started */
				ungetc(c, current_file->stream);
				done = 1;
			} else {
				current_file->line_number++;
			}
			prev_was_star = 0;
			break;
		default:
			prev_was_star = 0;
		}
	}
}

int
get_next_char(int comment_ok) {
	int c, nc;

	if (current_file->flags & LEX_EOF)
		return (EOF);

	c = getc(current_file->stream);

	if (comment_ok) {
		while (c == '/' || c == '#') {
			if (c == '#') {
				scan_to_comment_end(1);
				if (current_file->flags & LEX_EOF)
					return (EOF);
				c = getc(current_file->stream);
			} else {
				nc = getc(current_file->stream);
				switch (nc) {
				case EOF:
					current_file->flags |= LEX_EOF;
					return ('/');
				case '*':
				case '/':
					scan_to_comment_end((nc == '/'));
					if (current_file->flags & LEX_EOF)
						return (EOF);
					c = getc(current_file->stream);
					break;
				default:
					ungetc((nc), current_file->stream);
					return ('/');
				}
			}
		}
	}

	if (c == EOF)
		current_file->flags |= LEX_EOF;
	else if (c == '\n')
		current_file->line_number++;
	return (c);
}

void
put_back_char(int c) {
	if (c == EOF)
		current_file->flags |= LEX_EOF;
	else {
		ungetc((c), current_file->stream);
		if (c == '\n')
			current_file->line_number--;
	}
}


/*
 * Identifiers
 */

static void
clear_identifier(LexerIdentifier id) {
	INSIST(id != NULL);
	id->index = 0;
	id->num_dots = 0;
	id->flags = 0;
}

static char *
dup_identifier(LexerIdentifier id) {
	char *duplicate;

	INSIST(id != NULL);
	duplicate = savestr(id->buffer, 1);
	return (duplicate);
}

static void
finish_identifier(LexerIdentifier id) {
	INSIST(id != NULL && id->index < LEX_MAX_IDENT_SIZE);
	id->buffer[id->index] = '\0';
}

static void
add_to_identifier(LexerIdentifier id, int c) {
	INSIST(id != NULL);
	id->buffer[id->index] = c;
	id->index++;
	if (id->index >= LEX_MAX_IDENT_SIZE) {
		parser_error(0, "identifier too long");
		current_file->state = scan;
		/* discard chars until we hit a non-identifier char */
		while (identifier_char(c)) {
			c = get_next_char(1);
		}
		put_back_char(c);
		clear_identifier(id);
	} else {
		if (c == '.') {
			if (id->flags & LEX_LAST_WAS_DOT)
				id->flags |= LEX_CONSECUTIVE_DOTS;
			id->flags |= LEX_LAST_WAS_DOT;
			id->num_dots++;
		} else {
			id->flags &= ~LEX_LAST_WAS_DOT;
		}
	}
}

/*
 * yylex() -- return the next token from the current input stream
 */
int
yylex() {
	int c, i;
	int comment_ok = 1;
	int token = -1;
	symbol_value value;

	while (token < 0) {
		c = get_next_char(comment_ok);
		switch(current_file->state) {
		case scan:
			if (c == EOF) {
				if (current_file->next == NULL)
					/*
					 * We don't want to call
					 * lexer_end_file() here because we
					 * want to keep the toplevel file
					 * context to log errors against.
					 */
					token = 0;
				else {
					lexer_end_file();
					token = L_END_INCLUDE;
				}
				break;
			}
			if (whitespace(c))
				break;
			if (identifier_char(c)) {
				if (isdigit(c))
					current_file->state = number;
				else
					current_file->state = identifier;
				clear_identifier(id);
				add_to_identifier(id, c);
			} else
				if (special_char(c)) {
					if (c == ';') {
						token = L_EOS; 
						break;
					}
					if (c == '"') {
						clear_identifier(id);
						current_file->state =
							quoted_string;
						comment_ok = 0;
						break;
					}
					token = c;
				} else {
					parser_error(0,
						     "invalid character '%c'",
						     c);
				}
			break;

		case number:
			if (identifier_char(c)) {
				if (!isdigit(c))
					current_file->state =
						(c == '.') ? ipv4 : identifier;
				add_to_identifier(id, c);
			} else {
				put_back_char(c);
				current_file->state = scan;
				finish_identifier(id);
				yylval.num = atoi(id->buffer);
				token = L_NUMBER;
			}
			break;

		case identifier:
			if (identifier_char(c)) {
				add_to_identifier(id, c);
			} else {
				put_back_char(c);
				current_file->state = scan;
				finish_identifier(id);
				/* is it a keyword? */
				if (lookup_symbol(keyword_table, id->buffer,
						  0, &value)) {
					yylval.cp = id->buffer;
					token = value.integer;
				} else {
					yylval.cp = dup_identifier(id);
					token = L_STRING;
				}
			}
			break;

		case ipv4:
			if (identifier_char(c)) {
				if (!isdigit(c)) {
					if (c != '.' ||
					    (id->flags & LEX_CONSECUTIVE_DOTS))
						current_file->state =
							identifier;
				}
				add_to_identifier(id, c);
			} else {
				put_back_char(c);
				if (id->num_dots > 3 ||
				    (id->flags & LEX_LAST_WAS_DOT))
					current_file->state = identifier;
				else {
					if (id->num_dots == 1) {
						add_to_identifier(id, '.');
						add_to_identifier(id, '0');
						add_to_identifier(id, '.');
						add_to_identifier(id, '0');
					} else if (id->num_dots == 2) {
						add_to_identifier(id, '.');
						add_to_identifier(id, '0');
					}
					current_file->state = scan;
					finish_identifier(id);
					token = L_IPADDR;
					if (inet_aton(id->buffer,
						      &(yylval.ip_addr))==0) {
						yylval.cp = dup_identifier(id);
						token = L_STRING;
					}
				}
			}
			break;

		case quoted_string:
			if (c == EOF) {
				parser_error(0, "EOF in quoted string");
				return 0;
			} else {
				if (c == '"') {
					comment_ok = 1;
					current_file->state = scan;
					finish_identifier(id);
					yylval.cp = dup_identifier(id);
					token = L_QSTRING;
				} else {
					/* XXX add backslash escapes here */
					add_to_identifier(id, c);
				}
			}
			break;
			
		default:
			panic("unhandled state in yylex", NULL);
		}
	}

	last_token = token;
	last_yylval = yylval;
	return (token);
}

/*
 * Initialization
 */

symbol_table constants;

static void
import_constants(const struct ns_sym *s, int type) {
	symbol_value value;
	for ((void)NULL; s != NULL && s->name != NULL; s++) {
		value.integer = s->number;
		define_symbol(constants, s->name, type, value, 0);
	}
}	

static void
import_res_constants(const struct res_sym *r, int type) {
	symbol_value value;
	for ((void)NULL; r != NULL && r->name != NULL; r++) {
		value.integer = r->number;
		define_symbol(constants, r->name, type, value, 0);
	}
}	

#define CONSTANTS_TABLE_SIZE 397	/* should be prime */

static void
import_all_constants() {
	constants = new_symbol_table(CONSTANTS_TABLE_SIZE, NULL);
	import_res_constants(__p_class_syms, SYM_CLASS);
	import_constants(category_constants, SYM_CATEGORY);
	import_constants(logging_constants, SYM_LOGGING);
	import_constants(syslog_constants, SYM_SYSLOG);
}

void
lexer_initialize() {
	memset(special_chars, 0, sizeof special_chars);
	special_chars[';'] = 1;
	special_chars['{'] = 1;
	special_chars['}'] = 1;
	special_chars['!'] = 1;
	special_chars['/'] = 1;
	special_chars['"'] = 1;
	special_chars['*'] = 1;
	id = (LexerIdentifier)memget(sizeof (struct lexer_identifier));
	if (id == NULL)
		panic("memget failed in init_once", NULL);
	init_keywords();
	import_all_constants();
	lexer_initialized = 1;
}

void
lexer_setup(void) {
	REQUIRE(lexer_initialized);

	current_file = NULL;    /* XXX should we INSIST(current_file==NULL)? */
	INSIST(id != NULL);
}

void
lexer_shutdown(void) {
	REQUIRE(lexer_initialized);

	free_symbol_table(keyword_table);
	free_symbol_table(constants);
	memput(id, sizeof (struct lexer_identifier));
	lexer_initialized = 0;
}
