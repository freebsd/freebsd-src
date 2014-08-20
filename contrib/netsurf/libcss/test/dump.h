#ifndef test_dump_h_
#define test_dump_h_

#include <string.h>

#include <libcss/types.h>

#include "stylesheet.h"
#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/font_face.h"

#include "testutils.h"

static void dump_sheet(css_stylesheet *sheet, char *buf, size_t *len);
static void dump_rule_selector(css_rule_selector *s, char **buf, 
		size_t *buflen, uint32_t depth);
static void dump_rule_charset(css_rule_charset *s, char **buf, size_t *buflen);
static void dump_rule_import(css_rule_import *s, char **buf, size_t *buflen);
static void dump_rule_media(css_rule_media *s, char **buf, size_t *buflen);
static void dump_rule_page(css_rule_page *s, char **buf, size_t *buflen);
static void dump_rule_font_face(css_rule_font_face *s, 
		char **buf, size_t *buflen);
static void dump_selector_list(css_selector *list, char **ptr);
static void dump_selector(css_selector *selector, char **ptr);
static void dump_selector_detail(css_selector_detail *detail, char **ptr);
static void dump_bytecode(css_style *style, char **ptr, uint32_t depth);
static void dump_string(lwc_string *string, char **ptr);
static void dump_font_face(css_font_face *font_face, char**ptr);

void dump_sheet(css_stylesheet *sheet, char *buf, size_t *buflen)
{
	css_rule *rule;

	for (rule = sheet->rule_list; rule != NULL; rule = rule->next) {
		switch (rule->type) {
		case CSS_RULE_SELECTOR:
			dump_rule_selector((css_rule_selector *) rule, 
				&buf, buflen, 1);
			break;
		case CSS_RULE_CHARSET:
			dump_rule_charset((css_rule_charset *) rule, 
				&buf, buflen);
			break;
		case CSS_RULE_IMPORT:
			dump_rule_import((css_rule_import *) rule, 
				&buf, buflen);
			break;
		case CSS_RULE_MEDIA:
			dump_rule_media((css_rule_media *) rule,
				&buf, buflen);
			break;
		case CSS_RULE_PAGE:
			dump_rule_page((css_rule_page *) rule,
				&buf, buflen);
			break;
		case CSS_RULE_FONT_FACE:
			dump_rule_font_face((css_rule_font_face *) rule,
				&buf, buflen);
			break;
		default:
		{
			int written = sprintf(buf, "Unhandled rule type %d\n",
				rule->type);

			*buflen -= written;
			buf += written;
		}
			break;
		}
	}
}

void dump_rule_selector(css_rule_selector *s, char **buf, size_t *buflen, 
		uint32_t depth)
{
	uint32_t i;
	char *ptr = *buf;

	*ptr++ = '|';
	for (i = 0; i < depth; i++)
		*ptr++ = ' ';

	/* Build selector string */
	for (i = 0; i < s->base.items; i++) {
		dump_selector_list(s->selectors[i], &ptr);
		if (i != (uint32_t) (s->base.items - 1)) {
			memcpy(ptr, ", ", 2);
			ptr += 2;
		}
	}
	*ptr++ = '\n';

	if (s->style != NULL)
		dump_bytecode(s->style, &ptr, depth + 1);

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_rule_charset(css_rule_charset *s, char **buf, size_t *buflen)
{
	char *ptr = *buf;

	ptr += sprintf(ptr, "| @charset(");
	dump_string(s->encoding, &ptr);
	*ptr++ = ')';
	*ptr++ = '\n';

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_rule_import(css_rule_import *s, char **buf, size_t *buflen)
{
	char *ptr = *buf;

	ptr += sprintf(ptr, "| @import url(\"%.*s\")", 
                       (int) lwc_string_length(s->url), lwc_string_data(s->url));

	/** \todo media list */

	*ptr++ = '\n';

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_rule_media(css_rule_media *s, char **buf, size_t *buflen)
{
	char *ptr = *buf;
	css_rule *rule;

	ptr += sprintf(ptr, "| @media ");

	/* \todo media list */

	*ptr++ = '\n';

	for (rule = s->first_child; rule != NULL; rule = rule->next) {
		size_t len = *buflen - (ptr - *buf);

		dump_rule_selector((css_rule_selector *) rule, &ptr, &len, 2);
	}

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_rule_page(css_rule_page *s, char **buf, size_t *buflen)
{
	char *ptr = *buf;

	ptr += sprintf(ptr, "| @page ");

	if (s->selector != NULL)
		dump_selector_list(s->selector, &ptr);

	*ptr++ = '\n';

	if (s->style != NULL)
		dump_bytecode(s->style, &ptr, 2);

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_rule_font_face(css_rule_font_face *s, char **buf, size_t *buflen)
{
	char *ptr = *buf;

	ptr += sprintf(ptr, "| @font-face ");

	if (s->font_face != NULL) {
		dump_font_face(s->font_face, &ptr);
	}
	
	*ptr++ = '\n';

	*buflen -= ptr - *buf;
	*buf = ptr;
}

void dump_selector_list(css_selector *list, char **ptr)
{
	if (list->combinator != NULL) {
		dump_selector_list(list->combinator, ptr);
	}

	switch (list->data.comb) {
	case CSS_COMBINATOR_NONE:
		break;
	case CSS_COMBINATOR_ANCESTOR:
		(*ptr)[0] = ' ';
		*ptr += 1;
		break;
	case CSS_COMBINATOR_PARENT:
		memcpy(*ptr, " > ", 3);
		*ptr += 3;
		break;
	case CSS_COMBINATOR_SIBLING:
		memcpy(*ptr, " + ", 3);
		*ptr += 3;
		break;
	case CSS_COMBINATOR_GENERIC_SIBLING:
		memcpy(*ptr, " + ", 3);
		*ptr += 3;
		break;
	}

	dump_selector(list, ptr);
}


void dump_selector(css_selector *selector, char **ptr)
{
	css_selector_detail *d = &selector->data;

	while (true) {
		dump_selector_detail(d, ptr);

		if (d->next == 0)
			break;

		d++;
	}
}

void dump_selector_detail(css_selector_detail *detail, char **ptr)
{
	if (detail->negate)
		*ptr += sprintf(*ptr, ":not(");

	switch (detail->type) {
	case CSS_SELECTOR_ELEMENT:
                if (lwc_string_length(detail->qname.name) == 1 && 
                    lwc_string_data(detail->qname.name)[0] == '*' &&
				detail->next == 0) {
			dump_string(detail->qname.name, ptr);
		} else if (lwc_string_length(detail->qname.name) != 1 ||
                           lwc_string_data(detail->qname.name)[0] != '*') {
			dump_string(detail->qname.name, ptr);
		}
		break;
	case CSS_SELECTOR_CLASS:
		**ptr = '.';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		break;
	case CSS_SELECTOR_ID:
		**ptr = '#';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		break;
	case CSS_SELECTOR_PSEUDO_CLASS:
	case CSS_SELECTOR_PSEUDO_ELEMENT:
		**ptr = ':';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		if (detail->value_type == CSS_SELECTOR_DETAIL_VALUE_STRING) {
			if (detail->value.string != NULL) {
				**ptr = '(';
				*ptr += 1;
				dump_string(detail->value.string, ptr);
				**ptr = ')';
				*ptr += 1;
			}
		} else {
			*ptr += sprintf(*ptr, "(%dn+%d)",
					detail->value.nth.a,
					detail->value.nth.b);
		}
		break;
	case CSS_SELECTOR_ATTRIBUTE:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		**ptr = ']';
		*ptr += 1;
		break;
	case CSS_SELECTOR_ATTRIBUTE_EQUAL:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '=';
		(*ptr)[1] = '"';
		*ptr += 2;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	case CSS_SELECTOR_ATTRIBUTE_DASHMATCH:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '|';
		(*ptr)[1] = '=';
		(*ptr)[2] = '"';
		*ptr += 3;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	case CSS_SELECTOR_ATTRIBUTE_INCLUDES:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '~';
		(*ptr)[1] = '=';
		(*ptr)[2] = '"';
		*ptr += 3;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	case CSS_SELECTOR_ATTRIBUTE_PREFIX:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '^';
		(*ptr)[1] = '=';
		(*ptr)[2] = '"';
		*ptr += 3;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	case CSS_SELECTOR_ATTRIBUTE_SUFFIX:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '$';
		(*ptr)[1] = '=';
		(*ptr)[2] = '"';
		*ptr += 3;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	case CSS_SELECTOR_ATTRIBUTE_SUBSTRING:
		**ptr = '[';
		*ptr += 1;
		dump_string(detail->qname.name, ptr);
		(*ptr)[0] = '*';
		(*ptr)[1] = '=';
		(*ptr)[2] = '"';
		*ptr += 3;
		dump_string(detail->value.string, ptr);
		(*ptr)[0] = '"';
		(*ptr)[1] = ']';
		*ptr += 2;
		break;
	}

	if (detail->negate)
		*ptr += sprintf(*ptr, ")");
}

/**
 * Opcode names, indexed by opcode
 */
static const char *opcode_names[] = {
	"azimuth",
	"background-attachment",
	"background-color",
	"background-image",
	"background-position",
	"background-repeat",
	"border-collapse",
	"border-spacing",
	"border-top-color",
	"border-right-color",
	"border-bottom-color",
	"border-left-color",
	"border-top-style",
	"border-right-style",
	"border-bottom-style",
	"border-left-style",
	"border-top-width",
	"border-right-width",
	"border-bottom-width",
	"border-left-width",
	"bottom",
	"caption-side",
	"clear",
	"clip",
	"color",
	"content",
	"counter-increment",
	"counter-reset",
	"cue-after",
	"cue-before",
	"cursor",
	"direction",
	"display",
	"elevation",
	"empty-cells",
	"float",
	"font-family",
	"font-size",
	"font-style",
	"font-variant",
	"font-weight",
	"height",
	"left",
	"letter-spacing",
	"line-height",
	"list-style-image",
	"list-style-position",
	"list-style-type",
	"margin-top",
	"margin-right",
	"margin-bottom",
	"margin-left",
	"max-height",
	"max-width",
	"min-height",
	"min-width",
	"orphans",
	"outline-color",
	"outline-style",
	"outline-width",
	"overflow",
	"padding-top",
	"padding-right",
	"padding-bottom",
	"padding-left",
	"page-break-after",
	"page-break-before",
	"page-break-inside",
	"pause-after",
	"pause-before",
	"pitch-range",
	"pitch",
	"play-during",
	"position",
	"quotes",
	"richness",
	"right",
	"speak-header",
	"speak-numeral",
	"speak-punctuation",
	"speak",
	"speech-rate",
	"stress",
	"table-layout",
	"text-align",
	"text-decoration",
	"text-indent",
	"text-transform",
	"top",
	"unicode-bidi",
	"vertical-align",
	"visibility",
	"voice-family",
	"volume",
	"white-space",
	"widows",
	"width",
	"word-spacing",
	"z-index",
	"opacity",
	"break-after",
	"break-before",
	"break-inside",
	"column-count",
	"column-fill",
	"column-gap",
	"column-rule-color",
	"column-rule-style",
	"column-rule-width",
	"column-span",
	"column-width",
	"writing-mode",
};

static void dump_css_fixed(css_fixed f, char **ptr)
{
#define ABS(x) (uint32_t)((x) < 0 ? -(x) : (x))
	uint32_t uintpart = FIXTOINT(ABS(f));
	/* + 500 to ensure round to nearest (division will truncate) */
	uint32_t fracpart = ((ABS(f) & 0x3ff) * 1000 + 500) / (1 << 10);
#undef ABS
	size_t flen = 0;
	char tmp[20];
	size_t tlen = 0;
	char *buf = *ptr;

	if (f < 0) {
		buf[0] = '-';
		buf++;
	}

	do {
		tmp[tlen] = "0123456789"[uintpart % 10];
		tlen++;

		uintpart /= 10;
	} while (tlen < 20 && uintpart != 0);

	while (tlen > 0) {
		buf[0] = tmp[--tlen];
		buf++;
	}

	buf[0] = '.';
	buf++;

	do {
		tmp[tlen] = "0123456789"[fracpart % 10];
		tlen++;

		fracpart /= 10;
	} while (tlen < 20 && fracpart != 0);

	while (tlen > 0) {
		buf[0] = tmp[--tlen];
		buf++;
		flen++;
	}

	while (flen < 3) {
		buf[0] = '0';
		buf++;
		flen++;
	}

	buf[0] = '\0';

	*ptr = buf;
}

static void dump_number(css_fixed val, char **ptr)
{
	if (INTTOFIX(FIXTOINT(val)) == val)
		*ptr += sprintf(*ptr, "%d", FIXTOINT(val));
	else
		dump_css_fixed(val, ptr);
}

static void dump_unit(css_fixed val, uint32_t unit, char **ptr)
{
	dump_number(val, ptr);

	switch (unit) {
	case UNIT_PX:
		*ptr += sprintf(*ptr, "px");
		break;
	case UNIT_EX:
		*ptr += sprintf(*ptr, "ex");
		break;
	case UNIT_EM:
		*ptr += sprintf(*ptr, "em");
		break;
	case UNIT_IN:
		*ptr += sprintf(*ptr, "in");
		break;
	case UNIT_CM:
		*ptr += sprintf(*ptr, "cm");
		break;
	case UNIT_MM:
		*ptr += sprintf(*ptr, "mm");
		break;
	case UNIT_PT:
		*ptr += sprintf(*ptr, "pt");
		break;
	case UNIT_PC:
		*ptr += sprintf(*ptr, "pc");
		break;
	case UNIT_PCT:
		*ptr += sprintf(*ptr, "%%");
		break;
	case UNIT_DEG:
		*ptr += sprintf(*ptr, "deg");
		break;
	case UNIT_GRAD:
		*ptr += sprintf(*ptr, "grad");
		break;
	case UNIT_RAD:
		*ptr += sprintf(*ptr, "rad");
		break;
	case UNIT_MS:
		*ptr += sprintf(*ptr, "ms");
		break;
	case UNIT_S:
		*ptr += sprintf(*ptr, "s");
		break;
	case UNIT_HZ:
		*ptr += sprintf(*ptr, "Hz");
		break;
	case UNIT_KHZ:
		*ptr += sprintf(*ptr, "kHz");
		break;
	}
}

static void dump_counter(lwc_string *name, uint32_t value,
		char **ptr)
{
	*ptr += sprintf(*ptr, "counter(%.*s",
                        (int) lwc_string_length(name), lwc_string_data(name));

	value >>= CONTENT_COUNTER_STYLE_SHIFT;

	switch (value) {
	case LIST_STYLE_TYPE_DISC:
		*ptr += sprintf(*ptr, ", disc");
		break;
	case LIST_STYLE_TYPE_CIRCLE:
		*ptr += sprintf(*ptr, ", circle");
		break;
	case LIST_STYLE_TYPE_SQUARE:
		*ptr += sprintf(*ptr, ", square");
		break;
	case LIST_STYLE_TYPE_DECIMAL:
		break;
	case LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		*ptr += sprintf(*ptr, ", decimal-leading-zero");
		break;
	case LIST_STYLE_TYPE_LOWER_ROMAN:
		*ptr += sprintf(*ptr, ", lower-roman");
		break;
	case LIST_STYLE_TYPE_UPPER_ROMAN:
		*ptr += sprintf(*ptr, ", upper-roman");
		break;
	case LIST_STYLE_TYPE_LOWER_GREEK:
		*ptr += sprintf(*ptr, ", lower-greek");
		break;
	case LIST_STYLE_TYPE_LOWER_LATIN:
		*ptr += sprintf(*ptr, ", lower-latin");
		break;
	case LIST_STYLE_TYPE_UPPER_LATIN:
		*ptr += sprintf(*ptr, ", upper-latin");
		break;
	case LIST_STYLE_TYPE_ARMENIAN:
		*ptr += sprintf(*ptr, ", armenian");
		break;
	case LIST_STYLE_TYPE_GEORGIAN:
		*ptr += sprintf(*ptr, ", georgian");
		break;
	case LIST_STYLE_TYPE_LOWER_ALPHA:
		*ptr += sprintf(*ptr, ", lower-alpha");
		break;
	case LIST_STYLE_TYPE_UPPER_ALPHA:
		*ptr += sprintf(*ptr, ", upper-alpha");
		break;
	case LIST_STYLE_TYPE_NONE:
		*ptr += sprintf(*ptr, ", none");
		break;
	}

	*ptr += sprintf(*ptr, ")");
}

static void dump_counters(lwc_string *name, lwc_string *separator,
		uint32_t value, char **ptr)
{
	*ptr += sprintf(*ptr, "counter(%.*s, %.*s", 
			(int) lwc_string_length(name),
                        lwc_string_data(name),
			(int) lwc_string_length(separator),
                        lwc_string_data(separator));

	value >>= CONTENT_COUNTER_STYLE_SHIFT;

	switch (value) {
	case LIST_STYLE_TYPE_DISC:
		*ptr += sprintf(*ptr, ", disc");
		break;
	case LIST_STYLE_TYPE_CIRCLE:
		*ptr += sprintf(*ptr, ", circle");
		break;
	case LIST_STYLE_TYPE_SQUARE:
		*ptr += sprintf(*ptr, ", square");
		break;
	case LIST_STYLE_TYPE_DECIMAL:
		break;
	case LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		*ptr += sprintf(*ptr, ", decimal-leading-zero");
		break;
	case LIST_STYLE_TYPE_LOWER_ROMAN:
		*ptr += sprintf(*ptr, ", lower-roman");
		break;
	case LIST_STYLE_TYPE_UPPER_ROMAN:
		*ptr += sprintf(*ptr, ", upper-roman");
		break;
	case LIST_STYLE_TYPE_LOWER_GREEK:
		*ptr += sprintf(*ptr, ", lower-greek");
		break;
	case LIST_STYLE_TYPE_LOWER_LATIN:
		*ptr += sprintf(*ptr, ", lower-latin");
		break;
	case LIST_STYLE_TYPE_UPPER_LATIN:
		*ptr += sprintf(*ptr, ", upper-latin");
		break;
	case LIST_STYLE_TYPE_ARMENIAN:
		*ptr += sprintf(*ptr, ", armenian");
		break;
	case LIST_STYLE_TYPE_GEORGIAN:
		*ptr += sprintf(*ptr, ", georgian");
		break;
	case LIST_STYLE_TYPE_LOWER_ALPHA:
		*ptr += sprintf(*ptr, ", lower-alpha");
		break;
	case LIST_STYLE_TYPE_UPPER_ALPHA:
		*ptr += sprintf(*ptr, ", upper-alpha");
		break;
	case LIST_STYLE_TYPE_NONE:
		*ptr += sprintf(*ptr, ", none");
		break;
	}

	*ptr += sprintf(*ptr, ")");
}

void dump_bytecode(css_style *style, char **ptr, uint32_t depth)
{
	void *bytecode = style->bytecode;
	size_t length = (style->used * sizeof(css_code_t));
	uint32_t offset = 0;

#define ADVANCE(n) do {					\
	offset += (n);					\
	bytecode = ((uint8_t *) bytecode) + (n);	\
} while(0)

	while (offset < length) {
		opcode_t op;
		uint32_t value;
		uint32_t opv = *((uint32_t *) bytecode);

		ADVANCE(sizeof(opv));

		op = getOpcode(opv);

		*((*ptr)++) = '|';
		for (uint32_t i = 0; i < depth; i++)
			*((*ptr)++) = ' ';
		*ptr += sprintf(*ptr, "%s: ", opcode_names[op]);

		if (isInherit(opv)) {
			*ptr += sprintf(*ptr, "inherit");
		} else { 
			value = getValue(opv);

			switch (op) {
			case CSS_PROP_AZIMUTH:
				switch (value & ~AZIMUTH_BEHIND) {
				case AZIMUTH_ANGLE:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case AZIMUTH_LEFTWARDS:
					*ptr += sprintf(*ptr, "leftwards");
					break;
				case AZIMUTH_RIGHTWARDS:
					*ptr += sprintf(*ptr, "rightwards");
					break;
				case AZIMUTH_LEFT_SIDE:
					*ptr += sprintf(*ptr, "left-side");
					break;
				case AZIMUTH_FAR_LEFT:
					*ptr += sprintf(*ptr, "far-left");
					break;
				case AZIMUTH_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case AZIMUTH_CENTER_LEFT:
					*ptr += sprintf(*ptr, "center-left");
					break;
				case AZIMUTH_CENTER:
					*ptr += sprintf(*ptr, "center");
					break;
				case AZIMUTH_CENTER_RIGHT:
					*ptr += sprintf(*ptr, "center-right");
					break;
				case AZIMUTH_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case AZIMUTH_FAR_RIGHT:
					*ptr += sprintf(*ptr, "far-right");
					break;
				case AZIMUTH_RIGHT_SIDE:
					*ptr += sprintf(*ptr, "right-side");
					break;
				}
				if (value & AZIMUTH_BEHIND)
					*ptr += sprintf(*ptr, " behind");
				break;
			case CSS_PROP_BACKGROUND_ATTACHMENT:
				switch (value) {
				case BACKGROUND_ATTACHMENT_FIXED:
					*ptr += sprintf(*ptr, "fixed");
					break;
				case BACKGROUND_ATTACHMENT_SCROLL:
					*ptr += sprintf(*ptr, "scroll");
					break;
				}
				break;
			case CSS_PROP_BORDER_TOP_COLOR:
			case CSS_PROP_BORDER_RIGHT_COLOR:
			case CSS_PROP_BORDER_BOTTOM_COLOR:
			case CSS_PROP_BORDER_LEFT_COLOR:
			case CSS_PROP_BACKGROUND_COLOR:
			case CSS_PROP_COLUMN_RULE_COLOR:
				assert(BACKGROUND_COLOR_TRANSPARENT ==
						(enum op_background_color)
						BORDER_COLOR_TRANSPARENT);
				assert(BACKGROUND_COLOR_CURRENT_COLOR ==
						(enum op_background_color)
						BORDER_COLOR_CURRENT_COLOR);
				assert(BACKGROUND_COLOR_SET ==
						(enum op_background_color)
						BORDER_COLOR_SET);

				switch (value) {
				case BACKGROUND_COLOR_TRANSPARENT:
					*ptr += sprintf(*ptr, "transparent");
					break;
				case BACKGROUND_COLOR_CURRENT_COLOR:
					*ptr += sprintf(*ptr, "currentColor");
					break;
				case BACKGROUND_COLOR_SET:
				{
					uint32_t colour = 
						*((uint32_t *) bytecode);
					ADVANCE(sizeof(colour));
					*ptr += sprintf(*ptr, "#%08x", colour);
				}	
					break;
				}
				break;
			case CSS_PROP_BACKGROUND_IMAGE:
			case CSS_PROP_CUE_AFTER:
			case CSS_PROP_CUE_BEFORE:
			case CSS_PROP_LIST_STYLE_IMAGE:
				assert(BACKGROUND_IMAGE_NONE ==
						(enum op_background_image)
						CUE_AFTER_NONE);
				assert(BACKGROUND_IMAGE_URI ==
						(enum op_background_image)
						CUE_AFTER_URI);
				assert(BACKGROUND_IMAGE_NONE ==
						(enum op_background_image)
						CUE_BEFORE_NONE);
				assert(BACKGROUND_IMAGE_URI ==
						(enum op_background_image)
						CUE_BEFORE_URI);
				assert(BACKGROUND_IMAGE_NONE ==
						(enum op_background_image)
						LIST_STYLE_IMAGE_NONE);
				assert(BACKGROUND_IMAGE_URI ==
						(enum op_background_image)
						LIST_STYLE_IMAGE_URI);

				switch (value) {
				case BACKGROUND_IMAGE_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				case BACKGROUND_IMAGE_URI:
				{
					uint32_t snum = *((uint32_t *) bytecode);
					lwc_string *he;
					css__stylesheet_string_get(style->sheet, 
								  snum, 
								  &he);
					ADVANCE(sizeof(snum));
					*ptr += sprintf(*ptr, "url('%.*s')", 
							(int) lwc_string_length(he), 
							lwc_string_data(he));
				}
					break;
				}
				break;
			case CSS_PROP_BACKGROUND_POSITION:
				switch (value & 0xf0) {
				case BACKGROUND_POSITION_HORZ_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case BACKGROUND_POSITION_HORZ_CENTER:
					*ptr += sprintf(*ptr, "center");
					break;
				case BACKGROUND_POSITION_HORZ_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case BACKGROUND_POSITION_HORZ_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				}
				*ptr += sprintf(*ptr, " ");
				switch (value & 0x0f) {
				case BACKGROUND_POSITION_VERT_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case BACKGROUND_POSITION_VERT_CENTER:
					*ptr += sprintf(*ptr, "center");
					break;
				case BACKGROUND_POSITION_VERT_BOTTOM:
					*ptr += sprintf(*ptr, "bottom");
					break;
				case BACKGROUND_POSITION_VERT_TOP:
					*ptr += sprintf(*ptr, "top");
					break;
				}
				break;
			case CSS_PROP_BACKGROUND_REPEAT:
				switch (value) {
				case BACKGROUND_REPEAT_NO_REPEAT:
					*ptr += sprintf(*ptr, "no-repeat");
					break;
				case BACKGROUND_REPEAT_REPEAT_X:
					*ptr += sprintf(*ptr, "repeat-x");
					break;
				case BACKGROUND_REPEAT_REPEAT_Y:
					*ptr += sprintf(*ptr, "repeat-y");
					break;
				case BACKGROUND_REPEAT_REPEAT:
					*ptr += sprintf(*ptr, "repeat");
					break;
				}
				break;
			case CSS_PROP_BORDER_COLLAPSE:
				switch (value) {
				case BORDER_COLLAPSE_SEPARATE:
					*ptr += sprintf(*ptr, "separate");
					break;
				case BORDER_COLLAPSE_COLLAPSE:
					*ptr += sprintf(*ptr, "collapse");
					break;
				}
				break;
			case CSS_PROP_BORDER_SPACING:
				switch (value) {
				case BORDER_SPACING_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);

					val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				}
				break;
			case CSS_PROP_BORDER_TOP_STYLE:
			case CSS_PROP_BORDER_RIGHT_STYLE:
			case CSS_PROP_BORDER_BOTTOM_STYLE:
			case CSS_PROP_BORDER_LEFT_STYLE:
			case CSS_PROP_COLUMN_RULE_STYLE:
			case CSS_PROP_OUTLINE_STYLE:
				assert(BORDER_STYLE_NONE ==
						(enum op_border_style)
						OUTLINE_STYLE_NONE);
				assert(BORDER_STYLE_NONE ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_NONE);
				assert(BORDER_STYLE_HIDDEN ==
						(enum op_border_style)
						OUTLINE_STYLE_HIDDEN);
				assert(BORDER_STYLE_HIDDEN ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_HIDDEN);
				assert(BORDER_STYLE_DOTTED ==
						(enum op_border_style)
						OUTLINE_STYLE_DOTTED);
				assert(BORDER_STYLE_DOTTED ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_DOTTED);
				assert(BORDER_STYLE_DASHED ==
						(enum op_border_style)
						OUTLINE_STYLE_DASHED);
				assert(BORDER_STYLE_DASHED ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_DASHED);
				assert(BORDER_STYLE_SOLID ==
						(enum op_border_style)
						OUTLINE_STYLE_SOLID);
				assert(BORDER_STYLE_SOLID ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_SOLID);
				assert(BORDER_STYLE_DOUBLE ==
						(enum op_border_style)
						OUTLINE_STYLE_DOUBLE);
				assert(BORDER_STYLE_DOUBLE ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_DOUBLE);
				assert(BORDER_STYLE_GROOVE ==
						(enum op_border_style)
						OUTLINE_STYLE_GROOVE);
				assert(BORDER_STYLE_GROOVE ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_GROOVE);
				assert(BORDER_STYLE_RIDGE ==
						(enum op_border_style)
						OUTLINE_STYLE_RIDGE);
				assert(BORDER_STYLE_RIDGE ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_RIDGE);
				assert(BORDER_STYLE_INSET ==
						(enum op_border_style)
						OUTLINE_STYLE_INSET);
				assert(BORDER_STYLE_INSET ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_INSET);
				assert(BORDER_STYLE_OUTSET ==
						(enum op_border_style)
						OUTLINE_STYLE_OUTSET);
				assert(BORDER_STYLE_OUTSET ==
						(enum op_border_style)
						COLUMN_RULE_STYLE_OUTSET);

				switch (value) {
				case BORDER_STYLE_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				case BORDER_STYLE_HIDDEN:
					*ptr += sprintf(*ptr, "hidden");
					break;
				case BORDER_STYLE_DOTTED:
					*ptr += sprintf(*ptr, "dotted");
					break;
				case BORDER_STYLE_DASHED:
					*ptr += sprintf(*ptr, "dashed");
					break;
				case BORDER_STYLE_SOLID:
					*ptr += sprintf(*ptr, "solid");
					break;
				case BORDER_STYLE_DOUBLE:
					*ptr += sprintf(*ptr, "double");
					break;
				case BORDER_STYLE_GROOVE:
					*ptr += sprintf(*ptr, "groove");
					break;
				case BORDER_STYLE_RIDGE:
					*ptr += sprintf(*ptr, "ridge");
					break;
				case BORDER_STYLE_INSET:
					*ptr += sprintf(*ptr, "inset");
					break;
				case BORDER_STYLE_OUTSET:
					*ptr += sprintf(*ptr, "outset");
					break;
				}
				break;
			case CSS_PROP_BORDER_TOP_WIDTH:
			case CSS_PROP_BORDER_RIGHT_WIDTH:
			case CSS_PROP_BORDER_BOTTOM_WIDTH:
			case CSS_PROP_BORDER_LEFT_WIDTH:
			case CSS_PROP_COLUMN_RULE_WIDTH:
			case CSS_PROP_OUTLINE_WIDTH:
				assert(BORDER_WIDTH_SET ==
						(enum op_border_width)
						OUTLINE_WIDTH_SET);
				assert(BORDER_WIDTH_THIN ==
						(enum op_border_width)
						OUTLINE_WIDTH_THIN);
				assert(BORDER_WIDTH_MEDIUM ==
						(enum op_border_width)
						OUTLINE_WIDTH_MEDIUM);
				assert(BORDER_WIDTH_THICK ==
						(enum op_border_width)
						OUTLINE_WIDTH_THICK);

				switch (value) {
				case BORDER_WIDTH_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case BORDER_WIDTH_THIN:
					*ptr += sprintf(*ptr, "thin");
					break;
				case BORDER_WIDTH_MEDIUM:
					*ptr += sprintf(*ptr, "medium");
					break;
				case BORDER_WIDTH_THICK:
					*ptr += sprintf(*ptr, "thick");
					break;
				}
				break;
			case CSS_PROP_MARGIN_TOP:
			case CSS_PROP_MARGIN_RIGHT:
			case CSS_PROP_MARGIN_BOTTOM:
			case CSS_PROP_MARGIN_LEFT:
			case CSS_PROP_BOTTOM:
			case CSS_PROP_LEFT:
			case CSS_PROP_RIGHT:
			case CSS_PROP_TOP:
			case CSS_PROP_HEIGHT:
			case CSS_PROP_WIDTH:
			case CSS_PROP_COLUMN_WIDTH:
				assert(BOTTOM_SET ==
						(enum op_bottom) LEFT_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) LEFT_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom) RIGHT_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) RIGHT_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom) TOP_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) TOP_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom) HEIGHT_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) HEIGHT_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom) MARGIN_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) MARGIN_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom) WIDTH_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom) WIDTH_AUTO);
				assert(BOTTOM_SET ==
						(enum op_bottom)
							COLUMN_WIDTH_SET);
				assert(BOTTOM_AUTO ==
						(enum op_bottom)
							COLUMN_WIDTH_AUTO);

				switch (value) {
				case BOTTOM_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case BOTTOM_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				}
				break;
			case CSS_PROP_BREAK_AFTER:
			case CSS_PROP_BREAK_BEFORE:
				assert(BREAK_AFTER_AUTO ==
						(enum op_break_after)
						BREAK_BEFORE_AUTO);
				assert(BREAK_AFTER_ALWAYS ==
						(enum op_break_after)
						BREAK_BEFORE_ALWAYS);
				assert(BREAK_AFTER_AVOID ==
						(enum op_break_after)
						BREAK_BEFORE_AVOID);
				assert(BREAK_AFTER_LEFT ==
						(enum op_break_after)
						BREAK_BEFORE_LEFT);
				assert(BREAK_AFTER_RIGHT ==
						(enum op_break_after)
						BREAK_BEFORE_RIGHT);
				assert(BREAK_AFTER_PAGE ==
						(enum op_break_after)
						BREAK_BEFORE_PAGE);
				assert(BREAK_AFTER_COLUMN ==
						(enum op_break_after)
						BREAK_BEFORE_COLUMN);
				assert(BREAK_AFTER_AVOID_PAGE ==
						(enum op_break_after)
						BREAK_BEFORE_AVOID_PAGE);
				assert(BREAK_AFTER_AVOID_COLUMN ==
						(enum op_break_after)
						BREAK_BEFORE_AVOID_COLUMN);

				switch (value) {
				case BREAK_AFTER_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case BREAK_AFTER_ALWAYS:
					*ptr += sprintf(*ptr, "always");
					break;
				case BREAK_AFTER_AVOID:
					*ptr += sprintf(*ptr, "avoid");
					break;
				case BREAK_AFTER_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case BREAK_AFTER_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case BREAK_AFTER_PAGE:
					*ptr += sprintf(*ptr, "page");
					break;
				case BREAK_AFTER_COLUMN:
					*ptr += sprintf(*ptr, "column");
					break;
				case BREAK_AFTER_AVOID_PAGE:
					*ptr += sprintf(*ptr, "avoid-page");
					break;
				case BREAK_AFTER_AVOID_COLUMN:
					*ptr += sprintf(*ptr, "avoid-column");
					break;
				}
				break;
			case CSS_PROP_BREAK_INSIDE:
				switch (value) {
				case BREAK_INSIDE_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case BREAK_INSIDE_AVOID:
					*ptr += sprintf(*ptr, "avoid");
					break;
				case BREAK_INSIDE_AVOID_PAGE:
					*ptr += sprintf(*ptr, "avoid-page");
					break;
				case BREAK_INSIDE_AVOID_COLUMN:
					*ptr += sprintf(*ptr, "avoid-column");
					break;
				}
				break;
			case CSS_PROP_CAPTION_SIDE:
				switch (value) {
				case CAPTION_SIDE_TOP:
					*ptr += sprintf(*ptr, "top");
					break;
				case CAPTION_SIDE_BOTTOM:
					*ptr += sprintf(*ptr, "bottom");
					break;
				}
				break;
			case CSS_PROP_CLEAR:
				switch (value) {
				case CLEAR_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				case CLEAR_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case CLEAR_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case CLEAR_BOTH:
					*ptr += sprintf(*ptr, "both");
					break;
				}
				break;
			case CSS_PROP_CLIP:
				if ((value & CLIP_SHAPE_MASK) == 
						CLIP_SHAPE_RECT) {
					*ptr += sprintf(*ptr, "rect(");
					if (value & CLIP_RECT_TOP_AUTO) {
						*ptr += sprintf(*ptr, "auto");
					} else {
						uint32_t unit;
						css_fixed val = 
							*((css_fixed *) bytecode);
						ADVANCE(sizeof(val));
						unit = *((uint32_t *) bytecode);
						ADVANCE(sizeof(unit));
						dump_unit(val, unit, ptr);
					}
					*ptr += sprintf(*ptr, ", ");
					if (value & CLIP_RECT_RIGHT_AUTO) {
						*ptr += sprintf(*ptr, "auto");
					} else {
						uint32_t unit;
						css_fixed val = 
							*((css_fixed *) bytecode);
						ADVANCE(sizeof(val));
						unit = *((uint32_t *) bytecode);
						ADVANCE(sizeof(unit));
						dump_unit(val, unit, ptr);
					}
					*ptr += sprintf(*ptr, ", ");
					if (value & CLIP_RECT_BOTTOM_AUTO) {
						*ptr += sprintf(*ptr, "auto");
					} else {
						uint32_t unit;
						css_fixed val = 
							*((css_fixed *) bytecode);
						ADVANCE(sizeof(val));
						unit = *((uint32_t *) bytecode);
						ADVANCE(sizeof(unit));
						dump_unit(val, unit, ptr);
					}
					*ptr += sprintf(*ptr, ", ");
					if (value & CLIP_RECT_LEFT_AUTO) {
						*ptr += sprintf(*ptr, "auto");
					} else {
						uint32_t unit;
						css_fixed val = 
							*((css_fixed *) bytecode);
						ADVANCE(sizeof(val));
						unit = *((uint32_t *) bytecode);
						ADVANCE(sizeof(unit));
						dump_unit(val, unit, ptr);
					}
					*ptr += sprintf(*ptr, ")");
				} else
					*ptr += sprintf(*ptr, "auto");
				break;
			case CSS_PROP_COLOR:
				switch (value) {
				case COLOR_TRANSPARENT:
					*ptr += sprintf(*ptr, "transparent");
					break;
				case COLOR_CURRENT_COLOR:
					*ptr += sprintf(*ptr, "currentColor");
					break;
				case COLOR_SET:
				{
					uint32_t colour = 
						*((uint32_t *) bytecode);
					ADVANCE(sizeof(colour));
					*ptr += sprintf(*ptr, "#%08x", colour);
				}	
					break;
				}
				break;
			case CSS_PROP_COLUMN_COUNT:
				switch (value) {
				case COLUMN_COUNT_SET:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				case COLUMN_COUNT_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				}
				break;
			case CSS_PROP_COLUMN_FILL:
				switch (value) {
				case COLUMN_FILL_BALANCE:
					*ptr += sprintf(*ptr, "balance");
					break;
				case COLUMN_FILL_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				}
				break;
			case CSS_PROP_COLUMN_GAP:
				switch (value) {
				case COLUMN_GAP_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case COLUMN_GAP_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				}
				break;
			case CSS_PROP_COLUMN_SPAN:
				switch (value) {
				case COLUMN_SPAN_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				case COLUMN_SPAN_ALL:
					*ptr += sprintf(*ptr, "all");
					break;
				}
				break;
			case CSS_PROP_CONTENT:
				if (value == CONTENT_NORMAL) {
					*ptr += sprintf(*ptr, "normal");
					break;
				} else if (value == CONTENT_NONE) {
					*ptr += sprintf(*ptr, "none");
					break;
				}

				while (value != CONTENT_NORMAL) {
					uint32_t snum = *((uint32_t *) bytecode);					
					lwc_string *he;
					const char *end = "";

					switch (value & 0xff) {
					case CONTENT_COUNTER:
						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(snum));
						dump_counter(he, value, ptr);
						break;

					case CONTENT_COUNTERS:
					{
						lwc_string *sep;
						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(snum));

						sep = *((lwc_string **) bytecode);
						ADVANCE(sizeof(sep));

						dump_counters(he, sep, value,
							ptr);
					}
						break;

					case CONTENT_URI:
					case CONTENT_ATTR:	
					case CONTENT_STRING:
						css__stylesheet_string_get(style->sheet, snum, &he);
						if (value == CONTENT_URI) 
							*ptr += sprintf(*ptr, "url(");
						if (value == CONTENT_ATTR)
							*ptr += sprintf(*ptr, "attr(");
						if (value != CONTENT_STRING)
							end = ")";

						ADVANCE(sizeof(snum));

						*ptr += sprintf(*ptr, "'%.*s'%s",
                                                                (int) lwc_string_length(he),
                                                                lwc_string_data(he),
							end);
						break;

					case CONTENT_OPEN_QUOTE:
						*ptr += sprintf(*ptr, "open-quote");
						break;

					case CONTENT_CLOSE_QUOTE:
						*ptr += sprintf(*ptr, "close-quote");
						break;

					case CONTENT_NO_OPEN_QUOTE:
						*ptr += sprintf(*ptr, "no-open-quote");
						break;

					case CONTENT_NO_CLOSE_QUOTE:
						*ptr += sprintf(*ptr, "no-close-quote");
						break;
					}

					value = *((uint32_t *) bytecode);
					ADVANCE(sizeof(value));

					if (value != CONTENT_NORMAL)
						*ptr += sprintf(*ptr, " ");
				}
				break;
			case CSS_PROP_COUNTER_INCREMENT:
			case CSS_PROP_COUNTER_RESET:
				assert(COUNTER_INCREMENT_NONE ==
						(enum op_counter_increment)
						COUNTER_RESET_NONE);
				assert(COUNTER_INCREMENT_NAMED ==
						(enum op_counter_increment)
						COUNTER_RESET_NAMED);

				switch (value) {
				case COUNTER_INCREMENT_NAMED:
					while (value != COUNTER_INCREMENT_NONE) {
						css_fixed val;
					uint32_t snum = *((uint32_t *) bytecode);					lwc_string *he;
					css__stylesheet_string_get(style->sheet, 
								  snum, 
								  &he);
						ADVANCE(sizeof(snum));
						*ptr += sprintf(*ptr, "%.*s ", 
                                                                (int)lwc_string_length(he),
                                                                lwc_string_data(he));
						val = *((css_fixed *) bytecode);
						ADVANCE(sizeof(val));
						dump_number(val, ptr);

						value = *((uint32_t *) bytecode);
						ADVANCE(sizeof(value));

						if (value != 
							COUNTER_INCREMENT_NONE)
							*ptr += sprintf(*ptr, 
									" "); 
					}
					break;
				case COUNTER_INCREMENT_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_CURSOR:
				while (value == CURSOR_URI) {
					uint32_t snum = *((uint32_t *) bytecode);					lwc_string *he;
					css__stylesheet_string_get(style->sheet, 
								  snum, 
								  &he);
					ADVANCE(sizeof(snum));
					*ptr += sprintf(*ptr, "url('%.*s'), ", 
							(int) lwc_string_length(he),
							lwc_string_data(he));

					value = *((uint32_t *) bytecode);
					ADVANCE(sizeof(value));
				}

				switch (value) {
				case CURSOR_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case CURSOR_CROSSHAIR:
					*ptr += sprintf(*ptr, "crosshair");
					break;
				case CURSOR_DEFAULT:
					*ptr += sprintf(*ptr, "default");
					break;
				case CURSOR_POINTER:
					*ptr += sprintf(*ptr, "pointer");
					break;
				case CURSOR_MOVE:
					*ptr += sprintf(*ptr, "move");
					break;
				case CURSOR_E_RESIZE:
					*ptr += sprintf(*ptr, "e-resize");
					break;
				case CURSOR_NE_RESIZE:
					*ptr += sprintf(*ptr, "ne-resize");
					break;
				case CURSOR_NW_RESIZE:
					*ptr += sprintf(*ptr, "nw-resize");
					break;
				case CURSOR_N_RESIZE:
					*ptr += sprintf(*ptr, "n-resize");
					break;
				case CURSOR_SE_RESIZE:
					*ptr += sprintf(*ptr, "se-resize");
					break;
				case CURSOR_SW_RESIZE:
					*ptr += sprintf(*ptr, "sw-resize");
					break;
				case CURSOR_S_RESIZE:
					*ptr += sprintf(*ptr, "s-resize");
					break;
				case CURSOR_W_RESIZE:
					*ptr += sprintf(*ptr, "w-resize");
					break;
				case CURSOR_TEXT:
					*ptr += sprintf(*ptr, "text");
					break;
				case CURSOR_WAIT:
					*ptr += sprintf(*ptr, "wait");
					break;
				case CURSOR_HELP:
					*ptr += sprintf(*ptr, "help");
					break;
				case CURSOR_PROGRESS:
					*ptr += sprintf(*ptr, "progress");
					break;
				}
				break;
			case CSS_PROP_DIRECTION:
				switch (value) {
				case DIRECTION_LTR:
					*ptr += sprintf(*ptr, "ltr");
					break;
				case DIRECTION_RTL:
					*ptr += sprintf(*ptr, "rtl");
					break;
				}
				break;
			case CSS_PROP_DISPLAY:
				switch (value) {
				case DISPLAY_INLINE:
					*ptr += sprintf(*ptr, "inline");
					break;
				case DISPLAY_BLOCK:
					*ptr += sprintf(*ptr, "block");
					break;
				case DISPLAY_LIST_ITEM:
					*ptr += sprintf(*ptr, "list-item");
					break;
				case DISPLAY_RUN_IN:
					*ptr += sprintf(*ptr, "run-in");
					break;
				case DISPLAY_INLINE_BLOCK:
					*ptr += sprintf(*ptr, "inline-block");
					break;
				case DISPLAY_TABLE:
					*ptr += sprintf(*ptr, "table");
					break;
				case DISPLAY_INLINE_TABLE:
					*ptr += sprintf(*ptr, "inline-table");
					break;
				case DISPLAY_TABLE_ROW_GROUP:
					*ptr += sprintf(*ptr, "table-row-group");
					break;
				case DISPLAY_TABLE_HEADER_GROUP:
					*ptr += sprintf(*ptr, "table-header-group");
					break;
				case DISPLAY_TABLE_FOOTER_GROUP:
					*ptr += sprintf(*ptr, "table-footer-group");
					break;
				case DISPLAY_TABLE_ROW:
					*ptr += sprintf(*ptr, "table-row");
					break;
				case DISPLAY_TABLE_COLUMN_GROUP:
					*ptr += sprintf(*ptr, "table-column-group");
					break;
				case DISPLAY_TABLE_COLUMN:
					*ptr += sprintf(*ptr, "table-column");
					break;
				case DISPLAY_TABLE_CELL:
					*ptr += sprintf(*ptr, "table-cell");
					break;
				case DISPLAY_TABLE_CAPTION:
					*ptr += sprintf(*ptr, "table-caption");
					break;
				case DISPLAY_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_ELEVATION:
				switch (value) {
				case ELEVATION_ANGLE:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case ELEVATION_BELOW:
					*ptr += sprintf(*ptr, "below");
					break;
				case ELEVATION_LEVEL:
					*ptr += sprintf(*ptr, "level");
					break;
				case ELEVATION_ABOVE:
					*ptr += sprintf(*ptr, "above");
					break;
				case ELEVATION_HIGHER:
					*ptr += sprintf(*ptr, "higher");
					break;
				case ELEVATION_LOWER:
					*ptr += sprintf(*ptr, "lower");
					break;
				}
				break;
			case CSS_PROP_EMPTY_CELLS:
				switch (value) {
				case EMPTY_CELLS_SHOW:
					*ptr += sprintf(*ptr, "show");
					break;
				case EMPTY_CELLS_HIDE:
					*ptr += sprintf(*ptr, "hide");
					break;
				}
				break;
			case CSS_PROP_FLOAT:
				switch (value) {
				case FLOAT_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case FLOAT_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case FLOAT_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_FONT_FAMILY:
				while (value != FONT_FAMILY_END) {
					switch (value) {
					case FONT_FAMILY_STRING:
					case FONT_FAMILY_IDENT_LIST:
					{
						uint32_t snum = *((uint32_t *) bytecode);					
						lwc_string *he;
						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(snum));
						*ptr += sprintf(*ptr, "'%.*s'", 
                                                                (int) lwc_string_length(he), 
                                                                lwc_string_data(he));
					}
						break;
					case FONT_FAMILY_SERIF:
						*ptr += sprintf(*ptr, "serif");
						break;
					case FONT_FAMILY_SANS_SERIF:
						*ptr += sprintf(*ptr, "sans-serif");
						break;
					case FONT_FAMILY_CURSIVE:
						*ptr += sprintf(*ptr, "cursive");
						break;
					case FONT_FAMILY_FANTASY:
						*ptr += sprintf(*ptr, "fantasy");
						break;
					case FONT_FAMILY_MONOSPACE:
						*ptr += sprintf(*ptr, "monospace");
						break;
					}

					value = *((uint32_t *) bytecode);
					ADVANCE(sizeof(value));

					if (value != FONT_FAMILY_END)
						*ptr += sprintf(*ptr, ", ");
				}
				break;
			case CSS_PROP_FONT_SIZE:
				switch (value) {
				case FONT_SIZE_DIMENSION:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case FONT_SIZE_XX_SMALL:
					*ptr += sprintf(*ptr, "xx-small");
					break;
				case FONT_SIZE_X_SMALL:
					*ptr += sprintf(*ptr, "x-small");
					break;
				case FONT_SIZE_SMALL:
					*ptr += sprintf(*ptr, "small");
					break;
				case FONT_SIZE_MEDIUM:
					*ptr += sprintf(*ptr, "medium");
					break;
				case FONT_SIZE_LARGE:
					*ptr += sprintf(*ptr, "large");
					break;
				case FONT_SIZE_X_LARGE:
					*ptr += sprintf(*ptr, "x-large");
					break;
				case FONT_SIZE_XX_LARGE:
					*ptr += sprintf(*ptr, "xx-large");
					break;
				case FONT_SIZE_LARGER:
					*ptr += sprintf(*ptr, "larger");
					break;
				case FONT_SIZE_SMALLER:
					*ptr += sprintf(*ptr, "smaller");
					break;
				}
				break;
			case CSS_PROP_FONT_STYLE:
				switch (value) {
				case FONT_STYLE_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case FONT_STYLE_ITALIC:
					*ptr += sprintf(*ptr, "italic");
					break;
				case FONT_STYLE_OBLIQUE:
					*ptr += sprintf(*ptr, "oblique");
					break;
				}
				break;
			case CSS_PROP_FONT_VARIANT:
				switch (value) {
				case FONT_VARIANT_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case FONT_VARIANT_SMALL_CAPS:
					*ptr += sprintf(*ptr, "small-caps");
					break;
				}
				break;
			case CSS_PROP_FONT_WEIGHT:
				switch (value) {
				case FONT_WEIGHT_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case FONT_WEIGHT_BOLD:
					*ptr += sprintf(*ptr, "bold");
					break;
				case FONT_WEIGHT_BOLDER:
					*ptr += sprintf(*ptr, "bolder");
					break;
				case FONT_WEIGHT_LIGHTER:
					*ptr += sprintf(*ptr, "lighter");
					break;
				case FONT_WEIGHT_100:
					*ptr += sprintf(*ptr, "100");
					break;
				case FONT_WEIGHT_200:
					*ptr += sprintf(*ptr, "200");
					break;
				case FONT_WEIGHT_300:
					*ptr += sprintf(*ptr, "300");
					break;
				case FONT_WEIGHT_400:
					*ptr += sprintf(*ptr, "400");
					break;
				case FONT_WEIGHT_500:
					*ptr += sprintf(*ptr, "500");
					break;
				case FONT_WEIGHT_600:
					*ptr += sprintf(*ptr, "600");
					break;
				case FONT_WEIGHT_700:
					*ptr += sprintf(*ptr, "700");
					break;
				case FONT_WEIGHT_800:
					*ptr += sprintf(*ptr, "800");
					break;
				case FONT_WEIGHT_900:
					*ptr += sprintf(*ptr, "900");
					break;
				}
				break;
			case CSS_PROP_LETTER_SPACING:
			case CSS_PROP_WORD_SPACING:
				assert(LETTER_SPACING_SET ==
						(enum op_letter_spacing)
						WORD_SPACING_SET);
				assert(LETTER_SPACING_NORMAL ==
						(enum op_letter_spacing)
						WORD_SPACING_NORMAL);

				switch (value) {
				case LETTER_SPACING_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case LETTER_SPACING_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				}
				break;
			case CSS_PROP_LINE_HEIGHT:
				switch (value) {
				case LINE_HEIGHT_NUMBER:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				case LINE_HEIGHT_DIMENSION:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case LINE_HEIGHT_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				}
				break;
			case CSS_PROP_LIST_STYLE_POSITION:
				switch (value) {
				case LIST_STYLE_POSITION_INSIDE:
					*ptr += sprintf(*ptr, "inside");
					break;
				case LIST_STYLE_POSITION_OUTSIDE:
					*ptr += sprintf(*ptr, "outside");
					break;
				}
				break;
			case CSS_PROP_LIST_STYLE_TYPE:
				switch (value) {
				case LIST_STYLE_TYPE_DISC:
					*ptr += sprintf(*ptr, "disc");
					break;
				case LIST_STYLE_TYPE_CIRCLE:
					*ptr += sprintf(*ptr, "circle");
					break;
				case LIST_STYLE_TYPE_SQUARE:
					*ptr += sprintf(*ptr, "square");
					break;
				case LIST_STYLE_TYPE_DECIMAL:
					*ptr += sprintf(*ptr, "decimal");
					break;
				case LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
					*ptr += sprintf(*ptr, "decimal-leading-zero");
					break;
				case LIST_STYLE_TYPE_LOWER_ROMAN:
					*ptr += sprintf(*ptr, "lower-roman");
					break;
				case LIST_STYLE_TYPE_UPPER_ROMAN:
					*ptr += sprintf(*ptr, "upper-roman");
					break;
				case LIST_STYLE_TYPE_LOWER_GREEK:
					*ptr += sprintf(*ptr, "lower-greek");
					break;
				case LIST_STYLE_TYPE_LOWER_LATIN:
					*ptr += sprintf(*ptr, "lower-latin");
					break;
				case LIST_STYLE_TYPE_UPPER_LATIN:
					*ptr += sprintf(*ptr, "upper-latin");
					break;
				case LIST_STYLE_TYPE_ARMENIAN:
					*ptr += sprintf(*ptr, "armenian");
					break;
				case LIST_STYLE_TYPE_GEORGIAN:
					*ptr += sprintf(*ptr, "georgian");
					break;
				case LIST_STYLE_TYPE_LOWER_ALPHA:
					*ptr += sprintf(*ptr, "lower-alpha");
					break;
				case LIST_STYLE_TYPE_UPPER_ALPHA:
					*ptr += sprintf(*ptr, "upper-alpha");
					break;
				case LIST_STYLE_TYPE_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_MAX_HEIGHT:
			case CSS_PROP_MAX_WIDTH:
				assert(MAX_HEIGHT_SET ==
						(enum op_max_height)
						MAX_WIDTH_SET);
				assert(MAX_HEIGHT_NONE ==
						(enum op_max_height)
						MAX_WIDTH_NONE);

				switch (value) {
				case MAX_HEIGHT_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case MAX_HEIGHT_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_OPACITY:
				switch (value) {
				case OPACITY_SET:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				}
				break;
			case CSS_PROP_PADDING_TOP:
			case CSS_PROP_PADDING_RIGHT:
			case CSS_PROP_PADDING_BOTTOM:
			case CSS_PROP_PADDING_LEFT:
			case CSS_PROP_MIN_HEIGHT:
			case CSS_PROP_MIN_WIDTH:
			case CSS_PROP_PAUSE_AFTER:
			case CSS_PROP_PAUSE_BEFORE:
			case CSS_PROP_TEXT_INDENT:
				assert(MIN_HEIGHT_SET ==
						(enum op_min_height)
						MIN_WIDTH_SET);
				assert(MIN_HEIGHT_SET ==
						(enum op_min_height)
						PADDING_SET);
				assert(MIN_HEIGHT_SET ==
						(enum op_min_height)
						PAUSE_AFTER_SET);
				assert(MIN_HEIGHT_SET ==
						(enum op_min_height)
						PAUSE_BEFORE_SET);
				assert(MIN_HEIGHT_SET ==
						(enum op_min_height)
						TEXT_INDENT_SET);

				switch (value) {
				case MIN_HEIGHT_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				}
				break;
			case CSS_PROP_ORPHANS:
			case CSS_PROP_PITCH_RANGE:
			case CSS_PROP_RICHNESS:
			case CSS_PROP_STRESS:
			case CSS_PROP_WIDOWS:
				assert(ORPHANS_SET ==
						(enum op_orphans)
						PITCH_RANGE_SET);
				assert(ORPHANS_SET ==
						(enum op_orphans)
						RICHNESS_SET);
				assert(ORPHANS_SET ==
						(enum op_orphans)
						STRESS_SET);
				assert(ORPHANS_SET ==
						(enum op_orphans)
						WIDOWS_SET);

				switch (value) {
				case ORPHANS_SET:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				}
				break;
			case CSS_PROP_OUTLINE_COLOR:
				switch (value) {
				case OUTLINE_COLOR_TRANSPARENT:
					*ptr += sprintf(*ptr, "transparent");
					break;
				case OUTLINE_COLOR_CURRENT_COLOR:
					*ptr += sprintf(*ptr, "currentColor");
					break;
				case OUTLINE_COLOR_SET:
				{
					uint32_t colour = 
						*((uint32_t *) bytecode);
					ADVANCE(sizeof(colour));
					*ptr += sprintf(*ptr, "#%08x", colour);
				}
					break;
				case OUTLINE_COLOR_INVERT:
					*ptr += sprintf(*ptr, "invert");
					break;
				}
				break;
			case CSS_PROP_OVERFLOW:
				switch (value) {
				case OVERFLOW_VISIBLE:
					*ptr += sprintf(*ptr, "visible");
					break;
				case OVERFLOW_HIDDEN:
					*ptr += sprintf(*ptr, "hidden");
					break;
				case OVERFLOW_SCROLL:
					*ptr += sprintf(*ptr, "scroll");
					break;
				case OVERFLOW_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				}
				break;
			case CSS_PROP_PAGE_BREAK_AFTER:
			case CSS_PROP_PAGE_BREAK_BEFORE:
				assert(PAGE_BREAK_AFTER_AUTO ==
						(enum op_page_break_after)
						PAGE_BREAK_BEFORE_AUTO);
				assert(PAGE_BREAK_AFTER_ALWAYS ==
						(enum op_page_break_after)
						PAGE_BREAK_BEFORE_ALWAYS);
				assert(PAGE_BREAK_AFTER_AVOID ==
						(enum op_page_break_after)
						PAGE_BREAK_BEFORE_AVOID);
				assert(PAGE_BREAK_AFTER_LEFT ==
						(enum op_page_break_after)
						PAGE_BREAK_BEFORE_LEFT);
				assert(PAGE_BREAK_AFTER_RIGHT ==
						(enum op_page_break_after)
						PAGE_BREAK_BEFORE_RIGHT);

				switch (value) {
				case PAGE_BREAK_AFTER_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case PAGE_BREAK_AFTER_ALWAYS:
					*ptr += sprintf(*ptr, "always");
					break;
				case PAGE_BREAK_AFTER_AVOID:
					*ptr += sprintf(*ptr, "avoid");
					break;
				case PAGE_BREAK_AFTER_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case PAGE_BREAK_AFTER_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				}
				break;
			case CSS_PROP_PAGE_BREAK_INSIDE:
				switch (value) {
				case PAGE_BREAK_INSIDE_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case PAGE_BREAK_INSIDE_AVOID:
					*ptr += sprintf(*ptr, "avoid");
					break;
				}
				break;
			case CSS_PROP_PITCH:
				switch (value) {
				case PITCH_FREQUENCY:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case PITCH_X_LOW:
					*ptr += sprintf(*ptr, "x-low");
					break;
				case PITCH_LOW:
					*ptr += sprintf(*ptr, "low");
					break;
				case PITCH_MEDIUM:
					*ptr += sprintf(*ptr, "medium");
					break;
				case PITCH_HIGH:
					*ptr += sprintf(*ptr, "high");
					break;
				case PITCH_X_HIGH:
					*ptr += sprintf(*ptr, "x-high");
					break;
				}
				break;
			case CSS_PROP_PLAY_DURING:
				switch (value) {
				case PLAY_DURING_URI:
				{
					uint32_t snum = *((uint32_t *) bytecode);					
					lwc_string *he;
					css__stylesheet_string_get(style->sheet, snum, &he);
					ADVANCE(sizeof(snum));
					*ptr += sprintf(*ptr, "'%.*s'", 
                                                        (int) lwc_string_length(he), 
                                                        lwc_string_data(he));
				}
					break;
				case PLAY_DURING_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case PLAY_DURING_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}

				if (value & PLAY_DURING_MIX)
					*ptr += sprintf(*ptr, " mix");
				if (value & PLAY_DURING_REPEAT)
					*ptr += sprintf(*ptr, " repeat");
				break;
			case CSS_PROP_POSITION:
				switch (value) {
				case POSITION_STATIC:
					*ptr += sprintf(*ptr, "static");
					break;
				case POSITION_RELATIVE:
					*ptr += sprintf(*ptr, "relative");
					break;
				case POSITION_ABSOLUTE:
					*ptr += sprintf(*ptr, "absolute");
					break;
				case POSITION_FIXED:
					*ptr += sprintf(*ptr, "fixed");
					break;
				}
				break;
			case CSS_PROP_QUOTES:
				switch (value) {
				case QUOTES_STRING:
					while (value != QUOTES_NONE) {
						uint32_t snum = *((uint32_t *) bytecode);					
						lwc_string *he;
						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(snum));
						*ptr += sprintf(*ptr, " '%.*s' ", 
                                                                (int) lwc_string_length(he), 
                                                                lwc_string_data(he));

						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(he));
						*ptr += sprintf(*ptr, " '%.*s' ", 
                                                                (int) lwc_string_length(he), 
                                                                lwc_string_data(he));

						value = *((uint32_t *) bytecode);
						ADVANCE(sizeof(value));
					}
					break;
				case QUOTES_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_SPEAK_HEADER:
				switch (value) {
				case SPEAK_HEADER_ONCE:
					*ptr += sprintf(*ptr, "once");
					break;
				case SPEAK_HEADER_ALWAYS:
					*ptr += sprintf(*ptr, "always");
					break;
				}
				break;
			case CSS_PROP_SPEAK_NUMERAL:
				switch (value) {
				case SPEAK_NUMERAL_DIGITS:
					*ptr += sprintf(*ptr, "digits");
					break;
				case SPEAK_NUMERAL_CONTINUOUS:
					*ptr += sprintf(*ptr, "continuous");
					break;
				}
				break;
			case CSS_PROP_SPEAK_PUNCTUATION:
				switch (value) {
				case SPEAK_PUNCTUATION_CODE:
					*ptr += sprintf(*ptr, "code");
					break;
				case SPEAK_PUNCTUATION_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_SPEAK:
				switch (value) {
				case SPEAK_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case SPEAK_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				case SPEAK_SPELL_OUT:
					*ptr += sprintf(*ptr, "spell-out");
					break;
				}
				break;
			case CSS_PROP_SPEECH_RATE:
				switch (value) {
				case SPEECH_RATE_SET:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				case SPEECH_RATE_X_SLOW:
					*ptr += sprintf(*ptr, "x-slow");
					break;
				case SPEECH_RATE_SLOW:
					*ptr += sprintf(*ptr, "slow");
					break;
				case SPEECH_RATE_MEDIUM:
					*ptr += sprintf(*ptr, "medium");
					break;
				case SPEECH_RATE_FAST:
					*ptr += sprintf(*ptr, "fast");
					break;
				case SPEECH_RATE_X_FAST:
					*ptr += sprintf(*ptr, "x-fast");
					break;
				case SPEECH_RATE_FASTER:
					*ptr += sprintf(*ptr, "faster");
					break;
				case SPEECH_RATE_SLOWER:
					*ptr += sprintf(*ptr, "slower");
					break;
				}
				break;
			case CSS_PROP_TABLE_LAYOUT:
				switch (value) {
				case TABLE_LAYOUT_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				case TABLE_LAYOUT_FIXED:
					*ptr += sprintf(*ptr, "fixed");
					break;
				}
				break;
			case CSS_PROP_TEXT_ALIGN:
				switch (value) {
				case TEXT_ALIGN_LEFT:
					*ptr += sprintf(*ptr, "left");
					break;
				case TEXT_ALIGN_RIGHT:
					*ptr += sprintf(*ptr, "right");
					break;
				case TEXT_ALIGN_CENTER:
					*ptr += sprintf(*ptr, "center");
					break;
				case TEXT_ALIGN_JUSTIFY:
					*ptr += sprintf(*ptr, "justify");
					break;
				case TEXT_ALIGN_LIBCSS_LEFT:
					*ptr += sprintf(*ptr, "-libcss-left");
					break;
				case TEXT_ALIGN_LIBCSS_CENTER:
					*ptr += sprintf(*ptr, "-libcss-center");
					break;
				case TEXT_ALIGN_LIBCSS_RIGHT:
					*ptr += sprintf(*ptr, "-libcss-right");
					break;
				}
				break;
			case CSS_PROP_TEXT_DECORATION:
				if (value == TEXT_DECORATION_NONE)
					*ptr += sprintf(*ptr, "none");

				if (value & TEXT_DECORATION_UNDERLINE)
					*ptr += sprintf(*ptr, " underline");
				if (value & TEXT_DECORATION_OVERLINE)
					*ptr += sprintf(*ptr, " overline");
				if (value & TEXT_DECORATION_LINE_THROUGH)
					*ptr += sprintf(*ptr, " line-through");
				if (value & TEXT_DECORATION_BLINK)
					*ptr += sprintf(*ptr, " blink");
				break;
			case CSS_PROP_TEXT_TRANSFORM:
				switch (value) {
				case TEXT_TRANSFORM_CAPITALIZE:
					*ptr += sprintf(*ptr, "capitalize");
					break;
				case TEXT_TRANSFORM_UPPERCASE:
					*ptr += sprintf(*ptr, "uppercase");
					break;
				case TEXT_TRANSFORM_LOWERCASE:
					*ptr += sprintf(*ptr, "lowercase");
					break;
				case TEXT_TRANSFORM_NONE:
					*ptr += sprintf(*ptr, "none");
					break;
				}
				break;
			case CSS_PROP_UNICODE_BIDI:
				switch (value) {
				case UNICODE_BIDI_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case UNICODE_BIDI_EMBED:
					*ptr += sprintf(*ptr, "embed");
					break;
				case UNICODE_BIDI_BIDI_OVERRIDE:
					*ptr += sprintf(*ptr, "bidi-override");
					break;
				}
				break;
			case CSS_PROP_VERTICAL_ALIGN:
				switch (value) {
				case VERTICAL_ALIGN_SET:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case VERTICAL_ALIGN_BASELINE:
					*ptr += sprintf(*ptr, "baseline");
					break;
				case VERTICAL_ALIGN_SUB:
					*ptr += sprintf(*ptr, "sub");
					break;
				case VERTICAL_ALIGN_SUPER:
					*ptr += sprintf(*ptr, "super");
					break;
				case VERTICAL_ALIGN_TOP:
					*ptr += sprintf(*ptr, "top");
					break;
				case VERTICAL_ALIGN_TEXT_TOP:
					*ptr += sprintf(*ptr, "text-top");
					break;
				case VERTICAL_ALIGN_MIDDLE:
					*ptr += sprintf(*ptr, "middle");
					break;
				case VERTICAL_ALIGN_BOTTOM:
					*ptr += sprintf(*ptr, "bottom");
					break;
				case VERTICAL_ALIGN_TEXT_BOTTOM:
					*ptr += sprintf(*ptr, "text-bottom");
					break;
				}
				break;
			case CSS_PROP_VISIBILITY:
				switch (value) {
				case VISIBILITY_VISIBLE:
					*ptr += sprintf(*ptr, "visible");
					break;
				case VISIBILITY_HIDDEN:
					*ptr += sprintf(*ptr, "hidden");
					break;
				case VISIBILITY_COLLAPSE:
					*ptr += sprintf(*ptr, "collapse");
					break;
				}
				break;
			case CSS_PROP_VOICE_FAMILY:
				while (value != VOICE_FAMILY_END) {
					switch (value) {
					case VOICE_FAMILY_STRING:
					case VOICE_FAMILY_IDENT_LIST:
					{
						uint32_t snum = *((uint32_t *) bytecode);					
						lwc_string *he;
						css__stylesheet_string_get(style->sheet, snum, &he);
						ADVANCE(sizeof(snum));
						*ptr += sprintf(*ptr, "'%.*s'", 
                                                                (int) lwc_string_length(he),
                                                                lwc_string_data(he));
					}
						break;
					case VOICE_FAMILY_MALE:
						*ptr += sprintf(*ptr, "male");
						break;
					case VOICE_FAMILY_FEMALE:
						*ptr += sprintf(*ptr, "female");
						break;
					case VOICE_FAMILY_CHILD:
						*ptr += sprintf(*ptr, "child");
						break;
					}

					value = *((uint32_t *) bytecode);
					ADVANCE(sizeof(value));

					if (value != VOICE_FAMILY_END)
						*ptr += sprintf(*ptr, ", ");
				}
				break;
			case CSS_PROP_VOLUME:
				switch (value) {
				case VOLUME_NUMBER:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				case VOLUME_DIMENSION:
				{
					uint32_t unit;
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					unit = *((uint32_t *) bytecode);
					ADVANCE(sizeof(unit));
					dump_unit(val, unit, ptr);
				}
					break;
				case VOLUME_SILENT:
					*ptr += sprintf(*ptr, "silent");
					break;
				case VOLUME_X_SOFT:
					*ptr += sprintf(*ptr, "x-soft");
					break;
				case VOLUME_SOFT:
					*ptr += sprintf(*ptr, "soft");
					break;
				case VOLUME_MEDIUM:
					*ptr += sprintf(*ptr, "medium");
					break;
				case VOLUME_LOUD:
					*ptr += sprintf(*ptr, "loud");
					break;
				case VOLUME_X_LOUD:
					*ptr += sprintf(*ptr, "x-loud");
					break;
				}
				break;
			case CSS_PROP_WHITE_SPACE:
				switch (value) {
				case WHITE_SPACE_NORMAL:
					*ptr += sprintf(*ptr, "normal");
					break;
				case WHITE_SPACE_PRE:
					*ptr += sprintf(*ptr, "pre");
					break;
				case WHITE_SPACE_NOWRAP:
					*ptr += sprintf(*ptr, "nowrap");
					break;
				case WHITE_SPACE_PRE_WRAP:
					*ptr += sprintf(*ptr, "pre-wrap");
					break;
				case WHITE_SPACE_PRE_LINE:
					*ptr += sprintf(*ptr, "pre-line");
					break;
				}
				break;
			case CSS_PROP_WRITING_MODE:
				switch (value) {
				case WRITING_MODE_HORIZONTAL_TB:
					*ptr += sprintf(*ptr, "horizontal-tb");
					break;
				case WRITING_MODE_VERTICAL_RL:
					*ptr += sprintf(*ptr, "vertical-rl");
					break;
				case WRITING_MODE_VERTICAL_LR:
					*ptr += sprintf(*ptr, "vertical-lr");
					break;
				}
				break;
			case CSS_PROP_Z_INDEX:
				switch (value) {
				case Z_INDEX_SET:
				{
					css_fixed val = *((css_fixed *) bytecode);
					ADVANCE(sizeof(val));
					dump_number(val, ptr);
				}
					break;
				case Z_INDEX_AUTO:
					*ptr += sprintf(*ptr, "auto");
					break;
				}
				break;
			default:
				*ptr += sprintf(*ptr, "Unknown opcode %x", op);
				return;
			}
		}

		if (isImportant(opv))
			*ptr += sprintf(*ptr, " !important");

		*ptr += sprintf(*ptr, "\n");
	}

#undef ADVANCE

}

void dump_string(lwc_string *string, char **ptr)
{
	*ptr += sprintf(*ptr, "%.*s", 
                        (int) lwc_string_length(string),
                        lwc_string_data(string));
}

void dump_font_face(css_font_face *font_face, char **ptr)
{
	uint8_t style, weight;

	if (font_face->font_family != NULL) {
		*(*ptr)++ = '\n';
		*ptr += sprintf(*ptr, "|  font-family: %.*s",
				(int) lwc_string_length(font_face->font_family),
				lwc_string_data(font_face->font_family));
	}
	
	*ptr += sprintf(*ptr, "\n|  font-style: ");
	style = css_font_face_font_style(font_face);
	switch (style) {
	case CSS_FONT_STYLE_INHERIT:
		*ptr += sprintf(*ptr, "unspecified");
		break;
	case CSS_FONT_STYLE_NORMAL:
		*ptr += sprintf(*ptr, "normal");
		break;
	case CSS_FONT_STYLE_ITALIC:
		*ptr += sprintf(*ptr, "italic");
		break;
	case CSS_FONT_STYLE_OBLIQUE:
		*ptr += sprintf(*ptr, "oblique");
		break;
	}

	*ptr += sprintf(*ptr, "\n|  font-weight: ");
	weight = css_font_face_font_weight(font_face);
	switch (weight) {
	case CSS_FONT_WEIGHT_INHERIT:
		*ptr += sprintf(*ptr, "unspecified");
		break;
	case CSS_FONT_WEIGHT_NORMAL:
		*ptr += sprintf(*ptr, "normal");
		break;
	case CSS_FONT_WEIGHT_BOLD:
		*ptr += sprintf(*ptr, "bold");
		break;
	case CSS_FONT_WEIGHT_100:
		*ptr += sprintf(*ptr, "100");
		break;
	case CSS_FONT_WEIGHT_200:
		*ptr += sprintf(*ptr, "200");
		break;
	case CSS_FONT_WEIGHT_300:
		*ptr += sprintf(*ptr, "300");
		break;
	case CSS_FONT_WEIGHT_400:
		*ptr += sprintf(*ptr, "400");
		break;
	case CSS_FONT_WEIGHT_500:
		*ptr += sprintf(*ptr, "500");
		break;
	case CSS_FONT_WEIGHT_600:
		*ptr += sprintf(*ptr, "600");
		break;
	case CSS_FONT_WEIGHT_700:
		*ptr += sprintf(*ptr, "700");
		break;
	case CSS_FONT_WEIGHT_800:
		*ptr += sprintf(*ptr, "800");
		break;
	case CSS_FONT_WEIGHT_900:
		*ptr += sprintf(*ptr, "900");
		break;
	default:
		*ptr += sprintf(*ptr, "Unhandled weight %d\n", (int)weight);
		break;
	}

	
	if (font_face->srcs != NULL) {
		uint32_t i;
		css_font_face_src *srcs = font_face->srcs;
		for (i = 0; i < font_face->n_srcs; ++i) {
			*ptr += sprintf(*ptr, "\n|  src: ");
			
			css_font_face_format format = 
					css_font_face_src_format(&srcs[i]);
			
			*ptr += sprintf(*ptr, "\n|   format: ");
			
			switch (format) {
			case CSS_FONT_FACE_FORMAT_UNSPECIFIED:
				*ptr += sprintf(*ptr, "unspecified");
				break;
			case CSS_FONT_FACE_FORMAT_WOFF:
				*ptr += sprintf(*ptr, "WOFF");
				break;
			case CSS_FONT_FACE_FORMAT_OPENTYPE:
				*ptr += sprintf(*ptr, "OTF");
				break;
			case CSS_FONT_FACE_FORMAT_EMBEDDED_OPENTYPE:
				*ptr += sprintf(*ptr, "EOTF");
				break;
			case CSS_FONT_FACE_FORMAT_SVG:
				*ptr += sprintf(*ptr, "SVG");
				break;
			case CSS_FONT_FACE_FORMAT_UNKNOWN:
				*ptr += sprintf(*ptr, "unknown");
				break;
			default:
				*ptr += sprintf(*ptr, "UNEXPECTED");
				break;
			}

			if (srcs[i].location != NULL) {
				*ptr += sprintf(*ptr, "\n|   location: ");

				switch (css_font_face_src_location_type(
						&srcs[i])) {
				case CSS_FONT_FACE_LOCATION_TYPE_LOCAL:
					*ptr += sprintf(*ptr, "local");
					break;
				case CSS_FONT_FACE_LOCATION_TYPE_URI:
					*ptr += sprintf(*ptr, "url");
					break;
				default:
					*ptr += sprintf(*ptr, "UNKNOWN");
					break;
				}
				
				*ptr += sprintf(*ptr, "(%.*s)",
					(int) lwc_string_length(
							srcs[i].location), 
					lwc_string_data(srcs[i].location));
			}
		}
	}
}

#endif
