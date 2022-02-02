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

// -- Params --
class Params extends fncli.Params {
	constructor() {
		super();
		this.asciiChars = true;
		this.bbxExceeds = true;
		this.duplCodes = -1;
		this.extraBits = true;
		this.attributes = true;
		this.duplNames = -1;
		this.duplProps = true;
		this.commonSlant = true;
		this.commonWeight = true;
		this.xlfdFontNm = true;
		this.yWidthZero = true;
	}
}

// -- Options --
const HELP = ('' +
	'usage: bdfcheck [options] [INPUT...]\n' +
	'Check BDF font(s) for various problems\n' +
	'\n' +
	'  -A          disable non-ascii characters check\n' +
	'  -B          disable BBX exceeding FONTBOUNDINGBOX checks\n' +
	'  -c/-C       enable/disable duplicate character codes check\n' +
	'              (default = enabled for registry ISO10646)\n' +
	'  -E          disable extra bits check\n' +
	'  -I          disable ATTRIBUTES check\n' +
	'  -n/-N       enable duplicate character names check\n' +
	'              (default = enabled for registry ISO10646)\n' +
	'  -P          disable duplicate properties check\n' +
	'  -S          disable common slant check\n' +
	'  -W          disable common weight check\n' +
	'  -X          disable XLFD font name check\n' +
	'  -Y          disable zero WIDTH Y check\n' +
	'  --help      display this help and exit\n' +
	'  --version   display the program version and license, and exit\n' +
	'  --excstk    display the exception stack on error\n' +
	'\n' +
	'File directives: COMMENT bdfcheck --enable|disable-<check-name>\n' +
	'  (also available as long command line options)\n' +
	'\n' +
	'Check names: ascii-chars, bbx-exceeds, duplicate-codes, extra-bits,\n' +
	'  attributes, duplicate-names, duplicate-properties, common-slant,\n' +
	'  common-weight, xlfd-font, ywidth-zero\n' +
	'\n' +
	'The input BDF(s) must be v2.1 with unicode encoding.\n');

const VERSION = 'bdfcheck 1.61, Copyright (C) 2017-2020 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE;

class Options extends fncli.Options {
	constructor() {
		super([], HELP, VERSION);
	}

	parse(name, directive, params) {
		const value = name.startsWith('--enable') || name[1].match('[a-z]');

		switch (name) {
		case '-A':
		case '--enable-ascii-chars':
		case '--disable-ascii-chars':
			params.asciiChars = value;
			break;
		case '-B':
		case '--enable-bbx-exceeds':
		case '--disable-bbx-exceeds':
			params.bbxExceeds = value;
			break;
		case '-c':
		case '-C':
		case '--enable-duplicate-codes':
		case '--disable-duplicate-codes':
			params.duplCodes = value;
			break;
		case '-E':
		case '--enable-extra-bits':
		case '--disable-extra-bits':
			params.extraBits = value;
			break;
		case '-I':
		case '--enable-attributes':
		case '--disable-attributes':
			params.attributes = value;
			break;
		case '-n':
		case '-N':
		case '--enable-duplicate-names':
		case '--disable-duplicate-names':
			params.duplNames = value;
			break;
		case '-P':
		case '--enable-duplicate-properties':
		case '--disable-duplicate-properties':
			params.duplProps = value;
			break;
		case '-S':
		case '--enable-common-slant':
		case '--disable-common-slant':
			params.commonSlant = value;
			break;
		case '-W':
		case '--enable-common-weight':
		case '--disable-common-weight':
			params.commonWeight = value;
			break;
		case '-X':
		case '--enable-xlfd-font':
		case '--disable-xlfd-font':
			params.xlfdFontNm = value;
			break;
		case '-Y':
		case '--enable-ywidth-zero':
		case '--disable-ywidth-zero':
			params.yWidthZero = value;
			break;
		default:
			return directive !== true && this.fallback(name, params);
		}

		return directive !== true || name.startsWith('--');
	}
}

// -- DupMap --
class DupMap extends Map {
	constructor(prefix, descript, severity) {
		super();
		this.prefix = prefix;
		this.descript = descript;
		this.severity = severity;
	}

	check() {
		this.forEach((lines, value) => {
			if (lines.length > 1) {
				let text = `duplicate ${this.descript} ${value} at lines`;

				for (let index = 0; index < lines.length; index++) {
					text += (index === 0 ? ' ' : index === lines.length - 1 ? ' and ' : ', ');
					text += lines[index];
				}
				fnutil.message(this.prefix, this.severity, text);
			}
		});
	}

	push(value, lineNo) {
		let lines = this.get(value);

		if (lines != null) {
			lines.push(lineNo);
		} else {
			this.set(value, [lineNo]);
		}
	}
}

// -- InputFileStream --
const MODE = Object.freeze({
	META: 0,
	PROPS: 1,
	BITMAP: 2
});

class InputFileStream extends fnio.InputFileStream {
	constructor(fileName, parsed) {
		super(fileName);
		this.parsed = parsed;
		this.mode = MODE.META;
		this.proplocs = new DupMap(this.location(), 'property');
		this.namelocs = new DupMap(this.location(), 'character name', 'warning');
		this.codelocs = new DupMap(this.location(), 'encoding', 'warning');
		this.HANDLERS = [
			[ 'STARTCHAR',       value => this.appendName(value) ],
			[ 'ENCODING',        value => this.appendCode(value) ],
			[ 'SWIDTH',          value => this.checkWidth('SWIDTH', value, bdf.Width.parseS) ],
			[ 'DWIDTH',          value => this.checkWidth('DWIDTH', value, bdf.Width.parseD) ],
			[ 'BBX',             value => this.setLastBox(value) ],
			[ 'BITMAP',          () => this.setMode(MODE.BITMAP) ],
			[ 'SIZE',            InputFileStream.checkSize ],
			[ 'ATTRIBUTES',      value => this.checkAttr(value) ],
			[ 'STARTPROPERTIES', () => this.setMode(MODE.PROPS) ],
			[ 'FONTBOUNDINGBOX', value => this.setFontBox(value) ]
		];
		this.xlfdName = false;
		this.lastBox = null;
		this.fontBox = null;
		this.options = new Options();
	}

	append(option, valocs, value) {
		if (option) {
			valocs.push(value, this.lineNo);
		}
	}

	appendCode(value) {
		fnutil.parseDec('encoding', value);
		this.append(this.parsed.duplCodes, this.codelocs, value);
	}

	appendName(value) {
		this.append(this.parsed.duplNames, this.namelocs, `"${value}"`);
	}

	checkWidth(name, value, parse) {
		if (this.parsed.yWidthZero && parse(name, value).y !== 0) {
			fnutil.warning(this.location(), `non-zero ${name} Y`);
		}
	}

	setFontBox(value) {
		this.fontBox = bdf.BBX.parse('FONTBOUNDINGBOX', value);
	}

	setLastBox(value) {
		const bbx = bdf.BBX.parse('BBX', value);

		if (this.parsed.bbxExceeds) {
			let exceeds = [];

			if (bbx.xoff < this.fontBox.xoff) {
				exceeds.push('xoff < FONTBOUNDINGBOX xoff');
			}
			if (bbx.yoff < this.fontBox.yoff) {
				exceeds.push('yoff < FONTBOUNDINGBOX yoff');
			}
			if (bbx.width > this.fontBox.width) {
				exceeds.push('width > FONTBOUNDINGBOX width');
			}
			if (bbx.height > this.fontBox.height) {
				exceeds.push('height > FONTBOUNDINGBOX height');
			}
			exceeds.forEach(exceed => {
				fnutil.message(this.location(), '', exceed);
			});
		}
		this.lastBox = bbx;
	}

	setMode(newMode) {
		this.mode = newMode;
	}

	static checkSize(value) {
		const words = fnutil.splitWords('SIZE', value, 3);

		fnutil.parseDec('point size', words[0], 1, null);
		fnutil.parseDec('x resolution', words[1], 1, null);
		fnutil.parseDec('y resolution', words[2], 1, null);
	}

	checkAttr(value) {
		if (!value.match(/^[\dA-Fa-f]{4}$/)) {
			throw new Error('ATTRIBUTES must be 4 hex-encoded characters');
		}
		if (this.parsed.attributes) {
			fnutil.warning(this.location(), 'ATTRIBUTES may cause problems with freetype');
		}
	}

	checkFont(value) {
		const xlfd = value.substring(4).trimLeft().split('-', 16);

		if (xlfd.length === 15 && xlfd[0] === '') {
			let unicode = (xlfd[bdf.XLFD.CHARSET_REGISTRY].toUpperCase() === 'ISO10646');

			if (this.parsed.duplCodes === -1) {
				this.parsed.duplCodes = unicode;
			}
			if (this.parsed.duplNames === -1) {
				this.parsed.duplNames = unicode;
			}

			if (this.parsed.commonWeight) {
				let weight = xlfd[bdf.XLFD.WEIGHT_NAME];
				let compare = weight.toLowerCase();
				let consider = compare.includes('bold') ? 'Bold' : 'Normal';

				if (compare === 'medium' || compare === 'regular') {
					compare = 'normal';
				}
				if (compare !== consider.toLowerCase()) {
					fnutil.warning(this.location(), `weight "${weight}" may be considered ${consider}`);
				}
			}

			if (this.parsed.commonSlant) {
				let slant = xlfd[bdf.XLFD.SLANT];
				let consider = slant.match(/^[IO]/) ? 'Italic' : 'Regular';

				if (slant.match(/^[IOR]$/) == null) {
					fnutil.warning(this.location(), `slant "${slant}" may be considered ${consider}`);
				}
			}
		} else {
			if (this.parsed.xlfdFontNm) {
				fnutil.warning(this.location(), 'non-XLFD font name');
			}
			value = 'FONT --------------';
		}

		return value;
	}

	checkProp(line) {
		const match = line.match(/^(\w+)\s+([-\d"].*)$/);

		if (match == null) {
			throw new Error('invalid property format');
		}

		const name = match[1];
		const value = match[2];

		if (value.startsWith('"')) {
			if (value.length < 2 || !value.endsWith('"')) {
				throw new Error('no closing double quote');
			}
			if (value.substring(1, value.length - 1).match(/[^"]"[^"]/)) {
				throw new Error('unescaped double quote');
			}
		} else {
			fnutil.parseDec('value', value, null, null);
		}

		this.append(this.parsed.duplProps, this.proplocs, name);
		return `P${this.lineNo} 1`;
	}

	checkBitmap(line) {
		if (line.length !== this.lastBox.rowSize() * 2) {
			throw new Error('invalid bitmap length');
		} else if (line.match(/^[\dA-Fa-f]+$/) == null) {
			throw new Error('invalid bitmap data');
		} else if (this.parsed.extraBits) {
			const data = Buffer.from(line, 'hex');
			const checkX = (this.lastBox.width - 1) | 7;
			const lastByte = data[data.length - 1];
			let bitNo = 7 - (this.lastBox.Width & 7);

			for (let x = this.lastBox.Width; x <= checkX; x++) {
				if (lastByte & (1 << bitNo)) {
					fnutil.warning(this.location(), `extra bit(s) starting with x=${x}`);
					break;
				}
				bitNo--;
			}
		}
	}

	checkLine(line) {
		if (line.match(/[^\t\f\v\u0020-\u00ff]/)) {
			throw new Error('control character(s)');
		}
		if (this.parsed.asciiChars && line.match(/[\u007f-\u00ff]/)) {
			fnutil.warning(this.location(), 'non-ascii character(s)');
		}

		switch (this.mode) {
		case MODE.META:
			if (!this.xlfdName && line.startsWith('FONT')) {
				line = this.checkFont(line);
				this.xlfdName = true;
			} else {
				this.HANDLERS.findIndex(function(handler) {
					if (line.startsWith(handler[0])) {
						handler[1](line.substring(handler[0].length).trimLeft());
						return true;
					}
					return false;
				});
			}
			break;
		case MODE.PROPS:
			if (line.startsWith('ENDPROPERTIES')) {
				this.mode = MODE.META;
			} else {
				line = this.checkProp(line);
			}
			break;
		default: // MODE.BITMAP
			if (line.startsWith('ENDCHAR')) {
				this.mode = MODE.META;
			} else {
				this.checkBitmap(line);
			}
		}
		return line;
	}

	readCheck(line, callback) {
		const match = line.match(/^COMMENT\s*bdfcheck\s+(-.*)$/);

		if (match && !this.options.parse(match[1], true, this.parsed)) {
			throw new Error('invalid bdfcheck directive');
		}

		line = callback(line);
		return line != null ? this.checkLine(line) : null;
	}

	readLines(callback) {
		return super.readLines(line => this.readCheck(line, callback));
	}
}

// -- Main --
function mainProgram(nonopt, parsed) {
	(nonopt.length >= 1 ? nonopt : [null]).forEach(input => {
		let ifs = new InputFileStream(input, parsed);

		try {
			bdf.Font.read(ifs);
			ifs.close();
		} catch (e) {
			e.message = ifs.location() + e.message;
			throw e;
		}
		ifs.proplocs.check();
		ifs.namelocs.check();
		ifs.codelocs.check();
	});
}

if (require.main === module) {
	fncli.start('bdfcheck.js', new Options(), new Params(), mainProgram);
}
