#
# Copyright (C) 2017-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

import codecs
import struct
import sys
import os

# -- InputFileStream --
class InputFileStream:
	def __init__(self, file_name, encoding='binary'):
		if file_name is not None:
			self.file = open(file_name, 'r') if encoding is None else open(file_name, 'rb')
			self.st_name = file_name
		else:
			self.file = sys.stdin if encoding is None else sys.stdin.buffer
			self.st_name = '<stdin>'

		if encoding not in [None, 'binary']:
			self.file = codecs.getreader(encoding)(self.file)

		self.line_no = 0
		self.eof = False


	def close(self):
		self.unseek()
		self.file.close()


	def fstat(self):
		return None if (self.file == sys.stdin.buffer or self.file.isatty()) else os.fstat(self.file.fileno())


	def location(self):
		return '%s:%s' % (self.st_name, 'EOF: ' if self.eof else '%d: ' % self.line_no if self.line_no > 0 else ' ')


	def process(self, callback):
		try:
			result = callback(self)
			self.close()
			return result
		except Exception as ex:
			ex.message = self.location() + getattr(ex, 'message', str(ex))
			raise


	def read_line(self):
		return self.read_lines(lambda line: line)


	def read_lines(self, callback):
		try:
			for line in self.file:
				self.line_no += 1
				self.eof = False
				line = callback(line.rstrip())
				if line is not None:
					return line
		except OSError:
			self.unseek()
			raise

		self.eof = True
		return None


	def unseek(self):
		self.line_no = 0
		self.eof = False


# -- OutputFileStream --
class OutputFileStream:
	def __init__(self, file_name, encoding='binary'):
		if file_name is not None:
			self.file = open(file_name, 'wb')
			self.st_name = file_name
		else:
			self.file = sys.stdout.buffer
			self.st_name = '<stdout>'

		if encoding is None and self.file.isatty():
			raise Exception(self.location() + 'binary output may not be send to a terminal')

		self.encoding = (None if encoding == 'binary' else encoding)
		self.close_attempt = False


	def abort(self):
		errors = ''

		if self.file != sys.stdout.buffer:
			if not self.close_attempt:
				try:
					self.close()
				except Exception as ex:
					errors += '\n%sclose: %s' % (self.location(), str(ex))

			try:
				os.remove(self.st_name)
			except Exception as ex:
				errors += '\n%sunlink: %s' % (self.location(), str(ex))

		return errors


	def close(self):
		self.close_attempt = True
		self.file.close()


	def location(self):
		return self.st_name + ': '


	def process(self, callback):
		try:
			callback(self)
			self.close()
		except Exception as ex:
			ex.message = self.location() + getattr(ex, 'message', str(ex)) + self.abort()
			raise


	def write(self, data):
		self.file.write(data)


	def write8(self, value):
		self.write(struct.pack('B', value))


	def write16(self, value):
		self.write(struct.pack('<H', value))


	def write32(self, value):
		self.write(struct.pack('<L', value))


	def write_line(self, text):
		self.write((text if self.encoding is None else bytes(text, self.encoding)) + b'\n')


	def write_prop(self, name, value):
		self.write_line((bytes(name, 'ascii') + b' ' + value).rstrip())


	def write_zstr(self, bstr, num_zeros):
		self.write(bstr + bytes(num_zeros))


# -- read/write file --
def read_file(file_name, callback, encoding='binary'):
	return InputFileStream(file_name, encoding).process(callback)


def write_file(file_name, callback, encoding='binary'):
	return OutputFileStream(file_name, encoding).process(callback)
