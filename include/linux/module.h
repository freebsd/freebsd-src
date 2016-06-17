/*
 * Dynamic loading of modules into the kernel.
 *
 * Rewritten by Richard Henderson <rth@tamu.edu> Dec 1996
 */

#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#ifdef __GENKSYMS__
#  define _set_ver(sym) sym
#  undef  MODVERSIONS
#  define MODVERSIONS
#else /* ! __GENKSYMS__ */
# if !defined(MODVERSIONS) && defined(EXPORT_SYMTAB)
#   define _set_ver(sym) sym
#   include <linux/modversions.h>
# endif
#endif /* __GENKSYMS__ */

#include <asm/atomic.h>

/* Don't need to bring in all of uaccess.h just for this decl.  */
struct exception_table_entry;

/* Used by get_kernel_syms, which is obsolete.  */
struct kernel_sym
{
	unsigned long value;
	char name[60];		/* should have been 64-sizeof(long); oh well */
};

struct module_symbol
{
	unsigned long value;
	const char *name;
};

struct module_ref
{
	struct module *dep;	/* "parent" pointer */
	struct module *ref;	/* "child" pointer */
	struct module_ref *next_ref;
};

/* TBD */
struct module_persist;

struct module
{
	unsigned long size_of_struct;	/* == sizeof(module) */
	struct module *next;
	const char *name;
	unsigned long size;

	union
	{
		atomic_t usecount;
		long pad;
	} uc;				/* Needs to keep its size - so says rth */

	unsigned long flags;		/* AUTOCLEAN et al */

	unsigned nsyms;
	unsigned ndeps;

	struct module_symbol *syms;
	struct module_ref *deps;
	struct module_ref *refs;
	int (*init)(void);
	void (*cleanup)(void);
	const struct exception_table_entry *ex_table_start;
	const struct exception_table_entry *ex_table_end;
#ifdef __alpha__
	unsigned long gp;
#endif
	/* Members past this point are extensions to the basic
	   module support and are optional.  Use mod_member_present()
	   to examine them.  */
	const struct module_persist *persist_start;
	const struct module_persist *persist_end;
	int (*can_unload)(void);
	int runsize;			/* In modutils, not currently used */
	const char *kallsyms_start;	/* All symbols for kernel debugging */
	const char *kallsyms_end;
	const char *archdata_start;	/* arch specific data for module */
	const char *archdata_end;
	const char *kernel_data;	/* Reserved for kernel internal use */
};

struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};

/* Bits of module.flags.  */

#define MOD_UNINITIALIZED	0
#define MOD_RUNNING		1
#define MOD_DELETED		2
#define MOD_AUTOCLEAN		4
#define MOD_VISITED  		8
#define MOD_USED_ONCE		16
#define MOD_JUST_FREED		32
#define MOD_INITIALIZING	64

/* Values for query_module's which.  */

#define QM_MODULES	1
#define QM_DEPS		2
#define QM_REFS		3
#define QM_SYMBOLS	4
#define QM_INFO		5

/* Can the module be queried? */
#define MOD_CAN_QUERY(mod) (((mod)->flags & (MOD_RUNNING | MOD_INITIALIZING)) && !((mod)->flags & MOD_DELETED))

/* When struct module is extended, we must test whether the new member
   is present in the header received from insmod before we can use it.  
   This function returns true if the member is present.  */

#define mod_member_present(mod,member) 					\
	((unsigned long)(&((struct module *)0L)->member + 1)		\
	 <= (mod)->size_of_struct)

/*
 * Ditto for archdata.  Assumes mod->archdata_start and mod->archdata_end
 * are validated elsewhere.
 */
#define mod_archdata_member_present(mod, type, member)			\
	(((unsigned long)(&((type *)0L)->member) +			\
	  sizeof(((type *)0L)->member)) <=				\
	 ((mod)->archdata_end - (mod)->archdata_start))
	 

/* Check if an address p with number of entries n is within the body of module m */
#define mod_bound(p, n, m) ((unsigned long)(p) >= ((unsigned long)(m) + ((m)->size_of_struct)) && \
	         (unsigned long)((p)+(n)) <= (unsigned long)(m) + (m)->size)

/* Backwards compatibility definition.  */

#define GET_USE_COUNT(module)	(atomic_read(&(module)->uc.usecount))

/* Poke the use count of a module.  */

#define __MOD_INC_USE_COUNT(mod)					\
	(atomic_inc(&(mod)->uc.usecount), (mod)->flags |= MOD_VISITED|MOD_USED_ONCE)
#define __MOD_DEC_USE_COUNT(mod)					\
	(atomic_dec(&(mod)->uc.usecount), (mod)->flags |= MOD_VISITED)
#define __MOD_IN_USE(mod)						\
	(mod_member_present((mod), can_unload) && (mod)->can_unload	\
	 ? (mod)->can_unload() : atomic_read(&(mod)->uc.usecount))

/* Indirect stringification.  */

#define __MODULE_STRING_1(x)	#x
#define __MODULE_STRING(x)	__MODULE_STRING_1(x)

/* Generic inter module communication.
 *
 * NOTE: This interface is intended for small amounts of data that are
 *       passed between two objects and either or both of the objects
 *       might be compiled as modules.  Do not over use this interface.
 *
 *       If more than two objects need to communicate then you probably
 *       need a specific interface instead of abusing this generic
 *       interface.  If both objects are *always* built into the kernel
 *       then a global extern variable is good enough, you do not need
 *       this interface.
 *
 * Keith Owens <kaos@ocs.com.au> 28 Oct 2000.
 */

#ifdef __KERNEL__
#define HAVE_INTER_MODULE
extern void inter_module_register(const char *, struct module *, const void *);
extern void inter_module_unregister(const char *);
extern const void *inter_module_get(const char *);
extern const void *inter_module_get_request(const char *, const char *);
extern void inter_module_put(const char *);

struct inter_module_entry {
	struct list_head list;
	const char *im_name;
	struct module *owner;
	const void *userdata;
};

extern int try_inc_mod_count(struct module *mod);
#endif /* __KERNEL__ */

#if defined(MODULE) && !defined(__GENKSYMS__)

/* Embedded module documentation macros.  */

/* For documentation purposes only.  */

#define MODULE_AUTHOR(name)						   \
const char __module_author[] __attribute__((section(".modinfo"))) = 	   \
"author=" name

#define MODULE_DESCRIPTION(desc)					   \
const char __module_description[] __attribute__((section(".modinfo"))) =   \
"description=" desc

/* Could potentially be used by kmod...  */

#define MODULE_SUPPORTED_DEVICE(dev)					   \
const char __module_device[] __attribute__((section(".modinfo"))) = 	   \
"device=" dev

/* Used to verify parameters given to the module.  The TYPE arg should
   be a string in the following format:
   	[min[-max]]{b,h,i,l,s}
   The MIN and MAX specifiers delimit the length of the array.  If MAX
   is omitted, it defaults to MIN; if both are omitted, the default is 1.
   The final character is a type specifier:
	b	byte
	h	short
	i	int
	l	long
	s	string
*/

#define MODULE_PARM(var,type)			\
const char __module_parm_##var[]		\
__attribute__((section(".modinfo"))) =		\
"parm_" __MODULE_STRING(var) "=" type

#define MODULE_PARM_DESC(var,desc)		\
const char __module_parm_desc_##var[]		\
__attribute__((section(".modinfo"))) =		\
"parm_desc_" __MODULE_STRING(var) "=" desc

/*
 * MODULE_DEVICE_TABLE exports information about devices
 * currently supported by this module.  A device type, such as PCI,
 * is a C-like identifier passed as the first arg to this macro.
 * The second macro arg is the variable containing the device
 * information being made public.
 *
 * The following is a list of known device types (arg 1),
 * and the C types which are to be passed as arg 2.
 * pci - struct pci_device_id - List of PCI ids supported by this module
 * isapnp - struct isapnp_device_id - List of ISA PnP ids supported by this module
 * usb - struct usb_device_id - List of USB ids supported by this module
 */
#define MODULE_GENERIC_TABLE(gtype,name)	\
static const unsigned long __module_##gtype##_size \
  __attribute_used__ = sizeof(struct gtype##_id); \
static const struct gtype##_id * __module_##gtype##_table \
  __attribute_used__ = name

/*
 * The following license idents are currently accepted as indicating free
 * software modules
 *
 *	"GPL"				[GNU Public License v2 or later]
 *	"GPL v2"			[GNU Public License v2]
 *	"GPL and additional rights"	[GNU Public License v2 rights and more]
 *	"Dual BSD/GPL"			[GNU Public License v2 or BSD license choice]
 *	"Dual MPL/GPL"			[GNU Public License v2 or Mozilla license choice]
 *
 * The following other idents are available
 *
 *	"Proprietary"			[Non free products]
 *
 * There are dual licensed components, but when running with Linux it is the
 * GPL that is relevant so this is a non issue. Similarly LGPL linked with GPL
 * is a GPL combined work.
 *
 * This exists for several reasons
 * 1.	So modinfo can show license info for users wanting to vet their setup 
 *	is free
 * 2.	So the community can ignore bug reports including proprietary modules
 * 3.	So vendors can do likewise based on their own policies
 */
 
#define MODULE_LICENSE(license) 	\
static const char __module_license[] __attribute__((section(".modinfo"))) =   \
"license=" license

/* Define the module variable, and usage macros.  */
extern struct module __this_module;

#define THIS_MODULE		(&__this_module)
#define MOD_INC_USE_COUNT	__MOD_INC_USE_COUNT(THIS_MODULE)
#define MOD_DEC_USE_COUNT	__MOD_DEC_USE_COUNT(THIS_MODULE)
#define MOD_IN_USE		__MOD_IN_USE(THIS_MODULE)

#include <linux/version.h>
static const char __module_kernel_version[] __attribute__((section(".modinfo"))) =
"kernel_version=" UTS_RELEASE;
#ifdef MODVERSIONS
static const char __module_using_checksums[] __attribute__((section(".modinfo"))) =
"using_checksums=1";
#endif

#else /* MODULE */

#define MODULE_AUTHOR(name)
#define MODULE_LICENSE(license)
#define MODULE_DESCRIPTION(desc)
#define MODULE_SUPPORTED_DEVICE(name)
#define MODULE_PARM(var,type)
#define MODULE_PARM_DESC(var,desc)

/* Create a dummy reference to the table to suppress gcc unused warnings.  Put
 * the reference in the .data.exit section which is discarded when code is built
 * in, so the reference does not bloat the running kernel.  Note: cannot be
 * const, other exit data may be writable.
 */
#define MODULE_GENERIC_TABLE(gtype,name) \
static const struct gtype##_id * __module_##gtype##_table \
  __attribute_used__ __attribute__ ((__section__(".data.exit"))) = name

#ifndef __GENKSYMS__

#define THIS_MODULE		NULL
#define MOD_INC_USE_COUNT	do { } while (0)
#define MOD_DEC_USE_COUNT	do { } while (0)
#define MOD_IN_USE		1

extern struct module *module_list;

#endif /* !__GENKSYMS__ */

#endif /* MODULE */

#define MODULE_DEVICE_TABLE(type,name)		\
  MODULE_GENERIC_TABLE(type##_device,name)

/* Export a symbol either from the kernel or a module.

   In the kernel, the symbol is added to the kernel's global symbol table.

   In a module, it controls which variables are exported.  If no
   variables are explicitly exported, the action is controled by the
   insmod -[xX] flags.  Otherwise, only the variables listed are exported.
   This obviates the need for the old register_symtab() function.  */

#if defined(__GENKSYMS__)

/* We want the EXPORT_SYMBOL tag left intact for recognition.  */

#elif !defined(AUTOCONF_INCLUDED)

#define __EXPORT_SYMBOL(sym,str)   error config_must_be_included_before_module
#define EXPORT_SYMBOL(var)	   error config_must_be_included_before_module
#define EXPORT_SYMBOL_NOVERS(var)  error config_must_be_included_before_module
#define EXPORT_SYMBOL_GPL(var)  error config_must_be_included_before_module

#elif !defined(CONFIG_MODULES)

#define __EXPORT_SYMBOL(sym,str)
#define EXPORT_SYMBOL(var)
#define EXPORT_SYMBOL_NOVERS(var)
#define EXPORT_SYMBOL_GPL(var)

#elif !defined(EXPORT_SYMTAB)

#define __EXPORT_SYMBOL(sym,str)   error this_object_must_be_defined_as_export_objs_in_the_Makefile
#define EXPORT_SYMBOL(var)	   error this_object_must_be_defined_as_export_objs_in_the_Makefile
#define EXPORT_SYMBOL_NOVERS(var)  error this_object_must_be_defined_as_export_objs_in_the_Makefile
#define EXPORT_SYMBOL_GPL(var)  error this_object_must_be_defined_as_export_objs_in_the_Makefile

#else

#define __EXPORT_SYMBOL(sym, str)			\
const char __kstrtab_##sym[]				\
__attribute__((section(".kstrtab"))) = str;		\
const struct module_symbol __ksymtab_##sym 		\
__attribute__((section("__ksymtab"))) =			\
{ (unsigned long)&sym, __kstrtab_##sym }

#define __EXPORT_SYMBOL_GPL(sym, str)			\
const char __kstrtab_##sym[]				\
__attribute__((section(".kstrtab"))) = "GPLONLY_" str;	\
const struct module_symbol __ksymtab_##sym		\
__attribute__((section("__ksymtab"))) =			\
{ (unsigned long)&sym, __kstrtab_##sym }

#if defined(MODVERSIONS) || !defined(CONFIG_MODVERSIONS)
#define EXPORT_SYMBOL(var)  __EXPORT_SYMBOL(var, __MODULE_STRING(var))
#define EXPORT_SYMBOL_GPL(var)  __EXPORT_SYMBOL_GPL(var, __MODULE_STRING(var))
#else
#define EXPORT_SYMBOL(var)  __EXPORT_SYMBOL(var, __MODULE_STRING(__VERSIONED_SYMBOL(var)))
#define EXPORT_SYMBOL_GPL(var)  __EXPORT_SYMBOL(var, __MODULE_STRING(__VERSIONED_SYMBOL(var)))
#endif

#define EXPORT_SYMBOL_NOVERS(var)  __EXPORT_SYMBOL(var, __MODULE_STRING(var))

#endif /* __GENKSYMS__ */

#ifdef MODULE
/* Force a module to export no symbols.  */
#define EXPORT_NO_SYMBOLS  __asm__(".section __ksymtab\n.previous")
#else
#define EXPORT_NO_SYMBOLS
#endif /* MODULE */

#ifdef CONFIG_MODULES
#define SET_MODULE_OWNER(some_struct) do { (some_struct)->owner = THIS_MODULE; } while (0)
#else
#define SET_MODULE_OWNER(some_struct) do { } while (0)
#endif

#endif /* _LINUX_MODULE_H */
