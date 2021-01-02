#
# Copyright (c) 2019 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

import fnutil
import fncli
import fnio
import bmpf


class Params(fncli.Params):
	def __init__(self):
		fncli.Params.__init__(self)
		self.version = -1
		self.exchange = -1
		self.output = None


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
	'  -o OUTPUT   output file (default = stdout, must not be a terminal)\n' +
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

VERSION = 'bdftopsf 1.50, Copyright (C) 2019 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

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
			params.output = value
		else:
			self.fallback(name, params)


def main_program(nonopt, parsed):
	version = parsed.version
	exchange = parsed.exchange
	bdfile = len(nonopt) > 0 and nonopt[0].lower().endswith('.bdf')
	ver1_unicodes = True

	# READ INPUT
	ifs = fnio.InputStream(nonopt[0] if bdfile else None)

	try:
		font = bmpf.Font.read(ifs)
		ifs.close()

		for char in font.chars:
			prefix = 'char %d: ' % char.code

			if char.width != font.bbx.width:
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
		raise Exception(ifs.location() + str(ex))

	# READ TABLES
	tables = dict()

	def load_extra(line):
		nonlocal ver1_unicodes

		words = re.split(br'\s+', line)

		if len(words) < 2:
			raise Exception('invalid format')

		uni = fnutil.parse_hex('unicode', words[0])

		if uni == 0xFFFE:
			raise Exception('FFFE is not a character')

		if next((char for char in font.chars if char.code == uni), None):
			if uni >= 0x10000:
				ver1_unicodes = False

			if uni not in tables:
				tables[uni] = []

			table = tables[uni]

			for word in words[1:]:
				dup = fnutil.parse_hex('extra code', word)

				if dup == 0xFFFF:
					raise Exception('FFFF is not a character')

				if dup >= 0x10000:
					ver1_unicodes = False

				if not dup in table or 0xFFFE in table:
					tables[uni].append(dup)

			if version == 1 and not ver1_unicodes:
				raise Exception('-1 requires unicodes <= FFFF')

	for name in nonopt[int(bdfile):]:
		ifs = fnio.InputStream(name)

		try:
			ifs.read_lines(load_extra)
			ifs.close()
		except Exception as ex:
			raise Exception(ifs.location() + str(ex))

	# VERSION
	if version == -1:
		version = 1 if ver1_num_chars and ver1_unicodes and font.bbx.width == 8 else 2

	# EXCHANGE
	if exchange == -1:
		exchange = vga_text_size and version >= 1 and vga_num_chars and font.chars[0].code == 0x00A3

	if exchange:
		font.chars = font.chars[192:224] + font.chars[32:192] + font.chars[0:32] + font.chars[224:]

	# WRITE
	ofs = fnio.OutputStream(parsed.output)

	if ofs.file.isatty():
		raise Exception('binary output may not be send to a terminal, use -o or redirect/pipe it')

	try:
		# HEADER
		if version == 1:
			ofs.write8(0x36)
			ofs.write8(0x04)
			ofs.write8((len(font.chars) >> 8) + 1)
			ofs.write8(font.bbx.height)
		elif version == 2:
			ofs.write32(0x864AB572)
			ofs.write32(0x00000000)
			ofs.write32(0x00000020)
			ofs.write32(0x00000001)
			ofs.write32(len(font.chars))
			ofs.write32(len(font.chars[0].data))
			ofs.write32(font.bbx.height)
			ofs.write32(font.bbx.width)

		# GLYPHS
		for char in font.chars:
			ofs.write(char.data)

		# UNICODES
		if version > 0:
			def write_unicode(code):
				if version == 1:
					ofs.write16(code)
				elif code <= 0x7F:
					ofs.write8(code)
				elif code in [0xFFFE, 0xFFFF]:
					ofs.write8(code & 0xFF)
				else:
					if code <= 0x7FF:
						ofs.write8(0xC0 + (code >> 6))
					else:
						if code <= 0xFFFF:
							ofs.write8(0xE0 + (code >> 12))
						else:
							ofs.write8(0xF0 + (code >> 18))
							ofs.write8(0x80 + ((code >> 12) & 0x3F))

						ofs.write8(0x80 + ((code >> 6) & 0x3F))

					ofs.write8(0x80 + (code & 0x3F))

			for char in font.chars:
				if char.code != 0xFFFF:
					write_unicode(char.code)

				if char.code in tables:
					for extra in tables[char.code]:
						write_unicode(extra)

				write_unicode(0xFFFF)

		# FINISH
		ofs.close()

	except Exception as ex:
		raise Exception(ofs.location() + str(ex) + ofs.destroy())


if __name__ == '__main__':
	fncli.start('bdftopsf.py', Options(), Params(), main_program)
