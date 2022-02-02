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

// -- Width --
const DPARSE_LIMIT = 512;
const SPARSE_LIMIT = 32000;

class Width {
	constructor(x, y) {
		this.x = x;
		this.y = y;
	}

	static parse(name, value, limit) {
		const words = fnutil.splitWords(name, value, 2);

		return new Width(fnutil.parseDec(name + '.x', words[0], -limit, limit),
			fnutil.parseDec(name + '.y', words[1], -limit, limit));
	}

	static parseS(name, value) {
		return Width.parse(name, value, SPARSE_LIMIT);
	}

	static parseD(name, value) {
		return Width.parse(name, value, DPARSE_LIMIT);
	}

	toString() {
		return `${this.x} ${this.y}`;
	}
}

// -- BBX --
class BBX {
	constructor(width, height, xoff, yoff) {
		this.width = width;
		this.height = height;
		this.xoff = xoff;
		this.yoff = yoff;
	}

	static parse(name, value) {
		const words = fnutil.splitWords(name, value, 4);

		return new BBX(fnutil.parseDec(name + '.width', words[0], 1, DPARSE_LIMIT),
			fnutil.parseDec(name + '.height', words[1], 1, DPARSE_LIMIT),
			fnutil.parseDec(name + '.xoff', words[2], -DPARSE_LIMIT, DPARSE_LIMIT),
			fnutil.parseDec(name + '.yoff', words[3], -DPARSE_LIMIT, DPARSE_LIMIT));
	}

	rowSize() {
		return (this.width + 7) >> 3;
	}

	toString() {
		return `${this.width} ${this.height} ${this.xoff} ${this.yoff}`;
	}
}

// -- Props --
function skipComments(line) {
	return line.startsWith('COMMENT') ? null : line;
}

class Props extends Map {
	forEach(callback) {
		super.forEach((value, name) => callback(name, value));
	}

	read(input, name, callback) {
		return this.parse(input.readLines(skipComments), name, callback);
	}

	parse(line, name, callback) {
		if (line == null || !line.startsWith(name)) {
			throw new Error(name + ' expected');
		}

		const value = line.substring(name.length).trimLeft();

		this.set(name, value);
		return callback == null ? value : callback(name, value);
	}

	set(name, value) {
		super.set(name, value.toString());
	}
}

// -- Base --
class Base {
	constructor() {
		this.props = new Props();
		this.bbx = null;
	}
}

// -- Char --
class Char extends Base {
	constructor() {
		super();
		this.code = -1;
		this.swidth = null;
		this.dwidth = null;
		this.data = null;
	}

	bitmap() {
		const bitmap = this.data.toString('hex').toUpperCase();
		const regex = new RegExp(`.{${this.bbx.rowSize() << 1}}`, 'g');
		return bitmap.replace(regex, '$&\n');
	}

	_read(input) {
		// HEADER
		this.props.read(input, 'STARTCHAR');
		this.code = this.props.read(input, 'ENCODING', fnutil.parseDec);
		this.swidth = this.props.read(input, 'SWIDTH', Width.parseS);
		this.dwidth = this.props.read(input, 'DWIDTH', Width.parseD);
		this.bbx = this.props.read(input, 'BBX', BBX.parse);

		let line = input.readLines(skipComments);

		if (line != null && line.startsWith('ATTRIBUTES')) {
			this.props.parse(line, 'ATTRIBUTES');
			line = input.readLines(skipComments);
		}

		// BITMAP
		if (this.props.parse(line, 'BITMAP') !== '') {
			throw new Error('BITMAP expected');
		}

		const rowLen = this.bbx.rowSize() * 2;
		let bitmap = '';

		for (let y = 0; y < this.bbx.height; y++) {
			line = input.readLines(skipComments);

			if (line == null) {
				throw new Error('bitmap data expected');
			}
			if (line.match(/^[\dA-Fa-f]+$/) == null) {
				throw new Error('invalid bitmap character(s)');
			}
			if (line.length === rowLen) {
				bitmap += line;
			} else {
				throw new Error('invalid bitmap line length');
			}
		}

		this.data = Buffer.from(bitmap, 'hex');

		// FINAL
		if (input.readLines(skipComments) !== 'ENDCHAR') {
			throw new Error('ENDCHAR expected');
		}
		return this;
	}

	static read(input) {
		return (new Char())._read(input);
	}

	write(output) {
		let header = '';

		this.props.forEach((name, value) => {
			header += (name + ' ' + value).trim() + '\n';
		});
		output.writeLine(header + this.bitmap() + 'ENDCHAR');
	}
}

// -- Font --
const XLFD = {
	FOUNDRY:          1,
	FAMILY_NAME:      2,
	WEIGHT_NAME:      3,
	SLANT:            4,
	SETWIDTH_NAME:    5,
	ADD_STYLE_NAME:   6,
	PIXEL_SIZE:       7,
	POINT_SIZE:       8,
	RESOLUTION_X:     9,
	RESOLUTION_Y:     10,
	SPACING:          11,
	AVERAGE_WIDTH:    12,
	CHARSET_REGISTRY: 13,
	CHARSET_ENCODING: 14
};

const CHARS_MAX = 65535;

class Font extends Base {
	constructor() {
		super();
		this.chars = [];
		this.defaultCode = -1;
	}

	get bold() {
		return this.xlfd[XLFD.WEIGHT_NAME].toLowerCase().includes('bold');
	}

	get italic() {
		return ['I', 'O'].indexOf(this.xlfd[XLFD.SLANT]) !== -1;
	}

	get proportional() {
		return this.xlfd[XLFD.SPACING] === 'P';
	}

	_read(input) {
		// HEADER
		let line = input.readLine();

		if (this.props.parse(line, 'STARTFONT') !== '2.1') {
			throw new Error('STARTFONT 2.1 expected');
		}
		this.xlfd = this.props.read(input, 'FONT', (name, value) => value.split('-', 16));

		if (this.xlfd.length !== 15 || this.xlfd[0] !== '') {
			throw new Error('non-XLFD font names are not supported');
		}
		this.props.read(input, 'SIZE');
		this.bbx = this.props.read(input, 'FONTBOUNDINGBOX', BBX.parse);
		line = input.readLines(skipComments);

		if (line != null && line.startsWith('STARTPROPERTIES')) {
			const numProps = this.props.parse(line, 'STARTPROPERTIES', fnutil.parseDec);

			for (let i = 0; i < numProps; i++) {
				line = input.readLines(skipComments);

				if (line == null) {
					throw new Error('property expected');
				}

				const match = line.match(/^(\w+)\s+([-\d"].*)$/);

				if (match == null) {
					throw new Error('invalid property format');
				}

				const name = match[1];
				const value = match[2];

				if (this.props.get(name) != null) {
					throw new Error('duplicate property');
				}
				if (name === 'DEFAULT_CHAR') {
					this.defaultCode = fnutil.parseDec(name, value);
				}
				this.props.set(name, value);
			}

			if (this.props.read(input, 'ENDPROPERTIES') !== '') {
				throw new Error('ENDPROPERTIES expected');
			}
			line = input.readLines(skipComments);
		}

		// GLYPHS
		const numChars = fnutil.parseDec('CHARS', this.props.parse(line, 'CHARS'), 1, CHARS_MAX);

		for (let i = 0; i < numChars; i++) {
			this.chars.push(Char.read(input));
		}

		if (this.defaultCode !== -1 && this.chars.find(char => char.code === this.defaultCode) === -1) {
			throw new Error('invalid DEFAULT_CHAR');
		}

		// FINAL
		if (input.readLines(skipComments) !== 'ENDFONT') {
			throw new Error('ENDFONT expected');
		}
		if (input.readLine() != null) {
			throw new Error('garbage after ENDFONT');
		}
		return this;
	}

	static read(input) {
		return (new Font())._read(input, false);
	}

	write(output) {
		this.props.forEach((name, value) => output.writeProp(name, value));
		this.chars.forEach(char => char.write(output));
		output.writeLine('ENDFONT');
	}
}

// -- Export --
module.exports = Object.freeze({
	DPARSE_LIMIT,
	SPARSE_LIMIT,
	Width,
	BBX,
	skipComments,
	Props,
	Base,
	Char,
	XLFD,
	CHARS_MAX,
	Font
});
