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
const bdf = require('./bdf.js');

// -- Params --
class Params extends fncli.Params {
	constructor() {
		super();
		this.filter = false;
		this.family = null;
		this.output = null;
	}
}

// -- Options --
const HELP = ('' +
	'usage: ucstoany [-f] [-F FAMILY] [-o OUTPUT] INPUT REGISTRY ENCODING TABLE...\n' +
	'Generate a BDF font subset.\n' +
	'\n' +
	'  -f, --filter   Discard characters with unicode FFFF; with registry ISO10646,\n' +
	'                 encode the first 32 characters with their indexes; with other\n' +
	'                 registries, encode all characters with indexes\n' +
	'  -F FAMILY      output font family name (default = input)\n' +
	'  -o OUTPUT      output file (default = stdout)\n' +
	'  TABLE          text file, one hexadecimal unicode per line\n' +
	'  --help         display this help and exit\n' +
	'  --version      display the program version and license, and exit\n' +
	'  --excstk       display the exception stack on error\n' +
	'\n' +
	'The input must be a BDF 2.1 font with unicode encoding.\n' +
	'Unlike ucs2any, all TABLE-s form a single subset of the input font.\n');

const VERSION = 'ucstoany 1.55, Copyright (C) 2019 Dimitar Toshkov Zhekov\n\n' + fnutil.GPL2PLUS_LICENSE;

class Options extends fncli.Options {
	constructor() {
		super(['-F', '-o'], HELP, VERSION);
	}

	parse(name, value, params) {
		switch (name) {
		case '-f':
		case '--filter':
			params.filter = true;
			break;
		case '-F':
			if (value.includes('-')) {
				throw new Error('FAMILY may not contain "-"');
			}
			params.family = value;
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
	if (nonopt.length < 4) {
		throw new Error('invalid number of arguments, try --help');
	}

	const input = nonopt[0];
	const registry = nonopt[1];
	const encoding = nonopt[2];
	let newCodes = [];

	if (!registry.match(/^[A-Za-z][\w.:()]*$/) || !encoding.match(/^[\w.:()]+$/)) {
		throw new Error('invalid registry or encoding');
	}

	// READ INPUT
	let ifs = new fnio.InputFileStream(input);

	try {
		var oldFont = bdf.Font.read(ifs);
		ifs.close();
	} catch (e) {
		e.message = ifs.location() + e.message;
		throw e;
	}

	// READ TABLES
	nonopt.slice(3).forEach(name => {
		ifs = new fnio.InputFileStream(name);

		try {
			ifs.readLines(line => {
				newCodes.push(fnutil.parseHex('unicode', line));
			});
			ifs.close();
		} catch (e) {
			e.message = ifs.location() + e.message;
			throw e;
		}
	});

	if (newCodes.length === 0) {
		throw new Error('no characters in the output font');
	}

	// CREATE GLYPHS
	const newFont = new bdf.Font();
	const charMap = [];
	let index = 0;
	let unstart = 0;

	if (parsed.filter) {
		unstart = (registry === 'ISO10646') ? 32 : bdf.CHARS_MAX;
	}

	// faster than Map() for <= 4K chars
	oldFont.chars.forEach(char => (charMap[char.code] = char));

	newCodes.forEach(code => {
		let oldChar = charMap[code];
		const uniFFFF = (oldChar == null);

		if (code === 0xFFFF && parsed.filter) {
			index++;
			return;
		}

		if (uniFFFF) {
			if (code !== 0xFFFF) {
				throw new Error(`${input} does not contain U+${fnutil.unihex(code)}`);
			}

			if (oldFont.defaultCode !== -1) {
				oldChar = charMap[oldFont.defaultCode];
			} else {
				oldChar = charMap[0xFFFD];

				if (oldChar == null) {
					throw new Error(`${input} does not contain U+FFFF, and no replacement found`);
				}
			}
		}

		const newChar = Object.assign(new bdf.Char(), oldChar);

		newChar.code = index >= unstart ? code : index;
		index++;
		newChar.props = new bdf.Props();
		oldChar.props.forEach((name, value) => newChar.props.set(name, value));
		newChar.props.set('ENCODING', newChar.code);
		newFont.chars.push(newChar);

		if (uniFFFF) {
			newChar.props.set('STARTCHAR', 'uniFFFF');
		} else if (oldChar.code === oldFont.defaultCode || (oldChar.code === 0xFFFD && newFont.defaultCode === -1)) {
			newFont.defaultCode = newChar.code;
		}
	});

	// CREATE HEADER
	let numProps;
	const family = (parsed.family != null) ? parsed.family : oldFont.xlfd[bdf.XLFD.FAMILY_NAME];

	oldFont.props.forEach((name, value) => {
		switch (name) {
		case 'FONT':
			newFont.xlfd = oldFont.xlfd.slice();
			newFont.xlfd[bdf.XLFD.FAMILY_NAME] = family;
			newFont.xlfd[bdf.XLFD.CHARSET_REGISTRY] = registry;
			newFont.xlfd[bdf.XLFD.CHARSET_ENCODING] = encoding;
			value = newFont.xlfd.join('-');
			break;
		case 'STARTPROPERTIES':
			numProps = fnutil.parseDec(name, value, 1);
			break;
		case 'FAMILY_NAME':
			value = fnutil.quote(family);
			break;
		case 'CHARSET_REGISTRY':
			value = fnutil.quote(registry);
			break;
		case 'CHARSET_ENCODING':
			value = fnutil.quote(encoding);
			break;
		case 'DEFAULT_CHAR':
			if (newFont.defaultCode !== -1) {
				value = newFont.defaultCode;
			} else {
				numProps -= 1;
				return;
			}
			break;
		case 'ENDPROPERTIES':
			if (newFont.defaultCode !== -1 && newFont.props.get('DEFAULT_CHAR') == null) {
				newFont.props.set('DEFAULT_CHAR', newFont.defaultCode);
				numProps += 1;
			}
			newFont.props.set('STARTPROPERTIES', numProps);
			break;
		case 'CHARS':
			value = newFont.chars.length;
			break;
		}
		newFont.props.set(name, value);
	});

	// COPY FIELDS
	newFont.bbx = oldFont.bbx;

	// WRITE OUTPUT
	let ofs = new fnio.OutputFileStream(parsed.output);

	try {
		newFont.write(ofs);
		ofs.close();
	} catch (e) {
		e.message = ofs.location() + e.message + ofs.destroy();
		throw e;
	}
}

if (require.main === module) {
	fncli.start('ucstoany.js', new Options(), new Params(), mainProgram);
}
