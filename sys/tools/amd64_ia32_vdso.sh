#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Konstantin Belousov <kib@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

set -e

${CC} -x assembler-with-cpp -DLOCORE -fPIC -nostdinc -c -m32 \
   -o ia32_sigtramp.pico -I. -I"${S}" -include opt_global.h \
   "${S}"/amd64/ia32/ia32_sigtramp.S

${LD} --shared -Bsymbolic -soname="elf-vdso32.so.1" \
   -T "${S}"/conf/vdso_amd64_ia32.ldscript \
   --eh-frame-hdr --no-undefined -z rodynamic -z norelro -nmagic \
   --hash-style=sysv --fatal-warnings --strip-all \
   -o elf-vdso32.so.1 ia32_sigtramp.pico

if [ "$(wc -c elf-vdso32.so.1 | ${AWK} '{print $1}')" -gt 2048 ]
then
    echo "elf-vdso32.so.1 too large" 1>&2
    exit 1
fi

if [ -n "$(${ELFDUMP} -d elf-vdso32.so.1 | \
  ${AWK} '/DT_REL.*SZ/{print "RELOCS"}')" ]
then
    echo "elf-vdso32.so.1 contains runtime relocations" 1>&2
    exit 1
fi

${CC} -x assembler-with-cpp -DLOCORE -fPIC -nostdinc -c \
   -o elf-vdso32.so.o -I. -I"${S}" -include opt_global.h \
   -DVDSO_NAME=elf_vdso32_so_1 -DVDSO_FILE=elf-vdso32.so.1 \
   "${S}"/tools/vdso_wrap.S

${NM} -D elf-vdso32.so.1 | ${AWK} \
   '/__vdso_ia32_sigcode/{printf "#define VDSO_IA32_SIGCODE_OFFSET 0x%s\n",$1}
    /__vdso_freebsd4_ia32_sigcode/{printf "#define VDSO_FREEBSD4_IA32_SIGCODE_OFFSET 0x%s\n",$1}
    /__vdso_ia32_osigcode/{printf "#define VDSO_IA32_OSIGCODE_OFFSET 0x%s\n",$1}
    /__vdso_lcall_tramp/{printf "#define VDSO_LCALL_TRAMP_OFFSET 0x%s\n",$1}' \
   >vdso_ia32_offsets.h
