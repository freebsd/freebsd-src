
/*
 * ng_parse.h
 *
 * Copyright (c) 1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@whistle.com>
 *
 * $Whistle: ng_parse.h,v 1.2 1999/11/29 01:43:48 archie Exp $
 * $FreeBSD$
 */

#ifndef _NETGRAPH_PARSE_H_
#define _NETGRAPH_PARSE_H_

/*

  This defines a library of routines for converting between various C
  language types in binary form and ASCII strings.  Types are user
  definable.  Several pre-defined types are supplied, for some
  common C types: structures, variable and fixed length arrays,
  integer types, variable and fixed length strings, IP addresses,
  etc.

  Syntax
  ------

    Structures:

      '{' [ <name>=<value> ... ] '}'

      Omitted fields have their default values by implication.
      The order in which the fields are specified does not matter.

    Arrays:

      '[' [ [index=]<value> ... ] ']'

      Element value may be specified with or without the "<index>=" prefix;
      If omitted, the index after the previous element is used.
      Omitted fields have their default values by implication.

    Strings:

      "foo bar blah\r\n"

      That is, strings are specified just like C strings. The usual
      backslash escapes are accepted.

   Other simple types have their obvious ASCII forms.

  Example
  -------

      Structure			Binary (big endian)
      ---------			-------------------

      struct foo {
	struct in_addr ip;  	03 03 03 03
	int bar;		00 00 00 00
	u_char num;	  	02 00
	short ary[0];	  	00 05 00 00 00 0a
      };

      ASCII form:	"{ ip=3.3.3.3 num=3 ary=[ 5 2=10 ] }"

    Note that omitted fields or array elements get their default values
    ("bar" and ary[2]), and that the alignment is handled automatically
    (the extra 00 byte after "num").

  To define a type, you can define it as a sub-type of a predefined
  type, overriding some of the predefined type's methods and/or its
  alignment, or define your own syntax, with the restriction that
  the ASCII representation must not contain any whitespace or these
  characters: { } [ ] = "

*/

/************************************************************************
			METHODS REQUIRED BY A TYPE
 ************************************************************************/

/*
 * Three methods are required for a type. These may be given explicitly
 * or, if NULL, inherited from the super-type.
 */

struct ng_parse_type;

/*
 * Convert ASCII to binary according to the supplied type.
 *
 * The ASCII characters begin at offset *off in 'string'.  The binary
 * representation is put into 'buf', which has at least *buflen bytes.
 * 'start' points to the first byte output by ng_parse() (ie, start <= buf).
 *
 * Upon return, *buflen contains the length of the new binary data, and
 * *off is updated to point just past the end of the parsed range of
 * characters, or, in the case of an error, to the offending character(s).
 *
 * Return values:
 *	0		Success; *buflen holds the length of the data
 *			and *off points just past the last char parsed.
 *	EALREADY	Field specified twice
 *	ENOENT		Unknown field
 *	E2BIG		Array or character string overflow
 *	ERANGE		Output was longer than *buflen bytes
 *	EINVAL		Parse failure or other invalid content
 *	ENOMEM		Out of memory
 *	EOPNOTSUPP	Mandatory array/structure element missing
 */
typedef	int	ng_parse_t(const struct ng_parse_type *type, const char *string,
			int *off, const u_char *start,
			u_char *buf, int *buflen);

/*
 * Convert binary to ASCII according to the supplied type.
 *
 * The results are put into 'buf', which is at least buflen bytes long.
 * *off points to the current byte in 'data' and should be updated
 * before return to point just past the last byte unparsed.
 *
 * Returns:
 *	0		Success
 *	ERANGE		Output was longer than buflen bytes
 */
typedef	int	ng_unparse_t(const struct ng_parse_type *type,
			const u_char *data, int *off, char *buf, int buflen);

/*
 * Compute the default value according to the supplied type.
 *
 * Store the result in 'buf', which is at least *buflen bytes long.
 * Upon return *buflen contains the length of the output.
 *
 * Returns:
 *	0		Success
 *	ERANGE		Output was longer than *buflen bytes
 *	EOPNOTSUPP	Default value is not specified for this type
 */
typedef	int	ng_getDefault_t(const struct ng_parse_type *type,
			const u_char *start, u_char *buf, int *buflen);

/*
 * Return the alignment requirement of this type.  Zero is same as one.
 */
typedef	int	ng_getAlign_t(const struct ng_parse_type *type);

/************************************************************************
			TYPE DEFINITION
 ************************************************************************/

/*
 * This structure describes a type, which may be a sub-type of another
 * type by pointing to it with 'supertype' and omitting one or more methods.
 */
struct ng_parse_type {
	const struct ng_parse_type *supertype;	/* super-type, if any */
	const void		*info;		/* type-specific info */
	void			*private;	/* client private info */
	ng_parse_t		*parse;		/* parse method */
	ng_unparse_t		*unparse;	/* unparse method */
	ng_getDefault_t		*getDefault;	/* get default value method */
	ng_getAlign_t		*getAlign;	/* get alignment */
};

/************************************************************************
			PRE-DEFINED TYPES
 ************************************************************************/

/*
 * Structures
 *
 *   Default value:		Determined on a per-field basis
 *   Additional info:		struct ng_parse_struct_info *
 */
extern const struct ng_parse_type ng_parse_struct_type;

/* Each field has a name, type, and optional alignment override. If the
   override is non-zero, the alignment is determined from the field type.
   Note: add an extra struct ng_parse_struct_field with name == NULL
   to indicate the end of the list. */
struct ng_parse_struct_info {
	struct ng_parse_struct_field {
		const char	*name;		/* field name */
		const struct ng_parse_type
				*type;		/* field type */
		int		alignment;	/* override alignment */
	} fields[0];
};

/*
 * Fixed length arrays
 *
 *   Default value:		See below
 *   Additional info:		struct ng_parse_fixedarray_info *
 */
extern const struct ng_parse_type ng_parse_fixedarray_type;

typedef int	ng_parse_array_getLength_t(const struct ng_parse_type *type,
				const u_char *start, const u_char *buf);
typedef	int	ng_parse_array_getDefault_t(const struct ng_parse_type *type,
				int index, const u_char *start,
				u_char *buf, int *buflen);

/* The 'getDefault' method may be NULL, in which case the default value
   is computed from the element type.  If not, it should fill in the
   default value at *buf (having size *buflen) and update *buflen to the
   length of the filled-in value before return. */
struct ng_parse_fixedarray_info {
	const struct ng_parse_type	*elementType;
	int				length;
	ng_parse_array_getDefault_t	*getDefault;
};

/*
 * Variable length arrays
 *
 *   Default value:		Same as with fixed length arrays
 *   Additional info:		struct ng_parse_array_info *
 */
extern const struct ng_parse_type ng_parse_array_type;

struct ng_parse_array_info {
	const struct ng_parse_type	*elementType;
	ng_parse_array_getLength_t	*getLength;
	ng_parse_array_getDefault_t	*getDefault;
};

/*
 * Arbitrary length strings
 *
 *   Default value:		Empty string
 *   Additional info:		None required
 */
extern const struct ng_parse_type ng_parse_string_type;

/*
 * Bounded length strings.  These are strings that have a fixed-size
 * buffer, and always include a terminating NUL character.
 *
 *   Default value:		Empty string
 *   Additional info:		struct ng_parse_fixedsstring_info *
 */
extern const struct ng_parse_type ng_parse_fixedstring_type;

struct ng_parse_fixedsstring_info {
	int	bufSize;	/* size of buffer (including NUL) */
};

/*
 * Some commonly used bounded length string types
 */
extern const struct ng_parse_type ng_parse_nodebuf_type;  /* NG_NODELEN + 1 */
extern const struct ng_parse_type ng_parse_hookbuf_type;  /* NG_HOOKLEN + 1 */
extern const struct ng_parse_type ng_parse_pathbuf_type;  /* NG_PATHLEN + 1 */
extern const struct ng_parse_type ng_parse_typebuf_type;  /* NG_TYPELEN + 1 */
extern const struct ng_parse_type ng_parse_cmdbuf_type;   /* NG_CMDSTRLEN + 1 */

/*
 * Integer types
 *
 *   Default value:		0
 *   Additional info:		None required
 */
extern const struct ng_parse_type ng_parse_int8_type;
extern const struct ng_parse_type ng_parse_int16_type;
extern const struct ng_parse_type ng_parse_int32_type;
extern const struct ng_parse_type ng_parse_int64_type;

/*
 * IP address type
 *
 *   Default value:		0.0.0.0
 *   Additional info:		None required
 */
extern const struct ng_parse_type ng_parse_ipaddr_type;

/*
 * Variable length byte array. The bytes are displayed in hex.
 * ASCII form may be either an array of bytes or a string constant,
 * in which case the array is zero-filled after the string bytes.
 *
 *   Default value:		All bytes are zero
 *   Additional info:		ng_parse_array_getLength_t *
 */
extern const struct ng_parse_type ng_parse_bytearray_type;

/*
 * Netgraph control message type
 *
 *   Default value:		All fields zero
 *   Additional info:		None required
 */
extern const struct ng_parse_type ng_parse_ng_mesg_type;

/************************************************************************
		CONVERSTION AND PARSING ROUTINES
 ************************************************************************/

/* Tokens for parsing structs and arrays */
enum ng_parse_token {
	T_LBRACE,		/* '{' */
	T_RBRACE,		/* '}' */
	T_LBRACKET,		/* '[' */
	T_RBRACKET,		/* ']' */
	T_EQUALS,		/* '=' */
	T_STRING,		/* string in double quotes */
	T_ERROR,		/* error parsing string in double quotes */
	T_WORD,			/* anything else containing no whitespace */
	T_EOF,			/* end of string reached */
};

/*
 * See typedef ng_parse_t for definition
 */
extern int	ng_parse(const struct ng_parse_type *type, const char *string,
			int *off, u_char *buf, int *buflen);

/*
 * See typedef ng_unparse_t for definition (*off assumed to be zero).
 */
extern int	ng_unparse(const struct ng_parse_type *type,
			const u_char *data, char *buf, int buflen);

/*
 * See typedef ng_getDefault_t for definition
 */
extern int	ng_parse_getDefault(const struct ng_parse_type *type,
			u_char *buf, int *buflen);

/*
 * Parse a token: '*startp' is the offset to start looking.  Upon
 * successful return, '*startp' equals the beginning of the token
 * and '*lenp' the length.  If error, '*startp' points at the
 * offending character(s).
 */
extern enum	ng_parse_token ng_parse_get_token(const char *s,
			int *startp, int *lenp);

/*
 * Like above, but specifically for getting a string token and returning
 * the string value.  The string token must be enclosed in double quotes
 * and the normal C backslash escapes are recognized.  The caller must
 * eventually free() the returned result.  Returns NULL if token is
 * not a string token, or parse or other error.
 */
extern char	*ng_get_string_token(const char *s, int *startp, int *lenp);

/*
 * Convert a raw string into a doubly-quoted string including any
 * necessary backslash escapes.  Caller must free the result.
 * Returns NULL if ENOMEM.
 */
extern char	*ng_encode_string(const char *s);

#endif /* _NETGRAPH_PARSE_H_ */

