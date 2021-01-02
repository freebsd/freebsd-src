#
# Copyright (c) 2018 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

import sys
import re


UNICODE_MAX = 1114111  # 0x10FFFF


def parse_dec(name, s, min_value=0, max_value=UNICODE_MAX):
	try:
		value = int(s)
	except ValueError:
		raise Exception('invalid %s format' % name)

	if min_value is not None and value < min_value:
		raise Exception('%s must be >= %d' % (name, min_value))

	if max_value is not None and value > max_value:
		raise Exception('%s must be <= %d' % (name, max_value))

	return value


def parse_hex(name, s, min_value=0, max_value=UNICODE_MAX):
	try:
		value = int(s, 16)
	except ValueError:
		raise Exception('invalid %s format' % name)

	if min_value is not None and value < min_value:
		raise Exception('%s must be >= %X' % (name, min_value))

	if max_value is not None and value > max_value:
		raise Exception('%s must be <= %X' % (name, max_value))

	return value


def quote(bstr):
	return b'"%s"' % bstr.replace(b'"', b'""')


def unquote(bstr, name=None):
	if len(bstr) >= 2 and bstr.startswith(b'"') and bstr.endswith(b'"'):
		bstr = bstr[1 : len(bstr) - 1].replace(b'""', b'"')
	elif name is not None:
		raise Exception(name + ' must be quoted')

	return bstr


def warning(prefix, message):
	if prefix.endswith(':'):
		prefix += ' '
	elif prefix and not prefix.endswith(': '):
		prefix += ': '

	sys.stderr.write('%swarning: %s\n' % (prefix, message))


def split_words(name, bstr, count):
	words = re.split(br'\s+', bstr, count)

	if len(words) != count:
		raise Exception('%s must contain %d values' % (name, count))

	return words


GPL2PLUS_LICENSE = ('' +
	'This program is free software; you can redistribute it and/or\n' +
	'modify it under the terms of the GNU General Public License as\n' +
	'published by the Free Software Foundation; either version 2 of\n' +
	'the License, or (at your option) any later version.\n' +
	'\n' +
	'This program is distributed in the hope that it will be useful,\n' +
	'but WITHOUT ANY WARRANTY; without even the implied warranty of\n' +
	'MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n' +
	'GNU General Public License for more details.\n')
