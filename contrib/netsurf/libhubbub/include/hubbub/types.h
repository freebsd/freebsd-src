/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_types_h_
#define hubbub_types_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <inttypes.h>

/** Source of charset information, in order of importance
 * A client-dictated charset will override all others.
 * A document-specified charset will override autodetection or the default */
typedef enum hubbub_charset_source {
	HUBBUB_CHARSET_UNKNOWN		= 0,	/**< Unknown */
	HUBBUB_CHARSET_TENTATIVE	= 1,	/**< Charset may be changed
						 * with further data */
	HUBBUB_CHARSET_CONFIDENT	= 2	/**< Charset definite */
} hubbub_charset_source;

/**
 * Content model flag
 */
typedef enum hubbub_content_model {
	HUBBUB_CONTENT_MODEL_PCDATA,
	HUBBUB_CONTENT_MODEL_RCDATA,
	HUBBUB_CONTENT_MODEL_CDATA,
	HUBBUB_CONTENT_MODEL_PLAINTEXT
} hubbub_content_model;

/**
 * Quirks mode flag
 */
typedef enum hubbub_quirks_mode {
	HUBBUB_QUIRKS_MODE_NONE,
	HUBBUB_QUIRKS_MODE_LIMITED,
	HUBBUB_QUIRKS_MODE_FULL
} hubbub_quirks_mode;

/**
 * Type of an emitted token
 */
typedef enum hubbub_token_type {
	HUBBUB_TOKEN_DOCTYPE,
	HUBBUB_TOKEN_START_TAG,
	HUBBUB_TOKEN_END_TAG,
	HUBBUB_TOKEN_COMMENT,
	HUBBUB_TOKEN_CHARACTER,
	HUBBUB_TOKEN_EOF
} hubbub_token_type;

/**
 * Possible namespaces
 */
typedef enum hubbub_ns {
	HUBBUB_NS_NULL,
	HUBBUB_NS_HTML,
	HUBBUB_NS_MATHML,
	HUBBUB_NS_SVG,
	HUBBUB_NS_XLINK,
	HUBBUB_NS_XML,
	HUBBUB_NS_XMLNS
} hubbub_ns;

/**
 * Tokeniser string type
 */
typedef struct hubbub_string {
	const uint8_t *ptr;		/**< Pointer to data */
	size_t len;			/**< Byte length of string */
} hubbub_string;

/**
 * Tag attribute data
 */
typedef struct hubbub_attribute {
	hubbub_ns ns;			/**< Attribute namespace */
	hubbub_string name;		/**< Attribute name */
	hubbub_string value;		/**< Attribute value */
} hubbub_attribute;

/**
 * Data for doctype token
 */
typedef struct hubbub_doctype {
	hubbub_string name;		/**< Doctype name */

	bool public_missing;		/**< Whether the public id is missing */
	hubbub_string public_id;	/**< Doctype public identifier */

	bool system_missing;		/**< Whether the system id is missing */
	hubbub_string system_id;	/**< Doctype system identifier */

	bool force_quirks;		/**< Doctype force-quirks flag */
} hubbub_doctype;

/**
 * Data for a tag
 */
typedef struct hubbub_tag {
	hubbub_ns ns;			/**< Tag namespace */
	hubbub_string name;		/**< Tag name */
	uint32_t n_attributes;		/**< Count of attributes */
	hubbub_attribute *attributes;	/**< Array of attribute data */
	bool self_closing;		/**< Whether the tag can have children */
} hubbub_tag;

/**
 * Token data
 */
typedef struct hubbub_token {
	hubbub_token_type type;		/**< The token type */

	union {
		hubbub_doctype doctype;

		hubbub_tag tag;

		hubbub_string comment;

		hubbub_string character;
	} data;				/**< Type-specific data */
} hubbub_token;

#ifdef __cplusplus
}
#endif

#endif
