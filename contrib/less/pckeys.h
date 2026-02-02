/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Definitions of keys on the PC.
 * Special (non-ASCII) keys on the PC send a two-byte sequence,
 * where the first byte is 0 and the second is as defined below.
 */
#define PCK_SHIFT_TAB           '\x0f'
#define PCK_ALT_E               '\x12'
#define PCK_CAPS_LOCK           '\x3a'
#define PCK_F1                  '\x3b'
#define PCK_NUM_LOCK            '\x45'
#define PCK_HOME                '\x47'
#define PCK_UP                  '\x48'
#define PCK_PAGEUP              '\x49'
#define PCK_LEFT                '\x4b'
#define PCK_RIGHT               '\x4d'
#define PCK_END                 '\x4f'
#define PCK_DOWN                '\x50'
#define PCK_PAGEDOWN            '\x51'
#define PCK_INSERT              '\x52'
#define PCK_DELETE              '\x53'
/* The following are nonstandard and internal to less. */
#define PCK_SHIFT_END           '\x61'
#define PCK_CTL_DELETE          '\x62'
#define PCK_CTL_LEFT            '\x63'
#define PCK_CTL_RIGHT           '\x74'
#define PCK_CTL_HOME            '\x7a'
#define PCK_CTL_END             '\x7c'
#define PCK_SHIFT_HOME          '\x7d'
