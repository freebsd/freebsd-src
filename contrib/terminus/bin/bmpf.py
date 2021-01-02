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

import bdf


class Char:
	def __init__(self, code, name, width, data):
		self.code = code
		self.name = name
		self.width = width
		self.data = data


	@staticmethod
	def from_bdf(char, fbbox):
		delta_yoff = char.bbx.yoff - fbbox.yoff  # ~DSB

		if delta_yoff < 0:
			raise Exception('char %d: BBX yoff < FONTBOUNDINGBOX yoff' % char.code)

		if char.dwidth.x >= 0:
			if char.bbx.xoff >= 0:
				width = max(char.bbx.width + char.bbx.xoff, char.dwidth.x)
				dst_xoff = char.bbx.xoff
			else:
				width = max(char.bbx.width, char.dwidth.x - char.bbx.xoff)
				dst_xoff = 0
		else:
			dst_xoff = max(char.bbx.xoff - char.dwidth.x, 0)
			width = char.bbx.width + dst_xoff

		if width > bdf.WIDTH_MAX:
			raise Exception('char %d: output width > %d' % (char.code, bdf.WIDTH_MAX))

		height = fbbox.height
		src_row_size = char.bbx.row_size()
		dst_row_size = (width + 7) >> 3
		dst_ymax = height - delta_yoff
		dst_ymin = dst_ymax - char.bbx.height
		compat_row = dst_xoff == 0 and width >= char.bbx.width

		if compat_row and src_row_size == dst_row_size and dst_ymin == 0 and dst_ymax == height:
			data = char.data
		elif dst_ymin < 0:
			raise Exception('char %d: start row %d' % (char.code, dst_ymin))
		elif compat_row:
			src_byte_no = 0
			data = bytearray(dst_ymin * dst_row_size)
			line_fill = bytes(dst_row_size - src_row_size)

			for dst_y in range(dst_ymin, dst_ymax):
				data += char.data[src_byte_no : src_byte_no + src_row_size] + line_fill
				src_byte_no += src_row_size

			data += bytes(delta_yoff * dst_row_size)
		else:
			data = bytearray(dst_row_size * height)

			for dst_y in range(dst_ymin, dst_ymax):
				src_byte_no = (dst_y - dst_ymin) * src_row_size
				dst_byte_no = dst_y * dst_row_size + (dst_xoff >> 3)

				src_bit_no = 7
				dst_bit_no = 7 - (dst_xoff & 7)

				for _ in range(0, char.bbx.width):
					if char.data[src_byte_no] & (1 << src_bit_no):
						data[dst_byte_no] |= (1 << dst_bit_no)

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

		return Char(char.code, char.props.get('STARTCHAR'), width, data)


	def row_size(self):
		return (self.width + 7) >> 3


	def write(self, output, max_width, yoffset):
		output.write_line(b'STARTCHAR %s\nENCODING %d' % (self.name, self.code))
		output.write_line(b'SWIDTH %d 0\nDWIDTH %d 0' % (round(self.width * 1000 / max_width), self.width))
		output.write_line(b'BBX %d %d 0 %d' % (self.width, len(self.data) / self.row_size(), yoffset))
		output.write_line(b'BITMAP\n' + bdf.Char.bitmap(self.data, self.row_size()) + b'ENDCHAR')


class Font(bdf.Font):
	def __init__(self):
		bdf.Font.__init__(self)
		self.min_width = bdf.WIDTH_MAX
		self.avg_width = 0


	def _read(self, input):
		bdf.Font._read(self, input)
		self.chars = [Char.from_bdf(char, self.bbx) for char in self.chars]
		self.bbx.xoff = 0
		total_width = 0

		for char in self.chars:
			self.min_width = min(self.min_width, char.width)
			self.bbx.width = max(self.bbx.width, char.width)
			total_width += char.width

		self.avg_width = round(total_width / len(self.chars))
		self.props.set('FONTBOUNDINGBOX', bytes(str(self.bbx), 'ascii'))
		return self


	@staticmethod
	def read(input):
		return Font()._read(input)  # pylint: disable=protected-access


	def get_proportional(self):
		return int(self.bbx.width > self.min_width)


	def write(self, output):
		for [name, value] in self.props:
			output.write_prop(name, value)

		for char in self.chars:
			char.write(output, self.bbx.width, self.bbx.yoff)

		output.write_line(b'ENDFONT')
