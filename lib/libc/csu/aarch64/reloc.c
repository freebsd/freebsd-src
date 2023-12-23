/*-
 * Copyright (c) 2019 Leandro Lupori
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static void
crt1_handle_rela(const Elf_Rela *r)
{
	typedef Elf_Addr (*ifunc_resolver_t)(
	    uint64_t, uint64_t, uint64_t, uint64_t,
	    uint64_t, uint64_t, uint64_t, uint64_t);
	Elf_Addr *ptr, *where, target;

	switch (ELF_R_TYPE(r->r_info)) {
	case R_AARCH64_IRELATIVE:
		ptr = (Elf_Addr *)r->r_addend;
		where = (Elf_Addr *)r->r_offset;
		target = ((ifunc_resolver_t)ptr)(0, 0, 0, 0, 0, 0, 0, 0);
		*where = target;
		break;
	}
}
