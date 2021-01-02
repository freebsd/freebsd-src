#
# Copyright (c) 2018 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

import re
import codecs
from enum import IntEnum, unique

import fnutil


WIDTH_MAX = 127
HEIGHT_MAX = 255
SWIDTH_MAX = 32000

class Width:
	def __init__(self, x, y):
		self.x = x
		self.y = y


	@staticmethod
	def _parse(name, value, limit_x, limit_y):
		words = fnutil.split_words(name, value, 2)
		return Width(fnutil.parse_dec('width x', words[0], -limit_x, limit_x),
			fnutil.parse_dec('width y', words[1], -limit_y, limit_y))


	@staticmethod
	def parse_s(value):
		return Width._parse('SWIDTH', value, SWIDTH_MAX, SWIDTH_MAX)


	@staticmethod
	def parse_d(value):
		return Width._parse('DWIDTH', value, WIDTH_MAX, HEIGHT_MAX)


OFFSET_MIN = -128
OFFSET_MAX = 127

class BBX:
	def __init__(self, width, height, xoff, yoff):
		self.width = width
		self.height = height
		self.xoff = xoff
		self.yoff = yoff


	@staticmethod
	def parse(name, value):
		words = fnutil.split_words(name, value, 4)
		return BBX(fnutil.parse_dec('width', words[0], 1, WIDTH_MAX),
			fnutil.parse_dec('height', words[1], 1, HEIGHT_MAX),
			fnutil.parse_dec('bbxoff', words[2], -WIDTH_MAX, WIDTH_MAX),
			fnutil.parse_dec('bbyoff', words[3], -WIDTH_MAX, WIDTH_MAX))


	def row_size(self):
		return (self.width + 7) >> 3


	def __str__(self):
		return '%d %d %d %d' % (self.width, self.height, self.xoff, self.yoff)


class Props:
	def __init__(self):
		self.names = []
		self.values = []


	def add(self, name, value):
		self.names.append(name)
		self.values.append(value)


	def clone(self):
		props = Props()
		props.names = self.names[:]
		props.values = self.values[:]
		return props


	def get(self, name):
		try:
			return self.values[self.names.index(name)]
		except ValueError:
			return None


	class Iter:
		def __init__(self, props):
			self.index = 0
			self.props = props


		def __next__(self):
			if self.index == len(self.props.names):
				raise StopIteration

			result = (self.props.names[self.index], self.props.values[self.index])
			self.index += 1
			return result


	def __iter__(self):
		return Props.Iter(self)


	def parse(self, line, name, callback=None):
		if not line or not line.startswith(bytes(name, 'ascii')):
			raise Exception(name + ' expected')

		value = line[len(name):].lstrip()
		self.add(name, value)
		return value if callback is None else callback(name, value)


	def set(self, name, value):
		try:
			self.values[self.names.index(name)] = value
		except ValueError:
			self.add(name, value)


class Base:
	def __init__(self):
		self.props = Props()
		self.bbx = None
		self.finis = []


	def keep_comments(self, line):
		if not line.startswith(b'COMMENT'):
			return line

		self.props.add('', line)
		return None


	def keep_finishes(self, line):
		self.finis.append(line)
		return None if line.startswith(b'COMMENT') else line


class Char(Base):
	def __init__(self):
		Base.__init__(self)
		self.code = -1
		self.swidth = None
		self.dwidth = None
		self.data = None


	@staticmethod
	def bitmap(data, row_size):
		bitmap = ''

		for index in range(0, len(data), row_size):
			bitmap += data[index : index + row_size].hex() + '\n'

		return bytes(bitmap, 'ascii').upper()


	def _read(self, input):
		# HEADER
		read_next = lambda: input.read_lines(lambda line: self.keep_comments(line))
		read_prop = lambda name, callback=None: self.props.parse(read_next(), name, callback)

		read_prop('STARTCHAR')
		self.code = read_prop('ENCODING', fnutil.parse_dec)
		self.swidth = read_prop('SWIDTH', lambda _, value: Width.parse_s(value))
		self.dwidth = read_prop('DWIDTH', lambda _, value: Width.parse_d(value))
		self.bbx = read_prop('BBX', BBX.parse)
		line = read_next()

		if line and line.startswith(b'ATTRIBUTES'):
			self.props.parse(line, 'ATTRIBUTES')
			line = read_next()

		# BITMAP
		if self.props.parse(line, 'BITMAP') != b'':
			raise Exception('BITMAP expected')

		row_len = self.bbx.row_size() * 2
		bitmap = b''

		for _ in range(0, self.bbx.height):
			line = read_next()

			if not line:
				raise Exception('bitmap data expected')

			if len(line) == row_len:
				bitmap += line
			else:
				raise Exception('invalid bitmap length')

		# FINAL
		if input.read_lines(lambda line: self.keep_finishes(line)) != b'ENDCHAR':
			raise Exception('ENDCHAR expected')

		self.data = codecs.decode(bitmap, 'hex')  # no spaces allowed
		return self


	@staticmethod
	def read(input):
		return Char()._read(input)  # pylint: disable=protected-access


	def write(self, output):
		for [name, value] in self.props:
			output.write_prop(name, value)

		output.write_line(Char.bitmap(self.data, self.bbx.row_size()) + b'\n'.join(self.finis))


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


	def get_ascent(self):
		ascent = self.props.get('FONT_ASCENT')

		if ascent is not None:
			return fnutil.parse_dec('FONT_ASCENT', ascent, -HEIGHT_MAX, HEIGHT_MAX)

		return self.bbx.height + self.bbx.yoff


	def get_bold(self):
		return int(b'bold' in self.xlfd[XLFD.WEIGHT_NAME].lower())


	def get_italic(self):
		return int(re.search(b'^[IO]', self.xlfd[XLFD.SLANT]) is not None)


	def _read(self, input):
		# HEADER
		read_next = lambda: input.read_lines(lambda line: self.keep_comments(line))
		read_prop = lambda name, callback=None: self.props.parse(read_next(), name, callback)
		line = input.read_lines(Font.skip_empty)

		if self.props.parse(line, 'STARTFONT') != b'2.1':
			raise Exception('STARTFONT 2.1 expected')

		self.xlfd = read_prop('FONT', lambda name, value: value.split(b'-', 15))

		if len(self.xlfd) != 15 or self.xlfd[0] != b'':
			raise Exception('non-XLFD font names are not supported')

		read_prop('SIZE')
		self.bbx = read_prop('FONTBOUNDINGBOX', BBX.parse)
		line = read_next()

		if line and line.startswith(b'STARTPROPERTIES'):
			num_props = self.props.parse(line, 'STARTPROPERTIES', fnutil.parse_dec)

			for _ in range(0, num_props):
				line = read_next()

				if not line:
					raise Exception('property expected')

				match = re.fullmatch(br'(\w+)\s+([-\d"].*)', line)

				if not match:
					raise Exception('invalid property format')

				name = str(match.group(1), 'ascii')
				value = match.group(2)

				if name == 'DEFAULT_CHAR':
					self.default_code = fnutil.parse_dec(name, value)

				self.props.add(name, value)

			if read_prop('ENDPROPERTIES') != b'':
				raise Exception('ENDPROPERTIES expected')

			line = read_next()

		# GLYPHS
		num_chars = self.props.parse(line, 'CHARS', lambda name, value: fnutil.parse_dec(name, value, 1, CHARS_MAX))

		for _ in range(0, num_chars):
			self.chars.append(Char.read(input))

		if next((char.code for char in self.chars if char.code == self.default_code), -1) != self.default_code:
			raise Exception('invalid DEFAULT_CHAR')

		# FINAL
		if input.read_lines(lambda line: self.keep_finishes(line)) != b'ENDFONT':
			raise Exception('ENDFONT expected')

		if input.read_lines(Font.skip_empty):
			raise Exception('garbage after ENDFONT')

		return self


	@staticmethod
	def read(input):
		return Font()._read(input)  # pylint: disable=protected-access


	@staticmethod
	def skip_empty(line):
		return line if line else None


	def write(self, output):
		for [name, value] in self.props:
			output.write_prop(name, value)

		for char in self.chars:
			char.write(output)

		output.write_line(b'\n'.join(self.finis))
