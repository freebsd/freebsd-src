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

const tty = require('tty');
const fs = require('fs');

// -- InputFileStream --
const BLOCK_SIZE = 4096;

class InputFileStream {
	constructor(fileName, encoding = 'binary') {
		if (fileName != null) {
			this.fd = fs.openSync(fileName, 'r');
			this.stName = fileName;
		} else {
			this.fd = process.stdin.fd;
			this.stName = '<stdin>';
		}
		this.encoding = encoding;
		this.unseek();
		this.lines = [];
		this.index = 0;
		this.buffer = Buffer.alloc(BLOCK_SIZE);
		this.remainder = '';
	}

	close() {
		this.unseek();
		fs.closeSync(this.fd);
	}

	fstat() {
		return (this.fd === process.stdin.fd || tty.isatty(this.fd)) ? null : fs.fstatSync(this.fd);
	}

	location() {
		let location = ' ';

		if (this.eof) {
			location = 'EOF: ';
		} else if (this.lineNo > 0) {
			location = `${this.lineNo}: `;

		}
		return `${this.stName}:${location}`;
	}

	_readBlock() {
		for (;;) {
			try {
				return fs.readSync(this.fd, this.buffer, 0, BLOCK_SIZE);
			} catch (e) {
				if (e.code === 'EOF') {
					return 0;
				}
				if (e.code !== 'EAGAIN') {
					this.unseek();
					throw e;
				}
			}
		}
	}

	readLine() {
		return this.readLines(line => line);
	}

	readLines(callback) {
		let line;

		do {
			while (this.index < this.lines.length) {
				this.lineNo++;
				line = callback(this.lines[this.index++].trimRight());

				if (line != null) {
					return line;
				}
			}

			var count = this._readBlock();

			this.index = 0;
			this.lines = (this.remainder + this.buffer.toString(this.encoding, 0, count)).split('\n');
			this.remainder = this.lines.pop();
			this.eof = false;
		} while (count > 0);

		if (this.remainder.length > 0) {
			this.lineNo++;
			line = callback(this.remainder.trimRight());
			this.remainder = '';
		} else {
			this.eof = true;
			line = null;
		}

		return line;
	}

	unseek() {
		this.lineNo = 0;
		this.eof = false;
	}
}

// -- OutputFileStream --
class OutputFileStream {
	constructor(fileName, encoding = 'binary') {
		if (fileName != null) {
			this.fd = fs.openSync(fileName, 'w');
			this.stName = fileName;
		} else {
			this.fd = process.stdout.fd;
			this.stName = '<stdout>';
		}
		if (encoding == null && tty.isatty(this.fd)) {
			throw new Error(this.location() + 'binary output may not be send to a terminal');
		}
		this.encoding = (encoding == null ? 'binary' : encoding);
		this.fbbuf = Buffer.alloc(4);
		this.closeAttempt = false;
	}

	close() {
		this.closeAttempt = true;
		fs.closeSync(this.fd);
	}

	destroy() {
		let errors = '';

		if (this.fd !== process.stdout.fd) {
			if (!this.closeAttempt) {
				try {
					fs.closeSync(this.fd);
				} catch (e) {
					errors += `\n${this.stName}: close: ${e.message}`;
				}
			}

			try {
				fs.unlinkSync(this.stName);
			} catch (e) {
				errors += `\n${this.stName}: unlink: ${e.message}`;
			}
		}

		return errors;
	}

	location() {
		return this.stName + ': ';
	}

	write(buffer) {
		fs.writeSync(this.fd, buffer, 0, buffer.length);
	}

	write8(value) {
		this.fbbuf.writeUInt8(value, 0);
		fs.writeSync(this.fd, this.fbbuf, 0, 1);
	}

	write16(value) {
		this.fbbuf.writeUInt16LE(value, 0);
		fs.writeSync(this.fd, this.fbbuf, 0, 2);
	}

	write32(value) {
		this.fbbuf.writeUInt32LE(value, 0);
		fs.writeSync(this.fd, this.fbbuf, 0, 4);
	}

	writeLine(text) {
		fs.writeSync(this.fd, text + '\n', null, this.encoding);
	}

	writeProp(name, value) {
		this.writeLine((name + ' ' + value).trimRight());
	}

	writeZStr(bstr, numZeros) {
		fs.writeSync(this.fd, bstr, null, 'binary');
		this.write(Buffer.alloc(numZeros));
	}
}

// -- Export --
module.exports = Object.freeze({
	InputFileStream,
	OutputFileStream
});
