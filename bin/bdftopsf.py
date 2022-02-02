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

import fnutil
import fncli
import fnio
import bdfexp

# -- Params --
class Params(fncli.Params):
	def __init__(self):
		fncli.Params.__init__(self)
		self.version = -1
		self.exchange = -1
		self.output_name = None


# -- Options --
HELP = ('' +
	'usage: bdftopsf [-1|-2|-r] [-g|-G] [-o OUTPUT] [INPUT.bdf] [TABLE...]\n' +
	'Convert a BDF font to PC Screen Font or raw font\n' +
	'\n' +
	'  -1, -2      write a PSF version 1 or 2 font (default = 1 if possible)\n' +
	'  -r, --raw   write a RAW font\n' +
	'  -g, --vga   exchange the characters at positions 0...31 with these at\n' +
	'              192...223 (default for VGA text mode compliant PSF fonts\n' +
	'              with 224 to 512 characters starting with unicode 00A3)\n' +
	'  -G          do not exchange characters 0...31 and 192...223\n' +
	'  -o OUTPUT   output file (default = stdout, may not be a terminal)\n' +
	'  --help      display this help and exit\n' +
	'  --version   display the program version and license, and exit\n' +
	'  --excstk    display the exception stack on error\n' +
	'\n' +
	'The input must be a monospaced unicode-encoded BDF 2.1 font.\n' +
	'\n' +
	'The tables are text files with two or more hexadecimal unicodes per line:\n' +
	'a character code from the BDF, and extra code(s) for it. All extra codes\n' +
	'are stored sequentially in the PSF unicode table for their character.\n' +
	'<ss> is always specified as FFFE, although it is stored as FE in PSF2.\n')

VERSION = 'bdftopsf 1.62, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

class Options(fncli.Options):
	def __init__(self):
		fncli.Options.__init__(self, ['-o'], HELP, VERSION)


	def parse(self, name, value, params):
		if name in ['-1', '-2']:
			params.version = int(name[1])
		elif name in ['-r', '--raw']:
			params.version = 0
		elif name in ['-g', '--vga']:
			params.exchange = True
		elif name == '-G':
			params.exchange = False
		elif name == '-o':
			params.output_name = value
		else:
			self.fallback(name, params)


# -- Main --
def main_program(nonopt, parsed):
	version = parsed.version
	exchange = parsed.exchange
	bdfile = len(nonopt) > 0 and nonopt[0].lower().endswith('.bdf')
	ver1_unicodes = True

	# READ INPUT
	ifs = fnio.InputFileStream(nonopt[0] if bdfile else None)
	font = ifs.process(bdfexp.Font.read)

	try:
		for char in font.chars:
			prefix = 'char %d: ' % char.code

			if char.bbx.width != font.bbx.width:
				raise Exception(prefix + 'output width not equal to maximum output width')

			if char.code == 65534:
				raise Exception(prefix + 'not a character, use 65535 for empty position')

			if char.code >= 65536:
				if version == 1:
					raise Exception(prefix + '-1 requires unicodes <= 65535')
				ver1_unicodes = False

		# VERSION
		ver1_num_chars = len(font.chars) == 256 or len(font.chars) == 512

		if version == 1:
			if not ver1_num_chars:
				raise Exception('-1 requires a font with 256 or 512 characters')

			if font.bbx.width != 8:
				raise Exception('-1 requires a font with width 8')

		# EXCHANGE
		vga_num_chars = len(font.chars) >= 224 and len(font.chars) <= 512
		vga_text_size = font.bbx.width == 8 and font.bbx.height in [8, 14, 16]

		if exchange is True:
			if not vga_num_chars:
				raise Exception('-g/--vga requires a font with 224...512 characters')

			if not vga_text_size:
				raise Exception('-g/--vga requires an 8x8, 8x14 or 8x16 font')

	except Exception as ex:
		ex.message = ifs.location() + getattr(ex, 'message', str(ex))
		raise

	# READ TABLES
	tables = dict()

	def load_extra(line):
		nonlocal ver1_unicodes
		words = line.split()

		if len(words) < 2:
			raise Exception('invalid format')

		uni = fnutil.parse_hex('unicode', words[0])

		if uni == 0xFFFE:
			raise Exception('FFFE is not a character')

		if next((char for char in font.chars if char.code == uni), None):
			if uni > fnutil.UNICODE_BMP_MAX:
				ver1_unicodes = False

			if uni not in tables:
				tables[uni] = []

			table = tables[uni]

			for word in words[1:]:
				dup = fnutil.parse_hex('extra code', word)

				if dup == 0xFFFF:
					raise Exception('FFFF is not a character')

				if dup > fnutil.UNICODE_BMP_MAX:
					ver1_unicodes = False

				if not dup in table or 0xFFFE in table:
					tables[uni].append(dup)

			if version == 1 and not ver1_unicodes:
				raise Exception('-1 requires unicodes <= %X' % fnutil.UNICODE_BMP_MAX)

	for table_name in nonopt[int(bdfile):]:
		fnio.read_file(table_name, lambda ifs: ifs.read_lines(load_extra))

	# VERSION
	if version == -1:
		version = 1 if ver1_num_chars and ver1_unicodes and font.bbx.width == 8 else 2

	# EXCHANGE
	if exchange == -1:
		exchange = vga_text_size and version >= 1 and vga_num_chars and font.chars[0].code == 0x00A3

	if exchange:
		font.chars = font.chars[192:224] + font.chars[32:192] + font.chars[0:32] + font.chars[224:]

	# WRITE
	def write_psf(output):
		# HEADER
		if version == 1:
			output.write8(0x36)
			output.write8(0x04)
			output.write8((len(font.chars) >> 8) + 1)
			output.write8(font.bbx.height)
		elif version == 2:
			output.write32(0x864AB572)
			output.write32(0x00000000)
			output.write32(0x00000020)
			output.write32(0x00000001)
			output.write32(len(font.chars))
			output.write32(len(font.chars[0].data))
			output.write32(font.bbx.height)
			output.write32(font.bbx.width)

		# GLYPHS
		for char in font.chars:
			output.write(char.data)

		# UNICODES
		if version > 0:
			def write_unicode(code):
				if version == 1:
					output.write16(code)
				elif code <= 0x7F:
					output.write8(code)
				elif code in [0xFFFE, 0xFFFF]:
					output.write8(code & 0xFF)
				else:
					if code <= 0x7FF:
						output.write8(0xC0 + (code >> 6))
					else:
						if code <= 0xFFFF:
							output.write8(0xE0 + (code >> 12))
						else:
							output.write8(0xF0 + (code >> 18))
							output.write8(0x80 + ((code >> 12) & 0x3F))

						output.write8(0x80 + ((code >> 6) & 0x3F))

					output.write8(0x80 + (code & 0x3F))

			for char in font.chars:
				if char.code != 0xFFFF:
					write_unicode(char.code)

				if char.code in tables:
					for extra in tables[char.code]:
						write_unicode(extra)

				write_unicode(0xFFFF)

	fnio.write_file(parsed.output_name, write_psf, encoding=None)


if __name__ == '__main__':
	fncli.start('bdftopsf.py', Options(), Params(), main_program)
