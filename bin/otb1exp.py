#
# Copyright (C) 2017-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

import struct
import codecs
import math
from datetime import datetime, timezone
from itertools import groupby
from enum import IntEnum, unique
from collections import OrderedDict

import fnutil
import fncli
import bdf
import bdfexp
import otb1get

# -- Table --
class Table:
	def __init__(self, name):
		self.data = bytearray(0)
		self.table_name = name


	def check_size(self, size):
		if size != self.size:
			raise Exception('internal error: %s size = %d instead of %d' % (self.table_name, self.size, size))


	def checksum(self):
		cksum = 0
		data = self.data + self.padding

		for offset in range(0, self.size, 4):
			cksum += struct.unpack('>L', data[offset : offset + 4])[0]

		return cksum & 0xFFFFFFFF


	def pack(self, format, value, name):
		try:
			return struct.pack(format, value)
		except struct.error as ex:
			raise Exception('%s.%s: %s' % (self.table_name, name, str(ex)))


	@property
	def size(self):
		return len(self.data)


	@property
	def padding(self):
		return bytes(((self.size + 1) & 3) ^ 1)


	def rewrite_uint32(self, value, offset):
		self.data[offset : offset + 4] = struct.pack('>L', value)


	def write(self, data):
		self.data += data


	def write_int8(self, value, name):
		self.data += self.pack('b', value, name)


	def write_uint8(self, value, name):
		self.data += self.pack('B', value, name)


	def write_int16(self, value, name):
		self.data += self.pack('>h', value, name)


	def write_uint16(self, value, name):
		self.data += self.pack('>H', value, name)


	def write_uint32(self, value, name):
		self.data += self.pack('>L', value, name)


	def write_uint64(self, value, name):
		self.data += self.pack('>Q', value, name)


	def write_fixed(self, value, name):
		self.data += self.pack('>l', round(value * 65536), name)


	def write_table(self, table):
		self.data += table.data


# -- Params --
EM_SIZE_MIN = 64
EM_SIZE_MAX = 16384
EM_SIZE_DEFAULT = 1024

class Params(fncli.Params):  # pylint: disable=too-many-instance-attributes
	def __init__(self):
		fncli.Params.__init__(self)
		self.created = datetime.now(timezone.utc)
		self.modified = self.created
		self.dir_hint = 0
		self.em_size = EM_SIZE_DEFAULT
		self.line_gap = 0
		self.low_ppem = 0
		self.encoding = 'utf_8'
		self.w_lang_id = 0x0409
		self.x_max_extent = True
		self.single_loca = False
		self.post_names = False


# -- Options --
class Options(fncli.Options):
	def __init__(self, need_args, help_text, version_text):
		fncli.Options.__init__(self, need_args + ['-d', '-e', '-g', '-l', '-E', '-W'], help_text, version_text)


	def parse(self, name, value, params):
		if name == '-d':
			params.dir_hint = fnutil.parse_dec('DIR-HINT', value, -2, 2)
		elif name == '-e':
			params.em_size = fnutil.parse_dec('EM-SIZE', value, EM_SIZE_MIN, EM_SIZE_MAX)
		elif name == '-g':
			params.line_gap = fnutil.parse_dec('LINE-GAP', value, 0, EM_SIZE_MAX << 1)
		elif name == '-l':
			params.low_ppem = fnutil.parse_dec('LOW-PPEM', value, 1, bdf.DPARSE_LIMIT)
		elif name == '-E':
			params.encoding = value
		elif name == '-W':
			params.w_lang_id = fnutil.parse_hex('WLANG-ID', value, 0, 0x7FFF)
		elif name == '-X':
			params.x_max_extent = False
		elif name == '-L':
			params.single_loca = True
		elif name == '-P':
			params.post_names = True
		else:
			self.fallback(name, params)


# -- Font --
class Font(bdfexp.Font):
	def __init__(self, params):
		bdfexp.Font.__init__(self)
		self.params = params
		self.em_ascender = 0
		self.em_descender = 0
		self.em_max_width = 0
		self.mac_style = 0
		self.line_size = 0


	@property
	def bmp_only(self):
		return self.max_code <= fnutil.UNICODE_BMP_MAX

	@property
	def created(self):
		return Font.sfntime(self.params.created)

	def decode(self, data):
		return codecs.decode(data, self.params.encoding)

	def em_scale(self, value, divisor=0):
		return round(value * self.params.em_size / (divisor or self.bbx.height))

	def em_scale_width(self, base):
		return self.em_scale(base.bbx.width)

	@property
	def italic_angle(self):
		value = self.props.get('ITALIC_ANGLE')  # must be integer
		return fnutil.parse_dec('ITALIC_ANGLE', value, -45, 45) if value else -11.5 if self.italic else 0

	@property
	def max_code(self):
		return self.chars[-1].code

	@property
	def min_code(self):
		return self.chars[0].code

	@property
	def modified(self):
		return Font.sfntime(self.params.modified)


	def prepare(self):
		self.chars.sort(key=lambda c: c.code)
		self.chars = [next(elem[1]) for elem in groupby(self.chars, key=lambda c: c.code)]
		self.props.set('CHARS', len(self.chars))
		self.em_ascender = self.em_scale(self.px_ascender)
		self.em_descender = self.em_ascender - self.params.em_size
		self.em_max_width = self.em_scale_width(self)
		self.mac_style = int(self.bold) + (int(self.italic) << 1)
		self.line_size = self.em_scale(round(self.bbx.height / 17) or 1)


	def _read(self, input):
		bdfexp.Font._read(self, input)
		self.prepare()
		return self

	@staticmethod
	def read(input, params):  # pylint: disable=arguments-differ
		return Font(params)._read(input)  # pylint: disable=protected-access


	@staticmethod
	def sfntime(stamp):
		return math.floor((stamp - datetime(1904, 1, 1, tzinfo=timezone.utc)).total_seconds())

	@property
	def underline_position(self):
		return round((self.em_descender + self.line_size) / 2)

	@property
	def x_max_extent(self):
		return self.em_max_width if self.params.x_max_extent else 0


# -- BDAT --
BDAT_HEADER_SIZE = 4
BDAT_METRIC_SIZE = 5

class BDAT(Table):
	def __init__(self, font):
		Table.__init__(self, 'EBDT')
		# header
		self.write_fixed(2, 'version')
		# format 1 data
		for char in font.chars:
			self.write_uint8(font.bbx.height, 'height')
			self.write_uint8(char.bbx.width, 'width')
			self.write_int8(0, 'bearingX')
			self.write_int8(font.px_ascender, 'bearingY')
			self.write_uint8(char.bbx.width, 'advance')
			self.write(char.data)  # imageData


	@staticmethod
	def get_char_size(char):
		return BDAT_METRIC_SIZE + len(char.data)


# -- BLOC --
BLOC_TABLE_SIZE_OFFSET = 12
BLOC_PREFIX_SIZE = 0x38      # header 0x08 + 1 bitmapSizeTable * 0x30
BLOC_INDEX_ARRAY_SIZE = 8    # 1 index record * 0x08

class BLOC(Table):
	def __init__(self, font):
		Table.__init__(self, 'EBLC')
		# header
		self.write_fixed(2, 'version')
		self.write_uint32(1, 'numSizes')
		# bitmapSizeTable
		self.write_uint32(BLOC_PREFIX_SIZE, 'indexSubTableArrayOffset')
		self.write_uint32(0, 'indexTableSize')                    # adjusted later
		self.write_uint32(1, 'numberOfIndexSubTables')
		self.write_uint32(0, 'colorRef')
		# hori
		self.write_int8(font.px_ascender, 'hori ascender')
		self.write_int8(font.px_descender, 'hori descender')
		self.write_uint8(font.bbx.width, 'hori widthMax')
		self.write_int8(1, 'hori caretSlopeNumerator')
		self.write_int8(0, 'hori caretSlopeDenominator')
		self.write_int8(0, 'hori caretOffset')
		self.write_int8(0, 'hori minOriginSB')
		self.write_int8(0, 'hori minAdvanceSB')
		self.write_int8(font.px_ascender, 'hori maxBeforeBL')
		self.write_int8(font.px_descender, 'hori minAfterBL')
		self.write_int16(0, 'hori padd')
		# vert
		self.write_int8(0, 'vert ascender')
		self.write_int8(0, 'vert descender')
		self.write_uint8(0, 'vert widthMax')
		self.write_int8(0, 'vert caretSlopeNumerator')
		self.write_int8(0, 'vert caretSlopeDenominator')
		self.write_int8(0, 'vert caretOffset')
		self.write_int8(0, 'vert minOriginSB')
		self.write_int8(0, 'vert minAdvanceSB')
		self.write_int8(0, 'vert maxBeforeBL')
		self.write_int8(0, 'vert minAfterBL')
		self.write_int16(0, 'vert padd')
		# (bitmapSizeTable)
		self.write_uint16(0, 'startGlyphIndex')
		self.write_uint16(len(font.chars) - 1, 'endGlyphIndex')
		self.write_uint8(font.bbx.height, 'ppemX')
		self.write_uint8(font.bbx.height, 'ppemY')
		self.write_uint8(1, 'bitDepth')
		self.write_uint8(1, 'flags')                              # small metrics are horizontal
		# indexSubTableArray
		self.write_uint16(0, 'firstGlyphIndex')
		self.write_uint16(len(font.chars) - 1, 'lastGlyphIndex')
		self.write_uint32(BLOC_INDEX_ARRAY_SIZE, 'additionalOffsetToIndexSubtable')
		# indexSubtableHeader
		self.write_uint16(1 if font.proportional else 2, 'indexFormat')
		self.write_uint16(1, 'imageFormat')                       # BDAT -> small metrics, byte-aligned
		self.write_uint32(BDAT_HEADER_SIZE, 'imageDataOffset')
		# indexSubtable data
		if font.proportional:
			offset = 0

			for char in font.chars:
				self.write_uint32(offset, 'offsetArray[]')
				offset += BDAT.get_char_size(char)

			self.write_uint32(offset, 'offsetArray[]')
		else:
			self.write_uint32(BDAT.get_char_size(font.chars[0]), 'imageSize')
			self.write_uint8(font.bbx.height, 'height')
			self.write_uint8(font.bbx.width, 'width')
			self.write_int8(0, 'horiBearingX')
			self.write_int8(font.px_ascender, 'horiBearingY')
			self.write_uint8(font.bbx.width, 'horiAdvance')
			self.write_int8(-(font.bbx.width >> 1), 'vertBearingX')
			self.write_int8(0, 'vertBearingY')
			self.write_uint8(font.bbx.height, 'vertAdvance')
		# adjust
		self.rewrite_uint32(self.size - BLOC_PREFIX_SIZE, BLOC_TABLE_SIZE_OFFSET)


# -- OS/2 --
OS_2_TABLE_SIZE = 96

class OS_2(Table):  # pylint: disable=invalid-name
	def __init__(self, font):
		Table.__init__(self, 'OS/2')
		# Version 4
		x_avg_char_width = font.em_scale(font.avg_width)  # otb1get.x_avg_char_width(font)
		ul_char_ranges = otb1get.ul_char_ranges(font)
		ul_code_pages = otb1get.ul_code_pages(font) if font.bmp_only else [0, 0]
		# mostly from FontForge
		script_xsize = font.em_scale(30, 100)
		script_ysize = font.em_scale(40, 100)
		subscript_yoff = script_ysize >> 1
		xfactor = math.tan(font.italic_angle * math.pi / 180)
		subscript_xoff = 0  # stub, no overlapping characters yet
		superscript_yoff = font.em_ascender - script_ysize
		superscript_xoff = -round(xfactor * superscript_yoff)
		# write
		self.write_uint16(4, 'version')
		self.write_int16(x_avg_char_width, 'xAvgCharWidth')
		self.write_uint16(700 if font.bold else 400, 'usWeightClass')
		self.write_uint16(5, 'usWidthClass')                      # medium
		self.write_int16(0, 'fsType')
		self.write_int16(script_xsize, 'ySubscriptXSize')
		self.write_int16(script_ysize, 'ySubscriptYSize')
		self.write_int16(subscript_xoff, 'ySubscriptXOffset')
		self.write_int16(subscript_yoff, 'ySubscriptYOffset')
		self.write_int16(script_xsize, 'ySuperscriptXSize')
		self.write_int16(script_ysize, 'ySuperscriptYSize')
		self.write_int16(superscript_xoff, 'ySuperscriptXOffset')
		self.write_int16(superscript_yoff, 'ySuperscriptYOffset')
		self.write_int16(font.line_size, 'yStrikeoutSize')
		self.write_int16(font.em_scale(25, 100), 'yStrikeoutPosition')
		self.write_int16(0, 'sFamilyClass')                       # no classification
		self.write_uint8(2, 'bFamilyType')                        # text and display
		self.write_uint8(0, 'bSerifStyle')                        # any
		self.write_uint8(8 if font.bold else 6, 'bWeight')
		self.write_uint8(3 if font.proportional else 9, 'bProportion')
		self.write_uint8(0, 'bContrast')
		self.write_uint8(0, 'bStrokeVariation')
		self.write_uint8(0, 'bArmStyle')
		self.write_uint8(0, 'bLetterform')
		self.write_uint8(0, 'bMidline')
		self.write_uint8(0, 'bXHeight')
		self.write_uint32(ul_char_ranges[0], 'ulCharRange1')
		self.write_uint32(ul_char_ranges[1], 'ulCharRange2')
		self.write_uint32(ul_char_ranges[2], 'ulCharRange3')
		self.write_uint32(ul_char_ranges[3], 'ulCharRange4')
		self.write_uint32(0x586F7334, 'achVendID')                # 'Xos4'
		self.write_uint16(OS_2.fs_selection(font), 'fsSelection')
		self.write_uint16(min(font.min_code, fnutil.UNICODE_BMP_MAX), 'firstChar')
		self.write_uint16(min(font.max_code, fnutil.UNICODE_BMP_MAX), 'lastChar')
		self.write_int16(font.em_ascender, 'sTypoAscender')
		self.write_int16(font.em_descender, 'sTypoDescender')
		self.write_int16(font.params.line_gap, 'sTypoLineGap')
		self.write_uint16(font.em_ascender, 'usWinAscent')
		self.write_uint16(-font.em_descender, 'usWinDescent')
		self.write_uint32(ul_code_pages[0], 'ulCodePageRange1')
		self.write_uint32(ul_code_pages[1], 'ulCodePageRange2')
		self.write_int16(font.em_scale(font.px_ascender * 0.6), 'sxHeight')    # stub
		self.write_int16(font.em_scale(font.px_ascender * 0.8), 'sCapHeight')  # stub
		self.write_uint16(OS_2.default_char(font), 'usDefaultChar')
		self.write_uint16(OS_2.break_char(font), 'usBreakChar')
		self.write_uint16(1, 'usMaxContext')
		# check
		self.check_size(OS_2_TABLE_SIZE)


	@staticmethod
	def break_char(font):
		return 0x20 if next((char for char in font.chars if char.code == 0x20), None) else font.min_code


	@staticmethod
	def default_char(font):
		if font.default_code != -1 and font.default_code <= fnutil.UNICODE_BMP_MAX:
			return font.default_code

		return 0 if font.min_code == 0 else font.max_code


	@staticmethod
	def fs_selection(font):
		fs_selection = int(font.bold) * 5 + int(font.italic)
		return fs_selection if fs_selection != 0 else 0x40 if font.xlfd[bdf.XLFD.SLANT] == 'R' else 0


# -- cmap --
CMAP_4_PREFIX_SIZE = 12
CMAP_4_FORMAT_SIZE = 16
CMAP_4_SEGMENT_SIZE = 8

CMAP_12_PREFIX_SIZE = 20
CMAP_12_FORMAT_SIZE = 16
CMAP_12_GROUP_SIZE = 12

class CMapRange:
	def __init__(self, glyph_index=0, start_code=0, final_code=-2):
		self.glyph_index = glyph_index
		self.start_code = start_code
		self.final_code = final_code


	@property
	def id_delta(self):
		return (self.glyph_index - self.start_code) & 0xFFFF


class CMAP(Table):
	def __init__(self, font):
		Table.__init__(self, 'cmap')
		# make ranges
		ranges = []
		range = CMapRange()
		index = -1

		for char in font.chars:
			index += 1
			code = char.code

			if code == range.final_code + 1:
				range.final_code += 1
			else:
				range = CMapRange(index, code, code)
				ranges.append(range)
		# write
		if font.bmp_only:
			if font.max_code < 0xFFFF:
				ranges.append(CMapRange(0, 0xFFFF, 0xFFFF))

			self.write_format_4(ranges)
		else:
			self.write_format_12(ranges)


	def write_format_4(self, ranges):
		# index
		self.write_uint16(0, 'version')
		self.write_uint16(1, 'numberSubtables')
		# encoding subtables index
		self.write_uint16(3, 'platformID')                        # Microsoft
		self.write_uint16(1, 'platformSpecificID')                # Unicode BMP (UCS-2)
		self.write_uint32(CMAP_4_PREFIX_SIZE, 'offset')           # for Unicode BMP (UCS-2)
		# cmap format 4
		seg_count = len(ranges)
		subtable_size = CMAP_4_FORMAT_SIZE + seg_count * CMAP_4_SEGMENT_SIZE
		search_range = 2 << math.floor(math.log2(seg_count))

		self.write_uint16(4, 'format')
		self.write_uint16(subtable_size, 'length')
		self.write_uint16(0, 'language')                          # none/independent
		self.write_uint16(seg_count * 2, 'segCountX2')
		self.write_uint16(search_range, 'searchRange')
		self.write_uint16(int(math.log2(search_range / 2)), 'entrySelector')
		self.write_uint16((seg_count * 2) - search_range, 'rangeShift')
		for range in ranges:
			self.write_uint16(range.final_code, 'endCode')
		self.write_uint16(0, 'reservedPad')
		for range in ranges:
			self.write_uint16(range.start_code, 'startCode')
		for range in ranges:
			self.write_uint16(range.id_delta, 'idDelta')
		for _ in ranges:
			self.write_uint16(0, 'idRangeOffset')
		# check
		self.check_size(CMAP_4_PREFIX_SIZE + subtable_size)


	def write_format_12(self, ranges):
		# index
		self.write_uint16(0, 'version')
		self.write_uint16(2, 'numberSubtables')
		# encoding subtables
		self.write_uint16(0, 'platformID')                        # Unicode
		self.write_uint16(4, 'platformSpecificID')                # Unicode 2.0+ full range
		self.write_uint32(CMAP_12_PREFIX_SIZE, 'offset')          # for Unicode 2.0+ full range
		self.write_uint16(3, 'platformID')                        # Microsoft
		self.write_uint16(10, 'platformSpecificID')               # Unicode UCS-4
		self.write_uint32(CMAP_12_PREFIX_SIZE, 'offset')          # for Unicode UCS-4
		# cmap format 12
		subtable_size = CMAP_12_FORMAT_SIZE + len(ranges) * CMAP_12_GROUP_SIZE

		self.write_fixed(12, 'format')
		self.write_uint32(subtable_size, 'length')
		self.write_uint32(0, 'language')                          # none/independent
		self.write_uint32(len(ranges), 'nGroups')
		for range in ranges:
			self.write_uint32(range.start_code, 'startCharCode')
			self.write_uint32(range.final_code, 'endCharCode')
			self.write_uint32(range.glyph_index, 'startGlyphID')
		# check
		self.check_size(CMAP_12_PREFIX_SIZE + subtable_size)


# -- glyf --
class GLYF(Table):
	def __init__(self, _font):
		Table.__init__(self, 'glyf')


# -- head --
HEAD_TABLE_SIZE = 54
HEAD_CHECKSUM_OFFSET = 8

class HEAD(Table):
	def __init__(self, font):
		Table.__init__(self, 'head')
		self.write_fixed(1, 'version')
		self.write_fixed(1, 'fontRevision')
		self.write_uint32(0, 'checksumAdjustment')                # adjusted later
		self.write_uint32(0x5F0F3CF5, 'magicNumber')
		self.write_uint16(HEAD.flags(font), 'flags')
		self.write_uint16(font.params.em_size, 'unitsPerEm')
		self.write_uint64(font.created, 'created')
		self.write_uint64(font.modified, 'modified')
		self.write_int16(0, 'xMin')
		self.write_int16(font.em_descender, 'yMin')
		self.write_int16(font.em_max_width, 'xMax')
		self.write_int16(font.em_ascender, 'yMax')
		self.write_uint16(font.mac_style, 'macStyle')
		self.write_uint16(font.params.low_ppem or font.bbx.height, 'lowestRecPPEM')
		self.write_int16(font.params.dir_hint, 'fontDirectionHint')
		self.write_int16(0, 'indexToLocFormat')                   # short
		self.write_int16(0, 'glyphDataFormat')                    # current
		# check
		self.check_size(HEAD_TABLE_SIZE)


	@staticmethod
	def flags(font):
		return 0x20B if otb1get.contains_rtl(font) else 0x0B      # y0 base, x0 lsb, scale int


# -- hhea --
HHEA_TABLE_SIZE = 36

class HHEA(Table):
	def __init__(self, font):
		Table.__init__(self, 'hhea')
		self.write_fixed(1, 'version')
		self.write_int16(font.em_ascender, 'ascender')
		self.write_int16(font.em_descender, 'descender')
		self.write_int16(font.params.line_gap, 'lineGap')
		self.write_uint16(font.em_max_width, 'advanceWidthMax')
		self.write_int16(0, 'minLeftSideBearing')
		self.write_int16(0, 'minRightSideBearing')
		self.write_int16(font.x_max_extent, 'xMaxExtent')
		self.write_int16(100 if font.italic else 1, 'caretSlopeRise')
		self.write_int16(20 if font.italic else 0, 'caretSlopeRun')
		self.write_int16(0, 'caretOffset')
		self.write_int16(0, 'reserved')
		self.write_int16(0, 'reserved')
		self.write_int16(0, 'reserved')
		self.write_int16(0, 'reserved')
		self.write_int16(0, 'metricDataFormat')                   # current
		self.write_uint16(len(font.chars), 'numOfLongHorMetrics')
		# check
		self.check_size(HHEA_TABLE_SIZE)


# -- hmtx --
class HMTX(Table):
	def __init__(self, font):
		Table.__init__(self, 'hmtx')
		for char in font.chars:
			self.write_uint16(font.em_scale_width(char), 'advanceWidth')
			self.write_int16(0, 'leftSideBearing')


# -- loca --
class LOCA(Table):
	def __init__(self, font):
		Table.__init__(self, 'loca')
		if not font.params.single_loca:
			for _ in font.chars:
				self.write_uint16(0, 'offset')
		self.write_uint16(0, 'offset')


# -- maxp --
MAXP_TABLE_SIZE = 32

class MAXP(Table):
	def __init__(self, font):
		Table.__init__(self, 'maxp')
		self.write_fixed(1, 'version')
		self.write_uint16(len(font.chars), 'numGlyphs')
		self.write_uint16(0, 'maxPoints')
		self.write_uint16(0, 'maxContours')
		self.write_uint16(0, 'maxComponentPoints')
		self.write_uint16(0, 'maxComponentContours')
		self.write_uint16(2, 'maxZones')
		self.write_uint16(0, 'maxTwilightPoints')
		self.write_uint16(1, 'maxStorage')
		self.write_uint16(1, 'maxFunctionDefs')
		self.write_uint16(0, 'maxInstructionDefs')
		self.write_uint16(64, 'maxStackElements')
		self.write_uint16(0, 'maxSizeOfInstructions')
		self.write_uint16(0, 'maxComponentElements')
		self.write_uint16(0, 'maxComponentDepth')
		# check
		self.check_size(MAXP_TABLE_SIZE)


# -- name --
@unique  # pylint: disable=invalid-name
class NAME_ID(IntEnum):  # pylint: disable=invalid-name
	COPYRIGHT = 0
	FONT_FAMILY = 1
	FONT_SUBFAMILY = 2
	UNIQUE_SUBFAMILY = 3
	FULL_FONT_NAME = 4
	LICENSE = 14

NAME_HEADER_SIZE = 6
NAME_RECORD_SIZE = 12

class NAME(Table):
	def __init__(self, font):
		Table.__init__(self, 'name')
		# compute names
		names = OrderedDict()
		copyright = font.props.get('COPYRIGHT')

		if copyright is not None:
			names[NAME_ID.COPYRIGHT] = fnutil.unquote(copyright)

		family = font.xlfd[bdf.XLFD.FAMILY_NAME]
		style = [b'Regular', b'Bold', b'Italic', b'Bold Italic'][font.mac_style]

		names[NAME_ID.FONT_FAMILY] = family
		names[NAME_ID.FONT_SUBFAMILY] = style
		names[NAME_ID.UNIQUE_SUBFAMILY] = b'%s %s bitmap height %d' % (family, style, font.bbx.height)
		names[NAME_ID.FULL_FONT_NAME] = b'%s %s' % (family, style)

		license = font.props.get('LICENSE')
		notice = font.props.get('NOTICE')

		if license is None and notice is not None and b'license' in notice.lower():
			license = notice

		if license is not None:
			names[NAME_ID.LICENSE] = fnutil.unquote(license)

		# header
		count = len(names) * (1 + 1)  # Unicode + Microsoft
		string_offset = NAME_HEADER_SIZE + NAME_RECORD_SIZE * count

		self.write_uint16(0, 'format')
		self.write_uint16(count, 'count')
		self.write_uint16(string_offset, 'stringOffset')
		# name records / create values
		values = Table('name')

		for [name_id, bstr] in names.items():
			s = font.decode(bstr)
			value = codecs.encode(s, 'utf_16_be')
			bmp = font.bmp_only and len(value) == len(s) * 2
			# Unicode
			self.write_uint16(0, 'platformID')                  # Unicode
			self.write_uint16(3 if bmp else 4, 'platformSpecificID')
			self.write_uint16(0, 'languageID')                  # none
			self.write_uint16(name_id, 'nameID')
			self.write_uint16(len(value), 'length')             # in bytes
			self.write_uint16(values.size, 'offset')
			# Microsoft
			self.write_uint16(3, 'platformID')                  # Microsoft
			self.write_uint16(1 if bmp else 10, 'platformSpecificID')
			self.write_uint16(font.params.w_lang_id, 'languageID')
			self.write_uint16(name_id, 'nameID')
			self.write_uint16(len(value), 'length')             # in bytes
			self.write_uint16(values.size, 'offset')
			# value
			values.write(value)

		# write values
		self.write_table(values)
		# check
		self.check_size(string_offset + values.size)


# -- post --
POST_TABLE_SIZE = 32

class POST(Table):
	def __init__(self, font):
		Table.__init__(self, 'post')
		self.write_fixed(2 if font.params.post_names else 3, 'format')
		self.write_fixed(font.italic_angle, 'italicAngle')
		self.write_int16(font.underline_position, 'underlinePosition')
		self.write_int16(font.line_size, 'underlineThickness')
		self.write_uint32(0 if font.proportional else 1, 'isFixedPitch')
		self.write_uint32(0, 'minMemType42')
		self.write_uint32(0, 'maxMemType42')
		self.write_uint32(0, 'minMemType1')
		self.write_uint32(0, 'maxMemType1')
		# names
		if font.params.post_names:
			self.write_uint16(len(font.chars), 'numberOfGlyphs')
			post_names = otb1get.post_mac_names()
			post_mac_count = len(post_names)

			for name in [char.props['STARTCHAR'] for char in font.chars]:
				if name in post_names:
					self.write_uint16(post_names.index(name), 'glyphNameIndex')
				else:
					self.write_uint16(len(post_names), 'glyphNameIndex')
					post_names.append(name)

			for name in post_names[post_mac_count:]:
				self.write_uint8(len(name), 'glyphNameLength')
				self.write(name)
		# check
		else:
			self.check_size(POST_TABLE_SIZE)


# -- SFNT --
SFNT_HEADER_SIZE = 12
SFNT_RECORD_SIZE = 16
SFNT_SUBTABLES = (BDAT, BLOC, OS_2, CMAP, GLYF, HEAD, HHEA, HMTX, LOCA, MAXP, NAME, POST)

class SFNT(Table):
	def __init__(self, font):
		Table.__init__(self, 'SFNT')
		# create tables
		tables = []
		for ctor in SFNT_SUBTABLES:
			tables.append(ctor(font))
		# header
		num_tables = len(tables)
		entry_selector = math.floor(math.log2(num_tables))
		search_range = 16 << entry_selector
		content_offset = SFNT_HEADER_SIZE + num_tables * SFNT_RECORD_SIZE
		offset = content_offset
		content = Table('SFNT')
		head_checksum_offset = -1

		self.write_fixed(1, 'sfntVersion')
		self.write_uint16(num_tables, 'numTables')
		self.write_uint16(search_range, 'searchRange')
		self.write_uint16(entry_selector, 'entrySelector')
		self.write_uint16(num_tables * 16 - search_range, 'rangeShift')
		# table records / create content
		for table in tables:
			self.write(bytes(table.table_name, 'ascii'))
			self.write_uint32(table.checksum(), 'checkSum')
			self.write_uint32(offset, 'offset')
			self.write_uint32(len(table.data), 'length')
			# create content
			if table.table_name == 'head':
				head_checksum_offset = offset + HEAD_CHECKSUM_OFFSET

			padded_data = table.data + table.padding
			content.write(padded_data)
			offset += len(padded_data)
		# write content
		self.write_table(content)
		# check
		self.check_size(content_offset + len(content.data))
		# adjust
		if head_checksum_offset != -1:
			self.rewrite_uint32((0xB1B0AFBA - self.checksum()) & 0xFFFFFFFF, head_checksum_offset)
