// output.cc -- manage the output file for gold

#include "gold.h"

#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <algorithm>

#include "object.h"
#include "symtab.h"
#include "reloc.h"
#include "merge.h"
#include "output.h"

namespace gold
{

// Output_data variables.

bool Output_data::sizes_are_fixed;

// Output_data methods.

Output_data::~Output_data()
{
}

// Set the address and offset.

void
Output_data::set_address(uint64_t addr, off_t off)
{
  this->address_ = addr;
  this->offset_ = off;

  // Let the child class know.
  this->do_set_address(addr, off);
}

// Return the default alignment for a size--32 or 64.

uint64_t
Output_data::default_alignment(int size)
{
  if (size == 32)
    return 4;
  else if (size == 64)
    return 8;
  else
    gold_unreachable();
}

// Output_section_header methods.  This currently assumes that the
// segment and section lists are complete at construction time.

Output_section_headers::Output_section_headers(
    int size,
    bool big_endian,
    const Layout* layout,
    const Layout::Segment_list* segment_list,
    const Layout::Section_list* unattached_section_list,
    const Stringpool* secnamepool)
  : size_(size),
    big_endian_(big_endian),
    layout_(layout),
    segment_list_(segment_list),
    unattached_section_list_(unattached_section_list),
    secnamepool_(secnamepool)
{
  // Count all the sections.  Start with 1 for the null section.
  off_t count = 1;
  for (Layout::Segment_list::const_iterator p = segment_list->begin();
       p != segment_list->end();
       ++p)
    if ((*p)->type() == elfcpp::PT_LOAD)
      count += (*p)->output_section_count();
  count += unattached_section_list->size();

  int shdr_size;
  if (size == 32)
    shdr_size = elfcpp::Elf_sizes<32>::shdr_size;
  else if (size == 64)
    shdr_size = elfcpp::Elf_sizes<64>::shdr_size;
  else
    gold_unreachable();

  this->set_data_size(count * shdr_size);
}

// Write out the section headers.

void
Output_section_headers::do_write(Output_file* of)
{
  if (this->size_ == 32)
    {
      if (this->big_endian_)
	this->do_sized_write<32, true>(of);
      else
	this->do_sized_write<32, false>(of);
    }
  else if (this->size_ == 64)
    {
      if (this->big_endian_)
	this->do_sized_write<64, true>(of);
      else
	this->do_sized_write<64, false>(of);
    }
  else
    gold_unreachable();
}

template<int size, bool big_endian>
void
Output_section_headers::do_sized_write(Output_file* of)
{
  off_t all_shdrs_size = this->data_size();
  unsigned char* view = of->get_output_view(this->offset(), all_shdrs_size);

  const int shdr_size = elfcpp::Elf_sizes<size>::shdr_size;
  unsigned char* v = view;

  {
    typename elfcpp::Shdr_write<size, big_endian> oshdr(v);
    oshdr.put_sh_name(0);
    oshdr.put_sh_type(elfcpp::SHT_NULL);
    oshdr.put_sh_flags(0);
    oshdr.put_sh_addr(0);
    oshdr.put_sh_offset(0);
    oshdr.put_sh_size(0);
    oshdr.put_sh_link(0);
    oshdr.put_sh_info(0);
    oshdr.put_sh_addralign(0);
    oshdr.put_sh_entsize(0);
  }

  v += shdr_size;

  unsigned shndx = 1;
  for (Layout::Segment_list::const_iterator p = this->segment_list_->begin();
       p != this->segment_list_->end();
       ++p)
    v = (*p)->write_section_headers SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
	    this->layout_, this->secnamepool_, v, &shndx
	    SELECT_SIZE_ENDIAN(size, big_endian));
  for (Layout::Section_list::const_iterator p =
	 this->unattached_section_list_->begin();
       p != this->unattached_section_list_->end();
       ++p)
    {
      gold_assert(shndx == (*p)->out_shndx());
      elfcpp::Shdr_write<size, big_endian> oshdr(v);
      (*p)->write_header(this->layout_, this->secnamepool_, &oshdr);
      v += shdr_size;
      ++shndx;
    }

  of->write_output_view(this->offset(), all_shdrs_size, view);
}

// Output_segment_header methods.

Output_segment_headers::Output_segment_headers(
    int size,
    bool big_endian,
    const Layout::Segment_list& segment_list)
  : size_(size), big_endian_(big_endian), segment_list_(segment_list)
{
  int phdr_size;
  if (size == 32)
    phdr_size = elfcpp::Elf_sizes<32>::phdr_size;
  else if (size == 64)
    phdr_size = elfcpp::Elf_sizes<64>::phdr_size;
  else
    gold_unreachable();

  this->set_data_size(segment_list.size() * phdr_size);
}

void
Output_segment_headers::do_write(Output_file* of)
{
  if (this->size_ == 32)
    {
      if (this->big_endian_)
	this->do_sized_write<32, true>(of);
      else
	this->do_sized_write<32, false>(of);
    }
  else if (this->size_ == 64)
    {
      if (this->big_endian_)
	this->do_sized_write<64, true>(of);
      else
	this->do_sized_write<64, false>(of);
    }
  else
    gold_unreachable();
}

template<int size, bool big_endian>
void
Output_segment_headers::do_sized_write(Output_file* of)
{
  const int phdr_size = elfcpp::Elf_sizes<size>::phdr_size;
  off_t all_phdrs_size = this->segment_list_.size() * phdr_size;
  unsigned char* view = of->get_output_view(this->offset(),
					    all_phdrs_size);
  unsigned char* v = view;
  for (Layout::Segment_list::const_iterator p = this->segment_list_.begin();
       p != this->segment_list_.end();
       ++p)
    {
      elfcpp::Phdr_write<size, big_endian> ophdr(v);
      (*p)->write_header(&ophdr);
      v += phdr_size;
    }

  of->write_output_view(this->offset(), all_phdrs_size, view);
}

// Output_file_header methods.

Output_file_header::Output_file_header(int size,
				       bool big_endian,
				       const General_options& options,
				       const Target* target,
				       const Symbol_table* symtab,
				       const Output_segment_headers* osh)
  : size_(size),
    big_endian_(big_endian),
    options_(options),
    target_(target),
    symtab_(symtab),
    segment_header_(osh),
    section_header_(NULL),
    shstrtab_(NULL)
{
  int ehdr_size;
  if (size == 32)
    ehdr_size = elfcpp::Elf_sizes<32>::ehdr_size;
  else if (size == 64)
    ehdr_size = elfcpp::Elf_sizes<64>::ehdr_size;
  else
    gold_unreachable();

  this->set_data_size(ehdr_size);
}

// Set the section table information for a file header.

void
Output_file_header::set_section_info(const Output_section_headers* shdrs,
				     const Output_section* shstrtab)
{
  this->section_header_ = shdrs;
  this->shstrtab_ = shstrtab;
}

// Write out the file header.

void
Output_file_header::do_write(Output_file* of)
{
  if (this->size_ == 32)
    {
      if (this->big_endian_)
	this->do_sized_write<32, true>(of);
      else
	this->do_sized_write<32, false>(of);
    }
  else if (this->size_ == 64)
    {
      if (this->big_endian_)
	this->do_sized_write<64, true>(of);
      else
	this->do_sized_write<64, false>(of);
    }
  else
    gold_unreachable();
}

// Write out the file header with appropriate size and endianess.

template<int size, bool big_endian>
void
Output_file_header::do_sized_write(Output_file* of)
{
  gold_assert(this->offset() == 0);

  int ehdr_size = elfcpp::Elf_sizes<size>::ehdr_size;
  unsigned char* view = of->get_output_view(0, ehdr_size);
  elfcpp::Ehdr_write<size, big_endian> oehdr(view);

  unsigned char e_ident[elfcpp::EI_NIDENT];
  memset(e_ident, 0, elfcpp::EI_NIDENT);
  e_ident[elfcpp::EI_MAG0] = elfcpp::ELFMAG0;
  e_ident[elfcpp::EI_MAG1] = elfcpp::ELFMAG1;
  e_ident[elfcpp::EI_MAG2] = elfcpp::ELFMAG2;
  e_ident[elfcpp::EI_MAG3] = elfcpp::ELFMAG3;
  if (size == 32)
    e_ident[elfcpp::EI_CLASS] = elfcpp::ELFCLASS32;
  else if (size == 64)
    e_ident[elfcpp::EI_CLASS] = elfcpp::ELFCLASS64;
  else
    gold_unreachable();
  e_ident[elfcpp::EI_DATA] = (big_endian
			      ? elfcpp::ELFDATA2MSB
			      : elfcpp::ELFDATA2LSB);
  e_ident[elfcpp::EI_VERSION] = elfcpp::EV_CURRENT;
  // FIXME: Some targets may need to set EI_OSABI and EI_ABIVERSION.
  oehdr.put_e_ident(e_ident);

  elfcpp::ET e_type;
  // FIXME: ET_DYN.
  if (this->options_.is_relocatable())
    e_type = elfcpp::ET_REL;
  else
    e_type = elfcpp::ET_EXEC;
  oehdr.put_e_type(e_type);

  oehdr.put_e_machine(this->target_->machine_code());
  oehdr.put_e_version(elfcpp::EV_CURRENT);

  // FIXME: Need to support -e, and target specific entry symbol.
  Symbol* sym = this->symtab_->lookup("_start");
  typename Sized_symbol<size>::Value_type v;
  if (sym == NULL)
    v = 0;
  else
    {
      Sized_symbol<size>* ssym;
      ssym = this->symtab_->get_sized_symbol SELECT_SIZE_NAME(size) (
        sym SELECT_SIZE(size));
      v = ssym->value();
    }
  oehdr.put_e_entry(v);

  oehdr.put_e_phoff(this->segment_header_->offset());
  oehdr.put_e_shoff(this->section_header_->offset());

  // FIXME: The target needs to set the flags.
  oehdr.put_e_flags(0);

  oehdr.put_e_ehsize(elfcpp::Elf_sizes<size>::ehdr_size);
  oehdr.put_e_phentsize(elfcpp::Elf_sizes<size>::phdr_size);
  oehdr.put_e_phnum(this->segment_header_->data_size()
		     / elfcpp::Elf_sizes<size>::phdr_size);
  oehdr.put_e_shentsize(elfcpp::Elf_sizes<size>::shdr_size);
  oehdr.put_e_shnum(this->section_header_->data_size()
		     / elfcpp::Elf_sizes<size>::shdr_size);
  oehdr.put_e_shstrndx(this->shstrtab_->out_shndx());

  of->write_output_view(0, ehdr_size, view);
}

// Output_data_const methods.

void
Output_data_const::do_write(Output_file* of)
{
  of->write(this->offset(), this->data_.data(), this->data_.size());
}

// Output_data_const_buffer methods.

void
Output_data_const_buffer::do_write(Output_file* of)
{
  of->write(this->offset(), this->p_, this->data_size());
}

// Output_section_data methods.

// Record the output section, and set the entry size and such.

void
Output_section_data::set_output_section(Output_section* os)
{
  gold_assert(this->output_section_ == NULL);
  this->output_section_ = os;
  this->do_adjust_output_section(os);
}

// Return the section index of the output section.

unsigned int
Output_section_data::do_out_shndx() const
{
  gold_assert(this->output_section_ != NULL);
  return this->output_section_->out_shndx();
}

// Output_data_strtab methods.

// Set the address.  We don't actually care about the address, but we
// do set our final size.

void
Output_data_strtab::do_set_address(uint64_t, off_t)
{
  this->strtab_->set_string_offsets();
  this->set_data_size(this->strtab_->get_strtab_size());
}

// Write out a string table.

void
Output_data_strtab::do_write(Output_file* of)
{
  this->strtab_->write(of, this->offset());
}

// Output_reloc methods.

// Get the symbol index of a relocation.

template<bool dynamic, int size, bool big_endian>
unsigned int
Output_reloc<elfcpp::SHT_REL, dynamic, size, big_endian>::get_symbol_index()
  const
{
  unsigned int index;
  switch (this->local_sym_index_)
    {
    case INVALID_CODE:
      gold_unreachable();

    case GSYM_CODE:
      if (this->u1_.gsym == NULL)
	index = 0;
      else if (dynamic)
	index = this->u1_.gsym->dynsym_index();
      else
	index = this->u1_.gsym->symtab_index();
      break;

    case SECTION_CODE:
      if (dynamic)
	index = this->u1_.os->dynsym_index();
      else
	index = this->u1_.os->symtab_index();
      break;

    default:
      if (dynamic)
	{
	  // FIXME: It seems that some targets may need to generate
	  // dynamic relocations against local symbols for some
	  // reasons.  This will have to be addressed at some point.
	  gold_unreachable();
	}
      else
	index = this->u1_.relobj->symtab_index(this->local_sym_index_);
      break;
    }
  gold_assert(index != -1U);
  return index;
}

// Write out the offset and info fields of a Rel or Rela relocation
// entry.

template<bool dynamic, int size, bool big_endian>
template<typename Write_rel>
void
Output_reloc<elfcpp::SHT_REL, dynamic, size, big_endian>::write_rel(
    Write_rel* wr) const
{
  Address address = this->address_;
  if (this->shndx_ != INVALID_CODE)
    {
      off_t off;
      Output_section* os = this->u2_.relobj->output_section(this->shndx_,
							    &off);
      gold_assert(os != NULL);
      address += os->address() + off;
    }
  else if (this->u2_.od != NULL)
    address += this->u2_.od->address();
  wr->put_r_offset(address);
  wr->put_r_info(elfcpp::elf_r_info<size>(this->get_symbol_index(),
					  this->type_));
}

// Write out a Rel relocation.

template<bool dynamic, int size, bool big_endian>
void
Output_reloc<elfcpp::SHT_REL, dynamic, size, big_endian>::write(
    unsigned char* pov) const
{
  elfcpp::Rel_write<size, big_endian> orel(pov);
  this->write_rel(&orel);
}

// Write out a Rela relocation.

template<bool dynamic, int size, bool big_endian>
void
Output_reloc<elfcpp::SHT_RELA, dynamic, size, big_endian>::write(
    unsigned char* pov) const
{
  elfcpp::Rela_write<size, big_endian> orel(pov);
  this->rel_.write_rel(&orel);
  orel.put_r_addend(this->addend_);
}

// Output_data_reloc_base methods.

// Adjust the output section.

template<int sh_type, bool dynamic, int size, bool big_endian>
void
Output_data_reloc_base<sh_type, dynamic, size, big_endian>
    ::do_adjust_output_section(Output_section* os)
{
  if (sh_type == elfcpp::SHT_REL)
    os->set_entsize(elfcpp::Elf_sizes<size>::rel_size);
  else if (sh_type == elfcpp::SHT_RELA)
    os->set_entsize(elfcpp::Elf_sizes<size>::rela_size);
  else
    gold_unreachable();
  if (dynamic)
    os->set_should_link_to_dynsym();
  else
    os->set_should_link_to_symtab();
}

// Write out relocation data.

template<int sh_type, bool dynamic, int size, bool big_endian>
void
Output_data_reloc_base<sh_type, dynamic, size, big_endian>::do_write(
    Output_file* of)
{
  const off_t off = this->offset();
  const off_t oview_size = this->data_size();
  unsigned char* const oview = of->get_output_view(off, oview_size);

  unsigned char* pov = oview;
  for (typename Relocs::const_iterator p = this->relocs_.begin();
       p != this->relocs_.end();
       ++p)
    {
      p->write(pov);
      pov += reloc_size;
    }

  gold_assert(pov - oview == oview_size);

  of->write_output_view(off, oview_size, oview);

  // We no longer need the relocation entries.
  this->relocs_.clear();
}

// Output_data_got::Got_entry methods.

// Write out the entry.

template<int size, bool big_endian>
void
Output_data_got<size, big_endian>::Got_entry::write(
    const General_options* options,
    unsigned char* pov) const
{
  Valtype val = 0;

  switch (this->local_sym_index_)
    {
    case GSYM_CODE:
      {
	Symbol* gsym = this->u_.gsym;

	// If the symbol is resolved locally, we need to write out its
	// value.  Otherwise we just write zero.  The target code is
	// responsible for creating a relocation entry to fill in the
	// value at runtime.
	if (gsym->final_value_is_known(options))
	  {
	    Sized_symbol<size>* sgsym;
	    // This cast is a bit ugly.  We don't want to put a
	    // virtual method in Symbol, because we want Symbol to be
	    // as small as possible.
	    sgsym = static_cast<Sized_symbol<size>*>(gsym);
	    val = sgsym->value();
	  }
      }
      break;

    case CONSTANT_CODE:
      val = this->u_.constant;
      break;

    default:
      gold_unreachable();
    }

  elfcpp::Swap<size, big_endian>::writeval(pov, val);
}

// Output_data_got methods.

// Add an entry for a global symbol to the GOT.  This returns true if
// this is a new GOT entry, false if the symbol already had a GOT
// entry.

template<int size, bool big_endian>
bool
Output_data_got<size, big_endian>::add_global(Symbol* gsym)
{
  if (gsym->has_got_offset())
    return false;

  this->entries_.push_back(Got_entry(gsym));
  this->set_got_size();
  gsym->set_got_offset(this->last_got_offset());
  return true;
}

// Write out the GOT.

template<int size, bool big_endian>
void
Output_data_got<size, big_endian>::do_write(Output_file* of)
{
  const int add = size / 8;

  const off_t off = this->offset();
  const off_t oview_size = this->data_size();
  unsigned char* const oview = of->get_output_view(off, oview_size);

  unsigned char* pov = oview;
  for (typename Got_entries::const_iterator p = this->entries_.begin();
       p != this->entries_.end();
       ++p)
    {
      p->write(this->options_, pov);
      pov += add;
    }

  gold_assert(pov - oview == oview_size);

  of->write_output_view(off, oview_size, oview);

  // We no longer need the GOT entries.
  this->entries_.clear();
}

// Output_data_dynamic::Dynamic_entry methods.

// Write out the entry.

template<int size, bool big_endian>
void
Output_data_dynamic::Dynamic_entry::write(
    unsigned char* pov,
    const Stringpool* pool
    ACCEPT_SIZE_ENDIAN) const
{
  typename elfcpp::Elf_types<size>::Elf_WXword val;
  switch (this->classification_)
    {
    case DYNAMIC_NUMBER:
      val = this->u_.val;
      break;

    case DYNAMIC_SECTION_ADDRESS:
      val = this->u_.od->address();
      break;

    case DYNAMIC_SECTION_SIZE:
      val = this->u_.od->data_size();
      break;

    case DYNAMIC_SYMBOL:
      {
	const Sized_symbol<size>* s =
	  static_cast<const Sized_symbol<size>*>(this->u_.sym);
	val = s->value();
      }
      break;

    case DYNAMIC_STRING:
      val = pool->get_offset(this->u_.str);
      break;

    default:
      gold_unreachable();
    }

  elfcpp::Dyn_write<size, big_endian> dw(pov);
  dw.put_d_tag(this->tag_);
  dw.put_d_val(val);
}

// Output_data_dynamic methods.

// Adjust the output section to set the entry size.

void
Output_data_dynamic::do_adjust_output_section(Output_section* os)
{
  if (this->target_->get_size() == 32)
    os->set_entsize(elfcpp::Elf_sizes<32>::dyn_size);
  else if (this->target_->get_size() == 64)
    os->set_entsize(elfcpp::Elf_sizes<64>::dyn_size);
  else
    gold_unreachable();
}

// Set the final data size.

void
Output_data_dynamic::do_set_address(uint64_t, off_t)
{
  // Add the terminating entry.
  this->add_constant(elfcpp::DT_NULL, 0);

  int dyn_size;
  if (this->target_->get_size() == 32)
    dyn_size = elfcpp::Elf_sizes<32>::dyn_size;
  else if (this->target_->get_size() == 64)
    dyn_size = elfcpp::Elf_sizes<64>::dyn_size;
  else
    gold_unreachable();
  this->set_data_size(this->entries_.size() * dyn_size);
}

// Write out the dynamic entries.

void
Output_data_dynamic::do_write(Output_file* of)
{
  if (this->target_->get_size() == 32)
    {
      if (this->target_->is_big_endian())
	this->sized_write<32, true>(of);
      else
	this->sized_write<32, false>(of);
    }
  else if (this->target_->get_size() == 64)
    {
      if (this->target_->is_big_endian())
	this->sized_write<64, true>(of);
      else
	this->sized_write<64, false>(of);
    }
  else
    gold_unreachable();
}

template<int size, bool big_endian>
void
Output_data_dynamic::sized_write(Output_file* of)
{
  const int dyn_size = elfcpp::Elf_sizes<size>::dyn_size;

  const off_t offset = this->offset();
  const off_t oview_size = this->data_size();
  unsigned char* const oview = of->get_output_view(offset, oview_size);

  unsigned char* pov = oview;
  for (typename Dynamic_entries::const_iterator p = this->entries_.begin();
       p != this->entries_.end();
       ++p)
    {
      p->write SELECT_SIZE_ENDIAN_NAME(size, big_endian)(
          pov, this->pool_ SELECT_SIZE_ENDIAN(size, big_endian));
      pov += dyn_size;
    }

  gold_assert(pov - oview == oview_size);

  of->write_output_view(offset, oview_size, oview);

  // We no longer need the dynamic entries.
  this->entries_.clear();
}

// Output_section::Input_section methods.

// Return the data size.  For an input section we store the size here.
// For an Output_section_data, we have to ask it for the size.

off_t
Output_section::Input_section::data_size() const
{
  if (this->is_input_section())
    return this->u1_.data_size;
  else
    return this->u2_.posd->data_size();
}

// Set the address and file offset.

void
Output_section::Input_section::set_address(uint64_t addr, off_t off,
					   off_t secoff)
{
  if (this->is_input_section())
    this->u2_.object->set_section_offset(this->shndx_, off - secoff);
  else
    this->u2_.posd->set_address(addr, off);
}

// Try to turn an input address into an output address.

bool
Output_section::Input_section::output_address(const Relobj* object,
					      unsigned int shndx,
					      off_t offset,
					      uint64_t output_section_address,
					      uint64_t *poutput) const
{
  if (!this->is_input_section())
    return this->u2_.posd->output_address(object, shndx, offset,
					  output_section_address, poutput);
  else
    {
      if (this->u2_.object != object)
	return false;
      off_t output_offset;
      Output_section* os = object->output_section(shndx, &output_offset);
      gold_assert(os != NULL);
      *poutput = output_section_address + output_offset + offset;
      return true;
    }
}

// Write out the data.  We don't have to do anything for an input
// section--they are handled via Object::relocate--but this is where
// we write out the data for an Output_section_data.

void
Output_section::Input_section::write(Output_file* of)
{
  if (!this->is_input_section())
    this->u2_.posd->write(of);
}

// Output_section methods.

// Construct an Output_section.  NAME will point into a Stringpool.

Output_section::Output_section(const char* name, elfcpp::Elf_Word type,
			       elfcpp::Elf_Xword flags)
  : name_(name),
    addralign_(0),
    entsize_(0),
    link_section_(NULL),
    link_(0),
    info_section_(NULL),
    info_(0),
    type_(type),
    flags_(flags),
    out_shndx_(0),
    symtab_index_(0),
    dynsym_index_(0),
    input_sections_(),
    first_input_offset_(0),
    needs_symtab_index_(false),
    needs_dynsym_index_(false),
    should_link_to_symtab_(false),
    should_link_to_dynsym_(false)
{
}

Output_section::~Output_section()
{
}

// Set the entry size.

void
Output_section::set_entsize(uint64_t v)
{
  if (this->entsize_ == 0)
    this->entsize_ = v;
  else
    gold_assert(this->entsize_ == v);
}

// Add the input section SHNDX, with header SHDR, named SECNAME, in
// OBJECT, to the Output_section.  Return the offset of the input
// section within the output section.  We don't always keep track of
// input sections for an Output_section.  Instead, each Object keeps
// track of the Output_section for each of its input sections.

template<int size, bool big_endian>
off_t
Output_section::add_input_section(Relobj* object, unsigned int shndx,
				  const char* secname,
				  const elfcpp::Shdr<size, big_endian>& shdr)
{
  elfcpp::Elf_Xword addralign = shdr.get_sh_addralign();
  if ((addralign & (addralign - 1)) != 0)
    {
      fprintf(stderr, _("%s: %s: invalid alignment %lu for section \"%s\"\n"),
	      program_name, object->name().c_str(),
	      static_cast<unsigned long>(addralign), secname);
      gold_exit(false);
    }

  if (addralign > this->addralign_)
    this->addralign_ = addralign;

  // If this is a SHF_MERGE section, we pass all the input sections to
  // a Output_data_merge.
  if ((shdr.get_sh_flags() & elfcpp::SHF_MERGE) != 0)
    {
      if (this->add_merge_input_section(object, shndx, shdr.get_sh_flags(),
					shdr.get_sh_entsize(),
					addralign))
	{
	  // Tell the relocation routines that they need to call the
	  // output_address method to determine the final address.
	  return -1;
	}
    }

  off_t ssize = this->data_size();
  ssize = align_address(ssize, addralign);
  this->set_data_size(ssize + shdr.get_sh_size());

  // We need to keep track of this section if we are already keeping
  // track of sections, or if we are relaxing.  FIXME: Add test for
  // relaxing.
  if (! this->input_sections_.empty())
    this->input_sections_.push_back(Input_section(object, shndx,
						  shdr.get_sh_size(),
						  addralign));

  return ssize;
}

// Add arbitrary data to an output section.

void
Output_section::add_output_section_data(Output_section_data* posd)
{
  Input_section inp(posd);
  this->add_output_section_data(&inp);
}

// Add arbitrary data to an output section by Input_section.

void
Output_section::add_output_section_data(Input_section* inp)
{
  if (this->input_sections_.empty())
    this->first_input_offset_ = this->data_size();

  this->input_sections_.push_back(*inp);

  uint64_t addralign = inp->addralign();
  if (addralign > this->addralign_)
    this->addralign_ = addralign;

  inp->set_output_section(this);
}

// Add a merge section to an output section.

void
Output_section::add_output_merge_section(Output_section_data* posd,
					 bool is_string, uint64_t entsize)
{
  Input_section inp(posd, is_string, entsize);
  this->add_output_section_data(&inp);
}

// Add an input section to a SHF_MERGE section.

bool
Output_section::add_merge_input_section(Relobj* object, unsigned int shndx,
					uint64_t flags, uint64_t entsize,
					uint64_t addralign)
{
  // We only merge constants if the alignment is not more than the
  // entry size.  This could be handled, but it's unusual.
  if (addralign > entsize)
    return false;

  bool is_string = (flags & elfcpp::SHF_STRINGS) != 0;
  Input_section_list::iterator p;
  for (p = this->input_sections_.begin();
       p != this->input_sections_.end();
       ++p)
    if (p->is_merge_section(is_string, entsize))
      break;

  // We handle the actual constant merging in Output_merge_data or
  // Output_merge_string_data.
  if (p != this->input_sections_.end())
    p->add_input_section(object, shndx);
  else
    {
      Output_section_data* posd;
      if (!is_string)
	posd = new Output_merge_data(entsize);
      else if (entsize == 1)
	posd = new Output_merge_string<char>();
      else if (entsize == 2)
	posd = new Output_merge_string<uint16_t>();
      else if (entsize == 4)
	posd = new Output_merge_string<uint32_t>();
      else
	return false;

      this->add_output_merge_section(posd, is_string, entsize);
      posd->add_input_section(object, shndx);
    }

  return true;
}

// Return the output virtual address of OFFSET relative to the start
// of input section SHNDX in object OBJECT.

uint64_t
Output_section::output_address(const Relobj* object, unsigned int shndx,
			       off_t offset) const
{
  uint64_t addr = this->address() + this->first_input_offset_;
  for (Input_section_list::const_iterator p = this->input_sections_.begin();
       p != this->input_sections_.end();
       ++p)
    {
      addr = align_address(addr, p->addralign());
      uint64_t output;
      if (p->output_address(object, shndx, offset, addr, &output))
	return output;
      addr += p->data_size();
    }

  // If we get here, it means that we don't know the mapping for this
  // input section.  This might happen in principle if
  // add_input_section were called before add_output_section_data.
  // But it should never actually happen.

  gold_unreachable();
}

// Set the address of an Output_section.  This is where we handle
// setting the addresses of any Output_section_data objects.

void
Output_section::do_set_address(uint64_t address, off_t startoff)
{
  if (this->input_sections_.empty())
    return;

  off_t off = startoff + this->first_input_offset_;
  for (Input_section_list::iterator p = this->input_sections_.begin();
       p != this->input_sections_.end();
       ++p)
    {
      off = align_address(off, p->addralign());
      p->set_address(address + (off - startoff), off, startoff);
      off += p->data_size();
    }

  this->set_data_size(off - startoff);
}

// Write the section header to *OSHDR.

template<int size, bool big_endian>
void
Output_section::write_header(const Layout* layout,
			     const Stringpool* secnamepool,
			     elfcpp::Shdr_write<size, big_endian>* oshdr) const
{
  oshdr->put_sh_name(secnamepool->get_offset(this->name_));
  oshdr->put_sh_type(this->type_);
  oshdr->put_sh_flags(this->flags_);
  oshdr->put_sh_addr(this->address());
  oshdr->put_sh_offset(this->offset());
  oshdr->put_sh_size(this->data_size());
  if (this->link_section_ != NULL)
    oshdr->put_sh_link(this->link_section_->out_shndx());
  else if (this->should_link_to_symtab_)
    oshdr->put_sh_link(layout->symtab_section()->out_shndx());
  else if (this->should_link_to_dynsym_)
    oshdr->put_sh_link(layout->dynsym_section()->out_shndx());
  else
    oshdr->put_sh_link(this->link_);
  if (this->info_section_ != NULL)
    oshdr->put_sh_info(this->info_section_->out_shndx());
  else
    oshdr->put_sh_info(this->info_);
  oshdr->put_sh_addralign(this->addralign_);
  oshdr->put_sh_entsize(this->entsize_);
}

// Write out the data.  For input sections the data is written out by
// Object::relocate, but we have to handle Output_section_data objects
// here.

void
Output_section::do_write(Output_file* of)
{
  for (Input_section_list::iterator p = this->input_sections_.begin();
       p != this->input_sections_.end();
       ++p)
    p->write(of);
}

// Output segment methods.

Output_segment::Output_segment(elfcpp::Elf_Word type, elfcpp::Elf_Word flags)
  : output_data_(),
    output_bss_(),
    vaddr_(0),
    paddr_(0),
    memsz_(0),
    align_(0),
    offset_(0),
    filesz_(0),
    type_(type),
    flags_(flags),
    is_align_known_(false)
{
}

// Add an Output_section to an Output_segment.

void
Output_segment::add_output_section(Output_section* os,
				   elfcpp::Elf_Word seg_flags,
				   bool front)
{
  gold_assert((os->flags() & elfcpp::SHF_ALLOC) != 0);
  gold_assert(!this->is_align_known_);

  // Update the segment flags.
  this->flags_ |= seg_flags;

  Output_segment::Output_data_list* pdl;
  if (os->type() == elfcpp::SHT_NOBITS)
    pdl = &this->output_bss_;
  else
    pdl = &this->output_data_;

  // So that PT_NOTE segments will work correctly, we need to ensure
  // that all SHT_NOTE sections are adjacent.  This will normally
  // happen automatically, because all the SHT_NOTE input sections
  // will wind up in the same output section.  However, it is possible
  // for multiple SHT_NOTE input sections to have different section
  // flags, and thus be in different output sections, but for the
  // different section flags to map into the same segment flags and
  // thus the same output segment.

  // Note that while there may be many input sections in an output
  // section, there are normally only a few output sections in an
  // output segment.  This loop is expected to be fast.

  if (os->type() == elfcpp::SHT_NOTE && !pdl->empty())
    {
      Output_segment::Output_data_list::iterator p = pdl->end();
      do
	{
	  --p;
	  if ((*p)->is_section_type(elfcpp::SHT_NOTE))
	    {
	      // We don't worry about the FRONT parameter.
	      ++p;
	      pdl->insert(p, os);
	      return;
	    }
	}
      while (p != pdl->begin());
    }

  // Similarly, so that PT_TLS segments will work, we need to group
  // SHF_TLS sections.  An SHF_TLS/SHT_NOBITS section is a special
  // case: we group the SHF_TLS/SHT_NOBITS sections right after the
  // SHF_TLS/SHT_PROGBITS sections.  This lets us set up PT_TLS
  // correctly.
  if ((os->flags() & elfcpp::SHF_TLS) != 0 && !this->output_data_.empty())
    {
      pdl = &this->output_data_;
      bool nobits = os->type() == elfcpp::SHT_NOBITS;
      bool sawtls = false;
      Output_segment::Output_data_list::iterator p = pdl->end();
      do
	{
	  --p;
	  bool insert;
	  if ((*p)->is_section_flag_set(elfcpp::SHF_TLS))
	    {
	      sawtls = true;
	      // Put a NOBITS section after the first TLS section.
	      // But a PROGBITS section after the first TLS/PROGBITS
	      // section.
	      insert = nobits || !(*p)->is_section_type(elfcpp::SHT_NOBITS);
	    }
	  else
	    {
	      // If we've gone past the TLS sections, but we've seen a
	      // TLS section, then we need to insert this section now.
	      insert = sawtls;
	    }

	  if (insert)
	    {
	      // We don't worry about the FRONT parameter.
	      ++p;
	      pdl->insert(p, os);
	      return;
	    }
	}
      while (p != pdl->begin());

      // There are no TLS sections yet; put this one at the requested
      // location in the section list.
    }

  if (front)
    pdl->push_front(os);
  else
    pdl->push_back(os);
}

// Add an Output_data (which is not an Output_section) to the start of
// a segment.

void
Output_segment::add_initial_output_data(Output_data* od)
{
  gold_assert(!this->is_align_known_);
  this->output_data_.push_front(od);
}

// Return the maximum alignment of the Output_data in Output_segment.
// Once we compute this, we prohibit new sections from being added.

uint64_t
Output_segment::addralign()
{
  if (!this->is_align_known_)
    {
      uint64_t addralign;

      addralign = Output_segment::maximum_alignment(&this->output_data_);
      if (addralign > this->align_)
	this->align_ = addralign;

      addralign = Output_segment::maximum_alignment(&this->output_bss_);
      if (addralign > this->align_)
	this->align_ = addralign;

      this->is_align_known_ = true;
    }

  return this->align_;
}

// Return the maximum alignment of a list of Output_data.

uint64_t
Output_segment::maximum_alignment(const Output_data_list* pdl)
{
  uint64_t ret = 0;
  for (Output_data_list::const_iterator p = pdl->begin();
       p != pdl->end();
       ++p)
    {
      uint64_t addralign = (*p)->addralign();
      if (addralign > ret)
	ret = addralign;
    }
  return ret;
}

// Set the section addresses for an Output_segment.  ADDR is the
// address and *POFF is the file offset.  Set the section indexes
// starting with *PSHNDX.  Return the address of the immediately
// following segment.  Update *POFF and *PSHNDX.

uint64_t
Output_segment::set_section_addresses(uint64_t addr, off_t* poff,
				      unsigned int* pshndx)
{
  gold_assert(this->type_ == elfcpp::PT_LOAD);

  this->vaddr_ = addr;
  this->paddr_ = addr;

  off_t orig_off = *poff;
  this->offset_ = orig_off;

  *poff = align_address(*poff, this->addralign());

  addr = this->set_section_list_addresses(&this->output_data_, addr, poff,
					  pshndx);
  this->filesz_ = *poff - orig_off;

  off_t off = *poff;

  uint64_t ret = this->set_section_list_addresses(&this->output_bss_, addr,
						  poff, pshndx);
  this->memsz_ = *poff - orig_off;

  // Ignore the file offset adjustments made by the BSS Output_data
  // objects.
  *poff = off;

  return ret;
}

// Set the addresses and file offsets in a list of Output_data
// structures.

uint64_t
Output_segment::set_section_list_addresses(Output_data_list* pdl,
					   uint64_t addr, off_t* poff,
					   unsigned int* pshndx)
{
  off_t startoff = *poff;

  off_t off = startoff;
  for (Output_data_list::iterator p = pdl->begin();
       p != pdl->end();
       ++p)
    {
      off = align_address(off, (*p)->addralign());
      (*p)->set_address(addr + (off - startoff), off);

      // Unless this is a PT_TLS segment, we want to ignore the size
      // of a SHF_TLS/SHT_NOBITS section.  Such a section does not
      // affect the size of a PT_LOAD segment.
      if (this->type_ == elfcpp::PT_TLS
	  || !(*p)->is_section_flag_set(elfcpp::SHF_TLS)
	  || !(*p)->is_section_type(elfcpp::SHT_NOBITS))
	off += (*p)->data_size();

      if ((*p)->is_section())
	{
	  (*p)->set_out_shndx(*pshndx);
	  ++*pshndx;
	}
    }

  *poff = off;
  return addr + (off - startoff);
}

// For a non-PT_LOAD segment, set the offset from the sections, if
// any.

void
Output_segment::set_offset()
{
  gold_assert(this->type_ != elfcpp::PT_LOAD);

  if (this->output_data_.empty() && this->output_bss_.empty())
    {
      this->vaddr_ = 0;
      this->paddr_ = 0;
      this->memsz_ = 0;
      this->align_ = 0;
      this->offset_ = 0;
      this->filesz_ = 0;
      return;
    }

  const Output_data* first;
  if (this->output_data_.empty())
    first = this->output_bss_.front();
  else
    first = this->output_data_.front();
  this->vaddr_ = first->address();
  this->paddr_ = this->vaddr_;
  this->offset_ = first->offset();

  if (this->output_data_.empty())
    this->filesz_ = 0;
  else
    {
      const Output_data* last_data = this->output_data_.back();
      this->filesz_ = (last_data->address()
		       + last_data->data_size()
		       - this->vaddr_);
    }

  const Output_data* last;
  if (this->output_bss_.empty())
    last = this->output_data_.back();
  else
    last = this->output_bss_.back();
  this->memsz_ = (last->address()
		  + last->data_size()
		  - this->vaddr_);
}

// Return the number of Output_sections in an Output_segment.

unsigned int
Output_segment::output_section_count() const
{
  return (this->output_section_count_list(&this->output_data_)
	  + this->output_section_count_list(&this->output_bss_));
}

// Return the number of Output_sections in an Output_data_list.

unsigned int
Output_segment::output_section_count_list(const Output_data_list* pdl) const
{
  unsigned int count = 0;
  for (Output_data_list::const_iterator p = pdl->begin();
       p != pdl->end();
       ++p)
    {
      if ((*p)->is_section())
	++count;
    }
  return count;
}

// Write the segment data into *OPHDR.

template<int size, bool big_endian>
void
Output_segment::write_header(elfcpp::Phdr_write<size, big_endian>* ophdr)
{
  ophdr->put_p_type(this->type_);
  ophdr->put_p_offset(this->offset_);
  ophdr->put_p_vaddr(this->vaddr_);
  ophdr->put_p_paddr(this->paddr_);
  ophdr->put_p_filesz(this->filesz_);
  ophdr->put_p_memsz(this->memsz_);
  ophdr->put_p_flags(this->flags_);
  ophdr->put_p_align(this->addralign());
}

// Write the section headers into V.

template<int size, bool big_endian>
unsigned char*
Output_segment::write_section_headers(const Layout* layout,
				      const Stringpool* secnamepool,
				      unsigned char* v,
				      unsigned int *pshndx
                                      ACCEPT_SIZE_ENDIAN) const
{
  // Every section that is attached to a segment must be attached to a
  // PT_LOAD segment, so we only write out section headers for PT_LOAD
  // segments.
  if (this->type_ != elfcpp::PT_LOAD)
    return v;

  v = this->write_section_headers_list
      SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
	  layout, secnamepool, &this->output_data_, v, pshndx
          SELECT_SIZE_ENDIAN(size, big_endian));
  v = this->write_section_headers_list
      SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
          layout, secnamepool, &this->output_bss_, v, pshndx
          SELECT_SIZE_ENDIAN(size, big_endian));
  return v;
}

template<int size, bool big_endian>
unsigned char*
Output_segment::write_section_headers_list(const Layout* layout,
					   const Stringpool* secnamepool,
					   const Output_data_list* pdl,
					   unsigned char* v,
					   unsigned int* pshndx
                                           ACCEPT_SIZE_ENDIAN) const
{
  const int shdr_size = elfcpp::Elf_sizes<size>::shdr_size;
  for (Output_data_list::const_iterator p = pdl->begin();
       p != pdl->end();
       ++p)
    {
      if ((*p)->is_section())
	{
	  const Output_section* ps = static_cast<const Output_section*>(*p);
	  gold_assert(*pshndx == ps->out_shndx());
	  elfcpp::Shdr_write<size, big_endian> oshdr(v);
	  ps->write_header(layout, secnamepool, &oshdr);
	  v += shdr_size;
	  ++*pshndx;
	}
    }
  return v;
}

// Output_file methods.

Output_file::Output_file(const General_options& options)
  : options_(options),
    name_(options.output_file_name()),
    o_(-1),
    file_size_(0),
    base_(NULL)
{
}

// Open the output file.

void
Output_file::open(off_t file_size)
{
  this->file_size_ = file_size;

  int mode = this->options_.is_relocatable() ? 0666 : 0777;
  int o = ::open(this->name_, O_RDWR | O_CREAT | O_TRUNC, mode);
  if (o < 0)
    {
      fprintf(stderr, _("%s: %s: open: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }
  this->o_ = o;

  // Write out one byte to make the file the right size.
  if (::lseek(o, file_size - 1, SEEK_SET) < 0)
    {
      fprintf(stderr, _("%s: %s: lseek: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }
  char b = 0;
  if (::write(o, &b, 1) != 1)
    {
      fprintf(stderr, _("%s: %s: write: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }

  // Map the file into memory.
  void* base = ::mmap(NULL, file_size, PROT_READ | PROT_WRITE,
		      MAP_SHARED, o, 0);
  if (base == MAP_FAILED)
    {
      fprintf(stderr, _("%s: %s: mmap: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }
  this->base_ = static_cast<unsigned char*>(base);
}

// Close the output file.

void
Output_file::close()
{
  if (::munmap(this->base_, this->file_size_) < 0)
    {
      fprintf(stderr, _("%s: %s: munmap: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }
  this->base_ = NULL;

  if (::close(this->o_) < 0)
    {
      fprintf(stderr, _("%s: %s: close: %s\n"),
	      program_name, this->name_, strerror(errno));
      gold_exit(false);
    }
  this->o_ = -1;
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones for implemented targets.

template
off_t
Output_section::add_input_section<32, false>(
    Relobj* object,
    unsigned int shndx,
    const char* secname,
    const elfcpp::Shdr<32, false>& shdr);

template
off_t
Output_section::add_input_section<32, true>(
    Relobj* object,
    unsigned int shndx,
    const char* secname,
    const elfcpp::Shdr<32, true>& shdr);

template
off_t
Output_section::add_input_section<64, false>(
    Relobj* object,
    unsigned int shndx,
    const char* secname,
    const elfcpp::Shdr<64, false>& shdr);

template
off_t
Output_section::add_input_section<64, true>(
    Relobj* object,
    unsigned int shndx,
    const char* secname,
    const elfcpp::Shdr<64, true>& shdr);

template
class Output_data_reloc<elfcpp::SHT_REL, false, 32, false>;

template
class Output_data_reloc<elfcpp::SHT_REL, false, 32, true>;

template
class Output_data_reloc<elfcpp::SHT_REL, false, 64, false>;

template
class Output_data_reloc<elfcpp::SHT_REL, false, 64, true>;

template
class Output_data_reloc<elfcpp::SHT_REL, true, 32, false>;

template
class Output_data_reloc<elfcpp::SHT_REL, true, 32, true>;

template
class Output_data_reloc<elfcpp::SHT_REL, true, 64, false>;

template
class Output_data_reloc<elfcpp::SHT_REL, true, 64, true>;

template
class Output_data_reloc<elfcpp::SHT_RELA, false, 32, false>;

template
class Output_data_reloc<elfcpp::SHT_RELA, false, 32, true>;

template
class Output_data_reloc<elfcpp::SHT_RELA, false, 64, false>;

template
class Output_data_reloc<elfcpp::SHT_RELA, false, 64, true>;

template
class Output_data_reloc<elfcpp::SHT_RELA, true, 32, false>;

template
class Output_data_reloc<elfcpp::SHT_RELA, true, 32, true>;

template
class Output_data_reloc<elfcpp::SHT_RELA, true, 64, false>;

template
class Output_data_reloc<elfcpp::SHT_RELA, true, 64, true>;

template
class Output_data_got<32, false>;

template
class Output_data_got<32, true>;

template
class Output_data_got<64, false>;

template
class Output_data_got<64, true>;

} // End namespace gold.

