#
# Copyright (C) 2018-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# pylint: disable=bad-whitespace

# -- x_avg_char_width --
WEIGHT_FACTORS = (
	( 'a', 64 ),
	( 'b', 14 ),
	( 'c', 27 ),
	( 'd', 35 ),
	( 'e', 100 ),
	( 'f', 20 ),
	( 'g', 14 ),
	( 'h', 42 ),
	( 'i', 63 ),
	( 'j', 3 ),
	( 'k', 6 ),
	( 'l', 35 ),
	( 'm', 20 ),
	( 'n', 56 ),
	( 'o', 56 ),
	( 'p', 17 ),
	( 'q', 4 ),
	( 'r', 49 ),
	( 's', 56 ),
	( 't', 71 ),
	( 'u', 31 ),
	( 'v', 10 ),
	( 'w', 18 ),
	( 'x', 3 ),
	( 'y', 18 ),
	( 'z', 2 ),
	( ' ', 166 )
)

def x_avg_char_width(font):
	x_avg_total_width = 0

	for factor in WEIGHT_FACTORS:
		char = next((char for char in font.chars if char.code == ord(factor[0])), None)

		if char is None:
			return 0

		x_avg_total_width += font.scaleWidth(char) * factor[1]

	return round(x_avg_total_width / 1000)


# -- ul_char_ranges --
CHAR_RANGES = (
	( 0, 0x0000, 0x007F ),
	( 1, 0x0080, 0x00FF ),
	( 2, 0x0100, 0x017F ),
	( 3, 0x0180, 0x024F ),
	( 4, 0x0250, 0x02AF ),
	( 4, 0x1D00, 0x1D7F ),
	( 4, 0x1D80, 0x1DBF ),
	( 5, 0x02B0, 0x02FF ),
	( 5, 0xA700, 0xA71F ),
	( 6, 0x0300, 0x036F ),
	( 6, 0x1DC0, 0x1DFF ),
	( 7, 0x0370, 0x03FF ),
	( 8, 0x2C80, 0x2CFF ),
	( 9, 0x0400, 0x04FF ),
	( 9, 0x0500, 0x052F ),
	( 9, 0x2DE0, 0x2DFF ),
	( 9, 0xA640, 0xA69F ),
	( 10, 0x0530, 0x058F ),
	( 11, 0x0590, 0x05FF ),
	( 12, 0xA500, 0xA63F ),
	( 13, 0x0600, 0x06FF ),
	( 13, 0x0750, 0x077F ),
	( 14, 0x07C0, 0x07FF ),
	( 15, 0x0900, 0x097F ),
	( 16, 0x0980, 0x09FF ),
	( 17, 0x0A00, 0x0A7F ),
	( 18, 0x0A80, 0x0AFF ),
	( 19, 0x0B00, 0x0B7F ),
	( 20, 0x0B80, 0x0BFF ),
	( 21, 0x0C00, 0x0C7F ),
	( 22, 0x0C80, 0x0CFF ),
	( 23, 0x0D00, 0x0D7F ),
	( 24, 0x0E00, 0x0E7F ),
	( 25, 0x0E80, 0x0EFF ),
	( 26, 0x10A0, 0x10FF ),
	( 26, 0x2D00, 0x2D2F ),
	( 27, 0x1B00, 0x1B7F ),
	( 28, 0x1100, 0x11FF ),
	( 29, 0x1E00, 0x1EFF ),
	( 29, 0x2C60, 0x2C7F ),
	( 29, 0xA720, 0xA7FF ),
	( 30, 0x1F00, 0x1FFF ),
	( 31, 0x2000, 0x206F ),
	( 31, 0x2E00, 0x2E7F ),
	( 32, 0x2070, 0x209F ),
	( 33, 0x20A0, 0x20CF ),
	( 34, 0x20D0, 0x20FF ),
	( 35, 0x2100, 0x214F ),
	( 36, 0x2150, 0x218F ),
	( 37, 0x2190, 0x21FF ),
	( 37, 0x27F0, 0x27FF ),
	( 37, 0x2900, 0x297F ),
	( 37, 0x2B00, 0x2BFF ),
	( 38, 0x2200, 0x22FF ),
	( 38, 0x2A00, 0x2AFF ),
	( 38, 0x27C0, 0x27EF ),
	( 38, 0x2980, 0x29FF ),
	( 39, 0x2300, 0x23FF ),
	( 40, 0x2400, 0x243F ),
	( 41, 0x2440, 0x245F ),
	( 42, 0x2460, 0x24FF ),
	( 43, 0x2500, 0x257F ),
	( 44, 0x2580, 0x259F ),
	( 45, 0x25A0, 0x25FF ),
	( 46, 0x2600, 0x26FF ),
	( 47, 0x2700, 0x27BF ),
	( 48, 0x3000, 0x303F ),
	( 49, 0x3040, 0x309F ),
	( 50, 0x30A0, 0x30FF ),
	( 50, 0x31F0, 0x31FF ),
	( 51, 0x3100, 0x312F ),
	( 51, 0x31A0, 0x31BF ),
	( 52, 0x3130, 0x318F ),
	( 53, 0xA840, 0xA87F ),
	( 54, 0x3200, 0x32FF ),
	( 55, 0x3300, 0x33FF ),
	( 56, 0xAC00, 0xD7AF ),
	( 57, 0xD800, 0xDFFF ),
	( 58, 0x10900, 0x1091F ),
	( 59, 0x4E00, 0x9FFF ),
	( 59, 0x2E80, 0x2EFF ),
	( 59, 0x2F00, 0x2FDF ),
	( 59, 0x2FF0, 0x2FFF ),
	( 59, 0x3400, 0x4DBF ),
	( 59, 0x20000, 0x2A6DF ),
	( 59, 0x3190, 0x319F ),
	( 60, 0xE000, 0xF8FF ),
	( 61, 0x31C0, 0x31EF ),
	( 61, 0xF900, 0xFAFF ),
	( 61, 0x2F800, 0x2FA1F ),
	( 62, 0xFB00, 0xFB4F ),
	( 63, 0xFB50, 0xFDFF ),
	( 64, 0xFE20, 0xFE2F ),
	( 65, 0xFE10, 0xFE1F ),
	( 65, 0xFE30, 0xFE4F ),
	( 66, 0xFE50, 0xFE6F ),
	( 67, 0xFE70, 0xFEFF ),
	( 68, 0xFF00, 0xFFEF ),
	( 69, 0xFFF0, 0xFFFF ),
	( 70, 0x0F00, 0x0FFF ),
	( 71, 0x0700, 0x074F ),
	( 72, 0x0780, 0x07BF ),
	( 73, 0x0D80, 0x0DFF ),
	( 74, 0x1000, 0x109F ),
	( 75, 0x1200, 0x137F ),
	( 75, 0x1380, 0x139F ),
	( 75, 0x2D80, 0x2DDF ),
	( 76, 0x13A0, 0x13FF ),
	( 77, 0x1400, 0x167F ),
	( 78, 0x1680, 0x169F ),
	( 79, 0x16A0, 0x16FF ),
	( 80, 0x1780, 0x17FF ),
	( 80, 0x19E0, 0x19FF ),
	( 81, 0x1800, 0x18AF ),
	( 82, 0x2800, 0x28FF ),
	( 83, 0xA000, 0xA48F ),
	( 83, 0xA490, 0xA4CF ),
	( 84, 0x1700, 0x171F ),
	( 84, 0x1720, 0x173F ),
	( 84, 0x1740, 0x175F ),
	( 84, 0x1760, 0x177F ),
	( 85, 0x10300, 0x1032F ),
	( 86, 0x10330, 0x1034F ),
	( 87, 0x10400, 0x1044F ),
	( 88, 0x1D000, 0x1D0FF ),
	( 88, 0x1D100, 0x1D1FF ),
	( 88, 0x1D200, 0x1D24F ),
	( 89, 0x1D400, 0x1D7FF ),
	( 90, 0xF0000, 0xFFFFD ),
	( 90, 0x100000, 0x10FFFD ),
	( 91, 0xFE00, 0xFE0F ),
	( 91, 0xE0100, 0xE01EF ),
	( 92, 0xE0000, 0xE007F ),
	( 93, 0x1900, 0x194F ),
	( 94, 0x1950, 0x197F ),
	( 95, 0x1980, 0x19DF ),
	( 96, 0x1A00, 0x1A1F ),
	( 97, 0x2C00, 0x2C5F ),
	( 98, 0x2D30, 0x2D7F ),
	( 99, 0x4DC0, 0x4DFF ),
	( 100, 0xA800, 0xA82F ),
	( 101, 0x10000, 0x1007F ),
	( 101, 0x10080, 0x100FF ),
	( 101, 0x10100, 0x1013F ),
	( 102, 0x10140, 0x1018F ),
	( 103, 0x10380, 0x1039F ),
	( 104, 0x103A0, 0x103DF ),
	( 105, 0x10450, 0x1047F ),
	( 106, 0x10480, 0x104AF ),
	( 107, 0x10800, 0x1083F ),
	( 108, 0x10A00, 0x10A5F ),
	( 109, 0x1D300, 0x1D35F ),
	( 110, 0x12000, 0x123FF ),
	( 110, 0x12400, 0x1247F ),
	( 111, 0x1D360, 0x1D37F ),
	( 112, 0x1B80, 0x1BBF ),
	( 113, 0x1C00, 0x1C4F ),
	( 114, 0x1C50, 0x1C7F ),
	( 115, 0xA880, 0xA8DF ),
	( 116, 0xA900, 0xA92F ),
	( 117, 0xA930, 0xA95F ),
	( 118, 0xAA00, 0xAA5F ),
	( 119, 0x10190, 0x101CF ),
	( 120, 0x101D0, 0x101FF ),
	( 121, 0x102A0, 0x102DF ),
	( 121, 0x10280, 0x1029F ),
	( 121, 0x10920, 0x1093F ),
	( 122, 0x1F030, 0x1F09F ),
	( 122, 0x1F000, 0x1F02F )
)

def ul_char_ranges(font):
	char_ranges = [0, 0, 0, 0]

	for char in font.chars:
		range = next((range for range in CHAR_RANGES if range[1] <= char.code <= range[2]), None)

		if range is not None:
			char_ranges[range[0] >> 5] |= 1 << (range[0] & 0x1F)

	if font.max_code >= 0x10000:
		char_ranges[57 >> 5] |= 1 << (57 & 0x1F)

	return char_ranges


# -- ul_code_pages --
def ul_code_pages(font):
	space_index = next((index for index, char in enumerate(font.chars) if char.code == 0x20), len(font.chars))
	ascii = int(len(font.chars) >= space_index + 0x5F and font.chars[space_index + 0x5E].code == 0x7E)
	findf = lambda unicode: int(next((char for char in font.chars if char.code == unicode), None) is not None)
	graph = findf(0x2524)
	radic = findf(0x221A)
	code_pages = [0, 0]

	# conditions from FontForge
	for char in font.chars:
		unicode = char.code

		if unicode == 0x00DE:
			code_pages[0] |= (ascii) << 0                       # 1252  Latin1
		elif unicode == 0x255A:
			code_pages[1] |= (ascii) << 30                      # 850   WE/Latin1
			code_pages[1] |= (ascii) << 31                      # 437   US
		elif unicode == 0x013D:
			code_pages[0] |= (ascii) << 1                       # 1250  Latin 2: Eastern Europe
			code_pages[1] |= (ascii & graph) << 26              # 852   Latin 2
		elif unicode == 0x0411:
			code_pages[0] |= 1 << 2                             # 1251  Cyrillic
			code_pages[1] |= (findf(0x255C) & graph) << 17      # 866   MS-DOS Russian
			code_pages[1] |= (findf(0x0405) & graph) << 25      # 855   IBM Cyrillic
		elif unicode == 0x0386:
			code_pages[0] |= 1 << 3                             # 1253  Greek
			code_pages[1] |= (findf(0x00BD) & graph) << 16      # 869   IBM Greek
			code_pages[1] |= (graph & radic) << 28              # 737   Greek; former 437 G
		elif unicode == 0x0130:
			code_pages[0] |= (ascii) << 4                       # 1254  Turkish
			code_pages[1] |= (ascii & graph) << 24              # 857   IBM Turkish
		elif unicode == 0x05D0:
			code_pages[0] |= 1 << 5                             # 1255  Hebrew
			code_pages[1] |= (graph & radic) << 21              # 862   Hebrew
		elif unicode == 0x0631:
			code_pages[0] |= 1 << 6                             # 1256  Arabic
			code_pages[1] |= (radic) << 19                      # 864   Arabic
			code_pages[1] |= (graph) << 29                      # 708   Arabic; ASMO 708
		elif unicode == 0x0157:
			code_pages[0] |= (ascii) << 7                       # 1257  Windows Baltic
			code_pages[1] |= (ascii & graph) << 27              # 775   MS-DOS Baltic
		elif unicode == 0x20AB:
			code_pages[0] |= 1 << 8                             # 1258  Vietnamese
		elif unicode == 0x0E45:
			code_pages[0] |= 1 << 16                            # 874   Thai
		elif unicode == 0x30A8:
			code_pages[0] |= 1 << 17                            # 932   JIS/Japan
		elif unicode == 0x3105:
			code_pages[0] |= 1 << 18                            # 936   Chinese: Simplified chars
		elif unicode == 0x3131:
			code_pages[0] |= 1 << 19                            # 949   Korean Wansung
		elif unicode == 0x592E:
			code_pages[0] |= 1 << 20                            # 950   Chinese: Traditional chars
		elif unicode == 0xACF4:
			code_pages[0] |= 1 << 21                            # 1361  Korean Johab
		elif unicode == 0x2030:
			code_pages[0] |= (findf(0x2211) & ascii) << 29      #       Macintosh Character Set (Roman)
		elif unicode == 0x2665:
			code_pages[0] |= (ascii) << 30                      #       OEM Character Set
		elif unicode == 0x00C5:
			code_pages[1] |= (ascii & graph & radic) << 18      # 865   MS-DOS Nordic
		elif unicode == 0x00E9:
			code_pages[1] |= (ascii & graph & radic) << 20      # 863   MS-DOS Canadian French
		elif unicode == 0x00F5:
			code_pages[1] |= (ascii & graph & radic) << 23      # 860   MS-DOS Portuguese
		elif unicode == 0x00FE:
			code_pages[1] |= (ascii & graph) << 22              # 861   MS-DOS Icelandic
		elif 0xF000 <= unicode <= 0xF0FF:
			code_pages[0] |= 1 << 31	                      #       Symbol Character Set

	return code_pages


# -- strong_rtl_flag --
RTL_RANGES = (
	( 0x05BE, 0x05BE ),
	( 0x05C0, 0x05C0 ),
	( 0x05C3, 0x05C3 ),
	( 0x05C6, 0x05C6 ),
	( 0x05D0, 0x05EA ),
	( 0x05EF, 0x05F4 ),
	( 0x0608, 0x0608 ),
	( 0x060B, 0x060B ),
	( 0x060D, 0x060D ),
	( 0x061B, 0x061C ),
	( 0x061E, 0x064A ),
	( 0x066D, 0x066F ),
	( 0x0671, 0x06D5 ),
	( 0x06E5, 0x06E6 ),
	( 0x06EE, 0x06EF ),
	( 0x06FA, 0x070D ),
	( 0x070F, 0x0710 ),
	( 0x0712, 0x072F ),
	( 0x074D, 0x07A5 ),
	( 0x07B1, 0x07B1 ),
	( 0x07C0, 0x07EA ),
	( 0x07F4, 0x07F5 ),
	( 0x07FA, 0x07FA ),
	( 0x07FE, 0x0815 ),
	( 0x081A, 0x081A ),
	( 0x0824, 0x0824 ),
	( 0x0828, 0x0828 ),
	( 0x0830, 0x083E ),
	( 0x0840, 0x0858 ),
	( 0x085E, 0x085E ),
	( 0x0860, 0x086A ),
	( 0x08A0, 0x08B4 ),
	( 0x08B6, 0x08BD ),
	( 0x200F, 0x200F ),
	( 0x202B, 0x202B ),
	( 0x202E, 0x202E ),
	( 0xFB1D, 0xFB1D ),
	( 0xFB1F, 0xFB28 ),
	( 0xFB2A, 0xFB36 ),
	( 0xFB38, 0xFB3C ),
	( 0xFB3E, 0xFB3E ),
	( 0xFB40, 0xFB41 ),
	( 0xFB43, 0xFB44 ),
	( 0xFB46, 0xFBC1 ),
	( 0xFBD3, 0xFD3D ),
	( 0xFD50, 0xFD8F ),
	( 0xFD92, 0xFDC7 ),
	( 0xFDF0, 0xFDFC ),
	( 0xFE70, 0xFE74 ),
	( 0xFE76, 0xFEFC ),
	( 0x10800, 0x10FFF ),
	( 0x1E800, 0x1EFFF ),
	(-1, 0)
)

def contains_rtl(font):
	index = 0

	for char in font.chars:
		while char.code > RTL_RANGES[index][1]:
			index += 1
			if RTL_RANGES[index][0] == -1:
				break

		if char.code >= RTL_RANGES[index][0]:
			return True

	return False


# -- post_mac_names --
POST_MAC_NAMES = (
	b'.notdef',
	b'.null',
	b'nonmarkingreturn',
	b'space',
	b'exclam',
	b'quotedbl',
	b'numbersign',
	b'dollar',
	b'percent',
	b'ampersand',
	b'quotesingle',
	b'parenleft',
	b'parenright',
	b'asterisk',
	b'plus',
	b'comma',
	b'hyphen',
	b'period',
	b'slash',
	b'zero',
	b'one',
	b'two',
	b'three',
	b'four',
	b'five',
	b'six',
	b'seven',
	b'eight',
	b'nine',
	b'colon',
	b'semicolon',
	b'less',
	b'equal',
	b'greater',
	b'question',
	b'at',
	b'A',
	b'B',
	b'C',
	b'D',
	b'E',
	b'F',
	b'G',
	b'H',
	b'I',
	b'J',
	b'K',
	b'L',
	b'M',
	b'N',
	b'O',
	b'P',
	b'Q',
	b'R',
	b'S',
	b'T',
	b'U',
	b'V',
	b'W',
	b'X',
	b'Y',
	b'Z',
	b'bracketleft',
	b'backslash',
	b'bracketright',
	b'asciicircum',
	b'underscore',
	b'grave',
	b'a',
	b'b',
	b'c',
	b'd',
	b'e',
	b'f',
	b'g',
	b'h',
	b'i',
	b'j',
	b'k',
	b'l',
	b'm',
	b'n',
	b'o',
	b'p',
	b'q',
	b'r',
	b's',
	b't',
	b'u',
	b'v',
	b'w',
	b'x',
	b'y',
	b'z',
	b'braceleft',
	b'bar',
	b'braceright',
	b'asciitilde',
	b'Adieresis',
	b'Aring',
	b'Ccedilla',
	b'Eacute',
	b'Ntilde',
	b'Odieresis',
	b'Udieresis',
	b'aacute',
	b'agrave',
	b'acircumflex',
	b'adieresis',
	b'atilde',
	b'aring',
	b'ccedilla',
	b'eacute',
	b'egrave',
	b'ecircumflex',
	b'edieresis',
	b'iacute',
	b'igrave',
	b'icircumflex',
	b'idieresis',
	b'ntilde',
	b'oacute',
	b'ograve',
	b'ocircumflex',
	b'odieresis',
	b'otilde',
	b'uacute',
	b'ugrave',
	b'ucircumflex',
	b'udieresis',
	b'dagger',
	b'degree',
	b'cent',
	b'sterling',
	b'section',
	b'bullet',
	b'paragraph',
	b'germandbls',
	b'registered',
	b'copyright',
	b'trademark',
	b'acute',
	b'dieresis',
	b'notequal',
	b'AE',
	b'Oslash',
	b'infinity',
	b'plusminus',
	b'lessequal',
	b'greaterequal',
	b'yen',
	b'mu',
	b'partialdiff',
	b'summation',
	b'product',
	b'pi',
	b'integral',
	b'ordfeminine',
	b'ordmasculine',
	b'Omega',
	b'ae',
	b'oslash',
	b'questiondown',
	b'exclamdown',
	b'logicalnot',
	b'radical',
	b'florin',
	b'approxequal',
	b'Delta',
	b'guillemotleft',
	b'guillemotright',
	b'ellipsis',
	b'nonbreakingspace',
	b'Agrave',
	b'Atilde',
	b'Otilde',
	b'OE',
	b'oe',
	b'endash',
	b'emdash',
	b'quotedblleft',
	b'quotedblright',
	b'quoteleft',
	b'quoteright',
	b'divide',
	b'lozenge',
	b'ydieresis',
	b'Ydieresis',
	b'fraction',
	b'currency',
	b'guilsinglleft',
	b'guilsinglright',
	b'fi',
	b'fl',
	b'daggerdbl',
	b'periodcentered',
	b'quotesinglbase',
	b'quotedblbase',
	b'perthousand',
	b'Acircumflex',
	b'Ecircumflex',
	b'Aacute',
	b'Edieresis',
	b'Egrave',
	b'Iacute',
	b'Icircumflex',
	b'Idieresis',
	b'Igrave',
	b'Oacute',
	b'Ocircumflex',
	b'apple',
	b'Ograve',
	b'Uacute',
	b'Ucircumflex',
	b'Ugrave',
	b'dotlessi',
	b'circumflex',
	b'tilde',
	b'macron',
	b'breve',
	b'dotaccent',
	b'ring',
	b'cedilla',
	b'hungarumlaut',
	b'ogonek',
	b'caron',
	b'Lslash',
	b'lslash',
	b'Scaron',
	b'scaron',
	b'Zcaron',
	b'zcaron',
	b'brokenbar',
	b'Eth',
	b'eth',
	b'Yacute',
	b'yacute',
	b'Thorn',
	b'thorn',
	b'minus',
	b'multiply',
	b'onesuperior',
	b'twosuperior',
	b'threesuperior',
	b'onehalf',
	b'onequarter',
	b'threequarters',
	b'franc',
	b'Gbreve',
	b'gbreve',
	b'Idotaccent',
	b'Scedilla',
	b'scedilla',
	b'Cacute',
	b'cacute',
	b'Ccaron',
	b'ccaron',
	b'dcroat'
)

def post_mac_names():
	return list(POST_MAC_NAMES)
