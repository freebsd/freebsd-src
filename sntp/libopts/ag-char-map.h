/*
 *   Character mapping generated 04/29/11 15:43:58
 *
 *  This file contains the character classifications
 *  used by AutoGen and AutoOpts for identifying tokens.
 *  This file is part of AutoGen.
 *  AutoGen Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
 *  AutoGen is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation, either version 3 of the License, or (at your option) any later
 *  version.
 *  AutoGen is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 *  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef AG_CHAR_MAP_H_GUARD
#define AG_CHAR_MAP_H_GUARD 1

#ifdef HAVE_CONFIG_H
# if defined(HAVE_INTTYPES_H)
#  include <inttypes.h>
# elif defined(HAVE_STDINT_H)
#  include <stdint.h>

# else
#   ifndef HAVE_INT8_T
        typedef signed char     int8_t;
#   endif
#   ifndef HAVE_UINT8_T
        typedef unsigned char   uint8_t;
#   endif
#   ifndef HAVE_INT16_T
        typedef signed short    int16_t;
#   endif
#   ifndef HAVE_UINT16_T
        typedef unsigned short  uint16_t;
#   endif
#   ifndef HAVE_UINT_T
        typedef unsigned int    uint_t;
#   endif

#   ifndef HAVE_INT32_T
#    if SIZEOF_INT == 4
        typedef signed int      int32_t;
#    elif SIZEOF_LONG == 4
        typedef signed long     int32_t;
#    endif
#   endif

#   ifndef HAVE_UINT32_T
#    if SIZEOF_INT == 4
        typedef unsigned int    uint32_t;
#    elif SIZEOF_LONG == 4
        typedef unsigned long   uint32_t;
#    endif
#   endif
# endif /* HAVE_*INT*_H header */

#else /* not HAVE_CONFIG_H -- */
# ifdef __sun
#  include <inttypes.h>
# else
#  include <stdint.h>
# endif
#endif /* HAVE_CONFIG_H */

#if 0 /* mapping specification source (from autogen.map) */
// 
// %guard          autoopts_internal
// %file           ag-char-map.h
// %static-table   option-char-category
// 
// %comment
//   This file contains the character classifications
//   used by AutoGen and AutoOpts for identifying tokens.
// 
//   This file is part of AutoGen.
//   AutoGen Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
// 
//   AutoGen is free software: you can redistribute it and/or modify it under the
//   terms of the GNU General Public License as published by the Free Software
//   Foundation, either version 3 of the License, or (at your option) any later
//   version.
// 
//   AutoGen is distributed in the hope that it will be useful, but WITHOUT ANY
//   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
//   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// 
//   You should have received a copy of the GNU General Public License along
//   with this program.  If not, see <http://www.gnu.org/licenses/>.
// %
// 
// lower-case      "a-z"
// upper-case      "A-Z"
// alphabetic      +lower-case   +upper-case
// oct-digit       "0-7"
// dec-digit       "89"          +oct-digit
// hex-digit       "a-fA-F"      +dec-digit
// alphanumeric    +alphabetic   +dec-digit
// var-first       "_"           +alphabetic
// variable-name   +var-first    +dec-digit
// option-name     "^-"          +variable-name
// value-name      ":"           +option-name
// horiz-white     "\t "
// compound-name   "[.]"         +value-name   +horiz-white
// whitespace      "\v\f\r\n\b"  +horiz-white
// unquotable      "!-~"         -"\"#(),;<=>[\\]`{}?*'"
// end-xml-token   "/>"          +whitespace
// graphic         "!-~"
// plus-n-space    "+"           +whitespace
// punctuation     "!-~"         -alphanumeric -"_"
// suffix          "-._"         +alphanumeric
// suffix-fmt      "%/"          +suffix     
// false-type      "nNfF0\x00"
// file-name       "/"           +suffix
// end-token       "\x00"        +whitespace
// end-list-entry  ","           +end-token
//
#endif /* 0 -- mapping spec. source */

typedef uint32_t option_char_category_mask_t;
static option_char_category_mask_t const option_char_category[128];

static inline int is_option_char_category_char(char ch, option_char_category_mask_t mask) {
    unsigned int ix = (unsigned char)ch;
    return ((ix < 128) && ((option_char_category[ix] & mask) != 0)); }

#define IS_LOWER_CASE_CHAR(_c)      is_option_char_category_char((_c), 0x000001)
#define IS_UPPER_CASE_CHAR(_c)      is_option_char_category_char((_c), 0x000002)
#define IS_ALPHABETIC_CHAR(_c)      is_option_char_category_char((_c), 0x000003)
#define IS_OCT_DIGIT_CHAR(_c)       is_option_char_category_char((_c), 0x000004)
#define IS_DEC_DIGIT_CHAR(_c)       is_option_char_category_char((_c), 0x00000C)
#define IS_HEX_DIGIT_CHAR(_c)       is_option_char_category_char((_c), 0x00001C)
#define IS_ALPHANUMERIC_CHAR(_c)    is_option_char_category_char((_c), 0x00000F)
#define IS_VAR_FIRST_CHAR(_c)       is_option_char_category_char((_c), 0x000023)
#define IS_VARIABLE_NAME_CHAR(_c)   is_option_char_category_char((_c), 0x00002F)
#define IS_OPTION_NAME_CHAR(_c)     is_option_char_category_char((_c), 0x00006F)
#define IS_VALUE_NAME_CHAR(_c)      is_option_char_category_char((_c), 0x0000EF)
#define IS_HORIZ_WHITE_CHAR(_c)     is_option_char_category_char((_c), 0x000100)
#define IS_COMPOUND_NAME_CHAR(_c)   is_option_char_category_char((_c), 0x0003EF)
#define IS_WHITESPACE_CHAR(_c)      is_option_char_category_char((_c), 0x000500)
#define IS_UNQUOTABLE_CHAR(_c)      is_option_char_category_char((_c), 0x000800)
#define IS_END_XML_TOKEN_CHAR(_c)   is_option_char_category_char((_c), 0x001500)
#define IS_GRAPHIC_CHAR(_c)         is_option_char_category_char((_c), 0x002000)
#define IS_PLUS_N_SPACE_CHAR(_c)    is_option_char_category_char((_c), 0x004500)
#define IS_PUNCTUATION_CHAR(_c)     is_option_char_category_char((_c), 0x008000)
#define IS_SUFFIX_CHAR(_c)          is_option_char_category_char((_c), 0x01000F)
#define IS_SUFFIX_FMT_CHAR(_c)      is_option_char_category_char((_c), 0x03000F)
#define IS_FALSE_TYPE_CHAR(_c)      is_option_char_category_char((_c), 0x040000)
#define IS_FILE_NAME_CHAR(_c)       is_option_char_category_char((_c), 0x09000F)
#define IS_END_TOKEN_CHAR(_c)       is_option_char_category_char((_c), 0x100500)
#define IS_END_LIST_ENTRY_CHAR(_c)  is_option_char_category_char((_c), 0x300500)

#if 1 /* def AUTOOPTS_INTERNAL */
static option_char_category_mask_t const option_char_category[128] = {
  /*x00*/ 0x140000, /*x01*/ 0x000000, /*x02*/ 0x000000, /*x03*/ 0x000000,
  /*x04*/ 0x000000, /*x05*/ 0x000000, /*x06*/ 0x000000, /*\a */ 0x000000,
  /*\b */ 0x000400, /*\t */ 0x000100, /*\n */ 0x000400, /*\v */ 0x000400,
  /*\f */ 0x000400, /*\r */ 0x000400, /*x0E*/ 0x000000, /*x0F*/ 0x000000,
  /*x10*/ 0x000000, /*x11*/ 0x000000, /*x12*/ 0x000000, /*x13*/ 0x000000,
  /*x14*/ 0x000000, /*x15*/ 0x000000, /*x16*/ 0x000000, /*x17*/ 0x000000,
  /*x18*/ 0x000000, /*x19*/ 0x000000, /*x1A*/ 0x000000, /*x1B*/ 0x000000,
  /*x1C*/ 0x000000, /*x1D*/ 0x000000, /*x1E*/ 0x000000, /*x1F*/ 0x000000,
  /*   */ 0x000100, /* ! */ 0x00A800, /* " */ 0x00A000, /* # */ 0x00A000,
  /* $ */ 0x00A800, /* % */ 0x02A800, /* & */ 0x00A800, /* ' */ 0x00A000,
  /* ( */ 0x00A000, /* ) */ 0x00A000, /* * */ 0x00A000, /* + */ 0x00E800,
  /* , */ 0x20A000, /* - */ 0x01A840, /* . */ 0x01AA00, /* / */ 0x0AB800,
  /* 0 */ 0x042804, /* 1 */ 0x002804, /* 2 */ 0x002804, /* 3 */ 0x002804,
  /* 4 */ 0x002804, /* 5 */ 0x002804, /* 6 */ 0x002804, /* 7 */ 0x002804,
  /* 8 */ 0x002808, /* 9 */ 0x002808, /* : */ 0x00A880, /* ; */ 0x00A000,
  /* < */ 0x00A000, /* = */ 0x00A000, /* > */ 0x00B000, /* ? */ 0x00A000,
  /* @ */ 0x00A800, /* A */ 0x002812, /* B */ 0x002812, /* C */ 0x002812,
  /* D */ 0x002812, /* E */ 0x002812, /* F */ 0x042812, /* G */ 0x002802,
  /* H */ 0x002802, /* I */ 0x002802, /* J */ 0x002802, /* K */ 0x002802,
  /* L */ 0x002802, /* M */ 0x002802, /* N */ 0x042802, /* O */ 0x002802,
  /* P */ 0x002802, /* Q */ 0x002802, /* R */ 0x002802, /* S */ 0x002802,
  /* T */ 0x002802, /* U */ 0x002802, /* V */ 0x002802, /* W */ 0x002802,
  /* X */ 0x002802, /* Y */ 0x002802, /* Z */ 0x002802, /* [ */ 0x00A200,
  /* \ */ 0x00A000, /* ] */ 0x00A200, /* ^ */ 0x00A840, /* _ */ 0x012820,
  /* ` */ 0x00A000, /* a */ 0x002811, /* b */ 0x002811, /* c */ 0x002811,
  /* d */ 0x002811, /* e */ 0x002811, /* f */ 0x042811, /* g */ 0x002801,
  /* h */ 0x002801, /* i */ 0x002801, /* j */ 0x002801, /* k */ 0x002801,
  /* l */ 0x002801, /* m */ 0x002801, /* n */ 0x042801, /* o */ 0x002801,
  /* p */ 0x002801, /* q */ 0x002801, /* r */ 0x002801, /* s */ 0x002801,
  /* t */ 0x002801, /* u */ 0x002801, /* v */ 0x002801, /* w */ 0x002801,
  /* x */ 0x002801, /* y */ 0x002801, /* z */ 0x002801, /* { */ 0x00A000,
  /* | */ 0x00A800, /* } */ 0x00A000, /* ~ */ 0x00A800, /*x7F*/ 0x000000
};
#endif /* AUTOOPTS_INTERNAL */
#endif /* AG_CHAR_MAP_H_GUARD */
