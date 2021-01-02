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


const UNICODE_MAX = 1114111;  // 0x10FFFF

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
	let result = Math.round(value);

	return result - Number(result % 2 !== 0 && result - value === 0.5);
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

function warning(prefix, message) {
	if (prefix.endsWith(':')) {
		prefix += ' ';
	} else if (prefix.length > 0 && !prefix.endsWith(': ')) {
		prefix += ': ';
	}
	process.stderr.write(`${prefix}warning: ${message}\n`);
}

function splitWords(name, value, count) {
	const words = value.split(/\s+/, count + 1);

	if (words.length !== count) {
		throw new Error(`${name} must contain ${count} values`);
	}

	return words;
}

const GPL2PLUS_LICENSE = ('' +
	'This program is free software; you can redistribute it and/or\n' +
	'modify it under the terms of the GNU General Public License as\n' +
	'published by the Free Software Foundation; either version 2 of\n' +
	'the License, or (at your option) any later version.\n' +
	'\n' +
	'This program is distributed in the hope that it will be useful,\n' +
	'but WITHOUT ANY WARRANTY; without even the implied warranty of\n' +
	'MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n' +
	'GNU General Public License for more details.\n');


module.exports = Object.freeze({
	UNICODE_MAX,
	parseDec,
	parseHex,
	unihex,
	round,
	quote,
	unquote,
	warning,
	splitWords,
	GPL2PLUS_LICENSE
});
