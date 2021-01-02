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

import codecs
import struct
import sys
import os


BINARY_ENCODING = '_fnio_binary'

class InputStream:
	def __init__(self, file_name, encoding=BINARY_ENCODING):
		if file_name is not None:
			if encoding == BINARY_ENCODING:
				self.file = open(file_name, 'rb')
			else:
				self.file = open(file_name, 'r', encoding=encoding)

			self.st_name = file_name
		else:
			if encoding == BINARY_ENCODING:
				self.file = sys.stdin.buffer
			elif encoding is None:
				self.file = sys.stdin
			else:
				self.file = codecs.getreader(encoding)(sys.stdin.buffer)

			self.st_name = '<stdin>'

		self.line_no = 0
		self.eof = False


	def close(self):
		self.line_no = 0
		self.eof = False
		self.file.close()


	def location(self):
		return '%s:%s' % (self.st_name, 'EOF: ' if self.eof else '%d: ' % self.line_no if self.line_no > 0 else ' ')


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
			self.line_no = 0
			self.eof = False
			raise

		self.eof = True
		return None


class OutputStream:
	def __init__(self, file_name):
		if file_name is not None:
			self.file = open(file_name, 'wb')
			self.st_name = file_name
		else:
			self.file = sys.stdout.buffer
			self.st_name = '<stdout>'

		self.close_attempt = False


	def close(self):
		self.close_attempt = True
		self.file.close()


	def destroy(self):
		errors = ''

		if self.file != sys.stdout.buffer:
			if not self.close_attempt:
				try:
					self.file.close()
				except Exception as ex:
					errors += '\n%s: close: %s' % (self.st_name, str(ex))

			try:
				os.remove(self.st_name)
			except Exception as ex:
				errors += '\n%s: unlink: %s' % (self.st_name, str(ex))

		return errors


	def location(self):
		return self.st_name + ': '


	def write(self, buffer):
		self.file.write(buffer)


	def write8(self, value):
		self.file.write(struct.pack('B', value))


	def write16(self, value):
		self.file.write(struct.pack('<H', value))


	def write32(self, value):
		self.file.write(struct.pack('<L', value))


	def write_line(self, bstr):
		self.file.write(bstr + b'\n')


	def write_prop(self, name, value):
		self.write_line((bytes(name, 'ascii') + b' ' + value).strip())


	def write_zstr(self, bstr, num_zeros):
		self.file.write(bstr + bytes(num_zeros))
