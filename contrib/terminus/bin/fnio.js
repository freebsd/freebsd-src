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

const fs = require('fs');


const BINARY_ENCODING = 'latin1';

(function() {
	let orig = Buffer.alloc(256);

	for (let i = 0; i < 256; i++) {
		orig[i] = i;
	}

	const test = Buffer.from(orig.toString(BINARY_ENCODING), BINARY_ENCODING);

	if (orig.compare(test) !== 0) {
		throw new Error(`the ${BINARY_ENCODING} encoding is not 8-bit clean`);
	}
})();


const BLOCK_SIZE = 4096;

class InputStream {
	constructor(fileName, encoding = BINARY_ENCODING) {
		if (fileName != null) {
			this.fd = fs.openSync(fileName, 'r');
			this.stName = fileName;
		} else {
			this.fd = process.stdin.fd;
			this.stName = '<stdin>';
		}
		this.encoding = encoding;
		this.lineNo = 0;
		this.eof = false;
		this.lines = [];
		this.index = 0;
		this.buffer = Buffer.alloc(BLOCK_SIZE);
		this.remainder = '';
	}

	close() {
		this.lineNo = 0;
		this.eof = false;
		fs.closeSync(this.fd);
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
					this.lineNo = 0;
					this.eof = false;
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
}


class OutputStream {
	constructor(fileName) {
		if (fileName != null) {
			this.fd = fs.openSync(fileName, 'w');
			this.stName = fileName;
		} else {
			this.fd = process.stdout.fd;
			this.stName = '<stdout>';
		}
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

	writeLine(bstr) {
		fs.writeSync(this.fd, bstr + '\n', null, BINARY_ENCODING);
	}

	writeProp(name, value) {
		this.writeLine((name + ' ' + value).trim());
	}

	writeZStr(bstr, numZeros) {
		fs.writeSync(this.fd, bstr, null, BINARY_ENCODING);
		this.write(Buffer.alloc(numZeros));
	}
}


module.exports = Object.freeze({
	BINARY_ENCODING,
	InputStream,
	OutputStream
});
