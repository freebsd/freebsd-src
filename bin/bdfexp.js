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

// -- Font --
class Font extends bdf.Font {
	constructor() {
		super();
		this.minWidth = 0;  // used in proportional()
		this.avgWidth = 0;
	}

	_expand(char) {
		if (char.dwidth.x >= 0) {
			if (char.bbx.xoff >= 0) {
				var width = Math.max(char.bbx.xoff + char.bbx.width, char.dwidth.x);
				var dstXOff = char.bbx.xoff;
				var expXOff = 0;
			} else {
				width = Math.max(char.bbx.width, char.dwidth.x - char.bbx.xoff);
				dstXOff = 0;
				expXOff = char.bbx.xoff;
			}
		} else {
			const revXOff = char.bbx.xoff + char.bbx.width;

			if (revXOff <= 0) {
				width = -Math.min(char.dwidth.x, char.bbx.xoff);
				dstXOff = width + char.bbx.xoff;
				expXOff = -width;
			} else {
				width = Math.max(char.bbx.width, revXOff - char.dwidth.x);
				dstXOff = width - char.bbx.width;
				expXOff = revXOff - width;
			}
		}

		const height = this.bbx.height;

		if (width === char.bbx.width && height === char.bbx.height) {
			return;
		}

		const srcRowSize = char.bbx.rowSize();
		const dstRowSize = (width + 7) >> 3;
		const dstYMax = this.pxAscender - char.bbx.yoff;
		const dstYMin = dstYMax - char.bbx.height;
		const copyRow = (dstXOff & 7) === 0;
		const dstData = Buffer.alloc(dstRowSize * height);

		for (let dstY = dstYMin; dstY < dstYMax; dstY++) {
			let srcByteNo = (dstY - dstYMin) * srcRowSize;
			let dstByteNo = dstY * dstRowSize + (dstXOff >> 3);

			if (copyRow) {
				char.data.copy(dstData, dstByteNo, srcByteNo, srcByteNo + srcRowSize);
			} else {
				let srcBitNo = 7;
				let dstBitNo = 7 - (dstXOff & 7);

				for (let x = 0; x < char.bbx.width; x++) {
					if (char.data[srcByteNo] & (1 << srcBitNo)) {
						dstData[dstByteNo] |= (1 << dstBitNo);
					}
					if (--srcBitNo < 0) {
						srcBitNo = 7;
						srcByteNo++;
					}
					if (--dstBitNo < 0) {
						dstBitNo = 7;
						dstByteNo++;
					}
				}
			}
		}

		char.bbx = new bdf.BBX(width, height, expXOff, this.bbx.yoff);
		char.props.set('BBX', char.bbx);
		char.data = dstData;
	}

	expand() {
		// PREXPAND / VERTICAL
		const ascent = this.props.get('FONT_ASCENT');
		const descent = this.props.get('FONT_DESCENT');
		let pxAscent = (ascent == null ? 0 : fnutil.parseDec('FONT_ASCENT', ascent, 0, bdf.DPARSE_LIMIT));
		let pxDescent = (descent == null ? 0 : fnutil.parseDec('FONT_DESCENT', descent, 0, bdf.DPARSE_LIMIT));

		this.chars.forEach(char => {
			pxAscent = Math.max(pxAscent, char.bbx.height + char.bbx.yoff);
			pxDescent = Math.max(pxDescent, -char.bbx.yoff);
		});
		this.bbx.height = pxAscent + pxDescent;
		this.bbx.yoff = -pxDescent;

		// EXPAND / HORIZONTAL
		let totalWidth = 0;

		this.minWidth = this.chars[0].bbx.width;
		this.chars.forEach(char => {
			this._expand(char);
			this.minWidth = Math.min(this.minWidth, char.bbx.width);
			this.bbx.width = Math.max(this.bbx.width, char.bbx.width);
			this.bbx.xoff = Math.min(this.bbx.xoff, char.bbx.xoff);
			totalWidth += char.bbx.width;
		});
		this.avgWidth = fnutil.round(totalWidth / this.chars.length);
		this.props.set('FONTBOUNDINGBOX', this.bbx);
	}

	expandX() {
		this.chars.forEach(char => {
			if (char.dwidth.x !== char.bbx.width) {  // preserve SWIDTH if possible
				char.swidth.x = fnutil.round(char.bbx.width * 1000 / this.bbx.height);
				char.props.set('SWIDTH', char.swidth);
				char.dwidth.x = char.bbx.width;
				char.props.set('DWIDTH', char.dwidth);
			}
			char.bbx.xoff = 0;
			char.props.set('BBX', char.bbx);
		});
		this.bbx.xoff = 0;
		this.props.set('FONTBOUNDINGBOX', this.bbx);
	}

	expandY() {
		const props = new Map([
			[ 'FONT_ASCENT', this.pxAscender ],
			[ 'FONT_DESCENT', -this.pxDescender ],
			[ 'PIXEL_SIZE', this.bbx.height ]
		]);

		props.forEach((value, name) => {
			if (this.props.get(name) != null) {
				this.props.set(name, value);
			}
		});

		this.xlfd[bdf.XLFD.PIXEL_SIZE] = this.bbx.height.toString();
		this.props.set('FONT', this.xlfd.join('-'));
	}

	get proportional() {
		return this.bbx.width > this.minWidth || super.proportional;
	}

	get pxAscender() {
		return this.bbx.height + this.bbx.yoff;
	}

	get pxDescender() {
		return this.bbx.yoff;
	}

	_read(input) {
		super._read(input);
		this.expand();
		return this;
	}

	static read(input) {
		return (new Font())._read(input);
	}

	_updateProp(name, value) {
		if (this.props.get(name) != null) {
			this.props.set(name, value);
		}
	}
}

// -- Export --
module.exports = Object.freeze({
	Font
});

// -- Params --
class Params extends fncli.Params {
	constructor() {
		super();
		this.expandX = false;
		this.expandY = false;
		this.output = null;
	}
}

// -- Options --
const HELP = ('' +
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
	'The input must be a BDF 2.1 font with unicode encoding.\n');

const VERSION = 'bdfexp 1.60, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE;

class Options extends fncli.Options {
	constructor() {
		super(['-o'], HELP, VERSION);
	}

	parse(name, value, params) {
		switch (name) {
		case '-X':
			params.expandX = true;
			break;
		case '-Y':
			params.expandY = true;
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
	if (nonopt.length > 1) {
		throw new Error('invalid number of arguments, try --help');
	}

	// READ INPUT
	let ifs = new fnio.InputFileStream(nonopt[0]);

	try {
		var font = Font.read(ifs);
		ifs.close();
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// EXTRA ACTIONS
	if (parsed.expandX) {
		font.expandX();
	}
	if (parsed.expandY) {
		font.expandY();
	}

	// WRITE OUTPUT
	let ofs = new fnio.OutputFileStream(parsed.output);

	try {
		font.write(ofs);
		ofs.close();
	} catch (e) {
		e.message = ofs.location() + e.message() + ofs.destroy();
		throw e;
	}
}

if (require.main === module) {
	fncli.start('bdfexp.js', new Options(), new Params(), mainProgram);
}
