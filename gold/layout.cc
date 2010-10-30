// layout.cc -- lay out output file sections for gold

#include "gold.h"

#include <cstring>
#include <algorithm>
#include <iostream>
#include <utility>

#include "output.h"
#include "symtab.h"
#include "dynobj.h"
#include "layout.h"

namespace gold
{

// Layout_task_runner methods.

// Lay out the sections.  This is called after all the input objects
// have been read.

void
Layout_task_runner::run(Workqueue* workqueue)
{
  off_t file_size = this->layout_->finalize(this->input_objects_,
					    this->symtab_);

  // Now we know the final size of the output file and we know where
  // each piece of information goes.
  Output_file* of = new Output_file(this->options_);
  of->open(file_size);

  // Queue up the final set of tasks.
  gold::queue_final_tasks(this->options_, this->input_objects_,
			  this->symtab_, this->layout_, workqueue, of);
}

// Layout methods.

Layout::Layout(const General_options& options)
  : options_(options), namepool_(), sympool_(), dynpool_(), signatures_(),
    section_name_map_(), segment_list_(), section_list_(),
    unattached_section_list_(), special_output_list_(),
    tls_segment_(NULL), symtab_section_(NULL),
    dynsym_section_(NULL), dynamic_section_(NULL), dynamic_data_(NULL)
{
  // Make space for more than enough segments for a typical file.
  // This is just for efficiency--it's OK if we wind up needing more.
  this->segment_list_.reserve(12);

  // We expect three unattached Output_data objects: the file header,
  // the segment headers, and the section headers.
  this->special_output_list_.reserve(3);
}

// Hash a key we use to look up an output section mapping.

size_t
Layout::Hash_key::operator()(const Layout::Key& k) const
{
 return k.first + k.second.first + k.second.second;
}

// Whether to include this section in the link.

template<int size, bool big_endian>
bool
Layout::include_section(Object*, const char*,
			const elfcpp::Shdr<size, big_endian>& shdr)
{
  // Some section types are never linked.  Some are only linked when
  // doing a relocateable link.
  switch (shdr.get_sh_type())
    {
    case elfcpp::SHT_NULL:
    case elfcpp::SHT_SYMTAB:
    case elfcpp::SHT_DYNSYM:
    case elfcpp::SHT_STRTAB:
    case elfcpp::SHT_HASH:
    case elfcpp::SHT_DYNAMIC:
    case elfcpp::SHT_SYMTAB_SHNDX:
      return false;

    case elfcpp::SHT_RELA:
    case elfcpp::SHT_REL:
    case elfcpp::SHT_GROUP:
      return this->options_.is_relocatable();

    default:
      // FIXME: Handle stripping debug sections here.
      return true;
    }
}

// Return an output section named NAME, or NULL if there is none.

Output_section*
Layout::find_output_section(const char* name) const
{
  for (Section_name_map::const_iterator p = this->section_name_map_.begin();
       p != this->section_name_map_.end();
       ++p)
    if (strcmp(p->second->name(), name) == 0)
      return p->second;
  return NULL;
}

// Return an output segment of type TYPE, with segment flags SET set
// and segment flags CLEAR clear.  Return NULL if there is none.

Output_segment*
Layout::find_output_segment(elfcpp::PT type, elfcpp::Elf_Word set,
			    elfcpp::Elf_Word clear) const
{
  for (Segment_list::const_iterator p = this->segment_list_.begin();
       p != this->segment_list_.end();
       ++p)
    if (static_cast<elfcpp::PT>((*p)->type()) == type
	&& ((*p)->flags() & set) == set
	&& ((*p)->flags() & clear) == 0)
      return *p;
  return NULL;
}

// Return the output section to use for section NAME with type TYPE
// and section flags FLAGS.

Output_section*
Layout::get_output_section(const char* name, Stringpool::Key name_key,
			   elfcpp::Elf_Word type, elfcpp::Elf_Xword flags)
{
  // We should ignore some flags.
  flags &= ~ (elfcpp::SHF_INFO_LINK
	      | elfcpp::SHF_LINK_ORDER
	      | elfcpp::SHF_GROUP
	      | elfcpp::SHF_MERGE
	      | elfcpp::SHF_STRINGS);

  const Key key(name_key, std::make_pair(type, flags));
  const std::pair<Key, Output_section*> v(key, NULL);
  std::pair<Section_name_map::iterator, bool> ins(
    this->section_name_map_.insert(v));

  if (!ins.second)
    return ins.first->second;
  else
    {
      // This is the first time we've seen this name/type/flags
      // combination.
      Output_section* os = this->make_output_section(name, type, flags);
      ins.first->second = os;
      return os;
    }
}

// Return the output section to use for input section SHNDX, with name
// NAME, with header HEADER, from object OBJECT.  Set *OFF to the
// offset of this input section without the output section.

template<int size, bool big_endian>
Output_section*
Layout::layout(Relobj* object, unsigned int shndx, const char* name,
	       const elfcpp::Shdr<size, big_endian>& shdr, off_t* off)
{
  if (!this->include_section(object, name, shdr))
    return NULL;

  // If we are not doing a relocateable link, choose the name to use
  // for the output section.
  size_t len = strlen(name);
  if (!this->options_.is_relocatable())
    name = Layout::output_section_name(name, &len);

  // FIXME: Handle SHF_OS_NONCONFORMING here.

  // Canonicalize the section name.
  Stringpool::Key name_key;
  name = this->namepool_.add(name, len, &name_key);

  // Find the output section.  The output section is selected based on
  // the section name, type, and flags.
  Output_section* os = this->get_output_section(name, name_key,
						shdr.get_sh_type(),
						shdr.get_sh_flags());

  // FIXME: Handle SHF_LINK_ORDER somewhere.

  *off = os->add_input_section(object, shndx, name, shdr);

  return os;
}

// Add POSD to an output section using NAME, TYPE, and FLAGS.

void
Layout::add_output_section_data(const char* name, elfcpp::Elf_Word type,
				elfcpp::Elf_Xword flags,
				Output_section_data* posd)
{
  // Canonicalize the name.
  Stringpool::Key name_key;
  name = this->namepool_.add(name, &name_key);

  Output_section* os = this->get_output_section(name, name_key, type, flags);
  os->add_output_section_data(posd);
}

// Map section flags to segment flags.

elfcpp::Elf_Word
Layout::section_flags_to_segment(elfcpp::Elf_Xword flags)
{
  elfcpp::Elf_Word ret = elfcpp::PF_R;
  if ((flags & elfcpp::SHF_WRITE) != 0)
    ret |= elfcpp::PF_W;
  if ((flags & elfcpp::SHF_EXECINSTR) != 0)
    ret |= elfcpp::PF_X;
  return ret;
}

// Make a new Output_section, and attach it to segments as
// appropriate.

Output_section*
Layout::make_output_section(const char* name, elfcpp::Elf_Word type,
			    elfcpp::Elf_Xword flags)
{
  Output_section* os = new Output_section(name, type, flags);
  this->section_list_.push_back(os);

  if ((flags & elfcpp::SHF_ALLOC) == 0)
    this->unattached_section_list_.push_back(os);
  else
    {
      // This output section goes into a PT_LOAD segment.

      elfcpp::Elf_Word seg_flags = Layout::section_flags_to_segment(flags);

      // The only thing we really care about for PT_LOAD segments is
      // whether or not they are writable, so that is how we search
      // for them.  People who need segments sorted on some other
      // basis will have to wait until we implement a mechanism for
      // them to describe the segments they want.

      Segment_list::const_iterator p;
      for (p = this->segment_list_.begin();
	   p != this->segment_list_.end();
	   ++p)
	{
	  if ((*p)->type() == elfcpp::PT_LOAD
	      && ((*p)->flags() & elfcpp::PF_W) == (seg_flags & elfcpp::PF_W))
	    {
	      (*p)->add_output_section(os, seg_flags);
	      break;
	    }
	}

      if (p == this->segment_list_.end())
	{
	  Output_segment* oseg = new Output_segment(elfcpp::PT_LOAD,
						    seg_flags);
	  this->segment_list_.push_back(oseg);
	  oseg->add_output_section(os, seg_flags);
	}

      // If we see a loadable SHT_NOTE section, we create a PT_NOTE
      // segment.
      if (type == elfcpp::SHT_NOTE)
	{
	  // See if we already have an equivalent PT_NOTE segment.
	  for (p = this->segment_list_.begin();
	       p != segment_list_.end();
	       ++p)
	    {
	      if ((*p)->type() == elfcpp::PT_NOTE
		  && (((*p)->flags() & elfcpp::PF_W)
		      == (seg_flags & elfcpp::PF_W)))
		{
		  (*p)->add_output_section(os, seg_flags);
		  break;
		}
	    }

	  if (p == this->segment_list_.end())
	    {
	      Output_segment* oseg = new Output_segment(elfcpp::PT_NOTE,
							seg_flags);
	      this->segment_list_.push_back(oseg);
	      oseg->add_output_section(os, seg_flags);
	    }
	}

      // If we see a loadable SHF_TLS section, we create a PT_TLS
      // segment.  There can only be one such segment.
      if ((flags & elfcpp::SHF_TLS) != 0)
	{
	  if (this->tls_segment_ == NULL)
	    {
	      this->tls_segment_ = new Output_segment(elfcpp::PT_TLS,
						      seg_flags);
	      this->segment_list_.push_back(this->tls_segment_);
	    }
	  this->tls_segment_->add_output_section(os, seg_flags);
	}
    }

  return os;
}

// Create the dynamic sections which are needed before we read the
// relocs.

void
Layout::create_initial_dynamic_sections(const Input_objects* input_objects,
					Symbol_table* symtab)
{
  if (!input_objects->any_dynamic())
    return;

  const char* dynamic_name = this->namepool_.add(".dynamic", NULL);
  this->dynamic_section_ = this->make_output_section(dynamic_name,
						     elfcpp::SHT_DYNAMIC,
						     (elfcpp::SHF_ALLOC
						      | elfcpp::SHF_WRITE));

  symtab->define_in_output_data(input_objects->target(), "_DYNAMIC", NULL,
				this->dynamic_section_, 0, 0,
				elfcpp::STT_OBJECT, elfcpp::STB_LOCAL,
				elfcpp::STV_HIDDEN, 0, false, false);

  this->dynamic_data_ =  new Output_data_dynamic(input_objects->target(),
						 &this->dynpool_);

  this->dynamic_section_->add_output_section_data(this->dynamic_data_);
}

// Find the first read-only PT_LOAD segment, creating one if
// necessary.

Output_segment*
Layout::find_first_load_seg()
{
  for (Segment_list::const_iterator p = this->segment_list_.begin();
       p != this->segment_list_.end();
       ++p)
    {
      if ((*p)->type() == elfcpp::PT_LOAD
	  && ((*p)->flags() & elfcpp::PF_R) != 0
	  && ((*p)->flags() & elfcpp::PF_W) == 0)
	return *p;
    }

  Output_segment* load_seg = new Output_segment(elfcpp::PT_LOAD, elfcpp::PF_R);
  this->segment_list_.push_back(load_seg);
  return load_seg;
}

// Finalize the layout.  When this is called, we have created all the
// output sections and all the output segments which are based on
// input sections.  We have several things to do, and we have to do
// them in the right order, so that we get the right results correctly
// and efficiently.

// 1) Finalize the list of output segments and create the segment
// table header.

// 2) Finalize the dynamic symbol table and associated sections.

// 3) Determine the final file offset of all the output segments.

// 4) Determine the final file offset of all the SHF_ALLOC output
// sections.

// 5) Create the symbol table sections and the section name table
// section.

// 6) Finalize the symbol table: set symbol values to their final
// value and make a final determination of which symbols are going
// into the output symbol table.

// 7) Create the section table header.

// 8) Determine the final file offset of all the output sections which
// are not SHF_ALLOC, including the section table header.

// 9) Finalize the ELF file header.

// This function returns the size of the output file.

off_t
Layout::finalize(const Input_objects* input_objects, Symbol_table* symtab)
{
  Target* const target = input_objects->target();
  const int size = target->get_size();

  target->finalize_sections(&this->options_, this);

  Output_segment* phdr_seg = NULL;
  if (input_objects->any_dynamic())
    {
      // There was a dynamic object in the link.  We need to create
      // some information for the dynamic linker.

      // Create the PT_PHDR segment which will hold the program
      // headers.
      phdr_seg = new Output_segment(elfcpp::PT_PHDR, elfcpp::PF_R);
      this->segment_list_.push_back(phdr_seg);

      // Create the dynamic symbol table, including the hash table.
      Output_section* dynstr;
      std::vector<Symbol*> dynamic_symbols;
      unsigned int local_dynamic_count;
      Versions versions;
      this->create_dynamic_symtab(target, symtab, &dynstr,
				  &local_dynamic_count, &dynamic_symbols,
				  &versions);

      // Create the .interp section to hold the name of the
      // interpreter, and put it in a PT_INTERP segment.
      this->create_interp(target);

      // Finish the .dynamic section to hold the dynamic data, and put
      // it in a PT_DYNAMIC segment.
      this->finish_dynamic_section(input_objects, symtab);

      // We should have added everything we need to the dynamic string
      // table.
      this->dynpool_.set_string_offsets();

      // Create the version sections.  We can't do this until the
      // dynamic string table is complete.
      this->create_version_sections(target, &versions, local_dynamic_count,
				    dynamic_symbols, dynstr);
    }

  // FIXME: Handle PT_GNU_STACK.

  Output_segment* load_seg = this->find_first_load_seg();

  // Lay out the segment headers.
  bool big_endian = target->is_big_endian();
  Output_segment_headers* segment_headers;
  segment_headers = new Output_segment_headers(size, big_endian,
					       this->segment_list_);
  load_seg->add_initial_output_data(segment_headers);
  this->special_output_list_.push_back(segment_headers);
  if (phdr_seg != NULL)
    phdr_seg->add_initial_output_data(segment_headers);

  // Lay out the file header.
  Output_file_header* file_header;
  file_header = new Output_file_header(size,
				       big_endian,
				       this->options_,
				       target,
				       symtab,
				       segment_headers);
  load_seg->add_initial_output_data(file_header);
  this->special_output_list_.push_back(file_header);

  // We set the output section indexes in set_segment_offsets and
  // set_section_offsets.
  unsigned int shndx = 1;

  // Set the file offsets of all the segments, and all the sections
  // they contain.
  off_t off = this->set_segment_offsets(target, load_seg, &shndx);

  // Create the symbol table sections.
  this->create_symtab_sections(size, input_objects, symtab, &off);

  // Create the .shstrtab section.
  Output_section* shstrtab_section = this->create_shstrtab();

  // Set the file offsets of all the sections not associated with
  // segments.
  off = this->set_section_offsets(off, &shndx);

  // Create the section table header.
  Output_section_headers* oshdrs = this->create_shdrs(size, big_endian, &off);

  file_header->set_section_info(oshdrs, shstrtab_section);

  // Now we know exactly where everything goes in the output file.
  Output_data::layout_complete();

  return off;
}

// Return whether SEG1 should be before SEG2 in the output file.  This
// is based entirely on the segment type and flags.  When this is
// called the segment addresses has normally not yet been set.

bool
Layout::segment_precedes(const Output_segment* seg1,
			 const Output_segment* seg2)
{
  elfcpp::Elf_Word type1 = seg1->type();
  elfcpp::Elf_Word type2 = seg2->type();

  // The single PT_PHDR segment is required to precede any loadable
  // segment.  We simply make it always first.
  if (type1 == elfcpp::PT_PHDR)
    {
      gold_assert(type2 != elfcpp::PT_PHDR);
      return true;
    }
  if (type2 == elfcpp::PT_PHDR)
    return false;

  // The single PT_INTERP segment is required to precede any loadable
  // segment.  We simply make it always second.
  if (type1 == elfcpp::PT_INTERP)
    {
      gold_assert(type2 != elfcpp::PT_INTERP);
      return true;
    }
  if (type2 == elfcpp::PT_INTERP)
    return false;

  // We then put PT_LOAD segments before any other segments.
  if (type1 == elfcpp::PT_LOAD && type2 != elfcpp::PT_LOAD)
    return true;
  if (type2 == elfcpp::PT_LOAD && type1 != elfcpp::PT_LOAD)
    return false;

  // We put the PT_TLS segment last, because that is where the dynamic
  // linker expects to find it (this is just for efficiency; other
  // positions would also work correctly).
  if (type1 == elfcpp::PT_TLS && type2 != elfcpp::PT_TLS)
    return false;
  if (type2 == elfcpp::PT_TLS && type1 != elfcpp::PT_TLS)
    return true;

  const elfcpp::Elf_Word flags1 = seg1->flags();
  const elfcpp::Elf_Word flags2 = seg2->flags();

  // The order of non-PT_LOAD segments is unimportant.  We simply sort
  // by the numeric segment type and flags values.  There should not
  // be more than one segment with the same type and flags.
  if (type1 != elfcpp::PT_LOAD)
    {
      if (type1 != type2)
	return type1 < type2;
      gold_assert(flags1 != flags2);
      return flags1 < flags2;
    }

  // We sort PT_LOAD segments based on the flags.  Readonly segments
  // come before writable segments.  Then executable segments come
  // before non-executable segments.  Then the unlikely case of a
  // non-readable segment comes before the normal case of a readable
  // segment.  If there are multiple segments with the same type and
  // flags, we require that the address be set, and we sort by
  // virtual address and then physical address.
  if ((flags1 & elfcpp::PF_W) != (flags2 & elfcpp::PF_W))
    return (flags1 & elfcpp::PF_W) == 0;
  if ((flags1 & elfcpp::PF_X) != (flags2 & elfcpp::PF_X))
    return (flags1 & elfcpp::PF_X) != 0;
  if ((flags1 & elfcpp::PF_R) != (flags2 & elfcpp::PF_R))
    return (flags1 & elfcpp::PF_R) == 0;

  uint64_t vaddr1 = seg1->vaddr();
  uint64_t vaddr2 = seg2->vaddr();
  if (vaddr1 != vaddr2)
    return vaddr1 < vaddr2;

  uint64_t paddr1 = seg1->paddr();
  uint64_t paddr2 = seg2->paddr();
  gold_assert(paddr1 != paddr2);
  return paddr1 < paddr2;
}

// Set the file offsets of all the segments, and all the sections they
// contain.  They have all been created.  LOAD_SEG must be be laid out
// first.  Return the offset of the data to follow.

off_t
Layout::set_segment_offsets(const Target* target, Output_segment* load_seg,
			    unsigned int *pshndx)
{
  // Sort them into the final order.
  std::sort(this->segment_list_.begin(), this->segment_list_.end(),
	    Layout::Compare_segments());

  // Find the PT_LOAD segments, and set their addresses and offsets
  // and their section's addresses and offsets.
  uint64_t addr = target->text_segment_address();
  off_t off = 0;
  bool was_readonly = false;
  for (Segment_list::iterator p = this->segment_list_.begin();
       p != this->segment_list_.end();
       ++p)
    {
      if ((*p)->type() == elfcpp::PT_LOAD)
	{
	  if (load_seg != NULL && load_seg != *p)
	    gold_unreachable();
	  load_seg = NULL;

	  // If the last segment was readonly, and this one is not,
	  // then skip the address forward one page, maintaining the
	  // same position within the page.  This lets us store both
	  // segments overlapping on a single page in the file, but
	  // the loader will put them on different pages in memory.

	  uint64_t orig_addr = addr;
	  uint64_t orig_off = off;

	  uint64_t aligned_addr = addr;
	  uint64_t abi_pagesize = target->abi_pagesize();
	  if (was_readonly && ((*p)->flags() & elfcpp::PF_W) != 0)
	    {
	      uint64_t align = (*p)->addralign();

	      addr = align_address(addr, align);
	      aligned_addr = addr;
	      if ((addr & (abi_pagesize - 1)) != 0)
		addr = addr + abi_pagesize;
	    }

	  unsigned int shndx_hold = *pshndx;
	  off = orig_off + ((addr - orig_addr) & (abi_pagesize - 1));
	  uint64_t new_addr = (*p)->set_section_addresses(addr, &off, pshndx);

	  // Now that we know the size of this segment, we may be able
	  // to save a page in memory, at the cost of wasting some
	  // file space, by instead aligning to the start of a new
	  // page.  Here we use the real machine page size rather than
	  // the ABI mandated page size.

	  if (aligned_addr != addr)
	    {
	      uint64_t common_pagesize = target->common_pagesize();
	      uint64_t first_off = (common_pagesize
				    - (aligned_addr
				       & (common_pagesize - 1)));
	      uint64_t last_off = new_addr & (common_pagesize - 1);
	      if (first_off > 0
		  && last_off > 0
		  && ((aligned_addr & ~ (common_pagesize - 1))
		      != (new_addr & ~ (common_pagesize - 1)))
		  && first_off + last_off <= common_pagesize)
		{
		  *pshndx = shndx_hold;
		  addr = align_address(aligned_addr, common_pagesize);
		  off = orig_off + ((addr - orig_addr) & (abi_pagesize - 1));
		  new_addr = (*p)->set_section_addresses(addr, &off, pshndx);
		}
	    }

	  addr = new_addr;

	  if (((*p)->flags() & elfcpp::PF_W) == 0)
	    was_readonly = true;
	}
    }

  // Handle the non-PT_LOAD segments, setting their offsets from their
  // section's offsets.
  for (Segment_list::iterator p = this->segment_list_.begin();
       p != this->segment_list_.end();
       ++p)
    {
      if ((*p)->type() != elfcpp::PT_LOAD)
	(*p)->set_offset();
    }

  return off;
}

// Set the file offset of all the sections not associated with a
// segment.

off_t
Layout::set_section_offsets(off_t off, unsigned int* pshndx)
{
  for (Section_list::iterator p = this->unattached_section_list_.begin();
       p != this->unattached_section_list_.end();
       ++p)
    {
      (*p)->set_out_shndx(*pshndx);
      ++*pshndx;
      if ((*p)->offset() != -1)
	continue;
      off = align_address(off, (*p)->addralign());
      (*p)->set_address(0, off);
      off += (*p)->data_size();
    }
  return off;
}

// Create the symbol table sections.  Here we also set the final
// values of the symbols.  At this point all the loadable sections are
// fully laid out.

void
Layout::create_symtab_sections(int size, const Input_objects* input_objects,
			       Symbol_table* symtab,
			       off_t* poff)
{
  int symsize;
  unsigned int align;
  if (size == 32)
    {
      symsize = elfcpp::Elf_sizes<32>::sym_size;
      align = 4;
    }
  else if (size == 64)
    {
      symsize = elfcpp::Elf_sizes<64>::sym_size;
      align = 8;
    }
  else
    gold_unreachable();

  off_t off = *poff;
  off = align_address(off, align);
  off_t startoff = off;

  // Save space for the dummy symbol at the start of the section.  We
  // never bother to write this out--it will just be left as zero.
  off += symsize;
  unsigned int local_symbol_index = 1;

  // Add STT_SECTION symbols for each Output section which needs one.
  for (Section_list::iterator p = this->section_list_.begin();
       p != this->section_list_.end();
       ++p)
    {
      if (!(*p)->needs_symtab_index())
	(*p)->set_symtab_index(-1U);
      else
	{
	  (*p)->set_symtab_index(local_symbol_index);
	  ++local_symbol_index;
	  off += symsize;
	}
    }

  for (Input_objects::Relobj_iterator p = input_objects->relobj_begin();
       p != input_objects->relobj_end();
       ++p)
    {
      Task_lock_obj<Object> tlo(**p);
      unsigned int index = (*p)->finalize_local_symbols(local_symbol_index,
							off,
							&this->sympool_);
      off += (index - local_symbol_index) * symsize;
      local_symbol_index = index;
    }

  unsigned int local_symcount = local_symbol_index;
  gold_assert(local_symcount * symsize == off - startoff);

  off_t dynoff;
  size_t dyn_global_index;
  size_t dyncount;
  if (this->dynsym_section_ == NULL)
    {
      dynoff = 0;
      dyn_global_index = 0;
      dyncount = 0;
    }
  else
    {
      dyn_global_index = this->dynsym_section_->info();
      off_t locsize = dyn_global_index * this->dynsym_section_->entsize();
      dynoff = this->dynsym_section_->offset() + locsize;
      dyncount = (this->dynsym_section_->data_size() - locsize) / symsize;
      gold_assert(dyncount * symsize
		  == this->dynsym_section_->data_size() - locsize);
    }

  off = symtab->finalize(local_symcount, off, dynoff, dyn_global_index,
			 dyncount, &this->sympool_);

  this->sympool_.set_string_offsets();

  const char* symtab_name = this->namepool_.add(".symtab", NULL);
  Output_section* osymtab = this->make_output_section(symtab_name,
						      elfcpp::SHT_SYMTAB,
						      0);
  this->symtab_section_ = osymtab;

  Output_section_data* pos = new Output_data_space(off - startoff,
						   align);
  osymtab->add_output_section_data(pos);

  const char* strtab_name = this->namepool_.add(".strtab", NULL);
  Output_section* ostrtab = this->make_output_section(strtab_name,
						      elfcpp::SHT_STRTAB,
						      0);

  Output_section_data* pstr = new Output_data_strtab(&this->sympool_);
  ostrtab->add_output_section_data(pstr);

  osymtab->set_address(0, startoff);
  osymtab->set_link_section(ostrtab);
  osymtab->set_info(local_symcount);
  osymtab->set_entsize(symsize);

  *poff = off;
}

// Create the .shstrtab section, which holds the names of the
// sections.  At the time this is called, we have created all the
// output sections except .shstrtab itself.

Output_section*
Layout::create_shstrtab()
{
  // FIXME: We don't need to create a .shstrtab section if we are
  // stripping everything.

  const char* name = this->namepool_.add(".shstrtab", NULL);

  this->namepool_.set_string_offsets();

  Output_section* os = this->make_output_section(name, elfcpp::SHT_STRTAB, 0);

  Output_section_data* posd = new Output_data_strtab(&this->namepool_);
  os->add_output_section_data(posd);

  return os;
}

// Create the section headers.  SIZE is 32 or 64.  OFF is the file
// offset.

Output_section_headers*
Layout::create_shdrs(int size, bool big_endian, off_t* poff)
{
  Output_section_headers* oshdrs;
  oshdrs = new Output_section_headers(size, big_endian, this,
				      &this->segment_list_,
				      &this->unattached_section_list_,
				      &this->namepool_);
  off_t off = align_address(*poff, oshdrs->addralign());
  oshdrs->set_address(0, off);
  off += oshdrs->data_size();
  *poff = off;
  this->special_output_list_.push_back(oshdrs);
  return oshdrs;
}

// Create the dynamic symbol table.

void
Layout::create_dynamic_symtab(const Target* target, Symbol_table* symtab,
			      Output_section **pdynstr,
			      unsigned int* plocal_dynamic_count,
			      std::vector<Symbol*>* pdynamic_symbols,
			      Versions* pversions)
{
  // Count all the symbols in the dynamic symbol table, and set the
  // dynamic symbol indexes.

  // Skip symbol 0, which is always all zeroes.
  unsigned int index = 1;

  // Add STT_SECTION symbols for each Output section which needs one.
  for (Section_list::iterator p = this->section_list_.begin();
       p != this->section_list_.end();
       ++p)
    {
      if (!(*p)->needs_dynsym_index())
	(*p)->set_dynsym_index(-1U);
      else
	{
	  (*p)->set_dynsym_index(index);
	  ++index;
	}
    }

  // FIXME: Some targets apparently require local symbols in the
  // dynamic symbol table.  Here is where we will have to count them,
  // and set the dynamic symbol indexes, and add the names to
  // this->dynpool_.

  unsigned int local_symcount = index;
  *plocal_dynamic_count = local_symcount;

  // FIXME: We have to tell set_dynsym_indexes whether the
  // -E/--export-dynamic option was used.
  index = symtab->set_dynsym_indexes(&this->options_, target, index,
				     pdynamic_symbols, &this->dynpool_,
				     pversions);

  int symsize;
  unsigned int align;
  const int size = target->get_size();
  if (size == 32)
    {
      symsize = elfcpp::Elf_sizes<32>::sym_size;
      align = 4;
    }
  else if (size == 64)
    {
      symsize = elfcpp::Elf_sizes<64>::sym_size;
      align = 8;
    }
  else
    gold_unreachable();

  // Create the dynamic symbol table section.

  const char* dynsym_name = this->namepool_.add(".dynsym", NULL);
  Output_section* dynsym = this->make_output_section(dynsym_name,
						     elfcpp::SHT_DYNSYM,
						     elfcpp::SHF_ALLOC);

  Output_section_data* odata = new Output_data_space(index * symsize,
						     align);
  dynsym->add_output_section_data(odata);

  dynsym->set_info(local_symcount);
  dynsym->set_entsize(symsize);
  dynsym->set_addralign(align);

  this->dynsym_section_ = dynsym;

  Output_data_dynamic* const odyn = this->dynamic_data_;
  odyn->add_section_address(elfcpp::DT_SYMTAB, dynsym);
  odyn->add_constant(elfcpp::DT_SYMENT, symsize);

  // Create the dynamic string table section.

  const char* dynstr_name = this->namepool_.add(".dynstr", NULL);
  Output_section* dynstr = this->make_output_section(dynstr_name,
						     elfcpp::SHT_STRTAB,
						     elfcpp::SHF_ALLOC);

  Output_section_data* strdata = new Output_data_strtab(&this->dynpool_);
  dynstr->add_output_section_data(strdata);

  dynsym->set_link_section(dynstr);
  this->dynamic_section_->set_link_section(dynstr);

  odyn->add_section_address(elfcpp::DT_STRTAB, dynstr);
  odyn->add_section_size(elfcpp::DT_STRSZ, dynstr);

  *pdynstr = dynstr;

  // Create the hash tables.

  // FIXME: We need an option to create a GNU hash table.

  unsigned char* phash;
  unsigned int hashlen;
  Dynobj::create_elf_hash_table(target, *pdynamic_symbols, local_symcount,
				&phash, &hashlen);

  const char* hash_name = this->namepool_.add(".hash", NULL);
  Output_section* hashsec = this->make_output_section(hash_name,
						      elfcpp::SHT_HASH,
						      elfcpp::SHF_ALLOC);

  Output_section_data* hashdata = new Output_data_const_buffer(phash,
							       hashlen,
							       align);
  hashsec->add_output_section_data(hashdata);

  hashsec->set_link_section(dynsym);
  hashsec->set_entsize(4);

  odyn->add_section_address(elfcpp::DT_HASH, hashsec);
}

// Create the version sections.

void
Layout::create_version_sections(const Target* target, const Versions* versions,
				unsigned int local_symcount,
				const std::vector<Symbol*>& dynamic_symbols,
				const Output_section* dynstr)
{
  if (!versions->any_defs() && !versions->any_needs())
    return;

  if (target->get_size() == 32)
    {
      if (target->is_big_endian())
	this->sized_create_version_sections SELECT_SIZE_ENDIAN_NAME(32, true)(
            versions, local_symcount, dynamic_symbols, dynstr
            SELECT_SIZE_ENDIAN(32, true));
      else
	this->sized_create_version_sections SELECT_SIZE_ENDIAN_NAME(32, false)(
            versions, local_symcount, dynamic_symbols, dynstr
            SELECT_SIZE_ENDIAN(32, false));
    }
  else if (target->get_size() == 64)
    {
      if (target->is_big_endian())
	this->sized_create_version_sections SELECT_SIZE_ENDIAN_NAME(64, true)(
            versions, local_symcount, dynamic_symbols, dynstr
            SELECT_SIZE_ENDIAN(64, true));
      else
	this->sized_create_version_sections SELECT_SIZE_ENDIAN_NAME(64, false)(
            versions, local_symcount, dynamic_symbols, dynstr
            SELECT_SIZE_ENDIAN(64, false));
    }
  else
    gold_unreachable();
}

// Create the version sections, sized version.

template<int size, bool big_endian>
void
Layout::sized_create_version_sections(
    const Versions* versions,
    unsigned int local_symcount,
    const std::vector<Symbol*>& dynamic_symbols,
    const Output_section* dynstr
    ACCEPT_SIZE_ENDIAN)
{
  const char* vname = this->namepool_.add(".gnu.version", NULL);
  Output_section* vsec = this->make_output_section(vname,
						   elfcpp::SHT_GNU_versym,
						   elfcpp::SHF_ALLOC);

  unsigned char* vbuf;
  unsigned int vsize;
  versions->symbol_section_contents SELECT_SIZE_ENDIAN_NAME(size, big_endian)(
      &this->dynpool_, local_symcount, dynamic_symbols, &vbuf, &vsize
      SELECT_SIZE_ENDIAN(size, big_endian));

  Output_section_data* vdata = new Output_data_const_buffer(vbuf, vsize, 2);

  vsec->add_output_section_data(vdata);
  vsec->set_entsize(2);
  vsec->set_link_section(this->dynsym_section_);

  Output_data_dynamic* const odyn = this->dynamic_data_;
  odyn->add_section_address(elfcpp::DT_VERSYM, vsec);

  if (versions->any_defs())
    {
      const char* vdname = this->namepool_.add(".gnu.version_d", NULL);
      Output_section *vdsec;
      vdsec = this->make_output_section(vdname, elfcpp::SHT_GNU_verdef,
					elfcpp::SHF_ALLOC);

      unsigned char* vdbuf;
      unsigned int vdsize;
      unsigned int vdentries;
      versions->def_section_contents SELECT_SIZE_ENDIAN_NAME(size, big_endian)(
          &this->dynpool_, &vdbuf, &vdsize, &vdentries
          SELECT_SIZE_ENDIAN(size, big_endian));

      Output_section_data* vddata = new Output_data_const_buffer(vdbuf,
								 vdsize,
								 4);

      vdsec->add_output_section_data(vddata);
      vdsec->set_link_section(dynstr);
      vdsec->set_info(vdentries);

      odyn->add_section_address(elfcpp::DT_VERDEF, vdsec);
      odyn->add_constant(elfcpp::DT_VERDEFNUM, vdentries);
    }

  if (versions->any_needs())
    {
      const char* vnname = this->namepool_.add(".gnu.version_r", NULL);
      Output_section* vnsec;
      vnsec = this->make_output_section(vnname, elfcpp::SHT_GNU_verneed,
					elfcpp::SHF_ALLOC);

      unsigned char* vnbuf;
      unsigned int vnsize;
      unsigned int vnentries;
      versions->need_section_contents SELECT_SIZE_ENDIAN_NAME(size, big_endian)
        (&this->dynpool_, &vnbuf, &vnsize, &vnentries
         SELECT_SIZE_ENDIAN(size, big_endian));

      Output_section_data* vndata = new Output_data_const_buffer(vnbuf,
								 vnsize,
								 4);

      vnsec->add_output_section_data(vndata);
      vnsec->set_link_section(dynstr);
      vnsec->set_info(vnentries);

      odyn->add_section_address(elfcpp::DT_VERNEED, vnsec);
      odyn->add_constant(elfcpp::DT_VERNEEDNUM, vnentries);
    }
}

// Create the .interp section and PT_INTERP segment.

void
Layout::create_interp(const Target* target)
{
  const char* interp = this->options_.dynamic_linker();
  if (interp == NULL)
    {
      interp = target->dynamic_linker();
      gold_assert(interp != NULL);
    }

  size_t len = strlen(interp) + 1;

  Output_section_data* odata = new Output_data_const(interp, len, 1);

  const char* interp_name = this->namepool_.add(".interp", NULL);
  Output_section* osec = this->make_output_section(interp_name,
						   elfcpp::SHT_PROGBITS,
						   elfcpp::SHF_ALLOC);
  osec->add_output_section_data(odata);

  Output_segment* oseg = new Output_segment(elfcpp::PT_INTERP, elfcpp::PF_R);
  this->segment_list_.push_back(oseg);
  oseg->add_initial_output_section(osec, elfcpp::PF_R);
}

// Finish the .dynamic section and PT_DYNAMIC segment.

void
Layout::finish_dynamic_section(const Input_objects* input_objects,
			       const Symbol_table* symtab)
{
  Output_segment* oseg = new Output_segment(elfcpp::PT_DYNAMIC,
					    elfcpp::PF_R | elfcpp::PF_W);
  this->segment_list_.push_back(oseg);
  oseg->add_initial_output_section(this->dynamic_section_,
				   elfcpp::PF_R | elfcpp::PF_W);

  Output_data_dynamic* const odyn = this->dynamic_data_;

  for (Input_objects::Dynobj_iterator p = input_objects->dynobj_begin();
       p != input_objects->dynobj_end();
       ++p)
    {
      // FIXME: Handle --as-needed.
      odyn->add_string(elfcpp::DT_NEEDED, (*p)->soname());
    }

  // FIXME: Support --init and --fini.
  Symbol* sym = symtab->lookup("_init");
  if (sym != NULL && sym->is_defined() && !sym->is_from_dynobj())
    odyn->add_symbol(elfcpp::DT_INIT, sym);

  sym = symtab->lookup("_fini");
  if (sym != NULL && sym->is_defined() && !sym->is_from_dynobj())
    odyn->add_symbol(elfcpp::DT_FINI, sym);

  // FIXME: Support DT_INIT_ARRAY and DT_FINI_ARRAY.
}

// The mapping of .gnu.linkonce section names to real section names.

#define MAPPING_INIT(f, t) { f, sizeof(f) - 1, t, sizeof(t) - 1 }
const Layout::Linkonce_mapping Layout::linkonce_mapping[] =
{
  MAPPING_INIT("d.rel.ro", ".data.rel.ro"),	// Must be before "d".
  MAPPING_INIT("t", ".text"),
  MAPPING_INIT("r", ".rodata"),
  MAPPING_INIT("d", ".data"),
  MAPPING_INIT("b", ".bss"),
  MAPPING_INIT("s", ".sdata"),
  MAPPING_INIT("sb", ".sbss"),
  MAPPING_INIT("s2", ".sdata2"),
  MAPPING_INIT("sb2", ".sbss2"),
  MAPPING_INIT("wi", ".debug_info"),
  MAPPING_INIT("td", ".tdata"),
  MAPPING_INIT("tb", ".tbss"),
  MAPPING_INIT("lr", ".lrodata"),
  MAPPING_INIT("l", ".ldata"),
  MAPPING_INIT("lb", ".lbss"),
};
#undef MAPPING_INIT

const int Layout::linkonce_mapping_count =
  sizeof(Layout::linkonce_mapping) / sizeof(Layout::linkonce_mapping[0]);

// Return the name of the output section to use for a .gnu.linkonce
// section.  This is based on the default ELF linker script of the old
// GNU linker.  For example, we map a name like ".gnu.linkonce.t.foo"
// to ".text".  Set *PLEN to the length of the name.  *PLEN is
// initialized to the length of NAME.

const char*
Layout::linkonce_output_name(const char* name, size_t *plen)
{
  const char* s = name + sizeof(".gnu.linkonce") - 1;
  if (*s != '.')
    return name;
  ++s;
  const Linkonce_mapping* plm = linkonce_mapping;
  for (int i = 0; i < linkonce_mapping_count; ++i, ++plm)
    {
      if (strncmp(s, plm->from, plm->fromlen) == 0 && s[plm->fromlen] == '.')
	{
	  *plen = plm->tolen;
	  return plm->to;
	}
    }
  return name;
}

// Choose the output section name to use given an input section name.
// Set *PLEN to the length of the name.  *PLEN is initialized to the
// length of NAME.

const char*
Layout::output_section_name(const char* name, size_t* plen)
{
  if (Layout::is_linkonce(name))
    {
      // .gnu.linkonce sections are laid out as though they were named
      // for the sections are placed into.
      return Layout::linkonce_output_name(name, plen);
    }

  // If the section name has no '.', or only an initial '.', we use
  // the name unchanged (i.e., ".text" is unchanged).

  // Otherwise, if the section name does not include ".rel", we drop
  // the last '.'  and everything that follows (i.e., ".text.XXX"
  // becomes ".text").

  // Otherwise, if the section name has zero or one '.' after the
  // ".rel", we use the name unchanged (i.e., ".rel.text" is
  // unchanged).

  // Otherwise, we drop the last '.' and everything that follows
  // (i.e., ".rel.text.XXX" becomes ".rel.text").

  const char* s = name;
  if (*s == '.')
    ++s;
  const char* sdot = strchr(s, '.');
  if (sdot == NULL)
    return name;

  const char* srel = strstr(s, ".rel");
  if (srel == NULL)
    {
      *plen = sdot - name;
      return name;
    }

  sdot = strchr(srel + 1, '.');
  if (sdot == NULL)
    return name;
  sdot = strchr(sdot + 1, '.');
  if (sdot == NULL)
    return name;

  *plen = sdot - name;
  return name;
}

// Record the signature of a comdat section, and return whether to
// include it in the link.  If GROUP is true, this is a regular
// section group.  If GROUP is false, this is a group signature
// derived from the name of a linkonce section.  We want linkonce
// signatures and group signatures to block each other, but we don't
// want a linkonce signature to block another linkonce signature.

bool
Layout::add_comdat(const char* signature, bool group)
{
  std::string sig(signature);
  std::pair<Signatures::iterator, bool> ins(
    this->signatures_.insert(std::make_pair(sig, group)));

  if (ins.second)
    {
      // This is the first time we've seen this signature.
      return true;
    }

  if (ins.first->second)
    {
      // We've already seen a real section group with this signature.
      return false;
    }
  else if (group)
    {
      // This is a real section group, and we've already seen a
      // linkonce section with tihs signature.  Record that we've seen
      // a section group, and don't include this section group.
      ins.first->second = true;
      return false;
    }
  else
    {
      // We've already seen a linkonce section and this is a linkonce
      // section.  These don't block each other--this may be the same
      // symbol name with different section types.
      return true;
    }
}

// Write out data not associated with a section or the symbol table.

void
Layout::write_data(const Symbol_table* symtab, const Target* target,
		   Output_file* of) const
{
  const Output_section* symtab_section = this->symtab_section_;
  for (Section_list::const_iterator p = this->section_list_.begin();
       p != this->section_list_.end();
       ++p)
    {
      if ((*p)->needs_symtab_index())
	{
	  gold_assert(symtab_section != NULL);
	  unsigned int index = (*p)->symtab_index();
	  gold_assert(index > 0 && index != -1U);
	  off_t off = (symtab_section->offset()
		       + index * symtab_section->entsize());
	  symtab->write_section_symbol(target, *p, of, off);
	}
    }

  const Output_section* dynsym_section = this->dynsym_section_;
  for (Section_list::const_iterator p = this->section_list_.begin();
       p != this->section_list_.end();
       ++p)
    {
      if ((*p)->needs_dynsym_index())
	{
	  gold_assert(dynsym_section != NULL);
	  unsigned int index = (*p)->dynsym_index();
	  gold_assert(index > 0 && index != -1U);
	  off_t off = (dynsym_section->offset()
		       + index * dynsym_section->entsize());
	  symtab->write_section_symbol(target, *p, of, off);
	}
    }

  // Write out the Output_sections.  Most won't have anything to
  // write, since most of the data will come from input sections which
  // are handled elsewhere.  But some Output_sections do have
  // Output_data.
  for (Section_list::const_iterator p = this->section_list_.begin();
       p != this->section_list_.end();
       ++p)
    (*p)->write(of);

  // Write out the Output_data which are not in an Output_section.
  for (Data_list::const_iterator p = this->special_output_list_.begin();
       p != this->special_output_list_.end();
       ++p)
    (*p)->write(of);
}

// Write_data_task methods.

// We can always run this task.

Task::Is_runnable_type
Write_data_task::is_runnable(Workqueue*)
{
  return IS_RUNNABLE;
}

// We need to unlock FINAL_BLOCKER when finished.

Task_locker*
Write_data_task::locks(Workqueue* workqueue)
{
  return new Task_locker_block(*this->final_blocker_, workqueue);
}

// Run the task--write out the data.

void
Write_data_task::run(Workqueue*)
{
  this->layout_->write_data(this->symtab_, this->target_, this->of_);
}

// Write_symbols_task methods.

// We can always run this task.

Task::Is_runnable_type
Write_symbols_task::is_runnable(Workqueue*)
{
  return IS_RUNNABLE;
}

// We need to unlock FINAL_BLOCKER when finished.

Task_locker*
Write_symbols_task::locks(Workqueue* workqueue)
{
  return new Task_locker_block(*this->final_blocker_, workqueue);
}

// Run the task--write out the symbols.

void
Write_symbols_task::run(Workqueue*)
{
  this->symtab_->write_globals(this->target_, this->sympool_, this->dynpool_,
			       this->of_);
}

// Close_task_runner methods.

// Run the task--close the file.

void
Close_task_runner::run(Workqueue*)
{
  this->of_->close();
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones for implemented targets.

template
Output_section*
Layout::layout<32, false>(Relobj* object, unsigned int shndx, const char* name,
			  const elfcpp::Shdr<32, false>& shdr, off_t*);

template
Output_section*
Layout::layout<32, true>(Relobj* object, unsigned int shndx, const char* name,
			 const elfcpp::Shdr<32, true>& shdr, off_t*);

template
Output_section*
Layout::layout<64, false>(Relobj* object, unsigned int shndx, const char* name,
			  const elfcpp::Shdr<64, false>& shdr, off_t*);

template
Output_section*
Layout::layout<64, true>(Relobj* object, unsigned int shndx, const char* name,
			 const elfcpp::Shdr<64, true>& shdr, off_t*);


} // End namespace gold.
