/* Handle FreeBSD kernel modules as shared libraries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000,
   2001
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#define _KERNEL
#include <sys/linker.h>
#undef _KERNEL

/* XXX, kludge to avoid duplicate definitions while sys/linker.h is used. */
#define _ELF_COMMON_H

#include "defs.h"
#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"

#include "solist.h"

struct lm_info
  {
     CORE_ADDR address;
  };

static int try_modpath (char *buf, int buflen, char *fmt, ...);
static char *guess_modpath (char *modname);

static void
kgdb_relocate_section_addresses (struct so_list *so,
				 struct section_table *sec)
{
  sec->addr += so->lm_info->address;
  sec->endaddr += so->lm_info->address;
}

static int
kgdb_open_symbol_file_object (void *from_ttyp)
{
  warning ("kgdb_open_symbol_file_object called\n");
  return 0;
}

static struct so_list *
kgdb_current_sos (void)
{
  linker_file_list_t linker_files;
  struct linker_file lfile;
  struct minimal_symbol *msymbol;
  struct linker_file *lfilek;
  struct so_list *head = NULL;
  struct so_list **link_ptr = &head;

  CORE_ADDR lfiles_addr;

  msymbol = lookup_minimal_symbol ("linker_files", NULL, symfile_objfile);
  if (msymbol == NULL || SYMBOL_VALUE_ADDRESS (msymbol) == 0)
    {
      warning ("failed to find linker_files symbol\n");
      return 0;
    }
  lfiles_addr = SYMBOL_VALUE_ADDRESS (msymbol);
  if (target_read_memory (lfiles_addr, (char *)&linker_files,
			  sizeof (linker_files)))
    {
      warning ("failed to read linker_files data\n");
      return 0;
    }
  for (lfilek = TAILQ_FIRST (&linker_files); lfilek != NULL;
       lfilek = TAILQ_NEXT (&lfile, link))
   {
      struct so_list *new;
      struct cleanup *old_chain;
      char *buf;
      int errcode;

      if (target_read_memory ((CORE_ADDR) lfilek, (char *) &lfile,
	  sizeof (lfile)))
	{
	  warning ("failed to read linker file data at %p\n", lfilek);
	  return 0;
	}
      target_read_string ((CORE_ADDR) lfile.filename, &buf,
			  SO_NAME_MAX_PATH_SIZE - 1, &errcode);
      if (errcode != 0)
	{
	  warning ("cannot read linker file pathname: %s\n",
		   safe_strerror (errcode));
	  return 0;
	}
      if (strlen (buf) < 3 || strcmp (&buf[strlen (buf) - 3], ".ko") != 0)
	{
	  xfree (buf);
	  continue;
	}

      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      old_chain = make_cleanup (xfree, new);

      memset (new, 0, sizeof (*new));

      new->lm_info = xmalloc (sizeof (struct lm_info));
      make_cleanup (xfree, new->lm_info);

      new->lm_info->address = (CORE_ADDR) lfile.address;

      strncpy (new->so_original_name, buf, SO_NAME_MAX_PATH_SIZE - 1);
      new->so_original_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
      xfree (buf);
      snprintf (new->so_name, SO_NAME_MAX_PATH_SIZE, "%s",
		guess_modpath (new->so_original_name));

      new->next = NULL;
      *link_ptr = new;
      link_ptr = &new->next;

      discard_cleanups (old_chain);
    }
  return head;
}

static int
kgdb_in_dynsym_resolve_code (CORE_ADDR pc)
{
  warning ("kgdb_in_dynsym_resolve_code called\n");
  return 0;
}

static void
kgdb_special_symbol_handling (void)
{
}

static void
kgdb_solib_create_inferior_hook (void)
{
  struct so_list *inferior_sos;

  inferior_sos = kgdb_current_sos ();
  if (inferior_sos)
    {
      solib_add (NULL, /*from_tty*/0, NULL, auto_solib_add);
    }
}

static void
kgdb_clear_solib (void)
{
}

static void
kgdb_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}

static int
try_modpath (char *buf, int buflen, char *fmt, ...)
{
  struct stat sb;
  va_list ap;

  va_start (ap, fmt);
  vsnprintf (buf, buflen, fmt, ap);
  va_end(ap);

  return (stat (buf, &sb) == 0);
}

static char *
guess_modpath (char *modname)
{
  static char buf[2048], moddir[128], syspath[1024];
  struct minimal_symbol *msymbol;
  char *kernpath, *objpath, *p, *version;
  int errcode, n, syspathlen;

  /* Set default module location */
  snprintf (buf, sizeof (buf), "/boot/kernel/%s", modname);

  /* Guess at the subdirectory off sys/modules. XXX, only sometimes correct */
  n = strlen (modname);
  if (n > 3 && strcmp (&modname[n - 3], ".ko") == 0)
    n -= 3;
  snprintf (moddir, sizeof (moddir), "%.*s", n, modname);

  /* Try to locate the kernel compile location from version[] */
  msymbol = lookup_minimal_symbol ("version", NULL, symfile_objfile);
  if (msymbol == NULL || SYMBOL_VALUE_ADDRESS (msymbol) == 0)
    {
      warning("cannot find `version' symbol; using default module path\n");
      return buf;
    }
  target_read_string (SYMBOL_VALUE_ADDRESS (msymbol), &version, 2048, &errcode);
  if (errcode != 0)
    {
      warning ("cannot read `version' string; using default module path: %s\n",
	       safe_strerror (errcode));
      return buf;
    }

  /* Find the kernel build path after user@host: on the second line. */
  if ((p = strchr (version, '\n')) == NULL ||
      (kernpath = strchr (p, ':')) == NULL ||
      (p = strchr (kernpath, '\n')) == NULL)
    {
      warning ("cannot parse version[]; using default module path\n");
      xfree (version);
      return buf;
    }
  kernpath++;
  *p = '\0';

  /*
   * Find the absolute path to src/sys by skipping back over path
   * components until we find a "/sys/".
   */
  syspathlen = 0;
  while (p > kernpath && syspathlen == 0)
    {
      while (p > kernpath && *p != '/')
	p--;
      if (strncmp (p, "/sys/", 5) == 0)
        syspathlen = p - kernpath + 4;
      else if (p > kernpath)
	p--;
    }
  if (syspathlen == 0)
    {
      warning ("cannot find /sys/ in `%s'; using default module path\n",
	       kernpath);
      xfree (version);
      return buf;
    }
  /*
   * For kernels compiled with buildkernel, the object path will have
   * been prepended to the /sys/ path in `kernpath'.
   */
  objpath = getenv ("MAKEOBJDIRPREFIX");
  if (objpath == NULL)
    objpath = "/usr/obj";
  n = strlen (objpath);
  if (syspathlen > n + 1 && strncmp (kernpath, objpath, n) == 0 &&
      kernpath[n] == '/')
    snprintf (syspath, sizeof (syspath), "%.*s", syspathlen - n, kernpath + n);
  else
    snprintf (syspath, sizeof (syspath), "%.*s", syspathlen, kernpath);

  /* Now try to find the module file */
  if (!try_modpath (buf, sizeof (buf), "./%s.debug", modname) &&
      !try_modpath (buf, sizeof (buf), "./%s", modname) && !try_modpath (buf,
      sizeof (buf), "%s/modules%s/modules/%s/%s.debug", kernpath, syspath,
      moddir, modname) && !try_modpath (buf, sizeof (buf),
      "%s/modules%s/modules/%s/%s", kernpath, syspath, moddir, modname) &&
      !try_modpath (buf, sizeof (buf), "/boot/kernel/%s.debug", modname) &&
      !try_modpath (buf, sizeof (buf), "/boot/kernel/%s", modname))
    {
      warning ("cannot find file for module %s\n", modname);
      snprintf (buf, sizeof (buf), "%s", modname);
    }
  xfree (version);

  return buf;
}

struct target_so_ops kgdb_so_ops;

void
_initialize_kgdb_solib (void)
{
  kgdb_so_ops.relocate_section_addresses = kgdb_relocate_section_addresses;
  kgdb_so_ops.free_so = kgdb_free_so;
  kgdb_so_ops.clear_solib = kgdb_clear_solib;
  kgdb_so_ops.solib_create_inferior_hook = kgdb_solib_create_inferior_hook;
  kgdb_so_ops.special_symbol_handling = kgdb_special_symbol_handling;
  kgdb_so_ops.current_sos = kgdb_current_sos;
  kgdb_so_ops.open_symbol_file_object = kgdb_open_symbol_file_object;
  kgdb_so_ops.in_dynsym_resolve_code = kgdb_in_dynsym_resolve_code;
}
