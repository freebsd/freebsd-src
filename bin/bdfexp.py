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

from collections import OrderedDict

import fnutil
import fncli
import fnio
import bdf

# -- Font --
class Font(bdf.Font):
	def __init__(self):
		bdf.Font.__init__(self)
		self.min_width = 0  # used in proportional()
		self.avg_width = 0


	def _expand(self, char):
		if char.dwidth.x >= 0:
			if char.bbx.xoff >= 0:
				width = max(char.bbx.xoff + char.bbx.width, char.dwidth.x)
				dst_xoff = char.bbx.xoff
				exp_xoff = 0
			else:
				width = max(char.bbx.width, char.dwidth.x - char.bbx.xoff)
				dst_xoff = 0
				exp_xoff = char.bbx.xoff
		else:
			rev_xoff = char.bbx.xoff + char.bbx.width

			if rev_xoff <= 0:
				width = -min(char.dwidth.x, char.bbx.xoff)
				dst_xoff = width + char.bbx.xoff
				exp_xoff = -width
			else:
				width = max(char.bbx.width, rev_xoff - char.dwidth.x)
				dst_xoff = width - char.bbx.width
				exp_xoff = rev_xoff - width

		height = self.bbx.height

		if width == char.bbx.width and height == char.bbx.height:
			return

		src_row_size = char.bbx.row_size()
		dst_row_size = (width + 7) >> 3
		dst_ymax = self.px_ascender - char.bbx.yoff
		dst_ymin = dst_ymax - char.bbx.height
		copy_row = (dst_xoff & 7) == 0
		dst_data = bytearray(dst_row_size * height)

		for dst_y in range(dst_ymin, dst_ymax):
			src_byte_no = (dst_y - dst_ymin) * src_row_size
			dst_byte_no = dst_y * dst_row_size + (dst_xoff >> 3)

			if copy_row:
				dst_data[dst_byte_no : dst_byte_no + src_row_size] = \
					char.data[src_byte_no : src_byte_no + src_row_size]
			else:
				src_bit_no = 7
				dst_bit_no = 7 - (dst_xoff & 7)

				for _ in range(0, char.bbx.width):
					if char.data[src_byte_no] & (1 << src_bit_no):
						dst_data[dst_byte_no] |= (1 << dst_bit_no)

					if src_bit_no > 0:
						src_bit_no -= 1
					else:
						src_bit_no = 7
						src_byte_no += 1

					if dst_bit_no > 0:
						dst_bit_no -= 1
					else:
						dst_bit_no = 7
						dst_byte_no += 1

		char.bbx = bdf.BBX(width, height, exp_xoff, self.bbx.yoff)
		char.props.set('BBX', char.bbx)
		char.data = dst_data


	def expand(self):
		# PREXPAND / VERTICAL
		ascent = self.props.get('FONT_ASCENT')
		descent = self.props.get('FONT_DESCENT')
		px_ascent = 0 if ascent is None else fnutil.parse_dec('FONT_ASCENT', ascent, 0, bdf.DPARSE_LIMIT)
		px_descent = 0 if descent is None else fnutil.parse_dec('FONT_DESCENT', descent, 0, bdf.DPARSE_LIMIT)

		for char in self.chars:
			px_ascent = max(px_ascent, char.bbx.height + char.bbx.yoff)
			px_descent = max(px_descent, -char.bbx.yoff)

		self.bbx.height = px_ascent + px_descent
		self.bbx.yoff = -px_descent

		# EXPAND / HORIZONTAL
		total_width = 0
		self.min_width = self.chars[0].bbx.width

		for char in self.chars:
			self._expand(char)
			self.min_width = min(self.min_width, char.bbx.width)
			self.bbx.width = max(self.bbx.width, char.bbx.width)
			self.bbx.xoff = min(self.bbx.xoff, char.bbx.xoff)
			total_width += char.bbx.width

		self.avg_width = round(total_width / len(self.chars))
		self.props.set('FONTBOUNDINGBOX', self.bbx)


	def expand_x(self):
		for char in self.chars:
			if char.dwidth.x != char.bbx.width:
				char.swidth.x = round(char.bbx.width * 1000 / self.bbx.height)
				char.props.set('SWIDTH', char.swidth)
				char.dwidth.x = char.bbx.width
				char.props.set('DWIDTH', char.dwidth)

			char.bbx.xoff = 0
			char.props.set('BBX', char.bbx)

		self.bbx.xoff = 0
		self.props.set('FONTBOUNDINGBOX', self.bbx)


	def expand_y(self):
		props = OrderedDict((
			('FONT_ASCENT', self.px_ascender),
			('FONT_DESCENT', -self.px_descender),
			('PIXEL_SIZE', self.bbx.height)
		))

		for [name, value] in props.items():
			if self.props.get(name) is not None:
				self.props.set(name, value)

		self.xlfd[bdf.XLFD.PIXEL_SIZE] = bytes(str(self.bbx.height), 'ascii')
		self.props.set('FONT', b'-'.join(self.xlfd))


	@property
	def proportional(self):
		return self.bbx.width > self.min_width or bdf.Font.proportional.fget(self)  # pylint: disable=no-member

	@property
	def px_ascender(self):
		return self.bbx.height + self.bbx.yoff

	@property
	def px_descender(self):
		return self.bbx.yoff


	def _read(self, input):
		bdf.Font._read(self, input)
		self.expand()
		return self

	@staticmethod
	def read(input):
		return Font()._read(input)  # pylint: disable=protected-access


# -- Params --
class Params(fncli.Params):
	def __init__(self):
		fncli.Params.__init__(self)
		self.expand_x = False
		self.expand_y = False
		self.output_name = None


# -- Options --
HELP = ('' +
	'usage: bdfexp [-X] [-Y] [-o OUTPUT] [INPUT]\n' +
	'Expand BDF font bitmaps\n' +
	'\n' +
	'  -X           zero xoffs, set character S/DWIDTH.X from the output\n' +
	'               BBX.width if needed\n' +
	'  -Y           enlarge FONT_ASCENT, FONT_DESCENT and PIXEL_SIZE to\n' +
	'               cover the font bounding box, if needed\n' +
	'  -o OUTPUT    output file (default = stdout)\n' +
	'  --help       display this help and exit\n' +
	'  --version    display the program version and license, and exit\n' +
	'  --excstk     display the exception stack on error\n' +
	'\n' +
	'The input must be a BDF 2.1 font with unicode encoding.\n')

VERSION = 'bdfexp 1.62, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

class Options(fncli.Options):
	def __init__(self):
		fncli.Options.__init__(self, ['-o'], HELP, VERSION)


	def parse(self, name, value, params):
		if name == '-X':
			params.expand_x = True
		elif name == '-Y':
			params.expand_y = True
		elif name == '-o':
			params.output_name = value
		else:
			self.fallback(name, params)


# -- Main --
def main_program(nonopt, parsed):
	if len(nonopt) > 1:
		raise Exception('invalid number of arguments, try --help')

	# READ INPUT
	font = fnio.read_file(nonopt[0] if nonopt else None, Font.read)

	# EXTRA ACTIONS
	if parsed.expand_x:
		font.expand_x()

	if parsed.expand_y:
		font.expand_y()

	# WRITE OUTPUT
	fnio.write_file(parsed.output_name, lambda ofs: font.write(ofs))


if __name__ == '__main__':
	fncli.start('bdfexp.py', Options(), Params(), main_program)
