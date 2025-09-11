/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * The build options file.
 *
 */

project: @com.gavinhoward.bc

language: @C11

version: {
	min: @24.04.05
}

mode: {
	language: @iterative
	stampers: @metadata
	dependencies: @dynamic
}

default_target: @all

presets: {
	debug: {
		debug: true
		optimization: "0"
		memcheck: true
		devtools: true
		strip: false
	}
	release: {
		optimization: "3"
		lto: true
	}
	// This is the predefined build for BSDs.
	bsd: {
		optimization: "3"
		history: @editline
		generated_tests: false
		install_manpages: false
		install_locales: @system
		strip: true
		bc_default_banner: false
		bc_default_sigint_reset: true
		dc_default_sigint_reset: true
		bc_default_tty_mode: true
		dc_default_tty_mode: false
		bc_default_prompt: @off
		dc_default_prompt: @off
		bc_default_expr_exit: true
		dc_default_expr_exit: true
		bc_default_digit_clamp: false
		dc_default_digit_clamp: false
	}
	// This is the predefined build to match the GNU bc/dc.
	gnu: {
		optimization: "3"
		generated_tests: false
		install_manpages: true
		install_locales: @system
		strip: true
		bc_default_banner: true
		bc_default_sigint_reset: true
		dc_default_sigint_reset: false
		bc_default_tty_mode: true
		dc_default_tty_mode: false
		bc_default_prompt: @tty_mode
		dc_default_prompt: @tty_mode
		bc_default_expr_exit: true
		dc_default_expr_exit: true
		bc_default_digit_clamp: false
		dc_default_digit_clamp: false
	}
	// This is the preferred release build of the author, Gavin D. Howard.
	gdh: {
		optimization: "3"
		install_manpages: true
		install_locales: @none
		bc/default_banner: true
		bc/default_sigint_reset: true
		dc/default_sigint_reset: true
		bc/default_tty_mode: true
		dc/default_tty_mode: true
		bc/default_prompt: @tty_mode
		dc/default_prompt: @tty_mode
		bc/default_expr_exit: false
		dc/default_expr_exit: false
		bc/default_digit_clamp: true
		dc/default_digit_clamp: true
	}
	// This is the preferred debug build of the author, Gavin D. Howard.
	dbg: {
		optimization: "0"
		debug: true
		strip: false
		install_manpages: true
		install_locales: @system
		bc/default_banner: true
		bc/default_sigint_reset: true
		dc/default_sigint_reset: true
		bc/default_tty_mode: true
		dc/default_tty_mode: true
		bc/default_prompt: @tty_mode
		dc/default_prompt: @tty_mode
		bc/default_expr_exit: false
		dc/default_expr_exit: false
		bc/default_digit_clamp: true
		dc/default_digit_clamp: true
	}
}

default_development: @debug
default_release: @release

options: {
	build_mode: {
		type: @option
		options: [
			@both
			@bc
			@dc
			@library
		]
		default: @both
		desc: "Which of the executables or library to build."
	}
	extra_math: {
		type: @bool
		default: true
		desc: "Enable the extra math extensions."
	}
	history: {
		type: @option
		options: [
			@none
			@builtin
			@editline
			@readline
		]
		default: @builtin
		desc: "Which history implementation should be used, if any."
	}
	locales: {
		type: @option
		options: [
			@none
			@system
			@all
		]
		default: @system
		desc: "Whether to disable locales, use just the system ones, or use all (for building a package)."
	}
	bc/default_banner: {
		type: @bool
		default: false
		desc: "Whether to display the bc version banner by default when in interactive mode."
	}
	bc/default_sigint_reset: {
		type: @bool
		default: true
		desc: "Whether SIGINT will reset bc by default, instead of exiting, when in interactive mode."
	}
	dc/default_sigint_reset: {
		type: @bool
		default: true
		desc: "Whether SIGINT will reset dc by default, instead of exiting, when in interactive mode."
	}
	bc/default_tty_mode: {
		type: @bool
		default: true
		desc: "Whether TTY mode for bc should be on by default when available."
	}
	dc/default_tty_mode: {
		type: @bool
		default: false
		desc: "Whether TTY mode for dc should be on by default when available."
	}
	bc/default_prompt: {
		type: @option
		options: [
			@off
			@tty_mode
			@on
		]
		default: @tty_mode
		desc: "Whether the prompt for bc should be on by default in TTY mode. This defaults to match TTY mode."
	}
	dc/default_prompt: {
		type: @option
		options: [
			@off
			@tty_mode
			@on
		]
		default: @tty_mode
		desc: "Whether the prompt for dc should be on by default in TTY mode. This defaults to match TTY mode."
	}
	bc/default_expr_exit: {
		type: @bool
		default: true
		desc: "Whether to exit bc by default if an expression or expression file is given with the -e or -f options."
	}
	dc/default_expr_exit: {
		type: @bool
		default: true
		desc: "Whether to exit dc by default if an expression or expression file is given with the -e or -f options."
	}
	bc/default_digit_clamp: {
		type: @bool
		default: false
		desc: "Whether to have bc, by default, clamp digits that are greater than or equal to the current ibase when parsing numbers."
	}
	dc/default_digit_clamp: {
		type: @bool
		default: false
		desc: "Whether to have dc, by default, clamp digits that are greater than or equal to the current ibase when parsing numbers."
	}
	karatsuba_len: {
		type: @num
		default: 32
		desc: "Set the Karatsuba length (default is 32). Must be a number and greater than or equal to 16."
	}
	execprefix: {
		type: @string
		default: ""
		desc: "The prefix to prepend to the executable names, to prevent collisions."
	}
	execsuffix: {
		type: @string
		default: ""
		desc: "The suffix to append to the executable names, to prevent collisions."
	}
	debug: {
		type: @bool
		default: false
		desc: "Enable debug info."
	}
	optimization: {
		type: @string
		default: "0"
		desc: "The optimization level for the C compiler."
	}
	lto: {
		type: @bool
		default: false
		desc: "Build with link-time optimization, if available."
	}
	strip: {
		type: @bool
		default: true
		desc: "Strip any binaries."
	}
	strict: {
		type: @bool
		default: true
		desc: "Build with strict compiler options."
	}
	force: {
		type: @bool
		default: false
		desc: "Force options that don't work. THIS IS FOR DEV ONLY!"
	}
	memcheck: {
		type: @bool
		default: false
		desc: "Enable memcheck mode, to check for memory leaks."
	}
	valgrind: {
		type: @bool
		default: false
		desc: "Enable Valgrind mode, to check for memory bugs."
	}
	afl: {
		type: @bool
		default: false
		desc: "Enable AFL++ mode."
	}
	ossfuzz: {
		type: @bool
		default: false
		desc: "Enable OSSFUZZ mode."
	}
	generated_tests: {
		type: @bool
		default: true
		desc: "Enable tests generated from a GNU bc-compatible program."
	}
	problematic_tests: {
		type: @bool
		default: true
		desc: "Enable tests that may be problematic."
	}
	coverage: {
		type: @bool
		default: false
		desc: "Enable code coverage (only works on GCC)."
	}
	install_manpages: {
		type: @bool
		default: true
		desc: "Whether to install manpages or not."
	}
	cflags: {
		type: @list
		default: []
		desc: "The command-line flags for the C compiler."
	}
	ldflags: {
		type: @list
		default: []
		desc: "The command-line flags for the C linker."
	}
	destdir: {
		type: @path
		default: ""
		desc: "The equivalent of $DESTDIR in other build systems."
	}
	prefix: {
		type: @path
		default: "/usr/local"
		desc: "The default prefix to install everything into."
	}
	bindir: {
		type: @path
		default: ""
		desc: "The directory to install executables into. Defaults to \"$prefix/bin\"."
	}
	libdir: {
		type: @path
		default: ""
		desc: "The directory to install libraries into. Defaults to \"$prefix/lib\"."
	}
	includedir: {
		type: @path
		default: ""
		desc: "The location to install headers in. Defaults to \"$prefix/include\"."
	}
	nlspath: {
		type: @path
		default: "/usr/share/locale/%L/%N"
		desc: "The location to install locales."
	}
	pc_path: {
		type: @path
		default: ""
		desc: "The location to pkg-config files to. Defaults to the output of `pkg-config --variable=pc_path pkg-config`."
	}
	datarootdir: {
		type: @path
		default: ""
		desc: "The root directory for data files. Defaults to `$prefix/share`."
	}
	datadir: {
		type: @path
		default: ""
		desc: "The directory for data files. Defaults to `$datarootdir`."
	}
	mandir: {
		type: @path
		default: ""
		desc: "The root directory for manpages. Defaults to `$datadir/man`."
	}
	man1dir: {
		type: @path
		default: ""
		desc: "The directory for manpages in section 1. Defaults to `$mandir/man1`."
	}
	man3dir: {
		type: @path
		default: ""
		desc: "The directory for manpages in section 3. Defaults to `$mandir/man3`."
	}
}
