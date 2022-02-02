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
import fncli
import fnio
import bdf

# -- Params --
class Params(fncli.Params):  # pylint: disable=too-many-instance-attributes
	def __init__(self):
		fncli.Params.__init__(self)
		self.ascii_chars = True
		self.bbx_exceeds = True
		self.dupl_codes = -1
		self.extra_bits = True
		self.attributes = True
		self.dupl_names = -1
		self.dupl_props = True
		self.common_slant = True
		self.common_weight = True
		self.xlfd_fontnm = True
		self.ywidth_zero = True


# -- Options --
HELP = ('' +
	'usage: bdfcheck [options] [INPUT...]\n' +
	'Check BDF font(s) for various problems\n' +
	'\n' +
	'  -A          disable non-ascii characters check\n' +
	'  -B          disable BBX exceeding FONTBOUNDINGBOX checks\n' +
	'  -c/-C       enable/disable duplicate character codes check\n' +
	'              (default = enabled for registry ISO10646)\n' +
	'  -E          disable extra bits check\n' +
	'  -I          disable ATTRIBUTES check\n' +
	'  -n/-N       enable duplicate character names check\n' +
	'              (default = enabled for registry ISO10646)\n' +
	'  -P          disable duplicate properties check\n' +
	'  -S          disable common slant check\n' +
	'  -W          disable common weight check\n' +
	'  -X          disable XLFD font name check\n' +
	'  -Y          disable zero WIDTH Y check\n' +
	'  --help      display this help and exit\n' +
	'  --version   display the program version and license, and exit\n' +
	'  --excstk    display the exception stack on error\n' +
	'\n' +
	'File directives: COMMENT bdfcheck --enable|disable-<check-name>\n' +
	'  (also available as long command line options)\n' +
	'\n' +
	'Check names: ascii-chars, bbx-exceeds, duplicate-codes, extra-bits,\n' +
	'  attributes, duplicate-names, duplicate-properties, common-slant,\n' +
	'  common-weight, xlfd-font, ywidth-zero\n' +
	'\n' +
	'The input BDF(s) must be v2.1 with unicode encoding.\n')

VERSION = 'bdfcheck 1.62, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

class Options(fncli.Options):
	def __init__(self):
		fncli.Options.__init__(self, [], HELP, VERSION)


	def parse(self, name, directive, params):
		value = name.startswith('--enable') or name[1].islower()

		if name in ['-A', '--enable-ascii-chars', '--disable-ascii-chars']:
			params.ascii_chars = value
		elif name in ['-B', '--enable-bbx-exceeds', '--disable-bbx-exceeds']:
			params.bbx_exceeds = value
		elif name in ['-c', '-C', '--enable-duplicate-codes', '--disable-duplicate-codes']:
			params.dupl_codes = value
		elif name in ['-E', '--enable-extra-bits', '--disable-extra-bits']:
			params.extra_bits = value
		elif name in ['-I', '--enable-attributes', '--disable-attributes']:
			params.attributes = value
		elif name in ['-n', '-N', '--enable-duplicate-names', '--disable-duplicate-names']:
			params.dupl_names = value
		elif name in ['-P', '--enable-duplicate-properties', '--disable-duplicate-properties']:
			params.dupl_props = value
		elif name in ['-S', '--enable-common-slant', '--disable-common-slant']:
			params.common_slant = value
		elif name in ['-W', '--enable-common-weight', '--disable-common-weight']:
			params.common_weight = value
		elif name in ['-X', '--enable-xlfd-font', '--disable-xlfd-font']:
			params.xlfd_fontnm = value
		elif name in ['-Y', '--enable-ywidth-zero', '--disable-ywidth-zero']:
			params.ywidth_zero = value
		else:
			return directive is not True and self.fallback(name, params)

		return directive is not True or name.startswith('--')


# -- DupMap --
class DupMap(OrderedDict):
	def __init__(self, prefix, severity, descript, quote):
		OrderedDict.__init__(self)
		self.prefix = prefix
		self.descript = descript
		self.severity = severity
		self.quote = quote


	def check(self):
		for value, lines in self.items():
			if len(lines) > 1:
				text = 'duplicate %s %s at lines' % (self.descript, str(value))

				for index, line in enumerate(lines):
					text += ' ' if index == 0 else ' and ' if index == len(lines) - 1 else ', '
					text += str(line)

				fnutil.message(self.prefix, self.severity, text)


	def push(self, value, line_no):
		try:
			self[value].append(line_no)
		except KeyError:
			self[value] = [line_no]


# -- InputFileStream --
@unique
class MODE(IntEnum):
	META = 0
	PROPS = 1
	BITMAP = 2

class InputFileStream(fnio.InputFileStream):
	def __init__(self, file_name, parsed):
		fnio.InputFileStream.__init__(self, file_name)
		self.parsed = parsed
		self.mode = MODE.META
		self.proplocs = DupMap(self.location(), 'error', 'property', '')
		self.namelocs = DupMap(self.location(), 'warning', 'character name', '"')
		self.codelocs = DupMap(self.location(), 'warning', 'encoding', '')
		self.handlers = [
			(b'STARTCHAR', lambda value: self.append_name(value)),
			(b'ENCODING', lambda value: self.append_code(value)),
			(b'SWIDTH', lambda value: self.check_width('SWIDTH', value, bdf.Width.parse_s)),
			(b'DWIDTH', lambda value: self.check_width('DWIDTH', value, bdf.Width.parse_d)),
			(b'BBX', lambda value: self.set_last_box(value)),
			(b'BITMAP', lambda _: self.set_mode(MODE.BITMAP)),
			(b'SIZE', InputFileStream.check_size),
			(b'ATTRIBUTES', lambda value: self.check_attr(value)),
			(b'STARTPROPERTIES', lambda _: self.set_mode(MODE.PROPS)),
			(b'FONTBOUNDINGBOX', lambda value: self.set_font_box(value)),
		]
		self.xlfd_name = False
		self.last_box = None
		self.font_box = None
		self.options = Options()


	def append(self, option, valocs, value):
		if option:
			valocs.push(str(value, 'ascii'), self.line_no)


	def append_code(self, value):
		fnutil.parse_dec('encoding', value)
		self.append(self.parsed.dupl_codes, self.codelocs, value)


	def append_name(self, value):
		self.append(self.parsed.dupl_names, self.namelocs, b'"%s"' % value)


	def check_width(self, name, value, parse):
		if self.parsed.ywidth_zero and parse(name, value).y != 0:
			fnutil.warning(self.location(), 'non-zero %s Y' % name)


	def set_font_box(self, value):
		self.font_box = bdf.BBX.parse('FONTBOUNDINGBOX', value)


	def set_last_box(self, value):
		bbx = bdf.BBX.parse('BBX', value)

		if self.parsed.bbx_exceeds:
			exceeds = []

			if bbx.xoff < self.font_box.xoff:
				exceeds.append('xoff < FONTBOUNDINGBOX xoff')

			if bbx.yoff < self.font_box.yoff:
				exceeds.append('yoff < FONTBOUNDINGBOX yoff')

			if bbx.width > self.font_box.width:
				exceeds.append('width > FONTBOUNDINGBOX width')

			if bbx.height > self.font_box.height:
				exceeds.append('height > FONTBOUNDINGBOX height')

			for exceed in exceeds:
				fnutil.message(self.location(), '', exceed)

		self.last_box = bbx


	def set_mode(self, new_mode):
		self.mode = new_mode


	def check(self):
		self.process(bdf.Font.read)
		self.proplocs.check()
		self.namelocs.check()
		self.codelocs.check()


	@staticmethod
	def check_size(value):
		words = fnutil.split_words('SIZE', value, 3)
		fnutil.parse_dec('point size', words[0], 1, None)
		fnutil.parse_dec('x resolution', words[1], 1, None)
		fnutil.parse_dec('y resolution', words[2], 1, None)


	def check_attr(self, value):
		if not re.fullmatch(br'[\dA-Fa-f]{4}', value):
			raise Exception('ATTRIBUTES must be 4 hex-encoded characters')

		if self.parsed.attributes:
			fnutil.warning(self.location(), 'ATTRIBUTES may cause problems with freetype')


	def check_font(self, value):
		xlfd = value[4:].lstrip().split(b'-', 15)

		if len(xlfd) == 15 and xlfd[0] == b'':
			unicode = (xlfd[bdf.XLFD.CHARSET_REGISTRY].upper() == b'ISO10646')

			if self.parsed.dupl_codes == -1:
				self.parsed.dupl_codes = unicode

			if self.parsed.dupl_names == -1:
				self.parsed.dupl_names = unicode

			if self.parsed.common_weight:
				weight = str(xlfd[bdf.XLFD.WEIGHT_NAME], 'ascii')
				compare = weight.lower()
				consider = 'Bold' if 'bold' in compare else 'Normal'

				if compare in ['medium', 'regular']:
					compare = 'normal'

				if compare != consider.lower():
					fnutil.warning(self.location(), 'weight "%s" may be considered %s' % (weight, consider))

			if self.parsed.common_slant:
				slant = str(xlfd[bdf.XLFD.SLANT], 'ascii')
				consider = 'Italic' if re.search('^[IO]', slant) else 'Regular'

				if not re.fullmatch('[IOR]', slant):
					fnutil.warning(self.location(), 'slant "%s" may be considered %s' % (slant, consider))

		else:
			if self.parsed.xlfd_fontnm:
				fnutil.warning(self.location(), 'non-XLFD font name')

			value = b'FONT --------------'

		return value


	def check_prop(self, line):
		match = re.fullmatch(br'(\w+)\s+([-\d"].*)', line)

		if not match:
			raise Exception('invalid property format')

		name = match.group(1)
		value = match.group(2)

		if value.startswith(b'"'):
			if len(value) < 2 or not value.endswith(b'"'):
				raise Exception('no closing double quote')
			if re.search(b'[^"]"[^"]', value[1 : len(value) - 1]):
				raise Exception('unescaped double quote')
		else:
			fnutil.parse_dec('value', value, None, None)

		self.append(self.parsed.dupl_props, self.proplocs, name)
		return b'P%d 1' % self.line_no


	def check_bitmap(self, line):
		if len(line) != self.last_box.row_size() * 2:
			raise Exception('invalid bitmap length')

		data = codecs.decode(line, 'hex')

		if self.parsed.extra_bits:
			check_x = (self.last_box.width - 1) | 7
			last_byte = data[len(data) - 1]
			bit_no = 7 - (self.last_box.width & 7)

			for x in range(self.last_box.width, check_x + 1):
				if last_byte & (1 << bit_no):
					fnutil.warning(self.location(), 'extra bit(s) starting with x=%d' % x)
					break
				bit_no -= 1


	def check_line(self, line):
		if re.search(b'[^\t\f\v\x20-\xff]', line):
			raise Exception('control character(s)')

		if self.parsed.ascii_chars and re.search(b'[\x7f-\xff]', line):
			fnutil.warning(self.location(), 'non-ascii character(s)')

		if self.mode == MODE.META:
			if not self.xlfd_name and line.startswith(b'FONT'):
				line = self.check_font(line)
				self.xlfd_name = True
			else:
				for handler in self.handlers:
					if line.startswith(handler[0]):
						handler[1](line[len(handler[0]):].lstrip())
						break
		elif self.mode == MODE.PROPS:
			if line.startswith(b'ENDPROPERTIES'):
				self.mode = MODE.META
			else:
				line = self.check_prop(line)
		else:  # MODE.BITMAP
			if line.startswith(b'ENDCHAR'):
				self.mode = MODE.META
			else:
				self.check_bitmap(line)

		return line


	def read_check(self, line, callback):
		match = re.search(br'^COMMENT\s*bdfcheck\s+(-.*)$', line)

		if match and not self.options.parse(str(match[1], 'ascii'), True, self.parsed):
			raise Exception('invalid bdfcheck directive')

		line = callback(line)
		return self.check_line(line) if line is not None else None


	def read_lines(self, callback):
		return fnio.InputFileStream.read_lines(self, lambda line: self.read_check(line, callback))


# -- Main --
def main_program(nonopt, parsed):
	for input_name in nonopt or [None]:
		InputFileStream(input_name, parsed).check()


if __name__ == '__main__':
	fncli.start('bdfcheck.py', Options(), Params(), main_program)
