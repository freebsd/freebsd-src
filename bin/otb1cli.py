#
# Copyright (C) 2018-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

from datetime import datetime, timezone

import fnutil
import fncli
import fnio
import otb1exp

# -- Params --
class Params(otb1exp.Params):
	def __init__(self):
		otb1exp.Params.__init__(self)
		self.output_name = None
		self.real_time = True


# -- Options --
HELP = ('' +
	'usage: otb1cli [options] [INPUT]\n' +
	'Convert a BDF font to OTB\n' +
	'\n' +
	'  -o OUTPUT     output file (default = stdout, may not be a terminal)\n' +
	'  -d DIR-HINT   set font direction hint (default = 0)\n' +
	'  -e EM-SIZE    set em size (default = 1024)\n' +
	'  -g LINE-GAP   set line gap (default = 0)\n' +
	'  -l LOW-PPEM   set lowest recorded PPEM (default = font height)\n' +
	'  -E ENCODING   BDF string properties encoding (default = utf-8)\n' +
	'  -W WLANG-ID   set Windows name-s language ID (default = 0x0409)\n' +
	'  -T            use the current date and time for created/modified\n' +
	'                (default = get them from INPUT if not stdin/terminal)\n' +
	'  -X            set xMaxExtent = 0 (default = max character width)\n' +
	'  -L            write a single loca entry (default = CHARS entries)\n' +
	'  -P            write PostScript glyph names (default = no names)\n' +
	'\n' +
      'Notes:\n' +
	'  The input must be a BDF 2.1 font with unicode encoding.\n' +
      '  All bitmaps are expanded first. Bitmap widths are used.\n' +
      '  Overlapping characters are not supported.\n')

VERSION = 'otb1cli 0.24, Copyright (C) 2018-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE

class Options(otb1exp.Options):
	def __init__(self):
		otb1exp.Options.__init__(self, ['-o'], HELP, VERSION)


	def parse(self, name, value, params):
		if name == '-o':
			params.output_name = value
		elif name == '-T':
			params.real_time = False
		else:
			otb1exp.Options.parse(self, name, value, params)


# -- Main --
def main_program(nonopt, parsed):
	if len(nonopt) > 1:
		raise Exception('invalid number of arguments, try --help')

	# READ INPUT
	def read_otb(ifs):
		if parsed.real_time:
			try:
				stat = ifs.fstat()
				if stat:
					parsed.created = datetime.fromtimestamp(stat.st_ctime, timezone.utc)
					parsed.modified = datetime.fromtimestamp(stat.st_mtime, timezone.utc)
			except Exception as ex:
				fnutil.warning(ifs.location(), str(ex))

		return otb1exp.Font.read(ifs, parsed)

	font = fnio.read_file(nonopt[0] if nonopt else None, read_otb)

	# WRITE OUTPUT
	sfnt = otb1exp.SFNT(font)
	fnio.write_file(parsed.output_name, lambda ofs: ofs.write(sfnt.data), encoding=None)


if __name__ == '__main__':
	fncli.start('otb1cli.py', Options(), Params(), main_program)
