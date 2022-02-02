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
const bdf = require('./bdf.js');
const bdfexp = require('./bdfexp.js');
const otb1get = require('./otb1get.js');

// -- Table --
const TS_EMPTY = 0;
const TS_SMALL = 64;
const TS_LARGE = 1024;

class Table {
	constructor(size, name) {
		this.data = Buffer.alloc(size);
		this.size = 0;
		this.tableName = name;
	}

	checkSize(size) {
		if (size !== this.size) {
			throw new Error(`internal error: ${this.tableName} size = ${this.size} instead of ${size}`);
		}
	}

	checksum() {
		let cksum = 0;

		for (let offset = 0; offset < this.size; offset += 4) {
			cksum += this.data.readUInt32BE(offset);
		}

		return cksum >>> 0;
	}

	ensure(count) {
		if (this.size + count > this.data.length) {
			let newSize = this.data.length << 1;

			while (this.size + count > newSize) {
				newSize <<= 1;
			}

			const newData = Buffer.alloc(newSize);

			this.data.copy(newData, 0, 0, this.size);
			this.data = newData;
		}
	}

	get padding() {
		return ((this.size + 1) & 3) ^ 1;
	}

	rewriteUInt32(value, offset) {
		this.data.writeUInt32BE(value, offset);
	}

	write(buffer) {
		this.ensure(buffer.length);
		buffer.copy(this.data, this.size);
		this.size += buffer.length;
	}

	writeRC(size, writer, name) {
		this.ensure(size);
		try {
			writer(this.size);
		} catch (e) {
			e.message = e.message.replace('"value"', `"${this.tableName}.${name}"`);
			throw e;
		}
		this.size += size;
	}

	writeInt8(value, name) {
		this.writeRC(1, (offset) => this.data.writeInt8(value, offset), name);
	}

	writeInt16(value, name) {
		this.writeRC(2, (offset) => this.data.writeInt16BE(value, offset), name);
	}

	writeInt32(value, name) {
		this.writeRC(4, (offset) => this.data.writeInt32BE(value, offset), name);
	}

	writeInt64(value, name) {
		this.writeRC(8, (offset) => this.data.writeInt64BE(value, offset), name);
	}

	writeUInt8(value, name) {
		this.writeRC(1, (offset) => this.data.writeUInt8(value, offset), name);
	}

	writeUInt16(value, name) {
		this.writeRC(2, (offset) => this.data.writeUInt16BE(value, offset), name);
	}

	writeUInt32(value, name) {
		this.writeRC(4, (offset) => this.data.writeUInt32BE(value, offset), name);
	}

	writeUInt48(value, name) {
		this.writeUInt16(name, 0);
		this.writeRC(6, (offset) => this.data.writeUIntBE(value, offset, 6), name);
	}

	writeFixed(value, name) {
		this.writeRC(4, (offset) => this.data.writeInt32BE(fnutil.round(value * 65536), offset), name);
	}

	writeTable(table) {
		this.write(table.data.slice(0, table.size));
	}
}

// -- Params --
const EM_SIZE_MIN = 64;
const EM_SIZE_MAX = 16384;
const EM_SIZE_DEFAULT = 1024;

class Params extends fncli.Params {
	constructor() {
		super();
		this.created = new Date();
		this.modified = this.created;
		this.dirHint = 0;
		this.emSize = EM_SIZE_DEFAULT;
		this.lineGap = 0;
		this.lowPPem = 0;
		this.wLangId = 0x0409;
		this.xMaxExtent = true;
		this.singleLoca = false;
		this.postNames = false;
	}
}

// -- Options --
class Options extends fncli.Options {
	constructor(needArgs, helpText, versionText) {
		super(needArgs.concat(['-d', '-e', '-g', '-l', '-W']), helpText, versionText);
	}

	parse(name, value, params) {
		switch (name) {
		case '-d':
			params.dirHint = fnutil.parseDec('DIR-HINT', value, -2, 2);
			break;
		case '-e':
			params.emSize = fnutil.parseDec('EM-SIZE', value, EM_SIZE_MIN, EM_SIZE_MAX);
			break;
		case '-g':
			params.lineGap = fnutil.parseDec('LINE-GAP', value, 0, EM_SIZE_MAX << 1);
			break;
		case '-l':
			params.lowPPem = fnutil.parseDec('LOW-PPEM', value, 1, bdf.DPARSE_LIMIT);
			break;
		case '-W':
			params.wLangId = fnutil.parseHex('WLANG-ID', value, 0, 0x7FFF);
			break;
		case '-X':
			params.xMaxExtent = false;
			break;
		case '-L':
			params.singleLoca = true;
			break;
		case '-P':
			params.postNames = true;
			break;
		default:
			this.fallback(name, params);
		}
	}
}

// -- Font --
class Font extends bdfexp.Font {
	constructor(params) {
		super();
		this.params = params;
		this.emAscender = 0;
		this.emDescender = 0;
		this.emMaxWidth = 0;
		this.macStyle = 0;
		this.lineSize = 0;
	}

	get bmpOnly() {
		return this.maxCode <= fnutil.UNICODE_BMP_MAX;
	}

	get created() {
		return Font.sfntime(this.params.created);
	}

	emScale(value, divisor) {
		return fnutil.round(value * this.params.emSize / (divisor || this.bbx.height));
	}

	get italicAngle() {
		const value = this.props.get('ITALIC_ANGLE');  // must be integer
		return value != null ? fnutil.parseDec('ITALIC_ANGLE', value, -45, 45) : this.italic ? -11.5 : 0;
	}

	get maxCode() {
		return this.chars.slice(-1)[0].code;
	}

	get minCode() {
		return this.chars[0].code;
	}

	get modified() {
		return Font.sfntime(this.params.modified);
	}

	prepare() {
		this.chars.sort((c1, c2) => c1.code - c2.code);
		this.chars = this.chars.filter((c, index, array) => index === 0 || c.code !== array[index - 1].code);
		this.props.set('CHARS', this.chars.length);
		this.emAscender = this.emScale(this.pxAscender);
		this.emDescender = this.emAscender - this.params.emSize;
		this.emMaxWidth = this.emScaleWidth(this);
		this.macStyle = Number(this.bold) + (Number(this.italic) << 1);
		this.lineSize = this.emScale(fnutil.round(this.bbx.height / 17) || 1);
	}

	_read(input) {
		super._read(input);
		this.prepare();
		return this;
	}

	static read(input, params) {
		return (new Font(params))._read(input);
	}

	emScaleWidth(base) {
		return this.emScale(base.bbx.width);
	}

	static sfntime(stamp) {
		return Math.floor((stamp - Date.UTC(1904, 0, 1)) / 1000);
	}

	get underlinePosition() {
		return fnutil.round((this.emDescender + this.lineSize) / 2);
	}

	get xMaxExtent() {
		return this.params.xMaxExtent ? this.emMaxWidth : 0;
	}
}

// -- BDAT --
const BDAT_HEADER_SIZE = 4;
const BDAT_METRIC_SIZE = 5;

class BDAT extends Table {
	constructor(font) {
		super(TS_LARGE, 'EBDT');
		// header
		this.writeFixed(2, 'version');
		// format 1 data
		font.chars.forEach(char => {
			this.writeUInt8(font.bbx.height, 'height');
			this.writeUInt8(char.bbx.width, 'width');
			this.writeInt8(0, 'bearingX');
			this.writeInt8(font.pxAscender, 'bearingY');
			this.writeUInt8(char.bbx.width, 'advance');
			this.write(char.data);  // imageData
		});
	}

	static getCharSize(char) {
		return BDAT_METRIC_SIZE + char.data.length;
	}
}

// -- BLOC --
const BLOC_TABLE_SIZE_OFFSET = 12;
const BLOC_PREFIX_SIZE = 0x38;      // header 0x08 + 1 bitmapSizeTable * 0x30
const BLOC_INDEX_ARRAY_SIZE = 8;    // 1 index record * 0x08

class BLOC extends Table {
	constructor(font) {
		super(TS_SMALL, 'EBLC');
		// header
		this.writeFixed(2, 'version');
		this.writeUInt32(1, 'numSizes');
		// bitmapSizeTable
		this.writeUInt32(BLOC_PREFIX_SIZE, 'indexSubTableArrayOffset');
		this.writeUInt32(0, 'indexTableSize');                    // adjusted later
		this.writeUInt32(1, 'numberOfIndexSubTables');
		this.writeUInt32(0, 'colorRef');
		// hori
		this.writeInt8(font.pxAscender, 'hori ascender');
		this.writeInt8(font.pxDescender, 'hori descender');
		this.writeUInt8(font.bbx.width, 'hori widthMax');
		this.writeInt8(1, 'hori caretSlopeNumerator');
		this.writeInt8(0, 'hori caretSlopeDenominator');
		this.writeInt8(0, 'hori caretOffset');
		this.writeInt8(0, 'hori minOriginSB');
		this.writeInt8(0, 'hori minAdvanceSB');
		this.writeInt8(font.pxAscender, 'hori maxBeforeBL');
		this.writeInt8(font.pxDescender, 'hori minAfterBL');
		this.writeInt16(0, 'hori padd');
		// vert
		this.writeInt8(0, 'vert ascender');
		this.writeInt8(0, 'vert descender');
		this.writeUInt8(0, 'vert widthMax');
		this.writeInt8(0, 'vert caretSlopeNumerator');
		this.writeInt8(0, 'vert caretSlopeDenominator');
		this.writeInt8(0, 'vert caretOffset');
		this.writeInt8(0, 'vert minOriginSB');
		this.writeInt8(0, 'vert minAdvanceSB');
		this.writeInt8(0, 'vert maxBeforeBL');
		this.writeInt8(0, 'vert minAfterBL');
		this.writeInt16(0, 'vert padd');
		// (bitmapSizeTable)
		this.writeUInt16(0, 'startGlyphIndex');
		this.writeUInt16(font.chars.length - 1, 'endGlyphIndex');
		this.writeUInt8(font.bbx.height, 'ppemX');
		this.writeUInt8(font.bbx.height, 'ppemY');
		this.writeUInt8(1, 'bitDepth');
		this.writeUInt8(1, 'flags');                              // small metrics are horizontal
		// indexSubTableArray
		this.writeUInt16(0, 'firstGlyphIndex');
		this.writeUInt16(font.chars.length - 1, 'lastGlyphIndex');
		this.writeUInt32(BLOC_INDEX_ARRAY_SIZE, 'additionalOffsetToIndexSubtable');
		// indexSubtableHeader
		this.writeUInt16(font.proportional ? 1 : 2, 'indexFormat');
		this.writeUInt16(1, 'imageFormat');                       // BDAT -> small metrics, byte-aligned
		this.writeUInt32(BDAT_HEADER_SIZE, 'imageDataOffset');
		// indexSubtable data
		if (font.proportional) {
			let offset = 0;

			font.chars.forEach(char => {
				this.writeUInt32(offset, 'offsetArray[]');
				offset += BDAT.getCharSize(char);
			});
			this.writeUInt32(offset, 'offsetArray[]');
		} else {
			this.writeUInt32(BDAT.getCharSize(font.chars[0]), 'imageSize');
			this.writeUInt8(font.bbx.height, 'height');
			this.writeUInt8(font.bbx.width, 'width');
			this.writeInt8(0, 'horiBearingX');
			this.writeInt8(font.pxAscender, 'horiBearingY');
			this.writeUInt8(font.bbx.width, 'horiAdvance');
			this.writeInt8(-(font.bbx.width >> 1), 'vertBearingX');
			this.writeInt8(0, 'vertBearingY');
			this.writeUInt8(font.bbx.height, 'vertAdvance');
		}
		// adjust
		this.rewriteUInt32(this.size - BLOC_PREFIX_SIZE, BLOC_TABLE_SIZE_OFFSET);
	}
}

// -- OS/2 --
const OS_2_TABLE_SIZE = 96;

class OS_2 extends Table {
	constructor(font) {
		super(TS_SMALL, 'OS/2');
		// Version 4
		const xAvgCharWidth = font.emScale(font.avgWidth);  // otb1get.xAvgCharWidth(font);
		const ulCharRanges = otb1get.ulCharRanges(font);
		const ulCodePages = font.bmpOnly ? otb1get.ulCodePages(font) : [0, 0];
		// mostly from FontForge
		const scriptXSize = font.emScale(30, 100);
		const scriptYSize = font.emScale(40, 100);
		const subscriptYOff = scriptYSize >> 1;
		const xfactor = Math.tan(font.italicAngle * Math.PI / 180);
		const subscriptXOff = 0;  // stub, no overlapping characters yet
		const superscriptYOff = font.emAscender - scriptYSize;
		const superscriptXOff = -fnutil.round(xfactor * superscriptYOff);
		// write
		this.writeUInt16(4, 'version');
		this.writeInt16(xAvgCharWidth, 'xAvgCharWidth');
		this.writeUInt16(font.bold ? 700 : 400, 'usWeightClass');
		this.writeUInt16(5, 'usWidthClass');                      // medium
		this.writeInt16(0, 'fsType');
		this.writeInt16(scriptXSize, 'ySubscriptXSize');
		this.writeInt16(scriptYSize, 'ySubscriptYSize');
		this.writeInt16(subscriptXOff, 'ySubscriptXOffset');
		this.writeInt16(subscriptYOff, 'ySubscriptYOffset');
		this.writeInt16(scriptXSize, 'ySuperscriptXSize');
		this.writeInt16(scriptYSize, 'ySuperscriptYSize');
		this.writeInt16(superscriptXOff, 'ySuperscriptXOffset');
		this.writeInt16(superscriptYOff, 'ySuperscriptYOffset');
		this.writeInt16(font.lineSize, 'yStrikeoutSize');
		this.writeInt16(font.emScale(25, 100), 'yStrikeoutPosition');
		this.writeInt16(0, 'sFamilyClass');                       // no classification
		this.writeUInt8(2, 'bFamilyType');                        // text and display
		this.writeUInt8(0, 'bSerifStyle');                        // any
		this.writeUInt8(font.bold ? 8 : 6, 'bWeight');
		this.writeUInt8(font.proportional ? 3 : 9, 'bProportion');
		this.writeUInt8(0, 'bContrast');
		this.writeUInt8(0, 'bStrokeVariation');
		this.writeUInt8(0, 'bArmStyle');
		this.writeUInt8(0, 'bLetterform');
		this.writeUInt8(0, 'bMidline');
		this.writeUInt8(0, 'bXHeight');
		this.writeUInt32(ulCharRanges[0], 'ulCharRange1');
		this.writeUInt32(ulCharRanges[1], 'ulCharRange2');
		this.writeUInt32(ulCharRanges[2], 'ulCharRange3');
		this.writeUInt32(ulCharRanges[3], 'ulCharRange4');
		this.writeUInt32(0x586F7334, 'achVendID');                // 'Xos4'
		this.writeUInt16(OS_2.fsSelection(font), 'fsSelection');
		this.writeUInt16(Math.min(font.minCode, fnutil.UNICODE_BMP_MAX), 'firstChar');
		this.writeUInt16(Math.min(font.maxCode, fnutil.UNICODE_BMP_MAX), 'lastChar');
		this.writeInt16(font.emAscender, 'sTypoAscender');
		this.writeInt16(font.emDescender, 'sTypoDescender');
		this.writeInt16(font.params.lineGap, 'sTypoLineGap');
		this.writeUInt16(font.emAscender, 'usWinAscent');
		this.writeUInt16(-font.emDescender, 'usWinDescent');
		this.writeUInt32(ulCodePages[0], 'ulCodePageRange1');
		this.writeUInt32(ulCodePages[1], 'ulCodePageRange2');
		this.writeInt16(font.emScale(font.pxAscender * 0.6), 'sxHeight');    // stub
		this.writeInt16(font.emScale(font.pxAscender * 0.8), 'sCapHeight');  // stub
		this.writeUInt16(OS_2.defaultChar(font), 'usDefaultChar');
		this.writeUInt16(OS_2.breakChar(font), 'usBreakChar');
		this.writeUInt16(1, 'usMaxContext');
		// check
		this.checkSize(OS_2_TABLE_SIZE);
	}

	static breakChar(font) {
		return font.chars.findIndex(char => char.code === 0x20) !== -1 ? 0x20 : font.minCode;
	}

	static defaultChar(font) {
		if (font.defaultCode !== -1 && font.defaultCode <= fnutil.UNICODE_BMP_MAX) {
			return font.defaultCode;
		}
		return font.minCode && font.maxCode;
	}

	static fsSelection(font) {
		const fsSelection = Number(font.bold) * 5 + Number(font.italic);
		return fsSelection || (font.xlfd[bdf.XLFD.SLANT] === 'R' ? 0x40 : 0);
	}
}

// -- cmap --
const CMAP_4_PREFIX_SIZE = 12;
const CMAP_4_FORMAT_SIZE = 16;
const CMAP_4_SEGMENT_SIZE = 8;

const CMAP_12_PREFIX_SIZE = 20;
const CMAP_12_FORMAT_SIZE = 16;
const CMAP_12_GROUP_SIZE = 12;

class CMapRange {
	constructor(glyphIndex = 0, startCode = 0, finalCode = -2) {
		this.glyphIndex = glyphIndex;
		this.startCode = startCode;
		this.finalCode = finalCode;
	}

	get idDelta() {
		return (this.glyphIndex - this.startCode) & 0xFFFF;
	}
}

class CMAP extends Table {
	constructor(font) {
		super(TS_LARGE, 'cmap');
		// make ranges
		let ranges = [];
		let range = new CMapRange();

		for (let index = 0; index < font.chars.length; index++) {
			let code = font.chars[index].code;

			if (code === range.finalCode + 1) {
				range.finalCode++;
			} else {
				range = new CMapRange(index, code, code);
				ranges.push(range);
			}
		}
		// write
		if (font.bmpOnly) {
			if (font.maxCode < 0xFFFF) {
				ranges.push(new CMapRange(0, 0xFFFF, 0xFFFF));
			}
			this.writeFormat4(ranges);
		} else {
			this.writeFormat12(ranges);
		}
	}

	writeFormat4(ranges) {
		// index
		this.writeUInt16(0, 'version');
		this.writeUInt16(1, 'numberSubtables');
		// encoding subtables index
		this.writeUInt16(3, 'platformID');                        // Microsoft
		this.writeUInt16(1, 'platformSpecificID');                // Unicode BMP (UCS-2)
		this.writeUInt32(CMAP_4_PREFIX_SIZE, 'offset');           // for Unicode BMP (UCS-2)
		// cmap format 4
		const segCount = ranges.length;
		const subtableSize = CMAP_4_FORMAT_SIZE + segCount * CMAP_4_SEGMENT_SIZE;
		const searchRange = 2 << Math.floor(Math.log2(segCount));

		this.writeUInt16(4, 'format');
		this.writeUInt16(subtableSize, 'length');
		this.writeUInt16(0, 'language');                          // none/independent
		this.writeUInt16(segCount * 2, 'segCountX2');
		this.writeUInt16(searchRange, 'searchRange');
		this.writeUInt16(Math.log2(searchRange / 2), 'entrySelector');
		this.writeUInt16((segCount * 2) - searchRange, 'rangeShift');
		ranges.forEach(range => {
			this.writeUInt16(range.finalCode, 'endCode');
		});
		this.writeUInt16(0, 'reservedPad');
		ranges.forEach(range => {
			this.writeUInt16(range.startCode, 'startCode');
		});
		ranges.forEach(range => {
			this.writeUInt16(range.idDelta, 'idDelta');
		});
		ranges.forEach(() => this.writeUInt16(0), 'idRangeOffset');
		// check
		this.checkSize(CMAP_4_PREFIX_SIZE + subtableSize);
	}

	writeFormat12(ranges) {
		// index
		this.writeUInt16(0, 'version');
		this.writeUInt16(2, 'numberSubtables');
		// encoding subtables
		this.writeUInt16(0, 'platformID');                        // Unicode
		this.writeUInt16(4, 'platformSpecificID');                // Unicode 2.0+ full range
		this.writeUInt32(CMAP_12_PREFIX_SIZE, 'offset');          // for Unicode 2.0+ full range
		this.writeUInt16(3, 'platformID');                        // Microsoft
		this.writeUInt16(10, 'platformSpecificID');               // Unicode UCS-4
		this.writeUInt32(CMAP_12_PREFIX_SIZE, 'offset');          // for Unicode UCS-4
		// cmap format 12
		const subtableSize = CMAP_12_FORMAT_SIZE + ranges.length * CMAP_12_GROUP_SIZE;

		this.writeFixed(12, 'format');
		this.writeUInt32(subtableSize, 'length');
		this.writeUInt32(0, 'language');                          // none/independent
		this.writeUInt32(ranges.length, 'nGroups');
		this.ranges.forEach(range => {
			this.writeUInt32(range.startCode, 'startCharCode');
			this.writeUInt32(range.finalCode, 'endCharCode');
			this.writeUInt32(range.glyphIndex, 'startGlyphID');
		});
		// check
		this.checkSize(CMAP_12_PREFIX_SIZE + subtableSize);
	}
}

// -- glyf --
class GLYF extends Table {
	constructor() {
		super(TS_EMPTY, 'glyf');
	}
}

// -- head --
const HEAD_TABLE_SIZE = 54;
const HEAD_CHECKSUM_OFFSET = 8;

class HEAD extends Table {
	constructor(font) {
		super(TS_SMALL, 'head');
		this.writeFixed(1, 'version');
		this.writeFixed(1, 'fontRevision');
		this.writeUInt32(0, 'checksumAdjustment');                // adjusted later
		this.writeUInt32(0x5F0F3CF5, 'magicNumber');
		this.writeUInt16(HEAD.flags(font), 'flags');
		this.writeUInt16(font.params.emSize, 'unitsPerEm');
		this.writeUInt48(font.created, 'created');
		this.writeUInt48(font.modified, 'modified');
		this.writeInt16(0, 'xMin');
		this.writeInt16(font.emDescender, 'yMin');
		this.writeInt16(font.emMaxWidth, 'xMax');
		this.writeInt16(font.emAscender, 'yMax');
		this.writeUInt16(font.macStyle, 'macStyle');
		this.writeUInt16(font.params.lowPPem || font.bbx.height, 'lowestRecPPEM');
		this.writeInt16(font.params.dirHint, 'fontDirectionHint');
		this.writeInt16(0, 'indexToLocFormat');                   // short
		this.writeInt16(0, 'glyphDataFormat');                    // current
		// check
		this.checkSize(HEAD_TABLE_SIZE);
	}

	static flags(font) {
		return otb1get.containsRTL(font) ? 0x020B : 0x0B;         // y0 base, x0 lsb, scale int
	}
}

// -- hhea --
const HHEA_TABLE_SIZE = 36;

class HHEA extends Table {
	constructor(font) {
		super(TS_SMALL, 'hhea');
		this.writeFixed(1, 'version');
		this.writeInt16(font.emAscender, 'ascender');
		this.writeInt16(font.emDescender, 'descender');
		this.writeInt16(font.params.lineGap, 'lineGap');
		this.writeUInt16(font.emMaxWidth, 'advanceWidthMax');
		this.writeInt16(0, 'minLeftSideBearing');
		this.writeInt16(0, 'minRightSideBearing');
		this.writeInt16(font.xMaxExtent, 'xMaxExtent');
		this.writeInt16(font.italic ? 100 : 1, 'caretSlopeRise');
		this.writeInt16(font.italic ? 20 : 0, 'caretSlopeRun');
		this.writeInt16(0, 'caretOffset');
		this.writeInt16(0, 'reserved');
		this.writeInt16(0, 'reserved');
		this.writeInt16(0, 'reserved');
		this.writeInt16(0, 'reserved');
		this.writeInt16(0, 'metricDataFormat');                   // current
		this.writeUInt16(font.chars.length, 'numOfLongHorMetrics');
		// check
		this.checkSize(HHEA_TABLE_SIZE);
	}
}

// -- hmtx --
class HMTX extends Table {
	constructor(font) {
		super(TS_LARGE, 'hmtx');
		font.chars.forEach(char => {
			this.writeUInt16(font.emScaleWidth(char), 'advanceWidth');
			this.writeInt16(0, 'leftSideBearing');
		});
	}
}

// -- loca --
class LOCA extends Table {
	constructor(font) {
		super(TS_SMALL, 'loca');
		if (!font.params.singleLoca) {
			font.chars.forEach(() => this.writeUInt16(0, 'offset'));
		}
		this.writeUInt16(0, 'offset');
	}
}

// -- maxp --
const MAXP_TABLE_SIZE = 32;

class MAXP extends Table {
	constructor(font) {
		super(TS_SMALL, 'maxp');
		this.writeFixed(1, 'version');
		this.writeUInt16(font.chars.length, 'numGlyphs');
		this.writeUInt16(0, 'maxPoints');
		this.writeUInt16(0, 'maxContours');
		this.writeUInt16(0, 'maxComponentPoints');
		this.writeUInt16(0, 'maxComponentContours');
		this.writeUInt16(2, 'maxZones');
		this.writeUInt16(0, 'maxTwilightPoints');
		this.writeUInt16(1, 'maxStorage');
		this.writeUInt16(1, 'maxFunctionDefs');
		this.writeUInt16(0, 'maxInstructionDefs');
		this.writeUInt16(64, 'maxStackElements');
		this.writeUInt16(0, 'maxSizeOfInstructions');
		this.writeUInt16(0, 'maxComponentElements');
		this.writeUInt16(0, 'maxComponentDepth');
		// check
		this.checkSize(MAXP_TABLE_SIZE);
	}
}

// -- name --
const NAME_ID = {
	COPYRIGHT:        0,
	FONT_FAMILY:      1,
	FONT_SUBFAMILY:   2,
	UNIQUE_SUBFAMILY: 3,
	FULL_FONT_NAME:   4,
	LICENSE:          14
};

const NAME_HEADER_SIZE = 6;
const NAME_RECORD_SIZE = 12;

class NAME extends Table {
	constructor(font) {
		super(TS_LARGE, 'name');
		// compute names
		let names = new Map();
		const copyright = font.props.get('COPYRIGHT');

		if (copyright != null) {
			names.set(NAME_ID.COPYRIGHT, fnutil.unquote(copyright));
		}

		const family = font.xlfd[bdf.XLFD.FAMILY_NAME];
		const style = ['Regular', 'Bold', 'Italic', 'Bold Italic'][font.macStyle];

		names.set(NAME_ID.FONT_FAMILY, family);
		names.set(NAME_ID.FONT_SUBFAMILY, style);
		names.set(NAME_ID.UNIQUE_SUBFAMILY, `${family} ${style} bitmap height ${font.bbx.height}`);
		names.set(NAME_ID.FULL_FONT_NAME, `${family} ${style}`);

		let license = font.props.get('LICENSE');
		const notice = font.props.get('NOTICE');

		if (license == null && notice != null && notice.toLowerCase().includes('license')) {
			license = notice;
		}
		if (license != null) {
			names.set(NAME_ID.LICENSE, fnutil.unquote(license));
		}
		// header
		const count = names.size * (1 + 1);  // Unicode + Microsoft
		const stringOffset = NAME_HEADER_SIZE + NAME_RECORD_SIZE * count;

		this.writeUInt16(0, 'format');
		this.writeUInt16(count, 'count');
		this.writeUInt16(stringOffset, 'stringOffset');
		// name records / create values
		let values = new Table(TS_LARGE, 'name');

		names.forEach((str, nameID) => {
			const value = Buffer.from(str, 'utf16le').swap16();
			const bmp = font.bmpOnly && value.length === str.length * 2;
			// Unicode
			this.writeUInt16(0, 'platformID');                  // Unicode
			this.writeUInt16(bmp ? 3 : 4, 'platformSpecificID');
			this.writeUInt16(0, 'languageID');
			this.writeUInt16(nameID, 'nameID');
			this.writeUInt16(value.length, 'length');           // in bytes
			this.writeUInt16(values.size, 'offset');
			// Windows
			this.writeUInt16(3, 'platformID');                  // Microsoft
			this.writeUInt16(bmp ? 1 : 10, 'platformSpecificID');
			this.writeUInt16(font.params.wLangId, 'languageID');
			this.writeUInt16(nameID, 'nameID');
			this.writeUInt16(value.length, 'length');           // in bytes
			this.writeUInt16(values.size, 'offset');
			// value
			values.write(value);
		});
		// write values
		this.writeTable(values);
		// check
		this.checkSize(stringOffset + values.size);
	}
}

// -- post --
const POST_TABLE_SIZE = 32;

class POST extends Table {
	constructor(font) {
		super(TS_SMALL, 'post');
		this.writeFixed(font.params.postNames ? 2 : 3, 'format');
		this.writeFixed(font.italicAngle, 'italicAngle');
		this.writeInt16(font.underlinePosition, 'underlinePosition');
		this.writeInt16(font.lineSize, 'underlineThickness');
		this.writeUInt32(font.proportional ? 0 : 1, 'isFixedPitch');
		this.writeUInt32(0, 'minMemType42');
		this.writeUInt32(0, 'maxMemType42');
		this.writeUInt32(0, 'minMemType1');
		this.writeUInt32(0, 'maxMemType1');
		// names
		if (font.params.postNames) {
			let postNames = otb1get.postMacNames();
			const postMacCount = postNames.length;

			this.writeUInt16(font.chars.length, 'numberOfGlyphs');
			font.chars.forEach(char => {
				const name = char.props.get('STARTCHAR');
				const index = postNames.indexOf(name);

				if (index !== -1) {
					this.writeUInt16(index, 'glyphNameIndex');
				} else {
					this.writeUInt16(postNames.length, 'glyphNameIndex');
					postNames.push(name);
				}
			});

			postNames.slice(postMacCount).forEach(name => {
				this.writeUInt8(name.length, 'glyphNameLength');
				this.write(Buffer.from(name, 'binary'));
			});
		// check
		} else {
			this.checkSize(POST_TABLE_SIZE);
		}
	}
}

// -- SFNT --
const SFNT_HEADER_SIZE = 12;
const SFNT_RECORD_SIZE = 16;
const SFNT_SUBTABLES = [ BDAT, BLOC, OS_2, CMAP, GLYF, HEAD, HHEA, HMTX, LOCA, MAXP, NAME, POST ];

class SFNT extends Table {
	constructor(font) {
		super(TS_LARGE, 'SFNT');
		// create tables
		let tables = [];

		SFNT_SUBTABLES.forEach(Ctor => {
			tables.push(new Ctor(font));
		});
		// header
		const numTables = tables.length;
		const entrySelector = Math.floor(Math.log2(numTables));
		const searchRange = 16 << entrySelector;
		const contentOffset = SFNT_HEADER_SIZE + numTables * SFNT_RECORD_SIZE;
		let offset = contentOffset;
		let content = new Table(TS_LARGE, 'SFNT');
		let headChecksumOffset = -1;

		this.writeFixed(1, 'sfntVersion');
		this.writeUInt16(numTables, 'numTables');
		this.writeUInt16(searchRange, 'searchRange');
		this.writeUInt16(entrySelector, 'entrySelector');
		this.writeUInt16(numTables * 16 - searchRange, 'rangeShift');
		// table records / create content
		tables.forEach(table => {
			this.write(Buffer.from(table.tableName, 'binary'));
			this.writeUInt32(table.checksum(), 'checkSum');
			this.writeUInt32(offset, 'offset');
			this.writeUInt32(table.size, 'length');
			// create content
			if (table.tableName === 'head') {
				headChecksumOffset = offset + HEAD_CHECKSUM_OFFSET;
			}
			const paddedSize = table.size + table.padding;

			content.write(table.data.slice(0, paddedSize));
			offset += paddedSize;
		});
		// write content
		this.writeTable(content);
		// check
		this.checkSize(contentOffset + content.size);
		// adjust
		if (headChecksumOffset !== -1) {
			this.rewriteUInt32((0xB1B0AFBA - this.checksum()) >>> 0, headChecksumOffset);
		}
	}
}

// -- Export --
module.exports = Object.freeze({
	TS_EMPTY,
	TS_SMALL,
	TS_LARGE,
	Table,
	EM_SIZE_MIN,
	EM_SIZE_MAX,
	EM_SIZE_DEFAULT,
	Params,
	Options,
	Font,
	BDAT,
	BLOC,
	OS_2,
	CMAP,
	GLYF,
	HEAD,
	HHEA,
	HMTX,
	LOCA,
	MAXP,
	NAME,
	POST,
	SFNT
});
