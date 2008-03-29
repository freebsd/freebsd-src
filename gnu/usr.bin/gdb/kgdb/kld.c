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
#include <sys/stat.h>
#include <fcntl.h>
#include <kvm.h>
#include <libgen.h>

#include <defs.h>
#include <command.h>
#include <completer.h>
#include <environ.h>
#include <frame-unwind.h>
#include <inferior.h>
#include <objfiles.h>
#include <gdbcore.h>
#include <language.h>
#include <solist.h>

#include "kgdb.h"

struct lm_info {
	CORE_ADDR base_address;
};

/* Offsets of fields in linker_file structure. */
static CORE_ADDR off_address, off_filename, off_pathname, off_next;

/* KVA of 'linker_path' which corresponds to the kern.module_path sysctl .*/
static CORE_ADDR module_path_addr;

static struct target_so_ops kld_so_ops;

static int
kld_ok (char *path)
{
	struct stat sb;

	if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode))
		return (1);
	return (0);
}

/*
 * Look for a matching file checking for debug suffixes before the raw file:
 * - filename + ".symbols" (e.g. foo.ko.symbols)
 * - filename + ".debug" (e.g. foo.ko.debug)
 * - filename (e.g. foo.ko)
 */
static const char *kld_suffixes[] = {
	".debug",
	"",
	NULL
};

static int
check_kld_path (char *path, size_t path_size)
{
	const char **suffix;
	char *ep;

	ep = path + strlen(path);
	suffix = kld_suffixes;
	while (*suffix != NULL) {
		if (strlcat(path, *suffix, path_size) < path_size) {
			if (kld_ok(path))
				return (1);
		}

		/* Restore original path to remove suffix. */
		*ep = '\0';
		suffix++;
	}
	return (0);
}

/*
 * Try to find the path for a kld by looking in the kernel's directory and
 * in the various paths in the module path.
 */
static int
find_kld_path (char *filename, char *path, size_t path_size)
{
	char *module_path;
	char *kernel_dir, *module_dir, *cp;
	int error;

	kernel_dir = dirname(kernel);
	if (kernel_dir != NULL) {
		snprintf(path, path_size, "%s/%s", kernel_dir, filename);
		if (check_kld_path(path, path_size))
			return (1);
	}
	if (module_path_addr != 0) {
		target_read_string(module_path_addr, &module_path, PATH_MAX,
		    &error);
		if (error == 0) {
			make_cleanup(xfree, module_path);
			cp = module_path;
			while ((module_dir = strsep(&cp, ";")) != NULL) {
				snprintf(path, path_size, "%s/%s", module_dir,
				    filename);
				if (check_kld_path(path, path_size))
					return (1);
			}
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
	CORE_ADDR value;

	if (target_read_memory(address, (char *)&value, TARGET_PTR_BIT / 8) !=
	    0)
		return (0);	
	return (extract_unsigned_integer(&value, TARGET_PTR_BIT / 8));
}

/*
 * Try to find this kld in the kernel linker's list of linker files.
 */
static int
find_kld_address (char *arg, CORE_ADDR *address)
{
	CORE_ADDR kld;
	char *kld_filename;
	char *filename;
	int error;

	if (off_address == 0 || off_filename == 0 || off_next == 0)
		return (0);

	filename = basename(arg);
	for (kld = kgdb_parse("linker_files.tqh_first"); kld != 0;
	     kld = read_pointer(kld + off_next)) {
		/* Try to read this linker file's filename. */
		target_read_string(read_pointer(kld + off_filename),
		    &kld_filename, PATH_MAX, &error);
		if (error)
			continue;

		/* Compare this kld's filename against our passed in name. */
		if (strcmp(kld_filename, filename) != 0) {
			xfree(kld_filename);
			continue;
		}
		xfree(kld_filename);

		/*
		 * We found a match, use its address as the base
		 * address if we can read it.
		 */
		*address = read_pointer(kld + off_address);
		if (*address == 0)
			return (0);
		return (1);
	}
	return (0);
}

struct add_section_info {
	struct section_addr_info *section_addrs;
	int sect_index;
	CORE_ADDR base_addr;
	int add_kld_command;
};

static void
add_section (bfd *bfd, asection *sect, void *arg)
{
	struct add_section_info *asi = arg;
	CORE_ADDR address;
	char *name;

	/* Ignore non-resident sections. */
	if ((bfd_get_section_flags(bfd, sect) & (SEC_ALLOC | SEC_LOAD)) == 0)
		return;

	name = xstrdup(bfd_get_section_name(bfd, sect));
	make_cleanup(xfree, name);
	address = asi->base_addr + bfd_get_section_vma(bfd, sect);
	asi->section_addrs->other[asi->sect_index].name = name;
	asi->section_addrs->other[asi->sect_index].addr = address;
	asi->section_addrs->other[asi->sect_index].sectindex = sect->index;
	if (asi->add_kld_command)
		printf_unfiltered("\t%s_addr = %s\n", name,
		    local_hex_string(address));
	asi->sect_index++;
}

static void
load_kld (char *path, CORE_ADDR base_addr, int from_tty, int add_kld_command)
{
	struct add_section_info asi;
	struct cleanup *cleanup;
	bfd *bfd;

	/* Open the kld. */
	bfd = bfd_openr(path, gnutarget);
	if (bfd == NULL)
		error("\"%s\": can't open: %s", path,
		    bfd_errmsg(bfd_get_error()));
	cleanup = make_cleanup_bfd_close(bfd);

	if (!bfd_check_format(bfd, bfd_object))
		error("\%s\": not an object file", path);

	/* Make sure we have a .text section. */
	if (bfd_get_section_by_name (bfd, ".text") == NULL)
		error("\"%s\": can't find text section", path);

	if (add_kld_command)
		printf_unfiltered("add symbol table from file \"%s\" at\n",
		    path);

	/* Build a section table for symbol_file_add() from the bfd sections. */
	asi.section_addrs = alloc_section_addr_info(bfd_count_sections(bfd));
	cleanup = make_cleanup(xfree, asi.section_addrs);
	asi.sect_index = 0;
	asi.base_addr = base_addr;
	asi.add_kld_command = add_kld_command;
	bfd_map_over_sections(bfd, add_section, &asi);

	if (from_tty && (!query("%s", "")))
		error("Not confirmed.");

	symbol_file_add(path, from_tty, asi.section_addrs, 0,
	    add_kld_command ? OBJF_USERLOADED : 0);

	do_cleanups(cleanup);
}

void
kgdb_add_kld_cmd (char *arg, int from_tty)
{
	char path[PATH_MAX];
	CORE_ADDR base_addr;

	/* Try to open the raw path to handle absolute paths first. */
	snprintf(path, sizeof(path), "%s", arg);
	if (!check_kld_path(path, sizeof(path))) {

		/*
		 * If that didn't work, look in the various possible
		 * paths for the module.
		 */
		if (!find_kld_path(arg, path, sizeof(path))) {
			error("Unable to locate kld");
			return;
		}
	}

	if (!find_kld_address(arg, &base_addr)) {
		error("Unable to find kld in kernel");
		return;
	}

	load_kld(path, base_addr, from_tty, 1);

	reinit_frame_cache();
}

static void
kld_relocate_section_addresses (struct so_list *so, struct section_table *sec)
{

	sec->addr += so->lm_info->base_address;
	sec->endaddr += so->lm_info->base_address;
}

static void
kld_free_so (struct so_list *so)
{

	xfree(so->lm_info);
}

static void
kld_clear_solib (void)
{
}

static void
kld_solib_create_inferior_hook (void)
{
}

static void
kld_special_symbol_handling (void)
{
}

static struct so_list *
kld_current_sos (void)
{
	struct so_list *head, **prev, *new;
	CORE_ADDR kld, kernel;
	char *path;
	int error;

	head = NULL;
	prev = &head;

	/*
	 * Walk the list of linker files creating so_list entries for
	 * each non-kernel file.
	 */
	kernel = kgdb_parse("linker_kernel_file");
	for (kld = kgdb_parse("linker_files.tqh_first"); kld != 0;
	     kld = read_pointer(kld + off_next)) {
		/* Skip the main kernel file. */
		if (kld == kernel)
			continue;

		new = xmalloc(sizeof(*new));
		memset(new, 0, sizeof(*new));

		new->lm_info = xmalloc(sizeof(*new->lm_info));
		new->lm_info->base_address = 0;

		/* Read the base filename and store it in so_original_name. */
		target_read_string(read_pointer(kld + off_filename),
		    &path, sizeof(new->so_original_name), &error);
		if (error != 0) {
			warning("kld_current_sos: Can't read filename: %s\n",
			    safe_strerror(error));
			free_so(new);
			continue;
		}
		strlcpy(new->so_original_name, path,
		    sizeof(new->so_original_name));
		xfree(path);

		/*
		 * Try to read the pathname (if it exists) and store
		 * it in so_name.
		 */
		if (off_pathname != 0) {
			target_read_string(read_pointer(kld + off_pathname),
			    &path, sizeof(new->so_name), &error);
			if (error != 0) {
				warning(
		    "kld_current_sos: Can't read pathname for \"%s\": %s\n",
				    new->so_original_name,
				    safe_strerror(error));
				strlcpy(new->so_name, new->so_original_name,
				    sizeof(new->so_name));
			} else {
				strlcpy(new->so_name, path,
				    sizeof(new->so_name));
				xfree(path);
			}
		} else
			strlcpy(new->so_name, new->so_original_name,
			    sizeof(new->so_name));

		/* Read this kld's base address. */
		new->lm_info->base_address = read_pointer(kld + off_address);
		if (new->lm_info->base_address == 0) {
			warning(
			    "kld_current_sos: Invalid address for kld \"%s\"",
			    new->so_original_name);
			free_so(new);
			continue;
		}

		/* Append to the list. */
		*prev = new;
		prev = &new->next;
	}

	return (head);
}

static int
kld_open_symbol_file_object (void *from_ttyp)
{

	return (0);
}

static int
kld_in_dynsym_resolve_code (CORE_ADDR pc)
{

	return (0);
}

static int
kld_find_and_open_solib (char *solib, unsigned o_flags, char **temp_pathname)
{
	char path[PATH_MAX];
	int fd;

	*temp_pathname = NULL;
	if (!find_kld_path(solib, path, sizeof(path))) {
		errno = ENOENT;
		return (-1);
	}
	fd = open(path, o_flags, 0);
	if (fd >= 0)
		*temp_pathname = xstrdup(path);
	return (fd);
}

static int
load_klds_stub (void *arg)
{

	SOLIB_ADD(NULL, 1, &current_target, auto_solib_add);
	return (0);
}

void
kgdb_kld_init (void)
{
	struct cmd_list_element *c;

	/* Compute offsets of relevant members in struct linker_file. */
	off_address = kgdb_parse("&((struct linker_file *)0)->address");
	off_filename = kgdb_parse("&((struct linker_file *)0)->filename");
	off_pathname = kgdb_parse("&((struct linker_file *)0)->pathname");
	off_next = kgdb_parse("&((struct linker_file *)0)->link.tqe_next");
	if (off_address == 0 || off_filename == 0 || off_next == 0)
		return;

	module_path_addr = kgdb_parse("linker_path");

	kld_so_ops.relocate_section_addresses = kld_relocate_section_addresses;
	kld_so_ops.free_so = kld_free_so;
	kld_so_ops.clear_solib = kld_clear_solib;
	kld_so_ops.solib_create_inferior_hook = kld_solib_create_inferior_hook;
	kld_so_ops.special_symbol_handling = kld_special_symbol_handling;
	kld_so_ops.current_sos = kld_current_sos;
	kld_so_ops.open_symbol_file_object = kld_open_symbol_file_object;
	kld_so_ops.in_dynsym_resolve_code = kld_in_dynsym_resolve_code;
	kld_so_ops.find_and_open_solib = kld_find_and_open_solib;

	current_target_so_ops = &kld_so_ops;

	catch_errors(load_klds_stub, NULL, NULL, RETURN_MASK_ALL);

	c = add_com("add-kld", class_files, kgdb_add_kld_cmd,
	   "Usage: add-kld FILE\n\
Load the symbols from the kernel loadable module FILE.");
	set_cmd_completer(c, filename_completer);
}
