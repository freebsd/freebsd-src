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
const bdf = require('./bdf.js');


class Char {
	constructor(code, name, width, data) {
		this.code = code;
		this.name = name;
		this.width = width;
		this.data = data;
	}

	static from(char, fbbox) {
		const deltaYOff = char.bbx.yoff - fbbox.yoff;  // ~DSB
		let width;
		let dstXOff;

		if (char.dwidth.x >= 0) {
			if (char.bbx.xoff >= 0) {
				width = Math.max(char.bbx.width + char.bbx.xoff, char.dwidth.x);
				dstXOff = char.bbx.xoff;
			} else {
				width = Math.max(char.bbx.width, char.dwidth.x - char.bbx.xoff);
				dstXOff = 0;
			}
		} else {
			dstXOff = Math.max(char.bbx.xoff - char.dwidth.x, 0);
			width = char.bbx.width + dstXOff;
		}

		if (width > bdf.WIDTH_MAX) {
			throw new Error(`char ${char.code}: output width > ${bdf.WIDTH_MAX}`);
		}
		if (char.bbx.yoff < fbbox.yoff) {
			throw new Error(`char ${char.code}: BBX yoff < FONTBOUNDINGBOX yoff`);
		}

		const height = fbbox.height;
		const srcRowSize = char.bbx.rowSize();
		const dstRowSize = (width + 7) >> 3;
		const dstYMax = height - deltaYOff;
		const dstYMin = dstYMax - char.bbx.height;
		const compatRow = dstXOff === 0 && width >= char.bbx.width;
		let data;

		if (compatRow && srcRowSize === dstRowSize && dstYMin === 0 && dstYMax === height) {
			data = char.data;
		} else if (dstYMin < 0) {
			throw new Error(`char ${char.code}: start row ${dstYMin}`);
		} else {
			data = Buffer.alloc(dstRowSize * height);

			for (let dstY = dstYMin; dstY < dstYMax; dstY++) {
				let srcByteNo = (dstY - dstYMin) * srcRowSize;
				let dstByteNo = dstY * dstRowSize + (dstXOff >> 3);

				if (compatRow) {
					char.data.copy(data, dstByteNo, srcByteNo, srcByteNo + srcRowSize);
				} else {
					let srcBitNo = 7;
					let dstBitNo = 7 - (dstXOff & 7);

					for (let x = 0; x < char.bbx.width; x++) {
						if (char.data[srcByteNo] & (1 << srcBitNo)) {
							data[dstByteNo] |= (1 << dstBitNo);
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
		}

		return new Char(char.code, char.props.get('STARTCHAR'), width, data);
	}

	packedSize() {
		return (this.width * (this.data.length / this.rowSize()) + 7) >> 3;
	}

	rowSize() {
		return (this.width + 7) >> 3;
	}

	write(output, height, yoffset) {
		let header = `STARTCHAR ${this.name}\nENCODING ${this.code}\n`;
		const swidth = fnutil.round(this.width * 1000 / height);

		header += `SWIDTH ${swidth} 0\nDWIDTH ${this.width} 0\nBBX ${this.width} ${height} 0 ${yoffset}\n`;
		output.writeLine(header + 'BITMAP\n' + bdf.Char.bitmap(this.data, this.rowSize()) + 'ENDCHAR');
	}
}


class Font extends bdf.Font {
	constructor() {
		super();
		this.minWidth = bdf.WIDTH_MAX;
		this.avgWidth = 0;
	}

	_read(input) {
		let totalWidth = 0;

		super._read(input);
		this.chars = this.chars.map(char => Char.from(char, this.bbx));
		this.bbx.xoff = 0;
		this.chars.forEach(char => {
			this.minWidth = Math.min(this.minWidth, char.width);
			this.bbx.width = Math.max(this.bbx.width, char.width);
			totalWidth += char.width;
		});
		this.avgWidth = fnutil.round(totalWidth / this.chars.length);
		this.props.set('FONTBOUNDINGBOX', this.bbx.toString());
		return this;
	}

	static read(input) {
		return (new Font())._read(input);
	}

	getProportional() {
		return Number(this.bbx.width > this.minWidth);
	}

	write(output) {
		this.props.forEach((name, value) => output.writeProp(name, value));
		this.chars.forEach(char => char.write(output, this.bbx.height, this.bbx.yoff));
		output.writeLine('ENDFONT');
	}
}


module.exports = Object.freeze({
	Char,
	Font
});
