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

import re
import codecs
from collections import OrderedDict
from enum import IntEnum, unique

import fnutil

# -- Width --
DPARSE_LIMIT = 512
SPARSE_LIMIT = 32000

class Width:
	def __init__(self, x, y):
		self.x = x
		self.y = y


	@staticmethod
	def parse(name, value, limit):
		words = fnutil.split_words(name, value, 2)
		return Width(fnutil.parse_dec(name + '.x', words[0], -limit, limit),
			fnutil.parse_dec(name + '.y', words[1], -limit, limit))


	@staticmethod
	def parse_s(name, value):
		return Width.parse(name, value, SPARSE_LIMIT)


	@staticmethod
	def parse_d(name, value):
		return Width.parse(name, value, DPARSE_LIMIT)


	def __str__(self):
		return '%d %d' % (self.x, self.y)


# -- BXX --
class BBX:
	def __init__(self, width, height, xoff, yoff):
		self.width = width
		self.height = height
		self.xoff = xoff
		self.yoff = yoff


	@staticmethod
	def parse(name, value):
		words = fnutil.split_words(name, value, 4)
		return BBX(fnutil.parse_dec('width', words[0], 1, DPARSE_LIMIT),
			fnutil.parse_dec('height', words[1], 1, DPARSE_LIMIT),
			fnutil.parse_dec('bbxoff', words[2], -DPARSE_LIMIT, DPARSE_LIMIT),
			fnutil.parse_dec('bbyoff', words[3], -DPARSE_LIMIT, DPARSE_LIMIT))


	def row_size(self):
		return (self.width + 7) >> 3


	def __str__(self):
		return '%d %d %d %d' % (self.width, self.height, self.xoff, self.yoff)


# -- Props --
def skip_comments(line):
	return None if line[:7] == b'COMMENT' else line


class Props(OrderedDict):
	def __iter__(self):
		return self.items().__iter__()


	def read(self, input, name, callback=None):
		return self.parse(input.read_lines(skip_comments), name, callback)


	def parse(self, line, name, callback=None):
		if not line or not line.startswith(bytes(name, 'ascii')):
			raise Exception(name + ' expected')

		value = line[len(name):].lstrip()
		self[name] = value
		return value if callback is None else callback(name, value)


	def set(self, name, value):
		self[name] = value if isinstance(value, (bytes, bytearray)) else bytes(str(value), 'ascii')


# -- Base --
class Base:
	def __init__(self):
		self.props = Props()
		self.bbx = None


# -- Char
HEX_BYTES = (48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70)

class Char(Base):
	def __init__(self):
		Base.__init__(self)
		self.code = -1
		self.swidth = None
		self.dwidth = None
		self.data = None


	def bitmap(self):
		bitmap = ''
		row_size = self.bbx.row_size()

		for index in range(0, len(self.data), row_size):
			bitmap += self.data[index : index + row_size].hex() + '\n'

		return bytes(bitmap, 'ascii').upper()


	def _read(self, input):
		# HEADER
		self.props.read(input, 'STARTCHAR')
		self.code = self.props.read(input, 'ENCODING', fnutil.parse_dec)
		self.swidth = self.props.read(input, 'SWIDTH', Width.parse_s)
		self.dwidth = self.props.read(input, 'DWIDTH', Width.parse_d)
		self.bbx = self.props.read(input, 'BBX', BBX.parse)
		line = input.read_lines(skip_comments)

		if line and line.startswith(b'ATTRIBUTES'):
			self.props.parse(line, 'ATTRIBUTES')
			line = input.read_lines(skip_comments)

		# BITMAP
		if self.props.parse(line, 'BITMAP'):
			raise Exception('BITMAP expected')

		row_len = self.bbx.row_size() * 2
		self.data = bytearray()

		for _ in range(0, self.bbx.height):
			line = input.read_lines(skip_comments)

			if not line:
				raise Exception('bitmap data expected')

			if len(line) == row_len:
				self.data += codecs.decode(line, 'hex')
			else:
				raise Exception('invalid bitmap length')

		# FINAL
		if input.read_lines(skip_comments) != b'ENDCHAR':
			raise Exception('ENDCHAR expected')

		return self


	@staticmethod
	def read(input):
		return Char()._read(input)  # pylint: disable=protected-access


	def write(self, output):
		for [name, value] in self.props:
			output.write_prop(name, value)

		output.write_line(self.bitmap() + b'ENDCHAR')


# -- Font --
@unique
class XLFD(IntEnum):
	FOUNDRY = 1
	FAMILY_NAME = 2
	WEIGHT_NAME = 3
	SLANT = 4
	SETWIDTH_NAME = 5
	ADD_STYLE_NAME = 6
	PIXEL_SIZE = 7
	POINT_SIZE = 8
	RESOLUTION_X = 9
	RESOLUTION_Y = 10
	SPACING = 11
	AVERAGE_WIDTH = 12
	CHARSET_REGISTRY = 13
	CHARSET_ENCODING = 14

CHARS_MAX = 65535

class Font(Base):
	def __init__(self):
		Base.__init__(self)
		self.xlfd = []
		self.chars = []
		self.default_code = -1


	@property
	def bold(self):
		return b'bold' in self.xlfd[XLFD.WEIGHT_NAME].lower()


	@property
	def italic(self):
		return self.xlfd[XLFD.SLANT] in [b'I', b'O']


	@property
	def proportional(self):
		return self.xlfd[XLFD.SPACING] == b'P'


	def _read(self, input):
		# HEADER
		line = input.read_line()

		if self.props.parse(line, 'STARTFONT') != b'2.1':
			raise Exception('STARTFONT 2.1 expected')

		self.xlfd = self.props.read(input, 'FONT', lambda name, value: value.split(b'-', 15))

		if len(self.xlfd) != 15 or self.xlfd[0] != b'':
			raise Exception('non-XLFD font names are not supported')

		self.props.read(input, 'SIZE')
		self.bbx = self.props.read(input, 'FONTBOUNDINGBOX', BBX.parse)
		line = input.read_lines(skip_comments)

		if line and line.startswith(b'STARTPROPERTIES'):
			num_props = self.props.parse(line, 'STARTPROPERTIES', fnutil.parse_dec)

			for _ in range(0, num_props):
				line = input.read_lines(skip_comments)

				if line is None:
					raise Exception('property expected')

				match = re.fullmatch(br'(\w+)\s+([-\d"].*)', line)

				if not match:
					raise Exception('invalid property format')

				name = str(match.group(1), 'ascii')
				value = match.group(2)

				if self.props.get(name) is not None:
					raise Exception('duplicate property')

				if name == 'DEFAULT_CHAR':
					self.default_code = fnutil.parse_dec(name, value)

				self.props[name] = value

			if self.props.read(input, 'ENDPROPERTIES') != b'':
				raise Exception('ENDPROPERTIES expected')

			line = input.read_lines(skip_comments)

		# GLYPHS
		num_chars = fnutil.parse_dec('CHARS', self.props.parse(line, 'CHARS'), 1, CHARS_MAX)

		for _ in range(0, num_chars):
			self.chars.append(Char.read(input))

		if next((char.code for char in self.chars if char.code == self.default_code), -1) != self.default_code:
			raise Exception('invalid DEFAULT_CHAR')

		# FINAL
		if input.read_lines(skip_comments) != b'ENDFONT':
			raise Exception('ENDFONT expected')

		if input.read_line() is not None:
			raise Exception('garbage after ENDFONT')

		return self


	@staticmethod
	def read(input):
		return Font()._read(input)  # pylint: disable=protected-access


	def write(self, output):
		for [name, value] in self.props:
			output.write_prop(name, value)

		for char in self.chars:
			char.write(output)

		output.write_line(b'ENDFONT')
