//
// Copyright (c) 2018 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

'use strict';

const fnutil = require('./fnutil.js');


const WIDTH_MAX = 127;
const HEIGHT_MAX = 255;
const SWIDTH_MAX = 32000;

class Width {
	constructor(x, y) {
		this.x = x;
		this.y = y;
	}

	static _parse(name, value,  limitX, limitY) {
		const words = fnutil.splitWords(name, value, 2);

		return new Width(fnutil.parseDec(name + ' X', words[0], -limitX, limitX),
			fnutil.parseDec(name + ' Y', words[1], -limitY, limitY));
	}

	static parseS(value) {
		return Width._parse('SWIDTH', value, SWIDTH_MAX, SWIDTH_MAX);
	}

	static parseD(value) {
		return Width._parse('DWIDTH', value, WIDTH_MAX, HEIGHT_MAX);
	}
}


class BBX {
	constructor(width, height, xoff, yoff) {
		this.width = width;
		this.height = height;
		this.xoff = xoff;
		this.yoff = yoff;
	}

	static parse(name, value) {
		const words = fnutil.splitWords(name, value, 4);

		return new BBX(fnutil.parseDec('width', words[0], 1, WIDTH_MAX),
			fnutil.parseDec('height', words[1], 1, HEIGHT_MAX),
			fnutil.parseDec('bbxoff', words[2], -WIDTH_MAX, WIDTH_MAX),
			fnutil.parseDec('bbyoff', words[3], -WIDTH_MAX, WIDTH_MAX));
	}

	rowSize() {
		return (this.width + 7) >> 3;
	}

	toString() {
		return `${this.width} ${this.height} ${this.xoff} ${this.yoff}`;
	}
}


class Props {
	constructor() {
		this.names = [];
		this.values = [];
	}

	add(name, value) {
		this.names.push(name);
		this.values.push(value);
	}

	clone() {
		let props = new Props();

		props.names = this.names.slice();
		props.values = this.values.slice();
		return props;
	}

	forEach(callback) {
		for (let index = 0; index < this.names.length; index++) {
			callback(this.names[index], this.values[index]);
		}
	}

	get(name) {
		return this.values[this.names.indexOf(name)];
	}

	parse(line, name, callback) {
		if (line === null || !line.startsWith(name)) {
			throw new Error(name + ' expected');
		}

		let value = line.substring(name.length).trimLeft();

		this.add(name, value);
		return callback == null ? value : callback(name, value);
	}

	push(line) {
		this.add('', line);
	}

	set(name, value) {
		let index = this.names.indexOf(name);

		if (index !== -1) {
			this.values[index] = value;
		} else {
			this.add(name, value);
		}
	}
}


class Base {
	constructor() {
		this.props = new Props();
		this.bbx = null;
		this.finis = [];
	}

	readFinish(input, endText) {
		if (this.readNext(input, this.finis) !== endText) {
			throw new Error(endText + ' expected');
		}
		this.finis.push(endText);
	}

	readNext(input, comout = this.props) {
		return input.readLines(line => {
			if (line.startsWith('COMMENT')) {
				comout.push(line);
				return null;
			}
			return line;
		});
	}

	readProp(input, name, callback) {
		return this.props.parse(this.readNext(input), name, callback);
	}
}


class Char extends Base {
	constructor() {
		super();
		this.code = -1;
		this.swidth = null;
		this.dwidth = null;
		this.data = null;
	}

	static bitmap(data, rowSize) {
		const bitmap = data.toString('hex').toUpperCase();
		const regex = new RegExp(`.{${rowSize << 1}}`, 'g');
		return bitmap.replace(regex, '$&\n');
	}

	_read(input) {
		// HEADER
		this.readProp(input, 'STARTCHAR');
		this.code = this.readProp(input, 'ENCODING', fnutil.parseDec);
		this.swidth = this.readProp(input, 'SWIDTH', (name, value) => Width.parseS(value));
		this.dwidth = this.readProp(input, 'DWIDTH', (name, value) => Width.parseD(value));
		this.bbx = this.readProp(input, 'BBX', BBX.parse);

		let line = this.readNext(input);

		if (line !== null && line.startsWith('ATTRIBUTES')) {
			this.props.parse(line, 'ATTRIBUTES');
			line = this.readNext(input);
		}

		// BITMAP
		if (this.props.parse(line, 'BITMAP') !== '') {
			throw new Error('BITMAP expected');
		}

		const rowLen = this.bbx.rowSize() * 2;
		let bitmap = '';

		for (let y = 0; y < this.bbx.height; y++) {
			line = this.readNext(input);

			if (line === null) {
				throw new Error('bitmap data expected');
			}
			if (line.length === rowLen) {
				bitmap += line;
			} else {
				throw new Error('invalid bitmap line length');
			}
		}

		// FINAL
		this.readFinish(input, 'ENDCHAR');

		if (bitmap.match(/^[\dA-Fa-f]+$/) != null) {
			this.data = Buffer.from(bitmap, 'hex');
		} else {
			throw new Error('invalid BITMAP data characters');
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
		output.writeLine(header + Char.bitmap(this.data, this.bbx.rowSize()) + this.finis.join('\n'));
	}
}


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

	getAscent() {
		let ascent = this.props.get('FONT_ASCENT');

		if (ascent != null) {
			return fnutil.parseDec('FONT_ASCENT', ascent, -HEIGHT_MAX, HEIGHT_MAX);
		}
		return this.bbx.height + this.bbx.yoff;
	}

	getBold() {
		return Number(this.xlfd[XLFD.WEIGHT_NAME].toLowerCase().includes('bold'));
	}

	getItalic() {
		return Number(this.xlfd[XLFD.SLANT].match(/^[IO]/) != null);
	}

	_read(input) {
		// HEADER
		let line = input.readLines(Font.skipEmpty);

		if (this.props.parse(line, 'STARTFONT') !== '2.1') {
			throw new Error('STARTFONT 2.1 expected');
		}
		this.xlfd = this.readProp(input, 'FONT', (name, value) => value.split('-', 16));

		if (this.xlfd.length !== 15 || this.xlfd[0] !== '') {
			throw new Error('non-XLFD font names are not supported');
		}
		this.readProp(input, 'SIZE');
		this.bbx = this.readProp(input, 'FONTBOUNDINGBOX', BBX.parse);
		line = this.readNext(input);

		if (line !== null && line.startsWith('STARTPROPERTIES')) {
			const numProps = this.props.parse(line, 'STARTPROPERTIES', fnutil.parseDec);

			for (let i = 0; i < numProps; i++) {
				line = this.readNext(input);

				if (line === null) {
					throw new Error('property expected');
				}

				let match = line.match(/^(\w+)\s+([-\d"].*)$/);

				if (match == null) {
					throw new Error('invalid property format');
				}

				let name = match[1];
				let value = match[2];

				if (name === 'DEFAULT_CHAR') {
					this.defaultCode = fnutil.parseDec(name, value);
				}

				this.props.add(name, value);
			}

			if (this.readProp(input, 'ENDPROPERTIES') !== '') {
				throw new Error('ENDPROPERTIES expected');
			}
			line = this.readNext(input);
		}

		// GLYPHS
		const numChars = this.props.parse(line, 'CHARS', (name, value) => fnutil.parseDec(name, value, 1, CHARS_MAX));

		for (let i = 0; i < numChars; i++) {
			this.chars.push(Char.read(input));
		}

		if (this.defaultCode !== -1 && this.chars.find(char => char.code === this.defaultCode) === -1) {
			throw new Error('invalid DEFAULT_CHAR');
		}

		// FINAL
		this.readFinish(input, 'ENDFONT');

		if (input.readLines(Font.skipEmpty) != null) {
			throw new Error('garbage after ENDFONT');
		}
		return this;
	}

	static read(input) {
		return (new Font())._read(input);
	}

	static skipEmpty(line) {
		return line.length > 0 ? line : null;
	}

	write(output) {
		this.props.forEach((name, value) => output.writeProp(name, value));
		this.chars.forEach(char => char.write(output));
		output.writeLine(this.finis.join('\n'));
	}
}


module.exports = Object.freeze({
	WIDTH_MAX,
	HEIGHT_MAX,
	SWIDTH_MAX,
	Width,
	BBX,
	Char,
	XLFD,
	CHARS_MAX,
	Font
});
