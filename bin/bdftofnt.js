/*
  Copyright (C) 2017-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>

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
const bdf = require('./bdf.js');
const bdfexp = require('./bdfexp.js');

// -- Params --
class Params extends fncli.Params {
	constructor() {
		super();
		this.charSet = -1;
		this.minChar = -1;
		this.fntFamily = 0;
		this.output = null;
	}
}

// -- Options --
const HELP = ('' +
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
	'The input must be a BDF 2.1 font with unicode encoding.\n');

const VERSION = 'bdftofnt 1.60, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE;

const FNT_FAMILIES = [ 'DontCare', 'Roman', 'Swiss', 'Modern', 'Decorative' ];

class Options extends fncli.Options {
	constructor() {
		super(['-c', '-m', '-f', '-o'], HELP, VERSION);
	}

	parse(name, value, params) {
		switch (name) {
		case '-c':
			params.charSet = fnutil.parseDec('CHARSET', value, 0, 255);
			break;
		case '-m':
			params.minChar = fnutil.parseDec('MINCHAR', value, 0, 255);
			break;
		case '-f':
			params.fntFamily = FNT_FAMILIES.indexOf(value);

			if (params.fntFamily === -1) {
				throw new Error('invalid FAMILY');
			}
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
const FNT_HEADER_SIZE = 118;
const FNT_CHARSETS = [238, 204, 0, 161, 162, 177, 178, 186, 163];

function mainProgram(nonopt, parsed) {
	if (nonopt.length > 1) {
		throw new Error('invalid number of arguments, try --help');
	}

	let charSet = parsed.charSet;
	let minChar = parsed.minChar;

	// READ INPUT
	let ifs = new fnio.InputFileStream(nonopt[0]);

	try {
		var font = bdfexp.Font.read(ifs);
		ifs.close();
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// COMPUTE
	if (charSet === -1) {
		const encoding = font.xlfd[bdf.XLFD.CHARSET_ENCODING];

		if (encoding.toLowerCase().match(/^(cp)?125[0-8]$/)) {
			charSet = FNT_CHARSETS[parseInt(encoding.substring(encoding.length - 1), 10)];
		} else {
			charSet = 255;
		}
	}

	try {
		const numChars = font.chars.length;

		if (numChars > 256) {
			throw new Error('too many characters, the maximum is 256');
		}
		if (minChar === -1) {
			if (numChars === 192 || numChars === 256) {
				minChar = 256 - numChars;
			} else {
				minChar = font.chars[0].code;
			}
		}

		var maxChar = minChar + numChars - 1;

		if (maxChar >= 256) {
			throw new Error('the maximum character code is too big, (re)specify -m');
		}

		// HEADER
		var vtell = FNT_HEADER_SIZE + (numChars + 1) * 4;
		var bitsOffset = vtell;
		var ctable = [];
		var widthBytes = 0;

		// CTABLE/GLYPHS
		font.chars.forEach(char => {
			const rowSize = char.bbx.rowSize();

			ctable.push(char.bbx.width);
			ctable.push(vtell);
			vtell += rowSize * font.bbx.height;
			widthBytes += rowSize;
		});

		if (vtell > 0xFFFF) {
			throw new Error('too much character data');
		}

		// SENTINEL
		var sentinel = 2 - widthBytes % 2;

		ctable.push(sentinel * 8);
		ctable.push(vtell);
		vtell += sentinel * font.bbx.height;
		widthBytes += sentinel;

		if (widthBytes > 0xFFFF) {
			throw new Error('the total character width is too big');
		}
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// WRITE
	let ofs = new fnio.OutputFileStream(parsed.output, null);

	try {
		// HEADER
		const family = font.xlfd[bdf.XLFD.FAMILY_NAME];
		let copyright = font.props.get('COPYRIGHT');

		copyright = (copyright != null) ? fnutil.unquote(copyright).substring(0, 60) : '';
		ofs.write16(0x0200);                                              // font version
		ofs.write32(vtell + family.length + 1);                           // total size
		ofs.writeZStr(copyright, 60 - copyright.length);
		ofs.write16(0);                                                   // gdi, device type
		ofs.write16(fnutil.round(font.bbx.height * 72 / 96));
		ofs.write16(96);                                                  // vertical resolution
		ofs.write16(96);                                                  // horizontal resolution
		ofs.write16(font.pxAscender);                                     // base line
		ofs.write16(0);                                                   // internal leading
		ofs.write16(0);                                                   // external leading
		ofs.write8(Number(font.italic));
		ofs.write8(0);                                                    // underline
		ofs.write8(0);                                                    // strikeout
		ofs.write16(font.bold ? 700 : 400);
		ofs.write8(charSet);
		ofs.write16(font.proportional ? 0 : font.bbx.width);
		ofs.write16(font.bbx.height);
		ofs.write8((parsed.fntFamily << 4) + Number(font.proportional));
		ofs.write16(font.avgWidth);
		ofs.write16(font.bbx.width);
		ofs.write8(minChar);
		ofs.write8(maxChar);

		let defaultIndex = maxChar - minChar;
		let breakIndex = 0;

		if (font.defaultCode !== -1) {
			defaultIndex = font.chars.findIndex(char => char.code === font.defaultCode);
		}
		if (minChar <= 0x20 && maxChar >= 0x20) {
			breakIndex = 0x20 - minChar;
		}
		ofs.write8(defaultIndex);
		ofs.write8(breakIndex);
		ofs.write16(widthBytes);
		ofs.write32(0);           // device name
		ofs.write32(vtell);
		ofs.write32(0);           // gdi bits pointer
		ofs.write32(bitsOffset);
		ofs.write8(0);            // reserved

		// CTABLE
		ctable.forEach(value => ofs.write16(value));

		// GLYPHS
		const data = Buffer.alloc(font.bbx.height * font.bbx.rowSize());

		font.chars.forEach(char => {
			const rowSize = char.bbx.rowSize();
			let counter = 0;
			// MS coordinates
			for (let n = 0; n < rowSize; n++) {
				for (let y = 0; y < font.bbx.height; y++) {
					data[counter++] = char.data[rowSize * y + n];
				}
			}
			ofs.write(data.slice(0, counter));
		});
		ofs.write(Buffer.alloc(sentinel * font.bbx.height));

		// FAMILY
		ofs.writeZStr(family, 1);
		ofs.close();
	} catch (e) {
		e.message = ofs.location() + e.message + ofs.destroy();
		throw e;
	}
}

if (require.main === module) {
	fncli.start('bdftofnt.js', new Options(), new Params(), mainProgram);
}
