/*
  Copyright (C) 2018-2020 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>

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

// -- Params --
class Params {
	constructor() {
		this.excstk = false;
	}
}

// -- Options --
class Options {
	constructor(needArgs, helpText, versionText) {
		needArgs.forEach(name => {
			if (!name.match(/^(-[^-]|--[^=]+)$/)) {
				throw new Error(`invalid option name "${name}"`);
			}
		});
		this.needArgs = needArgs;
		this.helpText = helpText;
		this.versionText = versionText;
	}

	posixlyCorrect() {  // eslint-disable-line class-methods-use-this
		return process.env['POSIXLY_CORRECT'] != null;
	}

	needsArg(name) {
		return this.needArgs.includes(name);
	}

	fallback(name, params) {
		if (name === '--excstk') {
			params.excstk = true;
		} else if (name === '--help' && this.helpText != null) {
			process.stdout.write(this.helpText);
			process.exit(0);
		} else if (name === '--version' && this.versionText != null) {
			process.stdout.write(this.versionText);
			process.exit(0);
		} else {
			let suffix = this.needsArg(name) ? ' (taking an argument?)' : '';

			suffix += (this.helpText != null) ? ', try --help' : '';
			throw new Error(`unknown option "${name}"${suffix}`);
		}
	}

	reader(args, skip = 2) {
		return new Options.Reader(this, args, skip);
	}
}

Options.Reader = class {
	constructor(options, args, skip) {
		this.options = options;
		this.args = args;
		this.skip = skip;
	}

	forEach(callback) {
		let optind;

		for (optind = this.skip; optind < this.args.length; optind++) {
			let arg = this.args[optind];

			if (arg === '-' || !arg.startsWith('-')) {
				if (this.options.posixlyCorrect()) {
					break;
				}
				callback(null, arg);
			} else if (arg === '--') {
				optind++;
				break;
			} else {
				let name, value;

				if (!arg.startsWith('--')) {
					for (;;) {
						name = arg.substring(0, 2);
						value = (name !== arg) ? arg.substring(2) : null;

						if (this.options.needsArg(name) || value == null) {
							break;
						}
						callback(name, null);
						arg = '-' + value;
					}
				} else if (arg.indexOf('=') >= 3) {
					name = arg.split('=', 1)[0];
					if (!this.options.needsArg(name)) {
						throw new Error(`option "${name}" does not take an argument`);
					}
					value = arg.substring(name.length + 1);
				} else {
					name = arg;
					value = null;
				}

				if (value == null && Number(this.options.needsArg(name)) > 0) {
					if (++optind === this.args.length) {
						throw new Error(`option "${name}" requires an argument`);
					}
					value = this.args[optind];
				}
				callback(name, value);
			}
		}

		this.args.slice(optind).forEach(value => callback(null, value));
	}
};

Object.defineProperty(Options, 'Reader', { 'enumerable': false });
Object.defineProperty(Options.Reader, 'name', { value: 'Reader' });

// -- Main --
function start(programName, options, params, mainProgram) {  // eslint-disable-line consistent-return
	const parsed = (params != null) ? params : new Params();

	try {
		const version = process.version.match(/^v?(\d+)\.(\d+)/);

		if (version.length < 3) {
			throw new Error('unable to obtain node version');
		} else if ((parseInt(version[1]) * 1000 + parseInt(version[2])) < 6009) {
			throw new Error('node version 6.9.0 or later required');
		}

		if (params == null) {
			return mainProgram(options.reader(process.argv), name => options.fallback(name, parsed));
		} else {
			let nonopt = [];

			options.reader(process.argv).forEach((name, value) => {
				if (name == null) {
					nonopt.push(value);
				} else {
					options.parse(name, value, parsed);
				}
			});
			return mainProgram(nonopt, parsed);
		}
	} catch (e) {
		if (parsed.excstk) {
			if (e.stack != null) {
				process.stderr.write(e.stack + '\n');
			} else {
				throw e;
			}
		} else {
			process.stderr.write(`${process.argv.length >= 2 ? process.argv[1] : programName}: ${e.message}\n`);
		}
		process.exit(1);
	}
}

// -- Exports --
module.exports = Object.freeze({
	Params,
	Options,
	start
});
