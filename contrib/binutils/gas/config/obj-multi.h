/* hi */

#include "emul.h"
#include "targ-cpu.h"

#define OUTPUT_FLAVOR		(this_format->flavor)
#define obj_frob_symbol(S,P)	(this_format->frob_symbol)(S,&(P))
#define obj_frob_file		(this_format->frob_file)
#define obj_frob_file_after_relocs (this_format->frob_file_after_relocs)
#define obj_ecoff_set_ext	(this_format->ecoff_set_ext)
#define obj_pop_insert		(this_format->pop_insert)
#define obj_read_begin_hook()	(this_format->read_begin_hook?this_format->read_begin_hook():(void)0)
#define obj_symbol_new_hook	(this_format->symbol_new_hook)
#define obj_sec_sym_ok_for_reloc (this_format->sec_sym_ok_for_reloc)
#define S_GET_SIZE		(this_format->s_get_size)
#define S_SET_SIZE		(this_format->s_set_size)
#define S_GET_ALIGN		(this_format->s_get_align)
#define S_SET_ALIGN		(this_format->s_set_align)
#define OBJ_COPY_SYMBOL_ATTRIBUTES (this_format->copy_symbol_attributes)
#define OBJ_PROCESS_STAB	(this_format->process_stab)

#if defined (OBJ_MAYBE_ECOFF) || (defined (OBJ_MAYBE_ELF) && defined (TC_MIPS))
#define ECOFF_DEBUGGING 1
#endif

/* FIXME: What's the story here?  Why do we have to define
   OBJ_SYMFIELD_TYPE both here and in obj-elf.h?  */
#ifdef OBJ_MAYBE_ELF
struct elf_obj_sy
{
  expressionS *size;
  char *versioned_name;
};
#define OBJ_SYMFIELD_TYPE struct elf_obj_sy
#define ELF_TARGET_SYMBOL_FIELDS int local:1;
#else
#define ELF_TARGET_SYMBOL_FIELDS
#endif

#ifdef ECOFF_DEBUGGING
struct efdr;
struct localsym;
#define ECOFF_DEBUG_TARGET_SYMBOL_FIELDS struct efdr *ecoff_file; struct localsym *ecoff_symbol; valueT ecoff_extern_size;
#else
#define ECOFF_DEBUG_TARGET_SYMBOL_FIELDS
#endif

#define TARGET_SYMBOL_FIELDS \
	ELF_TARGET_SYMBOL_FIELDS \
	ECOFF_DEBUG_TARGET_SYMBOL_FIELDS
