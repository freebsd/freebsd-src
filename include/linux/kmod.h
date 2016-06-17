#ifndef __LINUX_KMOD_H__
#define __LINUX_KMOD_H__

/*
 *	include/linux/kmod.h
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/errno.h>

#ifdef CONFIG_KMOD
extern int request_module(const char * name);
#else
static inline int request_module(const char * name) { return -ENOSYS; }
#endif

extern int exec_usermodehelper(char *program_path, char *argv[], char *envp[]);
extern int call_usermodehelper(char *path, char *argv[], char *envp[]);

#ifdef CONFIG_HOTPLUG
extern char hotplug_path [];
#endif

#endif /* __LINUX_KMOD_H__ */
