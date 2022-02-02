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

import fnutil
import fncli
import fnio
import bdf
import bdfexp

# -- Params --
class Params(fncli.Params):
	def __init__(self):
		fncli.Params.__init__(self)
		self.char_set = -1
		self.min_char = -1
		self.fnt_family = 0
		self.output_name = None


# -- Options --
HELP = ('' +
	'usage: bdftofnt [-c CHARSET] [-m MINCHAR] [-f FAMILY] [-o OUTPUT] [INPUT]\n' +
	'Convert a BDF font to Windows FNT\n' +
	'\n' +
	'  -c CHARSET   fnt character set (default = 0, see wingdi.h ..._CHARSET)\n' +
	'  -m MINCHAR   fnt minimum character code (8-bit CP decimal, not unicode)\n' +
	'  -f FAMILY    fnt family: DontCare, Roman, Swiss, Modern or Decorative\n' +
	'  -o OUTPUT    output file (default = stdout, may not be a terminal)\n' +
	'  --help       display this help and exit\n' +
	'  --version    display the program version and license, and exit\n' +
	'  --excstk     display the exception stack on error\n' +
	'\n' +
	'The input must be a BDF 2.1 font with unicode encoding.\n')

VERSION = 'bdftofnt 1.62, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

FNT_FAMILIES = ['DontCare', 'Roman', 'Swiss', 'Modern', 'Decorative']

class Options(fncli.Options):
	def __init__(self):
		fncli.Options.__init__(self, ['-c', '-m', '-f', '-o'], HELP, VERSION)


	def parse(self, name, value, params):
		if name == '-c':
			params.char_set = fnutil.parse_dec('CHARSET', value, 0, 255)
		elif name == '-m':
			params.min_char = fnutil.parse_dec('MINCHAR', value, 0, 255)
		elif name == '-f':
			if value in FNT_FAMILIES:
				params.fnt_family = FNT_FAMILIES.index(value)
			else:
				raise Exception('invalid FAMILY')
		elif name == '-o':
			params.output_name = value
		else:
			self.fallback(name, params)


# -- Main --
FNT_HEADER_SIZE = 118
FNT_CHARSETS = [238, 204, 0, 161, 162, 177, 178, 186, 163]

def main_program(nonopt, parsed):
	if len(nonopt) > 1:
		raise Exception('invalid number of arguments, try --help')

	char_set = parsed.char_set
	min_char = parsed.min_char

	# READ INPUT
	ifs = fnio.InputFileStream(nonopt[0] if nonopt else None)
	font = ifs.process(bdfexp.Font.read)

	# COMPUTE
	if char_set == -1:
		encoding = font.xlfd[bdf.XLFD.CHARSET_ENCODING]

		if re.fullmatch(b'(cp)?125[0-8]', encoding.lower()):
			char_set = FNT_CHARSETS[int(encoding[-1:])]
		else:
			char_set = 255

	try:
		num_chars = len(font.chars)

		if num_chars > 256:
			raise Exception('too many characters, the maximum is 256')

		if min_char == -1:
			if num_chars in [192, 256]:
				min_char = 256 - num_chars
			else:
				min_char = font.chars[0].code

		max_char = min_char + num_chars - 1

		if max_char >= 256:
			raise Exception('the maximum character code is too big, (re)specify -m')

		# HEADER
		vtell = FNT_HEADER_SIZE + (num_chars + 1) * 4
		bits_offset = vtell
		ctable = []
		width_bytes = 0

		# CTABLE/GLYPHS
		for char in font.chars:
			row_size = char.bbx.row_size()
			ctable.append(char.bbx.width)
			ctable.append(vtell)
			vtell += row_size * font.bbx.height
			width_bytes += row_size

		if vtell > 0xFFFF:
			raise Exception('too much character data')

		# SENTINEL
		sentinel = 2 - width_bytes % 2
		ctable.append(sentinel * 8)
		ctable.append(vtell)
		vtell += sentinel * font.bbx.height
		width_bytes += sentinel

		if width_bytes > 0xFFFF:
			raise Exception('the total character width is too big')

	except Exception as ex:
		ex.message = ifs.location() + getattr(ex, 'message', str(ex))
		raise

	# WRITE
	def write_fnt(output):
		# HEADER
		family = font.xlfd[bdf.XLFD.FAMILY_NAME]
		copyright = font.props.get('COPYRIGHT')
		copyright = fnutil.unquote(copyright)[:60] if copyright is not None else b''

		output.write16(0x0200)                                              # font version
		output.write32(vtell + len(family) + 1)                             # total size
		output.write_zstr(copyright, 60 - len(copyright))
		output.write16(0)                                                   # gdi, device type
		output.write16(round(font.bbx.height * 72 / 96))
		output.write16(96)                                                  # vertical resolution
		output.write16(96)                                                  # horizontal resolution
		output.write16(font.px_ascender)                                    # base line
		output.write16(0)                                                   # internal leading
		output.write16(0)                                                   # external leading
		output.write8(int(font.italic))
		output.write8(0)                                                    # underline
		output.write8(0)                                                    # strikeout
		output.write16(700 if font.bold else 400)
		output.write8(char_set)
		output.write16(0 if font.proportional else font.bbx.width)
		output.write16(font.bbx.height)
		output.write8((parsed.fnt_family << 4) + int(font.proportional))
		output.write16(font.avg_width)
		output.write16(font.bbx.width)
		output.write8(min_char)
		output.write8(max_char)

		default_index = max_char - min_char
		break_index = 0

		if font.default_code != -1:
			default_index = next(index for index, char in enumerate(font.chars) if char.code == font.default_code)

		if min_char <= 0x20 <= max_char:
			break_index = 0x20 - min_char

		output.write8(default_index)
		output.write8(break_index)
		output.write16(width_bytes)
		output.write32(0)            # device name
		output.write32(vtell)
		output.write32(0)            # gdi bits pointer
		output.write32(bits_offset)
		output.write8(0)             # reserved

		# CTABLE
		for value in ctable:
			output.write16(value)

		# GLYPHS
		data = bytearray(font.bbx.height * font.bbx.row_size())

		for char in font.chars:
			row_size = char.bbx.row_size()
			counter = 0
			# MS coordinates
			for n in range(0, row_size):
				for y in range(0, font.bbx.height):
					data[counter] = char.data[row_size * y + n]
					counter += 1
			output.write(data[:counter])
		output.write(bytes(sentinel * font.bbx.height))

		# FAMILY
		output.write_zstr(family, 1)

	fnio.write_file(parsed.output_name, write_fnt, encoding=None)


if __name__ == '__main__':
	fncli.start('bdftofnt.py', Options(), Params(), main_program)
