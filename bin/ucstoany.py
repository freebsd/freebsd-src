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
import copy

import fnutil
import fncli
import fnio
import bdf

# -- Params --
class Params(fncli.Params):
	def __init__(self):
		fncli.Params.__init__(self)
		self.filter_ffff = False
		self.family_name = None
		self.output_name = None


# -- Options --
HELP = ('' +
	'usage: ucstoany [-f] [-F FAMILY] [-o OUTPUT] INPUT REGISTRY ENCODING TABLE...\n' +
	'Generate a BDF font subset.\n' +
	'\n' +
	'  -f, --filter   Discard characters with unicode FFFF; with registry ISO10646,\n' +
	'                 encode the first 32 characters with their indexes; with other\n' +
	'                 registries, encode all characters with indexes\n' +
	'  -F FAMILY      output font family name (default = input)\n' +
	'  -o OUTPUT      output file (default = stdout)\n' +
	'  TABLE          text file, one hexadecimal unicode per line\n' +
	'  --help         display this help and exit\n' +
	'  --version      display the program version and license, and exit\n' +
	'  --excstk       display the exception stack on error\n' +
	'\n' +
	'The input must be a BDF 2.1 font with unicode encoding.\n' +
	'Unlike ucs2any, all TABLE-s form a single subset of the input font.\n')

VERSION = 'ucstoany 1.62, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

class Options(fncli.Options):
	def __init__(self):
		fncli.Options.__init__(self, ['-F', '-o'], HELP, VERSION)


	def parse(self, name, value, params):
		if name in ['-f', '--filter']:
			params.filter_ffff = True
		elif name == '-F':
			params.family_name = bytes(value, 'ascii')
			if '-' in value:
				raise Exception('FAMILY may not contain "-"')
		elif name == '-o':
			params.output_name = value
		else:
			self.fallback(name, params)


# -- Main --
def main_program(nonopt, parsed):
	# NON-OPTIONS
	if len(nonopt) < 4:
		raise Exception('invalid number of arguments, try --help')

	input_name = nonopt[0]
	registry = nonopt[1]
	encoding = nonopt[2]
	new_codes = []

	if not re.fullmatch(r'[A-Za-z][\w.:()]*', registry) or not re.fullmatch(r'[\w.:()]+', encoding):
		raise Exception('invalid registry or encoding')

	# READ INPUT
	old_font = fnio.read_file(input_name, bdf.Font.read)

	# READ TABLES
	def load_code(line):
		new_codes.append(fnutil.parse_hex('unicode', line))

	for table_name in nonopt[3:]:
		fnio.read_file(table_name, lambda ifs: ifs.read_lines(load_code))

	if not new_codes:
		raise Exception('no characters in the output font')

	# CREATE GLYPHS
	new_font = bdf.Font()
	charmap = {char.code:char for char in old_font.chars}
	index = 0
	unstart = 0
	family = parsed.family_name if parsed.family_name is not None else old_font.xlfd[bdf.XLFD.FAMILY_NAME]

	if parsed.filter_ffff:
		unstart = 32 if registry == 'ISO10646' else bdf.CHARS_MAX

	for code in new_codes:
		if code == 0xFFFF and parsed.filter_ffff:
			index += 1
			continue

		if code in charmap:
			old_char = charmap[code]
			uni_ffff = False
		else:
			uni_ffff = True

			if code != 0xFFFF:
				raise Exception('%s does not contain U+%04X' % (input, code))

			if old_font.default_code != -1:
				old_char = charmap[old_font.default_code]
			elif 0xFFFD in charmap:
				old_char = charmap[0xFFFD]
			else:
				raise Exception('%s does not contain U+FFFF, and no replacement found' % input)

		new_char = copy.copy(old_char)
		new_char.code = code if index >= unstart else index
		index += 1
		new_char.props = copy.copy(old_char.props)
		new_char.props.set('ENCODING', new_char.code)
		new_font.chars.append(new_char)

		if uni_ffff:
			new_char.props.set('STARTCHAR', b'uniFFFF')
		elif old_char.code == old_font.default_code or (old_char.code == 0xFFFD and new_font.default_code == -1):
			new_font.default_code = new_char.code

	# CREATE HEADER
	registry = bytes(registry, 'ascii')
	encoding = bytes(encoding, 'ascii')

	for [name, value] in old_font.props:
		if name == 'FONT':
			new_font.xlfd = old_font.xlfd[:]
			new_font.xlfd[bdf.XLFD.FAMILY_NAME] = family
			new_font.xlfd[bdf.XLFD.CHARSET_REGISTRY] = registry
			new_font.xlfd[bdf.XLFD.CHARSET_ENCODING] = encoding
			value = b'-'.join(new_font.xlfd)
		elif name == 'STARTPROPERTIES':
			num_props = fnutil.parse_dec(name, value, 1)
		elif name == 'FAMILY_NAME':
			value = fnutil.quote(family)
		elif name == 'CHARSET_REGISTRY':
			value = fnutil.quote(registry)
		elif name == 'CHARSET_ENCODING':
			value = fnutil.quote(encoding)
		elif name == 'DEFAULT_CHAR':
			if new_font.default_code != -1:
				value = new_font.default_code
			else:
				num_props -= 1
				continue
		elif name == 'ENDPROPERTIES':
			if new_font.default_code != -1 and new_font.props.get('DEFAULT_CHAR') is None:
				new_font.props.set('DEFAULT_CHAR', new_font.default_code)
				num_props += 1

			new_font.props.set('STARTPROPERTIES', num_props)
		elif name == 'CHARS':
			value = len(new_font.chars)

		new_font.props.set(name, value)

	# COPY FIELDS
	new_font.bbx = old_font.bbx

	# WRITE OUTPUT
	fnio.write_file(parsed.output_name, lambda ofs: new_font.write(ofs))


if __name__ == '__main__':
	fncli.start('ucstoany.py', Options(), Params(), main_program)
