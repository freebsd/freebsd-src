/*
  Copyright (C) 2017-2019 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>

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
const bdfexp = require('./bdfexp.js');

// -- Params --
class Params extends fncli.Params {
	constructor() {
		super();
		this.version = -1;
		this.exchange = -1;
		this.output = null;
	}
}

// -- Options --
const HELP = ('' +
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
	'<ss> is always specified as FFFE, although it is stored as FE in PSF2.\n');

const VERSION = 'bdftopsf 1.58, Copyright (C) 2017-2019 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_VERSION;

class Options extends fncli.Options {
	constructor() {
		super(['-o'], HELP, VERSION);
	}

	parse(name, value, params) {
		switch (name) {
		case '-1':
		case '-2':
			params.version = parseInt(name[1]);
			break;
		case '-r':
		case '--raw':
			params.version = 0;
			break;
		case '-g':
		case '--vga':
			params.exchange = true;
			break;
		case '-G':
			params.exchange = false;
			break;
		case '-o':
			params.output = value;
			break;
		default:
			this.fallback(name, params);
		}
	}
}

// -- Main --
function mainProgram(nonopt, parsed) {
	const bdfile = nonopt.length > 0 && nonopt[0].toLowerCase().endsWith('.bdf');
	let version = parsed.version;
	let exchange = parsed.exchange;
	let ver1Unicodes = true;

	// READ INPUT
	let ifs = new fnio.InputFileStream(bdfile ? nonopt[0] : null);

	try {
		var font = bdfexp.Font.read(ifs);

		ifs.close();
		font.chars.forEach(char => {
			const prefix = `char ${char.code}: `;

			if (char.bbx.width !== font.bbx.width) {
				throw new Error(prefix + 'output width not equal to maximum output width');
			}
			if (char.code === 65534) {
				throw new Error(prefix + 'not a character, use 65535 for empty position');
			}
			if (char.code >= 65536) {
				if (version === 1) {
					throw new Error(prefix + '-1 requires unicodes <= 65535');
				}
				ver1Unicodes = false;
			}
		});

		// VERSION
		var ver1NumChars = (font.chars.length === 256 || font.chars.length === 512);

		if (version === 1) {
			if (!ver1NumChars) {
				throw new Error('-1 requires a font with 256 or 512 characters');
			}
			if (font.bbx.width !== 8) {
				throw new Error('-1 requires a font with width 8');
			}
		}

		// EXCHANGE
		var vgaNumChars = font.chars.length >= 224 && font.chars.length <= 512;
		var vgaTextSize = font.bbx.width === 8 && [8, 14, 16].indexOf(font.bbx.height) !== -1;

		if (exchange === true) {
			if (!vgaNumChars) {
				throw new Error('-g/--vga requires a font with 224...512 characters');
			}
			if (!vgaTextSize) {
				throw new Error('-g/--vga requires an 8x8, 8x14 or 8x16 font');
			}
		}
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// READ TABLES
	let tables = [];

	function loadExtra(line) {
		const words = line.split(/\s+/);

		if (words.length < 2) {
			throw new Error('invalid format');
		}

		const uni = fnutil.parseHex('unicode', words[0]);
		let table = tables[uni];

		if (uni === 0xFFFE) {
			throw new Error('FFFE is not a character');
		}

		if (font.chars.findIndex(char => char.code === uni) !== -1) {
			if (uni > fnutil.UNICODE_BMP_MAX) {
				ver1Unicodes = false;
			}
			if (table == null) {
				table = tables[uni] = [];
			}

			words.slice(1).forEach(word => {
				const dup = fnutil.parseHex('extra code', word);

				if (dup === 0xFFFF) {
					throw new Error('FFFF is not a character');
				}
				if (dup > fnutil.UNICODE_BMP_MAX) {
					ver1Unicodes = false;
				}
				if (table.indexOf(dup) === -1 || table.indexOf(0xFFFE) !== -1) {
					table.push(dup);
				}
			});
			if (version === 1 && !ver1Unicodes) {
				throw new Error('-1 requires unicodes <= ' + fnutil.UNICODE_BMP_MAX.toString(16));
			}
		}
	}

	nonopt.slice(Number(bdfile)).forEach(name => {
		ifs = new fnio.InputFileStream(name);

		try {
			ifs.readLines(loadExtra);
			ifs.close();
		} catch (e) {
			e.message = ifs.location() + e.message;
			throw e;
		}
	});

	// VERSION
	if (version === -1) {
		version = ver1NumChars && ver1Unicodes && font.bbx.width === 8 ? 1 : 2;
	}

	// EXCHANGE
	if (exchange === -1) {
		exchange = vgaTextSize && version >= 1 && vgaNumChars && font.chars[0].code === 0x00A3;
	}

	if (exchange) {
		const control = font.chars.splice(0, 32, ...font.chars.splice(192, 32));
		font.chars.splice(192, 0, ...control);
	}

	// WRITE
	let ofs = new fnio.OutputFileStream(parsed.output, null);

	try {
		// HEADER
		if (version === 1) {
			ofs.write8(0x36);
			ofs.write8(0x04);
			ofs.write8((font.chars.length >> 8) + 1);
			ofs.write8(font.bbx.height);
		} else if (version === 2) {
			ofs.write32(0x864AB572);
			ofs.write32(0x00000000);
			ofs.write32(0x00000020);
			ofs.write32(0x00000001);
			ofs.write32(font.chars.length);
			ofs.write32(font.chars[0].data.length);
			ofs.write32(font.bbx.height);
			ofs.write32(font.bbx.width);
		}

		// GLYPHS
		font.chars.forEach(char => ofs.write(char.data));

		// UNICODES
		if (version > 0) {
			const writeUnicode = function(code) {
				if (version === 1) {
					ofs.write16(code);
				} else if (code <= 0x7F) {
					ofs.write8(code);
				} else if (code === 0xFFFE || code === 0xFFFF) {
					ofs.write8(code & 0xFF);
				} else {
					if (code <= 0x7FF) {
						ofs.write8(0xC0 + (code >> 6));
					} else {
						if (code <= 0xFFFF) {
							ofs.write8(0xE0 + (code >> 12));
						} else {
							ofs.write8(0xF0 + (code >> 18));
							ofs.write8(0x80 + ((code >> 12) & 0x3F));
						}
						ofs.write8(0x80 + ((code >> 6) & 0x3F));
					}
					ofs.write8(0x80 + (code & 0x3F));
				}
			};

			font.chars.forEach(char => {
				if (char.code !== 0xFFFF) {
					writeUnicode(char.code);
				}
				if (tables[char.code] != null) {
					tables[char.code].forEach(extra => writeUnicode(extra));
				}
				writeUnicode(0xFFFF);
			});
		}

		// FINISH
		ofs.close();
	} catch (e) {
		e.message = ofs.location() + e.message + ofs.destroy();
		throw e;
	}
}

if (require.main === module) {
	fncli.start('bdftopsf.js', new Options(), new Params(), mainProgram);
}
