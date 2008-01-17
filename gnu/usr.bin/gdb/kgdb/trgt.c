/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libgen.h>
#include <kvm.h>
#include <string.h>

#include <defs.h>
#include <command.h>
#include <frame-unwind.h>
#include <gdbthread.h>
#include <inferior.h>
#include <regcache.h>
#include <target.h>
#include <objfiles.h>
#include <gdbcore.h>
#include <language.h>

#include "kgdb.h"

static struct target_ops kgdb_trgt_ops;

#define	KERNOFF		(kgdb_kernbase ())
#define	INKERNEL(x)	((x) >= KERNOFF)

static CORE_ADDR
kgdb_kernbase (void)
{
	static CORE_ADDR kernbase;
	struct minimal_symbol *sym;

	if (kernbase == 0) {
		sym = lookup_minimal_symbol ("kernbase", NULL, NULL);
		if (sym == NULL) {
			kernbase = KERNBASE;
		} else {
			kernbase = SYMBOL_VALUE_ADDRESS (sym);
		}
	}
	return kernbase;
}

static char *
kgdb_trgt_extra_thread_info(struct thread_info *ti)
{
	static char buf[64];
	char *p, *s;

	p = buf + snprintf(buf, sizeof(buf), "PID=%d", ptid_get_pid(ti->ptid));
	s = kgdb_thr_extra_thread_info(ptid_get_tid(ti->ptid));
	if (s != NULL)
		snprintf(p, sizeof(buf) - (p - buf), ": %s", s);
	return (buf);
}

static void
kgdb_trgt_files_info(struct target_ops *target)
{
	struct target_ops *tb;

	tb = find_target_beneath(target);
	if (tb->to_files_info != NULL)
		tb->to_files_info(tb);
}

static void
kgdb_trgt_find_new_threads(void)
{
	struct target_ops *tb;

	if (kvm != NULL)
		return;

	tb = find_target_beneath(&kgdb_trgt_ops);
	if (tb->to_find_new_threads != NULL)
		tb->to_find_new_threads();
}

static char *
kgdb_trgt_pid_to_str(ptid_t ptid)
{
	static char buf[33];

	snprintf(buf, sizeof(buf), "Thread %ld", ptid_get_tid(ptid));
	return (buf);
}

static int
kgdb_trgt_thread_alive(ptid_t ptid)
{
	return (kgdb_thr_lookup_tid(ptid_get_tid(ptid)) != NULL);
}

static int
kgdb_trgt_xfer_memory(CORE_ADDR memaddr, char *myaddr, int len, int write,
    struct mem_attrib *attrib, struct target_ops *target)
{
	struct target_ops *tb;

	if (kvm != NULL) {
		if (len == 0)
			return (0);
		if (!write)
			return (kvm_read(kvm, memaddr, myaddr, len));
		else
			return (kvm_write(kvm, memaddr, myaddr, len));
	}
	tb = find_target_beneath(target);
	return (tb->to_xfer_memory(memaddr, myaddr, len, write, attrib, tb));
}

static void
kgdb_switch_to_thread(struct kthr *thr)
{
	if (thr->tid == ptid_get_tid(inferior_ptid))
		return;

	inferior_ptid = ptid_build(thr->pid, 0, thr->tid);
	flush_cached_frames ();
	registers_changed ();
	stop_pc = read_pc ();
	select_frame (get_current_frame ());
}

static void
kgdb_set_proc_cmd (char *arg, int from_tty)
{
	CORE_ADDR addr;
	struct kthr *thr;

	if (!arg)
		error_no_arg ("proc address for the new context");

	if (kvm == NULL)
		error ("no kernel core file");

	addr = (CORE_ADDR) parse_and_eval_address (arg);

	if (!INKERNEL (addr)) {
		thr = kgdb_thr_lookup_pid((int)addr);
		if (thr == NULL)
			error ("invalid pid");
	} else {
		thr = kgdb_thr_lookup_paddr(addr);
		if (thr == NULL)
			error("invalid proc address");
	}
	kgdb_switch_to_thread(thr);
}

static void
kgdb_set_tid_cmd (char *arg, int from_tty)
{
	CORE_ADDR addr;
	struct kthr *thr;

	if (!arg)
		error_no_arg ("TID or thread address for the new context");

	if (kvm == NULL)
		error ("no kernel core file");

	addr = (CORE_ADDR) parse_and_eval_address (arg);

	if (!INKERNEL (addr)) {
		thr = kgdb_thr_lookup_tid((int)addr);
		if (thr == NULL)
			error ("invalid TID");
	} else {
		thr = kgdb_thr_lookup_taddr(addr);
		if (thr == NULL)
			error("invalid thread address");
	}
	kgdb_switch_to_thread(thr);
}

static int
kld_ok (char *path)
{
	struct stat sb;

	if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode))
		return (1);
	return (0);
}

/*
 * Look for a matching file in the following order:
 * - filename + ".symbols" (e.g. foo.ko.symbols)
 * - filename + ".debug" (e.g. foo.ko.debug)
 * - filename (e.g. foo.ko)
 * - dirname(kernel) + filename + ".symbols" (e.g. /boot/kernel/foo.ko.symbols)
 * - dirname(kernel) + filename + ".debug" (e.g. /boot/kernel/foo.ko.debug)
 * - dirname(kernel) + filename (e.g. /boot/kernel/foo.ko)
 * - iterate over each path in the module path looking for:
 *   - dir + filename + ".symbols" (e.g. /boot/modules/foo.ko.symbols)
 *   - dir + filename + ".debug" (e.g. /boot/modules/foo.ko.debug)
 *   - dir + filename (e.g. /boot/modules/foo.ko)
 */
static int
find_kld_path (char *filename, char *path, size_t path_size)
{
	CORE_ADDR module_path_addr;
	char module_path[PATH_MAX];
	char *kernel_dir, *module_dir, *cp;

	snprintf(path, path_size, "%s.symbols", filename);
	if (kld_ok(path))
		return (1);
	snprintf(path, path_size, "%s.debug", filename);
	if (kld_ok(path))
		return (1);
	snprintf(path, path_size, "%s", filename);
	if (kld_ok(path))
		return (1);
	kernel_dir = dirname(kernel);
	if (kernel_dir != NULL) {
		snprintf(path, path_size, "%s/%s.symbols", kernel_dir,
		    filename);
		if (kld_ok(path))
			return (1);
		snprintf(path, path_size, "%s/%s.debug", kernel_dir, filename);
		if (kld_ok(path))
			return (1);
		snprintf(path, path_size, "%s/%s", kernel_dir, filename);
		if (kld_ok(path))
			return (1);
	}
	module_path_addr = kgdb_parse("linker_path");
	if (module_path_addr != 0 &&
	    kvm_read(kvm, module_path_addr, module_path, sizeof(module_path)) ==
	    sizeof(module_path)) {
		module_path[PATH_MAX - 1] = '\0';
		cp = module_path;
		while ((module_dir = strsep(&cp, ";")) != NULL) {
			snprintf(path, path_size, "%s/%s.symbols", module_dir,
			    filename);
			if (kld_ok(path))
				return (1);
			snprintf(path, path_size, "%s/%s.debug", module_dir,
			    filename);
			if (kld_ok(path))
				return (1);
			snprintf(path, path_size, "%s/%s", module_dir,
			    filename);
			if (kld_ok(path))
				return (1);			
		}
	}	
	return (0);
}

/*
 * Read a kernel pointer given a KVA in 'address'.
 */
static CORE_ADDR
read_pointer (CORE_ADDR address)
{
	union {
		uint32_t d32;
		uint64_t d64;
	} val;

	switch (TARGET_PTR_BIT) {
	case 32:
		if (kvm_read(kvm, address, &val.d32, sizeof(val.d32)) !=
		    sizeof(val.d32))
			return (0);
		return (val.d32);
	case 64:
		if (kvm_read(kvm, address, &val.d64, sizeof(val.d64)) !=
		    sizeof(val.d64))
			return (0);
		return (val.d64);
	default:
		return (0);
	}
}

/*
 * Try to find this kld in the kernel linker's list of linker files.
 */
static int
find_kld_address (char *arg, CORE_ADDR *address)
{
	CORE_ADDR kld, filename_addr;
	CORE_ADDR off_address, off_filename, off_next;
	char kld_filename[PATH_MAX];
	char *filename;
	size_t filelen;

	/* Compute offsets of relevant members in struct linker_file. */
	off_address = kgdb_parse("&((struct linker_file *)0)->address");
	off_filename = kgdb_parse("&((struct linker_file *)0)->filename");
	off_next = kgdb_parse("&((struct linker_file *)0)->link.tqe_next");
	if (off_address == 0 || off_filename == 0 || off_next == 0)
		return (0);

	filename = basename(arg);
	filelen = strlen(filename) + 1;
	kld = kgdb_parse("linker_files.tqh_first");
	while (kld != 0) {
		/* Try to read this linker file's filename. */
		filename_addr = read_pointer(kld + off_filename);
		if (filename_addr == 0)
			goto next_kld;
		if (kvm_read(kvm, filename_addr, kld_filename, filelen) !=
		    filelen)
			goto next_kld;

		/* Compare this kld's filename against our passed in name. */
		if (kld_filename[filelen - 1] != '\0')
			goto next_kld;
		if (strcmp(kld_filename, filename) != 0)
			goto next_kld;

		/*
		 * We found a match, use its address as the base
		 * address if we can read it.
		 */
		*address = read_pointer(kld + off_address);
		if (*address == 0)
			return (0);
		return (1);

	next_kld:
		kld = read_pointer(kld + off_next);
	}
	return (0);
}

static void
add_section(struct section_addr_info *section_addrs, int *sect_indexp,
    char *name, CORE_ADDR address)
{
	int sect_index;

	sect_index = *sect_indexp;
	section_addrs->other[sect_index].name = name;
	section_addrs->other[sect_index].addr = address;
	printf_unfiltered("\t%s_addr = %s\n", name,
	    local_hex_string(address));
	sect_index++;
	*sect_indexp = sect_index;
}

static void
kgdb_add_kld_cmd (char *arg, int from_tty)
{
	struct section_addr_info *section_addrs;
	struct cleanup *cleanup;
	char path[PATH_MAX];
	asection *sect;
	CORE_ADDR base_addr;
	bfd *bfd;
	CORE_ADDR text_addr, data_addr, bss_addr, rodata_addr;
	int sect_count, sect_index;

	if (!find_kld_path(arg, path, sizeof(path))) {
		error("unable to locate kld");
		return;
	}

	if (!find_kld_address(arg, &base_addr)) {
		error("unable to find kld in kernel");
		return;
	}

	/* Open the kld and find the offsets of the various sections. */
	bfd = bfd_openr(path, gnutarget);
	if (bfd == NULL) {
		error("\"%s\": can't open: %s", path,
		    bfd_errmsg(bfd_get_error()));
		return;
	}
	cleanup = make_cleanup_bfd_close(bfd);

	if (!bfd_check_format(bfd, bfd_object)) {
		do_cleanups(cleanup);
		error("\%s\": not an object file", path);
		return;
	}

	data_addr = bss_addr = rodata_addr = 0;
	sect = bfd_get_section_by_name (bfd, ".text");
	if (sect == NULL) {
		do_cleanups(cleanup);
		error("\"%s\": can't find text section", path);
		return;
	}
	text_addr = bfd_get_section_vma(bfd, sect);
	sect_count = 1;

	/* Save the offsets of relevant sections. */
	sect = bfd_get_section_by_name (bfd, ".data");
	if (sect != NULL) {
		data_addr = bfd_get_section_vma(bfd, sect);
		sect_count++;
	}

	sect = bfd_get_section_by_name (bfd, ".bss");
	if (sect != NULL) {
		bss_addr = bfd_get_section_vma(bfd, sect);
		sect_count++;
	}

	sect = bfd_get_section_by_name (bfd, ".rodata");
	if (sect != NULL) {
		rodata_addr = bfd_get_section_vma(bfd, sect);
		sect_count++;
	}

	do_cleanups(cleanup);

	printf_unfiltered("add symbol table from file \"%s\" at\n", path);

	/* Build a section table for symbol_file_add(). */
	section_addrs = alloc_section_addr_info(sect_count);
	cleanup = make_cleanup(xfree, section_addrs);
	sect_index = 0;
	add_section(section_addrs, &sect_index, ".text", base_addr + text_addr);
	if (data_addr != 0)
		add_section(section_addrs, &sect_index, ".data",
		    base_addr + data_addr);
	if (bss_addr != 0)
		add_section(section_addrs, &sect_index, ".bss",
		    base_addr + bss_addr);
	if (rodata_addr != 0)
		add_section(section_addrs, &sect_index, ".rodata",
		    base_addr + rodata_addr);

	symbol_file_add(path, from_tty, section_addrs, 0, OBJF_USERLOADED);

	reinit_frame_cache();

	do_cleanups(cleanup);
}

void
kgdb_target(void)
{
	struct kthr *kt;
	struct thread_info *ti;

	kgdb_trgt_ops.to_magic = OPS_MAGIC;
	kgdb_trgt_ops.to_shortname = "kernel";
	kgdb_trgt_ops.to_longname = "kernel core files.";
	kgdb_trgt_ops.to_doc = "Kernel core files.";
	kgdb_trgt_ops.to_stratum = thread_stratum;
	kgdb_trgt_ops.to_has_memory = 1;
	kgdb_trgt_ops.to_has_registers = 1;
	kgdb_trgt_ops.to_has_stack = 1;

	kgdb_trgt_ops.to_extra_thread_info = kgdb_trgt_extra_thread_info;
	kgdb_trgt_ops.to_fetch_registers = kgdb_trgt_fetch_registers;
	kgdb_trgt_ops.to_files_info = kgdb_trgt_files_info;
	kgdb_trgt_ops.to_find_new_threads = kgdb_trgt_find_new_threads;
	kgdb_trgt_ops.to_pid_to_str = kgdb_trgt_pid_to_str;
	kgdb_trgt_ops.to_store_registers = kgdb_trgt_store_registers;
	kgdb_trgt_ops.to_thread_alive = kgdb_trgt_thread_alive;
	kgdb_trgt_ops.to_xfer_memory = kgdb_trgt_xfer_memory;
	add_target(&kgdb_trgt_ops);
	push_target(&kgdb_trgt_ops);

	kt = kgdb_thr_first();
	while (kt != NULL) {
		ti = add_thread(ptid_build(kt->pid, 0, kt->tid));
		kt = kgdb_thr_next(kt);
	}
	if (curkthr != 0)
		inferior_ptid = ptid_build(curkthr->pid, 0, curkthr->tid);
	add_com ("proc", class_obscure, kgdb_set_proc_cmd,
	   "Set current process context");
	add_com ("tid", class_obscure, kgdb_set_tid_cmd,
	   "Set current thread context");
	add_com ("add-kld", class_files, kgdb_add_kld_cmd,
	   "Usage: add-kld FILE\n\
Load the symbols from the kernel loadable module FILE.");
}
