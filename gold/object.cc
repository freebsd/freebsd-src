// object.cc -- support for an object file for linking in gold

#include "gold.h"

#include <cerrno>
#include <cstring>
#include <cstdarg>

#include "target-select.h"
#include "layout.h"
#include "output.h"
#include "symtab.h"
#include "object.h"
#include "dynobj.h"

namespace gold
{

// Class Object.

// Set the target based on fields in the ELF file header.

void
Object::set_target(int machine, int size, bool big_endian, int osabi,
		   int abiversion)
{
  Target* target = select_target(machine, size, big_endian, osabi, abiversion);
  if (target == NULL)
    {
      fprintf(stderr, _("%s: %s: unsupported ELF machine number %d\n"),
	      program_name, this->name().c_str(), machine);
      gold_exit(false);
    }
  this->target_ = target;
}

// Report an error for the elfcpp::Elf_file interface.

void
Object::error(const char* format, ...)
{
  va_list args;

  fprintf(stderr, "%s: %s: ", program_name, this->name().c_str());
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  putc('\n', stderr);

  gold_exit(false);
}

// Return a view of the contents of a section.

const unsigned char*
Object::section_contents(unsigned int shndx, off_t* plen)
{
  Location loc(this->do_section_contents(shndx));
  *plen = loc.data_size;
  return this->get_view(loc.file_offset, loc.data_size);
}

// Read the section data into SD.  This is code common to Sized_relobj
// and Sized_dynobj, so we put it into Object.

template<int size, bool big_endian>
void
Object::read_section_data(elfcpp::Elf_file<size, big_endian, Object>* elf_file,
			  Read_symbols_data* sd)
{
  const int shdr_size = elfcpp::Elf_sizes<size>::shdr_size;

  // Read the section headers.
  const off_t shoff = elf_file->shoff();
  const unsigned int shnum = this->shnum();
  sd->section_headers = this->get_lasting_view(shoff, shnum * shdr_size);

  // Read the section names.
  const unsigned char* pshdrs = sd->section_headers->data();
  const unsigned char* pshdrnames = pshdrs + elf_file->shstrndx() * shdr_size;
  typename elfcpp::Shdr<size, big_endian> shdrnames(pshdrnames);

  if (shdrnames.get_sh_type() != elfcpp::SHT_STRTAB)
    {
      fprintf(stderr,
	      _("%s: %s: section name section has wrong type: %u\n"),
	      program_name, this->name().c_str(),
	      static_cast<unsigned int>(shdrnames.get_sh_type()));
      gold_exit(false);
    }

  sd->section_names_size = shdrnames.get_sh_size();
  sd->section_names = this->get_lasting_view(shdrnames.get_sh_offset(),
					     sd->section_names_size);
}

// If NAME is the name of a special .gnu.warning section, arrange for
// the warning to be issued.  SHNDX is the section index.  Return
// whether it is a warning section.

bool
Object::handle_gnu_warning_section(const char* name, unsigned int shndx,
				   Symbol_table* symtab)
{
  const char warn_prefix[] = ".gnu.warning.";
  const int warn_prefix_len = sizeof warn_prefix - 1;
  if (strncmp(name, warn_prefix, warn_prefix_len) == 0)
    {
      symtab->add_warning(name + warn_prefix_len, this, shndx);
      return true;
    }
  return false;
}

// Class Sized_relobj.

template<int size, bool big_endian>
Sized_relobj<size, big_endian>::Sized_relobj(
    const std::string& name,
    Input_file* input_file,
    off_t offset,
    const elfcpp::Ehdr<size, big_endian>& ehdr)
  : Relobj(name, input_file, offset),
    elf_file_(this, ehdr),
    symtab_shndx_(-1U),
    local_symbol_count_(0),
    output_local_symbol_count_(0),
    symbols_(NULL),
    local_symbol_offset_(0),
    local_values_()
{
}

template<int size, bool big_endian>
Sized_relobj<size, big_endian>::~Sized_relobj()
{
}

// Set up an object file based on the file header.  This sets up the
// target and reads the section information.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::setup(
    const elfcpp::Ehdr<size, big_endian>& ehdr)
{
  this->set_target(ehdr.get_e_machine(), size, big_endian,
		   ehdr.get_e_ident()[elfcpp::EI_OSABI],
		   ehdr.get_e_ident()[elfcpp::EI_ABIVERSION]);

  const unsigned int shnum = this->elf_file_.shnum();
  this->set_shnum(shnum);
}

// Find the SHT_SYMTAB section, given the section headers.  The ELF
// standard says that maybe in the future there can be more than one
// SHT_SYMTAB section.  Until somebody figures out how that could
// work, we assume there is only one.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::find_symtab(const unsigned char* pshdrs)
{
  const unsigned int shnum = this->shnum();
  this->symtab_shndx_ = 0;
  if (shnum > 0)
    {
      // Look through the sections in reverse order, since gas tends
      // to put the symbol table at the end.
      const unsigned char* p = pshdrs + shnum * This::shdr_size;
      unsigned int i = shnum;
      while (i > 0)
	{
	  --i;
	  p -= This::shdr_size;
	  typename This::Shdr shdr(p);
	  if (shdr.get_sh_type() == elfcpp::SHT_SYMTAB)
	    {
	      this->symtab_shndx_ = i;
	      break;
	    }
	}
    }
}

// Read the sections and symbols from an object file.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_read_symbols(Read_symbols_data* sd)
{
  this->read_section_data(&this->elf_file_, sd);

  const unsigned char* const pshdrs = sd->section_headers->data();

  this->find_symtab(pshdrs);

  if (this->symtab_shndx_ == 0)
    {
      // No symbol table.  Weird but legal.
      sd->symbols = NULL;
      sd->symbols_size = 0;
      sd->symbol_names = NULL;
      sd->symbol_names_size = 0;
      return;
    }

  // Get the symbol table section header.
  typename This::Shdr symtabshdr(pshdrs
				 + this->symtab_shndx_ * This::shdr_size);
  gold_assert(symtabshdr.get_sh_type() == elfcpp::SHT_SYMTAB);

  // We only need the external symbols.
  const int sym_size = This::sym_size;
  const unsigned int loccount = symtabshdr.get_sh_info();
  this->local_symbol_count_ = loccount;
  off_t locsize = loccount * sym_size;
  off_t extoff = symtabshdr.get_sh_offset() + locsize;
  off_t extsize = symtabshdr.get_sh_size() - locsize;

  // Read the symbol table.
  File_view* fvsymtab = this->get_lasting_view(extoff, extsize);

  // Read the section header for the symbol names.
  unsigned int strtab_shndx = symtabshdr.get_sh_link();
  if (strtab_shndx >= this->shnum())
    {
      fprintf(stderr, _("%s: %s: invalid symbol table name index: %u\n"),
	      program_name, this->name().c_str(), strtab_shndx);
      gold_exit(false);
    }
  typename This::Shdr strtabshdr(pshdrs + strtab_shndx * This::shdr_size);
  if (strtabshdr.get_sh_type() != elfcpp::SHT_STRTAB)
    {
      fprintf(stderr,
	      _("%s: %s: symbol table name section has wrong type: %u\n"),
	      program_name, this->name().c_str(),
	      static_cast<unsigned int>(strtabshdr.get_sh_type()));
      gold_exit(false);
    }

  // Read the symbol names.
  File_view* fvstrtab = this->get_lasting_view(strtabshdr.get_sh_offset(),
					       strtabshdr.get_sh_size());

  sd->symbols = fvsymtab;
  sd->symbols_size = extsize;
  sd->symbol_names = fvstrtab;
  sd->symbol_names_size = strtabshdr.get_sh_size();
}

// Return whether to include a section group in the link.  LAYOUT is
// used to keep track of which section groups we have already seen.
// INDEX is the index of the section group and SHDR is the section
// header.  If we do not want to include this group, we set bits in
// OMIT for each section which should be discarded.

template<int size, bool big_endian>
bool
Sized_relobj<size, big_endian>::include_section_group(
    Layout* layout,
    unsigned int index,
    const elfcpp::Shdr<size, big_endian>& shdr,
    std::vector<bool>* omit)
{
  // Read the section contents.
  const unsigned char* pcon = this->get_view(shdr.get_sh_offset(),
					     shdr.get_sh_size());
  const elfcpp::Elf_Word* pword =
    reinterpret_cast<const elfcpp::Elf_Word*>(pcon);

  // The first word contains flags.  We only care about COMDAT section
  // groups.  Other section groups are always included in the link
  // just like ordinary sections.
  elfcpp::Elf_Word flags = elfcpp::Swap<32, big_endian>::readval(pword);
  if ((flags & elfcpp::GRP_COMDAT) == 0)
    return true;

  // Look up the group signature, which is the name of a symbol.  This
  // is a lot of effort to go to to read a string.  Why didn't they
  // just use the name of the SHT_GROUP section as the group
  // signature?

  // Get the appropriate symbol table header (this will normally be
  // the single SHT_SYMTAB section, but in principle it need not be).
  const unsigned int link = shdr.get_sh_link();
  typename This::Shdr symshdr(this, this->elf_file_.section_header(link));

  // Read the symbol table entry.
  if (shdr.get_sh_info() >= symshdr.get_sh_size() / This::sym_size)
    {
      fprintf(stderr, _("%s: %s: section group %u info %u out of range\n"),
	      program_name, this->name().c_str(), index, shdr.get_sh_info());
      gold_exit(false);
    }
  off_t symoff = symshdr.get_sh_offset() + shdr.get_sh_info() * This::sym_size;
  const unsigned char* psym = this->get_view(symoff, This::sym_size);
  elfcpp::Sym<size, big_endian> sym(psym);

  // Read the symbol table names.
  off_t symnamelen;
  const unsigned char* psymnamesu;
  psymnamesu = this->section_contents(symshdr.get_sh_link(), &symnamelen);
  const char* psymnames = reinterpret_cast<const char*>(psymnamesu);

  // Get the section group signature.
  if (sym.get_st_name() >= symnamelen)
    {
      fprintf(stderr, _("%s: %s: symbol %u name offset %u out of range\n"),
	      program_name, this->name().c_str(), shdr.get_sh_info(),
	      sym.get_st_name());
      gold_exit(false);
    }

  const char* signature = psymnames + sym.get_st_name();

  // It seems that some versions of gas will create a section group
  // associated with a section symbol, and then fail to give a name to
  // the section symbol.  In such a case, use the name of the section.
  // FIXME.
  std::string secname;
  if (signature[0] == '\0' && sym.get_st_type() == elfcpp::STT_SECTION)
    {
      secname = this->section_name(sym.get_st_shndx());
      signature = secname.c_str();
    }

  // Record this section group, and see whether we've already seen one
  // with the same signature.
  if (layout->add_comdat(signature, true))
    return true;

  // This is a duplicate.  We want to discard the sections in this
  // group.
  size_t count = shdr.get_sh_size() / sizeof(elfcpp::Elf_Word);
  for (size_t i = 1; i < count; ++i)
    {
      elfcpp::Elf_Word secnum =
	elfcpp::Swap<32, big_endian>::readval(pword + i);
      if (secnum >= this->shnum())
	{
	  fprintf(stderr,
		  _("%s: %s: section %u in section group %u out of range"),
		  program_name, this->name().c_str(), secnum,
		  index);
	  gold_exit(false);
	}
      (*omit)[secnum] = true;
    }

  return false;
}

// Whether to include a linkonce section in the link.  NAME is the
// name of the section and SHDR is the section header.

// Linkonce sections are a GNU extension implemented in the original
// GNU linker before section groups were defined.  The semantics are
// that we only include one linkonce section with a given name.  The
// name of a linkonce section is normally .gnu.linkonce.T.SYMNAME,
// where T is the type of section and SYMNAME is the name of a symbol.
// In an attempt to make linkonce sections interact well with section
// groups, we try to identify SYMNAME and use it like a section group
// signature.  We want to block section groups with that signature,
// but not other linkonce sections with that signature.  We also use
// the full name of the linkonce section as a normal section group
// signature.

template<int size, bool big_endian>
bool
Sized_relobj<size, big_endian>::include_linkonce_section(
    Layout* layout,
    const char* name,
    const elfcpp::Shdr<size, big_endian>&)
{
  const char* symname = strrchr(name, '.') + 1;
  bool include1 = layout->add_comdat(symname, false);
  bool include2 = layout->add_comdat(name, true);
  return include1 && include2;
}

// Lay out the input sections.  We walk through the sections and check
// whether they should be included in the link.  If they should, we
// pass them to the Layout object, which will return an output section
// and an offset.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_layout(const General_options& options,
					  Symbol_table* symtab,
					  Layout* layout,
					  Read_symbols_data* sd)
{
  const unsigned int shnum = this->shnum();
  if (shnum == 0)
    return;

  // Get the section headers.
  const unsigned char* pshdrs = sd->section_headers->data();

  // Get the section names.
  const unsigned char* pnamesu = sd->section_names->data();
  const char* pnames = reinterpret_cast<const char*>(pnamesu);

  std::vector<Map_to_output>& map_sections(this->map_to_output());
  map_sections.resize(shnum);

  // Keep track of which sections to omit.
  std::vector<bool> omit(shnum, false);

  // Skip the first, dummy, section.
  pshdrs += This::shdr_size;
  for (unsigned int i = 1; i < shnum; ++i, pshdrs += This::shdr_size)
    {
      typename This::Shdr shdr(pshdrs);

      if (shdr.get_sh_name() >= sd->section_names_size)
	{
	  fprintf(stderr,
		  _("%s: %s: bad section name offset for section %u: %lu\n"),
		  program_name, this->name().c_str(), i,
		  static_cast<unsigned long>(shdr.get_sh_name()));
	  gold_exit(false);
	}

      const char* name = pnames + shdr.get_sh_name();

      if (this->handle_gnu_warning_section(name, i, symtab))
	{
	  if (!options.is_relocatable())
	    omit[i] = true;
	}

      bool discard = omit[i];
      if (!discard)
	{
	  if (shdr.get_sh_type() == elfcpp::SHT_GROUP)
	    {
	      if (!this->include_section_group(layout, i, shdr, &omit))
		discard = true;
	    }
	  else if (Layout::is_linkonce(name))
	    {
	      if (!this->include_linkonce_section(layout, name, shdr))
		discard = true;
	    }
	}

      if (discard)
	{
	  // Do not include this section in the link.
	  map_sections[i].output_section = NULL;
	  continue;
	}

      off_t offset;
      Output_section* os = layout->layout(this, i, name, shdr, &offset);

      map_sections[i].output_section = os;
      map_sections[i].offset = offset;
    }

  delete sd->section_headers;
  sd->section_headers = NULL;
  delete sd->section_names;
  sd->section_names = NULL;
}

// Add the symbols to the symbol table.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_add_symbols(Symbol_table* symtab,
					       Read_symbols_data* sd)
{
  if (sd->symbols == NULL)
    {
      gold_assert(sd->symbol_names == NULL);
      return;
    }

  const int sym_size = This::sym_size;
  size_t symcount = sd->symbols_size / sym_size;
  if (symcount * sym_size != sd->symbols_size)
    {
      fprintf(stderr,
	      _("%s: %s: size of symbols is not multiple of symbol size\n"),
	      program_name, this->name().c_str());
      gold_exit(false);
    }

  this->symbols_ = new Symbol*[symcount];

  const char* sym_names =
    reinterpret_cast<const char*>(sd->symbol_names->data());
  symtab->add_from_relobj(this, sd->symbols->data(), symcount, sym_names, 
			  sd->symbol_names_size, this->symbols_);

  delete sd->symbols;
  sd->symbols = NULL;
  delete sd->symbol_names;
  sd->symbol_names = NULL;
}

// Finalize the local symbols.  Here we record the file offset at
// which they should be output, we add their names to *POOL, and we
// add their values to THIS->LOCAL_VALUES_.  Return the symbol index.
// This function is always called from the main thread.  The actual
// output of the local symbols will occur in a separate task.

template<int size, bool big_endian>
unsigned int
Sized_relobj<size, big_endian>::do_finalize_local_symbols(unsigned int index,
							  off_t off,
							  Stringpool* pool)
{
  gold_assert(this->symtab_shndx_ != -1U);
  if (this->symtab_shndx_ == 0)
    {
      // This object has no symbols.  Weird but legal.
      return index;
    }

  gold_assert(off == static_cast<off_t>(align_address(off, size >> 3)));

  this->local_symbol_offset_ = off;

  // Read the symbol table section header.
  const unsigned int symtab_shndx = this->symtab_shndx_;
  typename This::Shdr symtabshdr(this,
				 this->elf_file_.section_header(symtab_shndx));
  gold_assert(symtabshdr.get_sh_type() == elfcpp::SHT_SYMTAB);

  // Read the local symbols.
  const int sym_size = This::sym_size;
  const unsigned int loccount = this->local_symbol_count_;
  gold_assert(loccount == symtabshdr.get_sh_info());
  off_t locsize = loccount * sym_size;
  const unsigned char* psyms = this->get_view(symtabshdr.get_sh_offset(),
					      locsize);

  this->local_values_.resize(loccount);

  // Read the symbol names.
  const unsigned int strtab_shndx = symtabshdr.get_sh_link();
  off_t strtab_size;
  const unsigned char* pnamesu = this->section_contents(strtab_shndx,
							&strtab_size);
  const char* pnames = reinterpret_cast<const char*>(pnamesu);

  // Loop over the local symbols.

  const std::vector<Map_to_output>& mo(this->map_to_output());
  unsigned int shnum = this->shnum();
  unsigned int count = 0;
  // Skip the first, dummy, symbol.
  psyms += sym_size;
  for (unsigned int i = 1; i < loccount; ++i, psyms += sym_size)
    {
      elfcpp::Sym<size, big_endian> sym(psyms);

      Symbol_value<size>& lv(this->local_values_[i]);

      unsigned int shndx = sym.get_st_shndx();
      lv.set_input_shndx(shndx);

      if (shndx >= elfcpp::SHN_LORESERVE)
	{
	  if (shndx == elfcpp::SHN_ABS)
	    lv.set_output_value(sym.get_st_value());
	  else
	    {
	      // FIXME: Handle SHN_XINDEX.
	      fprintf(stderr,
		      _("%s: %s: unknown section index %u "
			"for local symbol %u\n"),
		      program_name, this->name().c_str(), shndx, i);
	      gold_exit(false);
	    }
	}
      else
	{
	  if (shndx >= shnum)
	    {
	      fprintf(stderr,
		      _("%s: %s: local symbol %u section index %u "
			"out of range\n"),
		      program_name, this->name().c_str(), i, shndx);
	      gold_exit(false);
	    }

	  Output_section* os = mo[shndx].output_section;

	  if (os == NULL)
	    {
	      lv.set_output_value(0);
	      lv.set_no_output_symtab_entry();
	      continue;
	    }

	  if (mo[shndx].offset == -1)
	    lv.set_input_value(sym.get_st_value());
	  else
	    lv.set_output_value(mo[shndx].output_section->address()
				+ mo[shndx].offset
				+ sym.get_st_value());
	}

      // Decide whether this symbol should go into the output file.

      if (sym.get_st_type() == elfcpp::STT_SECTION)
	{
	  lv.set_no_output_symtab_entry();
	  continue;
	}

      if (sym.get_st_name() >= strtab_size)
	{
	  fprintf(stderr,
		  _("%s: %s: local symbol %u section name "
		    "out of range: %u >= %u\n"),
		  program_name, this->name().c_str(),
		  i, sym.get_st_name(),
		  static_cast<unsigned int>(strtab_size));
	  gold_exit(false);
	}

      const char* name = pnames + sym.get_st_name();
      pool->add(name, NULL);
      lv.set_output_symtab_index(index);
      ++index;
      ++count;
    }

  this->output_local_symbol_count_ = count;

  return index;
}

// Return the value of a local symbol defined in input section SHNDX,
// with value VALUE, adding addend ADDEND.  This handles SHF_MERGE
// sections.
template<int size, bool big_endian>
typename elfcpp::Elf_types<size>::Elf_Addr
Sized_relobj<size, big_endian>::local_value(unsigned int shndx,
					    Address value,
					    Address addend) const
{
  const std::vector<Map_to_output>& mo(this->map_to_output());
  Output_section* os = mo[shndx].output_section;
  if (os == NULL)
    return addend;
  gold_assert(mo[shndx].offset == -1);
  return os->output_address(this, shndx, value + addend);
}

// Write out the local symbols.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::write_local_symbols(Output_file* of,
						    const Stringpool* sympool)
{
  gold_assert(this->symtab_shndx_ != -1U);
  if (this->symtab_shndx_ == 0)
    {
      // This object has no symbols.  Weird but legal.
      return;
    }

  // Read the symbol table section header.
  const unsigned int symtab_shndx = this->symtab_shndx_;
  typename This::Shdr symtabshdr(this,
				 this->elf_file_.section_header(symtab_shndx));
  gold_assert(symtabshdr.get_sh_type() == elfcpp::SHT_SYMTAB);
  const unsigned int loccount = this->local_symbol_count_;
  gold_assert(loccount == symtabshdr.get_sh_info());

  // Read the local symbols.
  const int sym_size = This::sym_size;
  off_t locsize = loccount * sym_size;
  const unsigned char* psyms = this->get_view(symtabshdr.get_sh_offset(),
					      locsize);

  // Read the symbol names.
  const unsigned int strtab_shndx = symtabshdr.get_sh_link();
  off_t strtab_size;
  const unsigned char* pnamesu = this->section_contents(strtab_shndx,
							&strtab_size);
  const char* pnames = reinterpret_cast<const char*>(pnamesu);

  // Get a view into the output file.
  off_t output_size = this->output_local_symbol_count_ * sym_size;
  unsigned char* oview = of->get_output_view(this->local_symbol_offset_,
					     output_size);

  const std::vector<Map_to_output>& mo(this->map_to_output());

  gold_assert(this->local_values_.size() == loccount);

  unsigned char* ov = oview;
  psyms += sym_size;
  for (unsigned int i = 1; i < loccount; ++i, psyms += sym_size)
    {
      elfcpp::Sym<size, big_endian> isym(psyms);

      if (!this->local_values_[i].needs_output_symtab_entry())
	continue;

      unsigned int st_shndx = isym.get_st_shndx();
      if (st_shndx < elfcpp::SHN_LORESERVE)
	{
	  gold_assert(st_shndx < mo.size());
	  if (mo[st_shndx].output_section == NULL)
	    continue;
	  st_shndx = mo[st_shndx].output_section->out_shndx();
	}

      elfcpp::Sym_write<size, big_endian> osym(ov);

      gold_assert(isym.get_st_name() < strtab_size);
      const char* name = pnames + isym.get_st_name();
      osym.put_st_name(sympool->get_offset(name));
      osym.put_st_value(this->local_values_[i].value(this, 0));
      osym.put_st_size(isym.get_st_size());
      osym.put_st_info(isym.get_st_info());
      osym.put_st_other(isym.get_st_other());
      osym.put_st_shndx(st_shndx);

      ov += sym_size;
    }

  gold_assert(ov - oview == output_size);

  of->write_output_view(this->local_symbol_offset_, output_size, oview);
}

// Input_objects methods.

// Add a regular relocatable object to the list.  Return false if this
// object should be ignored.

bool
Input_objects::add_object(Object* obj)
{
  if (!obj->is_dynamic())
    this->relobj_list_.push_back(static_cast<Relobj*>(obj));
  else
    {
      // See if this is a duplicate SONAME.
      Dynobj* dynobj = static_cast<Dynobj*>(obj);

      std::pair<Unordered_set<std::string>::iterator, bool> ins =
	this->sonames_.insert(dynobj->soname());
      if (!ins.second)
	{
	  // We have already seen a dynamic object with this soname.
	  return false;
	}

      this->dynobj_list_.push_back(dynobj);
    }

  Target* target = obj->target();
  if (this->target_ == NULL)
    this->target_ = target;
  else if (this->target_ != target)
    {
      fprintf(stderr, "%s: %s: incompatible target\n",
	      program_name, obj->name().c_str());
      gold_exit(false);
    }

  return true;
}

// Relocate_info methods.

// Return a string describing the location of a relocation.  This is
// only used in error messages.

template<int size, bool big_endian>
std::string
Relocate_info<size, big_endian>::location(size_t relnum, off_t) const
{
  std::string ret(this->object->name());
  ret += ": reloc ";
  char buf[100];
  snprintf(buf, sizeof buf, "%zu", relnum);
  ret += buf;
  ret += " in reloc section ";
  snprintf(buf, sizeof buf, "%u", this->reloc_shndx);
  ret += buf;
  ret += " (" + this->object->section_name(this->reloc_shndx);
  ret += ") for section ";
  snprintf(buf, sizeof buf, "%u", this->data_shndx);
  ret += buf;
  ret += " (" + this->object->section_name(this->data_shndx) + ")";
  return ret;
}

} // End namespace gold.

namespace
{

using namespace gold;

// Read an ELF file with the header and return the appropriate
// instance of Object.

template<int size, bool big_endian>
Object*
make_elf_sized_object(const std::string& name, Input_file* input_file,
		      off_t offset, const elfcpp::Ehdr<size, big_endian>& ehdr)
{
  int et = ehdr.get_e_type();
  if (et == elfcpp::ET_REL)
    {
      Sized_relobj<size, big_endian>* obj =
	new Sized_relobj<size, big_endian>(name, input_file, offset, ehdr);
      obj->setup(ehdr);
      return obj;
    }
  else if (et == elfcpp::ET_DYN)
    {
      Sized_dynobj<size, big_endian>* obj =
	new Sized_dynobj<size, big_endian>(name, input_file, offset, ehdr);
      obj->setup(ehdr);
      return obj;
    }
  else
    {
      fprintf(stderr, _("%s: %s: unsupported ELF file type %d\n"),
	      program_name, name.c_str(), et);
      gold_exit(false);
    }
}

} // End anonymous namespace.

namespace gold
{

// Read an ELF file and return the appropriate instance of Object.

Object*
make_elf_object(const std::string& name, Input_file* input_file, off_t offset,
		const unsigned char* p, off_t bytes)
{
  if (bytes < elfcpp::EI_NIDENT)
    {
      fprintf(stderr, _("%s: %s: ELF file too short\n"),
	      program_name, name.c_str());
      gold_exit(false);
    }

  int v = p[elfcpp::EI_VERSION];
  if (v != elfcpp::EV_CURRENT)
    {
      if (v == elfcpp::EV_NONE)
	fprintf(stderr, _("%s: %s: invalid ELF version 0\n"),
		program_name, name.c_str());
      else
	fprintf(stderr, _("%s: %s: unsupported ELF version %d\n"),
		program_name, name.c_str(), v);
      gold_exit(false);
    }

  int c = p[elfcpp::EI_CLASS];
  if (c == elfcpp::ELFCLASSNONE)
    {
      fprintf(stderr, _("%s: %s: invalid ELF class 0\n"),
	      program_name, name.c_str());
      gold_exit(false);
    }
  else if (c != elfcpp::ELFCLASS32
	   && c != elfcpp::ELFCLASS64)
    {
      fprintf(stderr, _("%s: %s: unsupported ELF class %d\n"),
	      program_name, name.c_str(), c);
      gold_exit(false);
    }

  int d = p[elfcpp::EI_DATA];
  if (d == elfcpp::ELFDATANONE)
    {
      fprintf(stderr, _("%s: %s: invalid ELF data encoding\n"),
	      program_name, name.c_str());
      gold_exit(false);
    }
  else if (d != elfcpp::ELFDATA2LSB
	   && d != elfcpp::ELFDATA2MSB)
    {
      fprintf(stderr, _("%s: %s: unsupported ELF data encoding %d\n"),
	      program_name, name.c_str(), d);
      gold_exit(false);
    }

  bool big_endian = d == elfcpp::ELFDATA2MSB;

  if (c == elfcpp::ELFCLASS32)
    {
      if (bytes < elfcpp::Elf_sizes<32>::ehdr_size)
	{
	  fprintf(stderr, _("%s: %s: ELF file too short\n"),
		  program_name, name.c_str());
	  gold_exit(false);
	}
      if (big_endian)
	{
	  elfcpp::Ehdr<32, true> ehdr(p);
	  return make_elf_sized_object<32, true>(name, input_file,
						 offset, ehdr);
	}
      else
	{
	  elfcpp::Ehdr<32, false> ehdr(p);
	  return make_elf_sized_object<32, false>(name, input_file,
						  offset, ehdr);
	}
    }
  else
    {
      if (bytes < elfcpp::Elf_sizes<32>::ehdr_size)
	{
	  fprintf(stderr, _("%s: %s: ELF file too short\n"),
		  program_name, name.c_str());
	  gold_exit(false);
	}
      if (big_endian)
	{
	  elfcpp::Ehdr<64, true> ehdr(p);
	  return make_elf_sized_object<64, true>(name, input_file,
						 offset, ehdr);
	}
      else
	{
	  elfcpp::Ehdr<64, false> ehdr(p);
	  return make_elf_sized_object<64, false>(name, input_file,
						  offset, ehdr);
	}
    }
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones for implemented targets.

template
class Sized_relobj<32, false>;

template
class Sized_relobj<32, true>;

template
class Sized_relobj<64, false>;

template
class Sized_relobj<64, true>;

template
struct Relocate_info<32, false>;

template
struct Relocate_info<32, true>;

template
struct Relocate_info<64, false>;

template
struct Relocate_info<64, true>;

} // End namespace gold.
