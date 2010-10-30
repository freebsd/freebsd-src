// target-reloc.h -- target specific relocation support  -*- C++ -*-

#ifndef GOLD_TARGET_RELOC_H
#define GOLD_TARGET_RELOC_H

#include "elfcpp.h"
#include "object.h"
#include "symtab.h"
#include "reloc-types.h"

namespace gold
{

// This function implements the generic part of reloc scanning.  This
// is an inline function which takes a class whose operator()
// implements the machine specific part of scanning.  We do it this
// way to avoidmaking a function call for each relocation, and to
// avoid repeating the generic code for each target.

template<int size, bool big_endian, typename Target_type, int sh_type,
	 typename Scan>
inline void
scan_relocs(
    const General_options& options,
    Symbol_table* symtab,
    Layout* layout,
    Target_type* target,
    Sized_relobj<size, big_endian>* object,
    unsigned int data_shndx,
    const unsigned char* prelocs,
    size_t reloc_count,
    size_t local_count,
    const unsigned char* plocal_syms,
    Symbol** global_syms)
{
  typedef typename Reloc_types<sh_type, size, big_endian>::Reloc Reltype;
  const int reloc_size = Reloc_types<sh_type, size, big_endian>::reloc_size;
  const int sym_size = elfcpp::Elf_sizes<size>::sym_size;
  Scan scan;

  for (size_t i = 0; i < reloc_count; ++i, prelocs += reloc_size)
    {
      Reltype reloc(prelocs);

      typename elfcpp::Elf_types<size>::Elf_WXword r_info = reloc.get_r_info();
      unsigned int r_sym = elfcpp::elf_r_sym<size>(r_info);
      unsigned int r_type = elfcpp::elf_r_type<size>(r_info);

      if (r_sym < local_count)
	{
	  gold_assert(plocal_syms != NULL);
	  typename elfcpp::Sym<size, big_endian> lsym(plocal_syms
						      + r_sym * sym_size);
	  const unsigned int shndx = lsym.get_st_shndx();
	  if (shndx < elfcpp::SHN_LORESERVE
	      && shndx != elfcpp::SHN_UNDEF
	      && !object->is_section_included(lsym.get_st_shndx()))
	    {
	      // RELOC is a relocation against a local symbol in a
	      // section we are discarding.  We can ignore this
	      // relocation.  It will eventually become a reloc
	      // against the value zero.
	      //
	      // FIXME: We should issue a warning if this is an
	      // allocated section; is this the best place to do it?
	      // 
	      // FIXME: The old GNU linker would in some cases look
	      // for the linkonce section which caused this section to
	      // be discarded, and, if the other section was the same
	      // size, change the reloc to refer to the other section.
	      // That seems risky and weird to me, and I don't know of
	      // any case where it is actually required.

	      continue;
	    }

	  scan.local(options, symtab, layout, target, object, data_shndx,
		     reloc, r_type, lsym);
	}
      else
	{
	  Symbol* gsym = global_syms[r_sym - local_count];
	  gold_assert(gsym != NULL);
	  if (gsym->is_forwarder())
	    gsym = symtab->resolve_forwards(gsym);

	  scan.global(options, symtab, layout, target, object, data_shndx,
		      reloc, r_type, gsym);
	}
    }
}

// This function implements the generic part of relocation processing.
// This is an inline function which take a class whose operator()
// implements the machine specific part of relocation.  We do it this
// way to avoid making a function call for each relocation, and to
// avoid repeating the generic relocation handling code for each
// target.

// SIZE is the ELF size: 32 or 64.  BIG_ENDIAN is the endianness of
// the data.  SH_TYPE is the section type: SHT_REL or SHT_RELA.
// RELOCATE implements operator() to do a relocation.

// PRELOCS points to the relocation data.  RELOC_COUNT is the number
// of relocs.  VIEW is the section data, VIEW_ADDRESS is its memory
// address, and VIEW_SIZE is the size.

template<int size, bool big_endian, typename Target_type, int sh_type,
	 typename Relocate>
inline void
relocate_section(
    const Relocate_info<size, big_endian>* relinfo,
    Target_type* target,
    const unsigned char* prelocs,
    size_t reloc_count,
    unsigned char* view,
    typename elfcpp::Elf_types<size>::Elf_Addr view_address,
    off_t view_size)
{
  typedef typename Reloc_types<sh_type, size, big_endian>::Reloc Reltype;
  const int reloc_size = Reloc_types<sh_type, size, big_endian>::reloc_size;
  Relocate relocate;

  unsigned int local_count = relinfo->local_symbol_count;
  const typename Sized_relobj<size, big_endian>::Local_values* local_values =
    relinfo->local_values;
  const Symbol* const * global_syms = relinfo->symbols;

  for (size_t i = 0; i < reloc_count; ++i, prelocs += reloc_size)
    {
      Reltype reloc(prelocs);

      off_t offset = reloc.get_r_offset();

      typename elfcpp::Elf_types<size>::Elf_WXword r_info = reloc.get_r_info();
      unsigned int r_sym = elfcpp::elf_r_sym<size>(r_info);
      unsigned int r_type = elfcpp::elf_r_type<size>(r_info);

      const Sized_symbol<size>* sym;

      Symbol_value<size> symval;
      const Symbol_value<size> *psymval;
      if (r_sym < local_count)
	{
	  sym = NULL;
	  psymval = &(*local_values)[r_sym];
	}
      else
	{
	  const Symbol* gsym = global_syms[r_sym - local_count];
	  gold_assert(gsym != NULL);
	  if (gsym->is_forwarder())
	    gsym = relinfo->symtab->resolve_forwards(gsym);

	  sym = static_cast<const Sized_symbol<size>*>(gsym);
	  if (sym->has_symtab_index())
	    symval.set_output_symtab_index(sym->symtab_index());
	  else
	    symval.set_no_output_symtab_entry();
	  symval.set_output_value(sym->value());
	  psymval = &symval;
	}

      if (!relocate.relocate(relinfo, target, i, reloc, r_type, sym, psymval,
			     view + offset, view_address + offset, view_size))
	continue;

      if (offset < 0 || offset >= view_size)
	{
	  fprintf(stderr, _("%s: %s: reloc has bad offset %zu\n"),
		  program_name, relinfo->location(i, offset).c_str(),
		  static_cast<size_t>(offset));
	  gold_exit(false);
	}

      if (sym != NULL
	  && sym->is_undefined()
	  && sym->binding() != elfcpp::STB_WEAK)
	{
	  fprintf(stderr, _("%s: %s: undefined reference to '%s'\n"),
		  program_name, relinfo->location(i, offset).c_str(),
		  sym->name());
	  // gold_exit(false);
	}

      if (sym != NULL && sym->has_warning())
	relinfo->symtab->issue_warning(sym, relinfo->location(i, offset));
    }
}

} // End namespace gold.

#endif // !defined(GOLD_TARGET_RELOC_H)
