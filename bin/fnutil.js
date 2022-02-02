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

// -- Various --
const UNICODE_MAX = 1114111;  // 0x10FFFF
const UNICODE_BMP_MAX = 65535;  // 0xFFFF

function parseDec(name, s, minValue = 0, maxValue = UNICODE_MAX) {
	if (s.match(/^\s*-?\d+\s*$/) == null) {
		throw new Error(`invalid ${name} format`);
	}

	const value = parseInt(s, 10);

	if (minValue != null && value < minValue) {
		throw new Error(`${name} must be >= ${minValue}`);
	}
	if (maxValue != null && value > maxValue) {
		throw new Error(`${name} must be <= ${maxValue}`);
	}

	return value;
}

function parseHex(name, s, minValue = 0, maxValue = UNICODE_MAX) {
	if (s.match(/^\s*(0[xX])?[\dA-Fa-f]+\s*$/) == null) {
		throw new Error(`invalid ${name} format`);
	}

	const value = parseInt(s, 16);

	if (minValue != null && value < minValue) {
		throw new Error(`${name} must be >= ` + minValue.toString(16).toUpperCase());
	}
	if (maxValue != null && value > maxValue) {
		throw new Error(`${name} must be <= ` + maxValue.toString(16).toUpperCase());
	}

	return value;
}

function unihex(code) {
	return ('000' + code.toString(16).toUpperCase()).replace(/0+(?=[\dA-F]{4})/, '');
}

function round(value) {
	const esround = Math.round(value);

	return esround - Number(esround % 2 !== 0 && esround - value === 0.5);
}

function quote(s) {
	return '"' + s.replace(/"/g, '""') + '"';
}

function unquote(s, name) {
	if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) {
		s = s.substring(1, s.length - 1).replace(/""/g, '"');
	} else if (name != null) {
		throw new Error(name + ' must be quoted');
	}

	return s;
}

function message(prefix, severity, text) {
	process.stderr.write(`${prefix}${severity ? severity + ': ' : ''}${text}\n`);
}

function warning(prefix, text) {
	message(prefix, 'warning', text);
}

function splitWords(name, value, count) {
	const words = value.split(/\s+/, count + 1);

	if (words.length !== count) {
		throw new Error(`${name} must contain ${count} values`);
	}

	return words;
}

const GPL2PLUS_LICENSE = ('' +
	'This program is free software; you can redistribute it and/or modify it\n' +
	'under the terms of the GNU General Public License as published by the Free\n' +
	'Software Foundation; either version 2 of the License, or (at your option)\n' +
	'any later version.\n' +
	'\n' +
	'This program is distributed in the hope that it will be useful, but\n' +
	'WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY\n' +
	'or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License\n' +
	'for more details.\n' +
	'\n' +
	'You should have received a copy of the GNU General Public License along\n' +
	'with this program; if not, write to the Free Software Foundation, Inc.,\n' +
	'51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n');

// -- Exports --
module.exports = Object.freeze({
	UNICODE_MAX,
	UNICODE_BMP_MAX,
	parseDec,
	parseHex,
	unihex,
	round,
	quote,
	unquote,
	message,
	warning,
	splitWords,
	GPL2PLUS_LICENSE
});
