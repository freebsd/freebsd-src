#ifndef __XFS_SUPPORT_KGDB_H__
#define __XFS_SUPPORT_KGDB_H__

#define	KDB_ARGCOUNT	EINVAL

struct pt_regs
{
	int	dummy;
};

#define	MODULE_AUTHOR(s)	static char __module_author[] = s;
#define	MODULE_DESCRIPTION(s)	static char __module_description[] = s;
#define	MODULE_LICENSE(s)	static char __module_license[] = s


typedef int (*kdb_func_t)(int, const char **, const char **, struct pt_regs *);
typedef register_t kdb_machreg_t;

/*
 * Symbol table format.
 */
typedef struct __ksymtab {
	unsigned long value;	/* Address of symbol */
	const char *sym_name;	/* Full symbol name, including any version */   
	unsigned long sym_start;
	unsigned long sym_end;
} kdb_symtab_t;

extern int	kdb_register(char *, kdb_func_t, char *, char *, short);
extern int	kdb_unregister(char *);

extern int	kdbgetaddrarg(int, const char**, int*, kdb_machreg_t *,
			long *, char **, struct pt_regs *);
extern int	kdbnearsym(unsigned long, kdb_symtab_t *);
extern void	kdb_printf(const char *,...)
			__attribute__ ((format (printf, 1, 2)));

extern int	kdb_getarea_size(void *, unsigned long, size_t);
extern int	kdb_putarea_size(unsigned long, void *, size_t);

#define kdb_getarea(x,addr)     kdb_getarea_size(&(x), addr, sizeof((x)))
#define kdb_putarea(addr,x)     kdb_putarea_size(addr, &(x), sizeof((x)))

#endif /* __XFS_SUPPORT_KGDB_H__ */
