/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_config_lex.c,v 1.3 1999/08/28 01:15:32 peter Exp $
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Parse a configuration file into tokens
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h> 
#include <netatm/queue.h> 
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
  
#include <ctype.h>
#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"
#include "scsp_config_parse.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_config_lex.c,v 1.3 1999/08/28 01:15:32 peter Exp $");
#endif


/*
 * Global variables
 */
int		parse_line = 1;

/*
 * Local definitions
 */
#define	TOK_MAX_LEN	128

/*
 * Character classes
 */
#define	CHAR_INVALID	0		/* Not allowed */
#define	CHAR_ALPHA	1		/* G-W, Y, Z */
#define	CHAR_HEX_DIGIT	2		/* A-F */
#define	CHAR_X		3		/* X */
#define	CHAR_0		4		/* '0' */
#define	CHAR_DIGIT	5		/* 1-9 */
#define	CHAR_SPACE	6		/* space, tab */
#define	CHAR_DECIMAL	7		/* period */
#define	CHAR_SLASH	8		/* slash */
#define	CHAR_ASTERISK	9		/* asterisk */
#define	CHAR_HASH	10		/* pound sign */
#define	CHAR_SPECIAL	11		/* semicolon, braces */
#define	CHAR_MISC	12		/* chars allowd in file names */
#define	CHAR_EOL	13		/* new line */
#define	CHAR_EOF	14		/* EOF */
#define	CHAR_CNT	CHAR_EOF + 1

/*
 * Character class table (initialized by init_class_tbl())
 */
static char class_tbl[128];

/*
 * State table element structure
 */
struct state_entry {
	int	action;
	int	next;
};

/*
 * Scanner states
 */
#define	TS_INIT		0
#define	TS_ALPHA	1
#define	TS_INT_1	2
#define	TS_INT		3
#define	TS_HEX		4
#define	TS_SLASH_1	5
#define	TS_COMMENT	6
#define	TS_COMMENT_1	7
#define	TS_FLUSH	8
#define	TS_HEX_1	9
#define	TS_CNT		TS_HEX_1 + 1

/*
 * Token scanner state table
 */
static struct state_entry token_state_tbl[CHAR_CNT][TS_CNT] = {
/*          0     1     2     3     4     5     6     7     8     9   */
/* bad */{{2,0},{2,0},{2,0},{2,0},{2,0},{2,0},{0,6},{0,6},{0,8},{2,0}},
/* g-z */{{1,1},{1,1},{1,1},{1,1},{2,0},{1,1},{0,6},{0,6},{0,8},{2,0}},
/* a-f */{{1,1},{1,1},{1,1},{1,1},{1,9},{1,1},{0,6},{0,6},{0,8},{1,4}},
/*  x  */{{1,1},{1,1},{1,4},{1,4},{2,0},{1,1},{0,6},{0,6},{0,8},{2,0}},
/*  0  */{{1,2},{1,1},{1,3},{1,3},{1,9},{1,1},{0,6},{0,6},{0,8},{1,4}},
/* 1-9 */{{1,3},{1,1},{1,3},{1,3},{1,9},{1,1},{0,6},{0,6},{0,8},{1,4}},
/* sp  */{{0,0},{6,0},{8,0},{8,0},{7,0},{6,0},{0,6},{0,6},{0,8},{2,0}},
/*  .  */{{2,0},{1,1},{1,1},{1,1},{1,4},{1,1},{0,6},{0,6},{0,8},{2,0}},
/*  /  */{{1,5},{1,1},{1,1},{1,1},{7,0},{4,8},{0,6},{0,0},{0,8},{2,0}},
/*  *  */{{2,0},{6,0},{8,0},{8,0},{7,0},{4,6},{0,7},{0,7},{0,8},{2,0}},
/*  #  */{{0,8},{6,0},{8,0},{8,0},{7,0},{6,0},{0,6},{0,6},{0,8},{2,0}},
/* ;{} */{{3,0},{6,0},{8,0},{8,0},{7,0},{6,0},{0,6},{0,6},{0,8},{2,0}},
/* Msc */{{2,0},{1,1},{1,1},{1,1},{2,0},{1,1},{0,6},{0,6},{0,8},{2,0}},
/* EOL */{{0,0},{6,0},{8,0},{8,0},{7,0},{6,0},{0,6},{0,6},{0,0},{2,0}},
/* EOF */{{9,0},{6,0},{8,0},{8,0},{7,0},{6,0},{2,0},{2,0},{9,0},{2,0}},
};


/*
 * Reserved words
 */
static struct {
	char	*word;
	int	token;
} rsvd_word_tbl[] = {
	{ "ATMaddr",		TOK_DCS_ADDR },
	{ "ATMARP",		TOK_ATMARP },
	{ "CAReXmitInt",	TOK_DCS_CA_REXMIT_INT },
	{ "CSUSReXmitInt",	TOK_DCS_CSUS_REXMIT_INT },
	{ "CSUReXmitInt",	TOK_DCS_CSU_REXMIT_INT  },
	{ "CSUReXmitMax",	TOK_DCS_CSU_REXMIT_MAX },
	{ "DCS",		TOK_DCS },
	{ "DHCP",		TOK_DHCP },
	{ "familyID",		TOK_FAMILY },
	{ "file",		TOK_LFN },
	{ "hops",		TOK_DCS_HOP_CNT },
	{ "HelloDead",		TOK_DCS_HELLO_DF },
	{ "HelloInt",		TOK_DCS_HELLO_INT },
	{ "ID",			TOK_DCS_ID },
	{ "LNNI",		TOK_LNNI },
	{ "log",		TOK_LOG },
	{ "MARS",		TOK_MARS },
	{ "netif",		TOK_NETIF },
	{ "NHRP",		TOK_NHRP },
	{ "protocol",		TOK_PROTOCOL },
	{ "server",		TOK_SERVER },
	{ "ServerGroupID",	TOK_SRVGRP },
	{ "syslog",		TOK_SYSLOG },
	{ (char *)0,		0 },
};


/*
 * Copy a character string
 *
 * Make a copy of a character string, using strdup.  If strdup fails,
 * meaning we're out of memory, then print an error message and exit.
 *
 * Arguments:
 *	s	string to be copied
 *
 * Returns:
 *	char *	pointer to area provided by strdup
 *
 */
static char *
copy_buffer(s)
	char	*s;
{
	char	*t;

	t = strdup(s);

	if (!t) {
		fprintf(stderr, "%s: strdup failed\n", prog);
		exit(1);
	}

	return(t);
}


/*
 * Push a character back onto the input stream.
 *
 * Arguments:
 *	c	character to be pushed
 *
 * Returns:
 *	none
 *
 */
static void
push_char(c)
	char	c;
{
	if (c == '\n')
		parse_line--;

	ungetc(c, cfg_file);
}


/*
 * Initialize the character class table.
 *
 * Set each entry in the character class table to the class
 * corresponding to the character.
 *
 * Arguments:
 *	tbl	pointer to table to be initialized
 *
 * Returns:
 *	None
 */
static void
init_class_tbl(tbl)
	char *tbl;
{
	int	i;
	char	c;

	/*
	 * Set up the table for all ASCII characters
	 */
	for (i=0; isascii((char)i); i++) {
		/*
		 * Clear entry
		 */
		tbl[i] = CHAR_INVALID;

		/*
		 * Set entries depending on character type
		 */
		c = (char)i;
		if (c == 'a' || c == 'b' || c == 'c' ||
				c == 'd' || c == 'e' || c == 'f' ||
				c == 'A' || c == 'B' || c == 'C' ||
				c == 'D' || c == 'E' || c == 'F')
			tbl[i] = CHAR_HEX_DIGIT;
		else if (c == 'x' || c == 'X')
			tbl[i] = CHAR_X;
		else if (isalpha(c))
			tbl[i] = CHAR_ALPHA;
		else if (c == '0')
			tbl[i] = CHAR_0;
		else if (isdigit(c))
			tbl[i] = CHAR_DIGIT;
		else if (c == '\n')
			tbl[i] = CHAR_EOL;
		else if (c == ' ' || c == '\t')
			tbl[i] = CHAR_SPACE;
		else if (c == '#')
			tbl[i] = CHAR_HASH;
		else if (c == '*')
			tbl[i] = CHAR_ASTERISK;
		else if (c == '.')
			tbl[i] = CHAR_DECIMAL;
		else if (c == '/')
			tbl[i] = CHAR_SLASH;
		else if (c == ';' || c == '{' || c == '}')
			tbl[i] = CHAR_SPECIAL;
		else if (c == '-' || c == '_' || c == '&' || c == '@' ||
				c == '~')
			tbl[i] = CHAR_MISC;
	}
}


/*
 * Get the class of a character.
 *
 * Arguments:
 *	c	character being scanned
 *
 * Returns:
 *	int	character class
 */
static int
char_class(c)
	char	c;
{
	int	class = CHAR_INVALID;

	if (c == EOF) {
		class = CHAR_EOF;
	} else if (c < 0 || !isascii(c)) {
		class = CHAR_INVALID;
	} else {
		class = class_tbl[(int)c];
	}

	return(class);
}


/*
 * Print an error message when the scanner finds an error
 *
 * Arguments:
 *	c	character on which the error was recognized
 *	state	scanner state at error
 *
 * Returns:
 *	None
 */
static void
scan_error(c, state)
	char	c;
	int	state;
{
	/*
	 * Check for invalid character
	 */
	if (char_class(c) == CHAR_INVALID) {
		parse_error("Invalid character 0x%x encountered",
			c);
		return;
	}

	/*
	 * Check for unexpected EOF
	 */
	if (char_class(c) == CHAR_EOF) {
		parse_error("Unexpected end of file");
		return;
	}

	/*
	 * Error depends on state
	 */
	switch(state) {
	case TS_INIT:
		parse_error("Syntax error at '%c'", c);
		break;
	case TS_ALPHA:
	case TS_INT_1:
	case TS_INT:
	case TS_SLASH_1:
	case TS_COMMENT:
	case TS_COMMENT_1:
	case TS_FLUSH:
		parse_error("Syntax error");
		break;
	case TS_HEX:
	case TS_HEX_1:
		parse_error("Syntax error in hex string");
		break;
	}
}


/*
 * Assemble a token
 *
 * Read a character at a time from the input file, assembling the
 * characters into tokens as specified by the token scanner state
 * table.  Return the completed token.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	token	the type of the token found
 */
int
yylex()
{
	int		i, state;
	char		c, token_buffer[TOK_MAX_LEN];

	/*
	 * Initialize
	 */
	if (class_tbl['A'] != CHAR_HEX_DIGIT)
		init_class_tbl(class_tbl);
	state = TS_INIT;
	UM_ZERO(token_buffer, sizeof(token_buffer));
	UM_ZERO(&yylval, sizeof(yylval));

	/*
	 * Handle a character at a time until a token is built
	 */
	while(1) {
		/*
		 * Read a character from the input file.
		 */
		c = (char)getc(cfg_file);
		if (c == '\n') {
			parse_line++;
		}

#ifdef NOTDEF
		printf("token_state: state=%d, char=%c, class=%d, action=%d, next=%d\n",
				state,
				c,
				char_class(c),
				token_state_tbl[char_class][state].action,
				token_state_tbl[char_class][state].next);
#endif

		/*
		 * Perform an action based on the state table
		 */
		switch(token_state_tbl[char_class(c)][state].action) {
		case 0:
			/*
			 * Ignore the character
			 */
			break;
		case 1:
			/*
			 * Add character to buffer
			 */
			if (strlen(token_buffer) < TOK_MAX_LEN) {
				token_buffer[strlen(token_buffer)] = c;
			}
			break;
		case 2:
			/*
			 * Error--print a message and start over
			 */
			scan_error(c, state);
			break;
		case 3:
			/*
			 * Return special character
			 */
			return(c);
			break;
		case 4:
			/*
			 * Clear the token buffer
			 */
			UM_ZERO(token_buffer, sizeof(token_buffer));
			break;
		case 5:
			/*
			 * Not used
			 */
			break;
		case 6:
			/*
			 * Return character token
			 */
			push_char(c);

			/*
			 * Check for reserved words
			 */
			for (i=0; rsvd_word_tbl[i].word; i++) {
				if (strcasecmp(token_buffer,
						rsvd_word_tbl[i].word) == 0)
					break;
			}
			if (rsvd_word_tbl[i].word) {
				return(rsvd_word_tbl[i].token);
			}

			/*
			 * Word isn't reserved, return alpha string
			 */
			yylval.tv_alpha = copy_buffer(token_buffer);
			return(TOK_NAME);
			break;
		case 7:
			/*
			 * Return hex string (ATM address)
			 */
			push_char(c);
			yylval.tv_hex = copy_buffer(token_buffer);
			return(TOK_HEX);
			break;
		case 8:
			/*
			 * Return integer
			 */
			push_char(c);
			yylval.tv_int = atoi(token_buffer);
			return(TOK_INTEGER);
			break;
		case 9:
			/*
			 * Return EOF
			 */
			return(0);
			break;
		default:
			fprintf(stderr, "Invalid action indicator, state=%d, char=0x%02x\n",
					state, c);
			break;
		}

		/*
		 * Set the next state and bump to the next character
		 */
		state = token_state_tbl[char_class(c)][state].next;
	}
}
