#ifndef _LINUX_INIT_H
#define _LINUX_INIT_H

#include <linux/config.h>
#include <linux/compiler.h>

/* These macros are used to mark some functions or 
 * initialized data (doesn't apply to uninitialized data)
 * as `initialization' functions. The kernel can take this
 * as hint that the function is used only during the initialization
 * phase and free up used memory resources after
 *
 * Usage:
 * For functions:
 * 
 * You should add __init immediately before the function name, like:
 *
 * static void __init initme(int x, int y)
 * {
 *    extern int z; z = x * y;
 * }
 *
 * If the function has a prototype somewhere, you can also add
 * __init between closing brace of the prototype and semicolon:
 *
 * extern int initialize_foobar_device(int, int, int) __init;
 *
 * For initialized data:
 * You should insert __initdata between the variable name and equal
 * sign followed by value, e.g.:
 *
 * static int init_variable __initdata = 0;
 * static char linux_logo[] __initdata = { 0x32, 0x36, ... };
 *
 * Don't forget to initialize data not at file scope, i.e. within a function,
 * as gcc otherwise puts the data into the bss section and not into the init
 * section.
 * 
 * Also note, that this data cannot be "const".
 */

#ifndef MODULE

#ifndef __ASSEMBLY__

/*
 * Used for initialization calls..
 */
typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

extern initcall_t __initcall_start, __initcall_end;

#define __initcall(fn)								\
	static initcall_t __initcall_##fn __attribute_used__ __init_call = fn
#define __exitcall(fn)								\
	static exitcall_t __exitcall_##fn __exit_call = fn

/*
 * Used for kernel command line parameter setup
 */
struct kernel_param {
	const char *str;
	int (*setup_func)(char *);
};

extern struct kernel_param __setup_start, __setup_end;

#define __setup(str, fn)								\
	static char __setup_str_##fn[] __initdata = str;				\
	static struct kernel_param __setup_##fn __attribute_used__ __initsetup = { __setup_str_##fn, fn }

#endif /* __ASSEMBLY__ */

/*
 * Mark functions and data as being only used at initialization
 * or exit time.
 */
#define __init		__attribute__ ((__section__ (".text.init")))
#define __exit		__attribute_used__ __attribute__ (( __section__(".text.exit")))
#define __initdata	__attribute__ ((__section__ (".data.init")))
#define __exitdata	__attribute_used__ __attribute__ ((__section__ (".data.exit")))
#define __initsetup	__attribute_used__ __attribute__ ((__section__ (".setup.init")))
#define __init_call	__attribute_used__ __attribute__ ((__section__ (".initcall.init")))
#define __exit_call	__attribute_used__ __attribute__ ((__section__ (".exitcall.exit")))

/* For assembly routines */
#define __INIT		.section	".text.init","ax"
#define __FINIT		.previous
#define __INITDATA	.section	".data.init","aw"

/**
 * module_init() - driver initialization entry point
 * @x: function to be run at kernel boot time or module insertion
 * 
 * module_init() will add the driver initialization routine in
 * the "__initcall.int" code segment if the driver is checked as
 * "y" or static, or else it will wrap the driver initialization
 * routine with init_module() which is used by insmod and
 * modprobe when the driver is used as a module.
 */
#define module_init(x)	__initcall(x);

/**
 * module_exit() - driver exit entry point
 * @x: function to be run when driver is removed
 * 
 * module_exit() will wrap the driver clean-up code
 * with cleanup_module() when used with rmmod when
 * the driver is a module.  If the driver is statically
 * compiled into the kernel, module_exit() has no effect.
 */
#define module_exit(x)	__exitcall(x);

#else	/* MODULE */

#define __init
#define __exit
#define __initdata
#define __exitdata
#define __initcall(fn)
/* For assembly routines */
#define __INIT
#define __FINIT
#define __INITDATA

/* These macros create a dummy inline: gcc 2.9x does not count alias
 as usage, hence the `unused function' warning when __init functions
 are declared static. We use the dummy __*_module_inline functions
 both to kill the warning and check the type of the init/cleanup
 function. */
typedef int (*__init_module_func_t)(void);
typedef void (*__cleanup_module_func_t)(void);
#define module_init(x) \
	int init_module(void) __attribute__((alias(#x))); \
	static inline __init_module_func_t __init_module_inline(void) \
	{ return x; }
#define module_exit(x) \
	void cleanup_module(void) __attribute__((alias(#x))); \
	static inline __cleanup_module_func_t __cleanup_module_inline(void) \
	{ return x; }

#define __setup(str,func) /* nothing */

#endif	/* !MODULE */

#ifdef CONFIG_HOTPLUG
#define __devinit
#define __devinitdata
#define __devexit
#define __devexitdata
#else
#define __devinit __init
#define __devinitdata __initdata
#define __devexit __exit
#define __devexitdata __exitdata
#endif

/* Functions marked as __devexit may be discarded at kernel link time, depending
   on config options.  Newer versions of binutils detect references from
   retained sections to discarded sections and flag an error.  Pointers to
   __devexit functions must use __devexit_p(function_name), the wrapper will
   insert either the function_name or NULL, depending on the config options.
 */
#if defined(MODULE) || defined(CONFIG_HOTPLUG)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif

#endif /* _LINUX_INIT_H */
