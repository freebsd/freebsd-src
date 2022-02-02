/*
  Copyright (C) 2018-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

'use strict';

const fnutil = require('./fnutil.js');
const fncli = require('./fncli.js');
const fnio = require('./fnio.js');
const otb1exp = require('./otb1exp.js');

// -- Params --
class Params extends otb1exp.Params {
	constructor() {
		super();
		this.output = null;
		this.encoding = 'utf-8';
		this.realTime = true;
	}
}

// -- Options --
const HELP = ('' +
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
      '  Overlapping characters are not supported.\n');

const VERSION = 'otb1cli 0.22, Copyright (C) 2018-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE;

class Options extends otb1exp.Options {
	constructor() {
		super(['-o', '-E'], HELP, VERSION);
	}

	parse(name, value, params) {
		switch (name) {
		case '-o':
			params.output = value;
			break;
		case '-E':
			params.encoding = value;
			break;
		case '-T':
			params.realTime = false;
			break;
		default:
			super.parse(name, value, params);
		}
	}
}

// -- Main --
function mainProgram(nonopt, parsed) {
	if (nonopt.length > 1) {
		throw new Error('invalid number of arguments, try --help');
	}

	// READ INPUT
	let ifs = new fnio.InputFileStream(nonopt[0], parsed.encoding);

	try {
		if (parsed.realTime) {
			try {
				const stat = ifs.fstat();

				if (stat != null) {
					parsed.created = stat.birthtime;
					parsed.modified = stat.mtime;
				}
			} catch (e) {
				fnutil.warning(ifs.location(), e.message);
			}
		}

		var font = otb1exp.Font.read(ifs, parsed);
		ifs.close();
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// WRITE OUTPUT
	let ofs = new fnio.OutputFileStream(parsed.output, null);

	try {
		const table = new otb1exp.SFNT(font);

		ofs.write(table.data.slice(0, table.size));
		ofs.close();
	} catch (e) {
		e.message = ofs.location() + e.message + ofs.destroy();
		throw e;
	}
}

if (require.main === module) {
	fncli.start('otb1cli.js', new Options(), new Params(), mainProgram);
}
