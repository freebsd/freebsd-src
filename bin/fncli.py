#
# Copyright (C) 2018-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

import sys
import os
import re

# -- Params --
class Params:
	def __init__(self):
		self.excstk = False


# -- Options --
class Options:
	def __init__(self, need_args, help_text, version_text):
		for name in need_args:
			if not re.fullmatch('(-[^-]|--[^=]+)', name):
				raise Exception('invalid option name "%s"' % name)

		self.need_args = need_args
		self.help_text = help_text
		self.version_text = version_text


	def posixly_correct(self):  # pylint: disable=no-self-use
		return 'POSIXLY_CORRECT' in os.environ


	def needs_arg(self, name):
		return name in self.need_args


	def fallback(self, name, params):
		if name == '--excstk':
			params.excstk = True
		elif name == '--help' and self.help_text is not None:
			sys.stdout.write(self.help_text)
			sys.exit(0)
		elif name == '--version' and self.version_text is not None:
			sys.stdout.write(self.version_text)
			sys.exit(0)
		else:
			suffix = ' (taking an argument?)' if self.needs_arg(name) else ''
			suffix += ', try --help' if self.help_text is not None else ''
			raise Exception('unknown option "%s"%s' % (name, suffix))


	def reader(self, args, skip_zero=True):
		return Options.Reader(self, args, skip_zero)


	class Reader:
		def __init__(self, options, args, skip_zero):
			self.options = options
			self.args = args
			self.skip_zero = skip_zero


		def __iter__(self):
			return Options.Reader.Iterator(self)


		class Iterator:
			def __init__(self, reader):
				self.options = reader.options
				self.args = reader.args
				self.optind = int(reader.skip_zero)
				self.chrind = 1
				self.endopt = False


			def __next__(self):
				if self.chrind == 0:
					self.optind += 1
					self.chrind = 1

				if self.optind == len(self.args):
					raise StopIteration

				arg = self.args[self.optind]

				if self.endopt or arg == '-' or not arg.startswith('-'):
					self.endopt = self.options.posixly_correct()
					name = None
					value = arg
				elif arg == '--':
					self.chrind = 0
					self.endopt = True
					return next(self)
				elif not arg.startswith('--'):
					name = '-' + arg[self.chrind]
					self.chrind += 1
					if self.chrind < len(arg):
						if not self.options.needs_arg(name):
							return (name, None)
						value = arg[self.chrind:]
					else:
						value = None
				elif '=' in arg and arg.index('=') >= 3:
					name = arg.split('=', 1)[0]
					if not self.options.needs_arg(name):
						raise Exception('option "%s" does not take an argument' % name)
					value = arg[len(name) + 1:]
				else:
					name = arg
					value = None

				if value is None and int(self.options.needs_arg(name)) > 0:
					self.optind += 1
					if self.optind == len(self.args):
						raise Exception('option "%s" requires an argument' % name)
					value = self.args[self.optind]

				self.chrind = 0
				return (name, value)


# -- Main --
def start(program_name, options, params, main_program):
	parsed = Params() if params is None else params

	try:

		if sys.hexversion < 0x3050000:
			raise Exception('python 3.5.0 or later required')

		if params is None:
			return main_program(options.reader(sys.argv), lambda name: options.fallback(name, parsed))

		nonopt = []

		for [name, value] in options.reader(sys.argv):
			if name is None:
				nonopt.append(value)
			else:
				options.parse(name, value, parsed)

		return main_program(nonopt, parsed)

	except Exception as ex:
		if parsed.excstk:
			raise  # loses the message information, but preserves the start() caller stack info

		message = getattr(ex, 'message', str(ex))
		sys.stderr.write('%s: %s\n' % (sys.argv[0] if sys.argv[0] else program_name, message))
		sys.exit(1)
