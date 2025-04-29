--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--

return {
	logo = {
		graphic = {
		    "  \027[31m```                        \027[31;1m`\027[31m",
		    " s` `.....---...\027[31;1m....--.```   -/\027[31m",
		    " +o   .--`         \027[31;1m/y:`      +.\027[31m",
		    "  yo`:.            \027[31;1m:o      `+-\027[31m",
		    "   y/               \027[31;1m-/`   -o/\027[31m",
		    "  .-                  \027[31;1m::/sy+:.\027[31m",
		    "  /                     \027[31;1m`--  /\027[31m",
		    " `:                          \027[31;1m:`\027[31m",
		    " `:                          \027[31;1m:`\027[31m",
		    "  /                          \027[31;1m/\027[31m",
		    "  .-                        \027[31;1m-.\027[31m",
		    "   --                      \027[31;1m-.\027[31m",
		    "    `:`                  \027[31;1m`:`",
		    "      \027[31;1m.--             `--.",
		    "         .---.....----.\027[m",
		},
		requires_color = true,
		shift = {x = 2, y = -1},
		image = "/boot/images/freebsd-logo-rev.png",
		image_rl = 15
	}
}
