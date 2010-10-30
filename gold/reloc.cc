// reloc.cc -- relocate input files for gold.

#include "gold.h"

#include "workqueue.h"
#include "object.h"
#include "symtab.h"
#include "output.h"
#include "reloc.h"

namespace gold
{

// Read_relocs methods.

// These tasks just read the relocation information from the file.
// After reading it, the start another task to process the
// information.  These tasks requires access to the file.

Task::Is_runnable_type
Read_relocs::is_runnable(Workqueue*)
{
  return this->object_->is_locked() ? IS_LOCKED : IS_RUNNABLE;
}

// Lock the file.

Task_locker*
Read_relocs::locks(Workqueue*)
{
  return new Task_locker_obj<Object>(*this->object_);
}

// Read the relocations and then start a Scan_relocs_task.

void
Read_relocs::run(Workqueue* workqueue)
{
  Read_relocs_data *rd = new Read_relocs_data;
  this->object_->read_relocs(rd);
  workqueue->queue_front(new Scan_relocs(this->options_, this->symtab_,
					 this->layout_, this->object_, rd,
					 this->symtab_lock_, this->blocker_));
}

// Scan_relocs methods.

// These tasks scan the relocations read by Read_relocs and mark up
// the symbol table to indicate which relocations are required.  We
// use a lock on the symbol table to keep them from interfering with
// each other.

Task::Is_runnable_type
Scan_relocs::is_runnable(Workqueue*)
{
  if (!this->symtab_lock_->is_writable() || this->object_->is_locked())
    return IS_LOCKED;
  return IS_RUNNABLE;
}

// Return the locks we hold: one on the file, one on the symbol table
// and one blocker.

class Scan_relocs::Scan_relocs_locker : public Task_locker
{
 public:
  Scan_relocs_locker(Object* object, Task_token& symtab_lock, Task* task,
		     Task_token& blocker, Workqueue* workqueue)
    : objlock_(*object), symtab_locker_(symtab_lock, task),
      blocker_(blocker, workqueue)
  { }

 private:
  Task_locker_obj<Object> objlock_;
  Task_locker_write symtab_locker_;
  Task_locker_block blocker_;
};

Task_locker*
Scan_relocs::locks(Workqueue* workqueue)
{
  return new Scan_relocs_locker(this->object_, *this->symtab_lock_, this,
				*this->blocker_, workqueue);
}

// Scan the relocs.

void
Scan_relocs::run(Workqueue*)
{
  this->object_->scan_relocs(this->options_, this->symtab_, this->layout_,
			     this->rd_);
  delete this->rd_;
  this->rd_ = NULL;
}

// Relocate_task methods.

// These tasks are always runnable.

Task::Is_runnable_type
Relocate_task::is_runnable(Workqueue*)
{
  return IS_RUNNABLE;
}

// We want to lock the file while we run.  We want to unblock
// FINAL_BLOCKER when we are done.

class Relocate_task::Relocate_locker : public Task_locker
{
 public:
  Relocate_locker(Task_token& token, Workqueue* workqueue,
		  Object* object)
    : blocker_(token, workqueue), objlock_(*object)
  { }

 private:
  Task_locker_block blocker_;
  Task_locker_obj<Object> objlock_;
};

Task_locker*
Relocate_task::locks(Workqueue* workqueue)
{
  return new Relocate_locker(*this->final_blocker_, workqueue,
			     this->object_);
}

// Run the task.

void
Relocate_task::run(Workqueue*)
{
  this->object_->relocate(this->options_, this->symtab_, this->layout_,
			  this->of_);
}

// Read the relocs and local symbols from the object file and store
// the information in RD.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_read_relocs(Read_relocs_data* rd)
{
  rd->relocs.clear();

  unsigned int shnum = this->shnum();
  if (shnum == 0)
    return;

  rd->relocs.reserve(shnum / 2);

  const unsigned char *pshdrs = this->get_view(this->elf_file_.shoff(),
					       shnum * This::shdr_size);
  // Skip the first, dummy, section.
  const unsigned char *ps = pshdrs + This::shdr_size;
  for (unsigned int i = 1; i < shnum; ++i, ps += This::shdr_size)
    {
      typename This::Shdr shdr(ps);

      unsigned int sh_type = shdr.get_sh_type();
      if (sh_type != elfcpp::SHT_REL && sh_type != elfcpp::SHT_RELA)
	continue;

      unsigned int shndx = shdr.get_sh_info();
      if (shndx >= shnum)
	{
	  fprintf(stderr, _("%s: %s: relocation section %u has bad info %u\n"),
		  program_name, this->name().c_str(), i, shndx);
	  gold_exit(false);
	}

      if (!this->is_section_included(shndx))
	continue;

      // We are scanning relocations in order to fill out the GOT and
      // PLT sections.  Relocations for sections which are not
      // allocated (typically debugging sections) should not add new
      // GOT and PLT entries.  So we skip them.
      typename This::Shdr secshdr(pshdrs + shndx * This::shdr_size);
      if ((secshdr.get_sh_flags() & elfcpp::SHF_ALLOC) == 0)
	continue;

      if (shdr.get_sh_link() != this->symtab_shndx_)
	{
	  fprintf(stderr,
		  _("%s: %s: relocation section %u uses unexpected "
		    "symbol table %u\n"),
		  program_name, this->name().c_str(), i, shdr.get_sh_link());
	  gold_exit(false);
	}

      off_t sh_size = shdr.get_sh_size();

      unsigned int reloc_size;
      if (sh_type == elfcpp::SHT_REL)
	reloc_size = elfcpp::Elf_sizes<size>::rel_size;
      else
	reloc_size = elfcpp::Elf_sizes<size>::rela_size;
      if (reloc_size != shdr.get_sh_entsize())
	{
	  fprintf(stderr,
		  _("%s: %s: unexpected entsize for reloc section %u: "
		    "%lu != %u"),
		  program_name, this->name().c_str(), i,
		  static_cast<unsigned long>(shdr.get_sh_entsize()),
		  reloc_size);
	  gold_exit(false);
	}

      size_t reloc_count = sh_size / reloc_size;
      if (reloc_count * reloc_size != sh_size)
	{
	  fprintf(stderr, _("%s: %s: reloc section %u size %lu uneven"),
		  program_name, this->name().c_str(), i,
		  static_cast<unsigned long>(sh_size));
	  gold_exit(false);
	}

      rd->relocs.push_back(Section_relocs());
      Section_relocs& sr(rd->relocs.back());
      sr.reloc_shndx = i;
      sr.data_shndx = shndx;
      sr.contents = this->get_lasting_view(shdr.get_sh_offset(), sh_size);
      sr.sh_type = sh_type;
      sr.reloc_count = reloc_count;
    }

  // Read the local symbols.
  gold_assert(this->symtab_shndx_ != -1U);
  if (this->symtab_shndx_ == 0 || this->local_symbol_count_ == 0)
    rd->local_symbols = NULL;
  else
    {
      typename This::Shdr symtabshdr(pshdrs
				     + this->symtab_shndx_ * This::shdr_size);
      gold_assert(symtabshdr.get_sh_type() == elfcpp::SHT_SYMTAB);
      const int sym_size = This::sym_size;
      const unsigned int loccount = this->local_symbol_count_;
      gold_assert(loccount == symtabshdr.get_sh_info());
      off_t locsize = loccount * sym_size;
      rd->local_symbols = this->get_lasting_view(symtabshdr.get_sh_offset(),
						 locsize);
    }
}

// Scan the relocs and adjust the symbol table.  This looks for
// relocations which require GOT/PLT/COPY relocations.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_scan_relocs(const General_options& options,
					       Symbol_table* symtab,
					       Layout* layout,
					       Read_relocs_data* rd)
{
  Sized_target<size, big_endian>* target = this->sized_target();

  const unsigned char* local_symbols;
  if (rd->local_symbols == NULL)
    local_symbols = NULL;
  else
    local_symbols = rd->local_symbols->data();

  for (Read_relocs_data::Relocs_list::iterator p = rd->relocs.begin();
       p != rd->relocs.end();
       ++p)
    {
      target->scan_relocs(options, symtab, layout, this, p->data_shndx,
			  p->sh_type, p->contents->data(), p->reloc_count,
			  this->local_symbol_count_,
			  local_symbols,
			  this->symbols_);
      delete p->contents;
      p->contents = NULL;
    }

  if (rd->local_symbols != NULL)
    {
      delete rd->local_symbols;
      rd->local_symbols = NULL;
    }
}

// Relocate the input sections and write out the local symbols.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::do_relocate(const General_options& options,
					    const Symbol_table* symtab,
					    const Layout* layout,
					    Output_file* of)
{
  unsigned int shnum = this->shnum();

  // Read the section headers.
  const unsigned char* pshdrs = this->get_view(this->elf_file_.shoff(),
					       shnum * This::shdr_size);

  Views views;
  views.resize(shnum);

  // Make two passes over the sections.  The first one copies the
  // section data to the output file.  The second one applies
  // relocations.

  this->write_sections(pshdrs, of, &views);

  // Apply relocations.

  this->relocate_sections(options, symtab, layout, pshdrs, &views);

  // Write out the accumulated views.
  for (unsigned int i = 1; i < shnum; ++i)
    {
      if (views[i].view != NULL)
	of->write_output_view(views[i].offset, views[i].view_size,
			      views[i].view);
    }

  // Write out the local symbols.
  this->write_local_symbols(of, layout->sympool());
}

// Write section data to the output file.  PSHDRS points to the
// section headers.  Record the views in *PVIEWS for use when
// relocating.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::write_sections(const unsigned char* pshdrs,
					       Output_file* of,
					       Views* pviews)
{
  unsigned int shnum = this->shnum();
  std::vector<Map_to_output>& map_sections(this->map_to_output());

  const unsigned char* p = pshdrs + This::shdr_size;
  for (unsigned int i = 1; i < shnum; ++i, p += This::shdr_size)
    {
      View_size* pvs = &(*pviews)[i];

      pvs->view = NULL;

      if (map_sections[i].offset == -1)
	continue;

      const Output_section* os = map_sections[i].output_section;
      if (os == NULL)
	continue;

      typename This::Shdr shdr(p);

      if (shdr.get_sh_type() == elfcpp::SHT_NOBITS)
	continue;

      off_t start = os->offset() + map_sections[i].offset;
      off_t sh_size = shdr.get_sh_size();

      if (sh_size == 0)
	continue;

      gold_assert(map_sections[i].offset >= 0
		  && map_sections[i].offset + sh_size <= os->data_size());

      unsigned char* view = of->get_output_view(start, sh_size);
      this->read(shdr.get_sh_offset(), sh_size, view);

      pvs->view = view;
      pvs->address = os->address() + map_sections[i].offset;
      pvs->offset = start;
      pvs->view_size = sh_size;
    }
}

// Relocate section data.  VIEWS points to the section data as views
// in the output file.

template<int size, bool big_endian>
void
Sized_relobj<size, big_endian>::relocate_sections(
    const General_options& options,
    const Symbol_table* symtab,
    const Layout* layout,
    const unsigned char* pshdrs,
    Views* pviews)
{
  unsigned int shnum = this->shnum();
  Sized_target<size, big_endian>* target = this->sized_target();

  Relocate_info<size, big_endian> relinfo;
  relinfo.options = &options;
  relinfo.symtab = symtab;
  relinfo.layout = layout;
  relinfo.object = this;
  relinfo.local_symbol_count = this->local_symbol_count_;
  relinfo.local_values = &this->local_values_;
  relinfo.symbols = this->symbols_;

  const unsigned char* p = pshdrs + This::shdr_size;
  for (unsigned int i = 1; i < shnum; ++i, p += This::shdr_size)
    {
      typename This::Shdr shdr(p);

      unsigned int sh_type = shdr.get_sh_type();
      if (sh_type != elfcpp::SHT_REL && sh_type != elfcpp::SHT_RELA)
	continue;

      unsigned int index = shdr.get_sh_info();
      if (index >= this->shnum())
	{
	  fprintf(stderr, _("%s: %s: relocation section %u has bad info %u\n"),
		  program_name, this->name().c_str(), i, index);
	  gold_exit(false);
	}

      if (!this->is_section_included(index))
	{
	  // This relocation section is against a section which we
	  // discarded.
	  continue;
	}

      gold_assert((*pviews)[index].view != NULL);

      if (shdr.get_sh_link() != this->symtab_shndx_)
	{
	  fprintf(stderr,
		  _("%s: %s: relocation section %u uses unexpected "
		    "symbol table %u\n"),
		  program_name, this->name().c_str(), i, shdr.get_sh_link());
	  gold_exit(false);
	}

      off_t sh_size = shdr.get_sh_size();
      const unsigned char* prelocs = this->get_view(shdr.get_sh_offset(),
						    sh_size);

      unsigned int reloc_size;
      if (sh_type == elfcpp::SHT_REL)
	reloc_size = elfcpp::Elf_sizes<size>::rel_size;
      else
	reloc_size = elfcpp::Elf_sizes<size>::rela_size;

      if (reloc_size != shdr.get_sh_entsize())
	{
	  fprintf(stderr,
		  _("%s: %s: unexpected entsize for reloc section %u: "
		    "%lu != %u"),
		  program_name, this->name().c_str(), i,
		  static_cast<unsigned long>(shdr.get_sh_entsize()),
		  reloc_size);
	  gold_exit(false);
	}

      size_t reloc_count = sh_size / reloc_size;
      if (reloc_count * reloc_size != sh_size)
	{
	  fprintf(stderr, _("%s: %s: reloc section %u size %lu uneven"),
		  program_name, this->name().c_str(), i,
		  static_cast<unsigned long>(sh_size));
	  gold_exit(false);
	}

      relinfo.reloc_shndx = i;
      relinfo.data_shndx = index;
      target->relocate_section(&relinfo,
			       sh_type,
			       prelocs,
			       reloc_count,
			       (*pviews)[index].view,
			       (*pviews)[index].address,
			       (*pviews)[index].view_size);
    }
}

// Copy_relocs::Copy_reloc_entry methods.

// Return whether we should emit this reloc.  We should emit it if the
// symbol is still defined in a dynamic object.  If we should not emit
// it, we clear it, to save ourselves the test next time.

template<int size, bool big_endian>
bool
Copy_relocs<size, big_endian>::Copy_reloc_entry::should_emit()
{
  if (this->sym_ == NULL)
    return false;
  if (this->sym_->is_from_dynobj())
    return true;
  this->sym_ = NULL;
  return false;
}

// Emit a reloc into a SHT_REL section.

template<int size, bool big_endian>
void
Copy_relocs<size, big_endian>::Copy_reloc_entry::emit(
    Output_data_reloc<elfcpp::SHT_REL, true, size, big_endian>* reloc_data)
{
  this->sym_->set_needs_dynsym_entry();
  reloc_data->add_global(this->sym_, this->reloc_type_, this->relobj_,
			 this->shndx_, this->address_);
}

// Emit a reloc into a SHT_RELA section.

template<int size, bool big_endian>
void
Copy_relocs<size, big_endian>::Copy_reloc_entry::emit(
    Output_data_reloc<elfcpp::SHT_RELA, true, size, big_endian>* reloc_data)
{
  this->sym_->set_needs_dynsym_entry();
  reloc_data->add_global(this->sym_, this->reloc_type_, this->relobj_,
			 this->shndx_, this->address_, this->addend_);
}

// Copy_relocs methods.

// Return whether we need a COPY reloc for a relocation against GSYM.
// The relocation is being applied to section SHNDX in OBJECT.

template<int size, bool big_endian>
bool
Copy_relocs<size, big_endian>::need_copy_reloc(
    const General_options*,
    Relobj* object,
    unsigned int shndx,
    Sized_symbol<size>* sym)
{
  // FIXME: Handle -z nocopyrelocs.

  if (sym->symsize() == 0)
    return false;

  // If this is a readonly section, then we need a COPY reloc.
  // Otherwise we can use a dynamic reloc.
  if ((object->section_flags(shndx) & elfcpp::SHF_WRITE) == 0)
    return true;

  return false;
}

// Save a Rel reloc.

template<int size, bool big_endian>
void
Copy_relocs<size, big_endian>::save(
    Symbol* sym,
    Relobj* relobj,
    unsigned int shndx,
    const elfcpp::Rel<size, big_endian>& rel)
{
  unsigned int reloc_type = elfcpp::elf_r_type<size>(rel.get_r_info());
  this->entries_.push_back(Copy_reloc_entry(sym, reloc_type, relobj, shndx,
					    rel.get_r_offset(), 0));
}

// Save a Rela reloc.

template<int size, bool big_endian>
void
Copy_relocs<size, big_endian>::save(
    Symbol* sym,
    Relobj* relobj,
    unsigned int shndx,
    const elfcpp::Rela<size, big_endian>& rela)
{
  unsigned int reloc_type = elfcpp::elf_r_type<size>(rela.get_r_info());
  this->entries_.push_back(Copy_reloc_entry(sym, reloc_type, relobj, shndx,
					    rela.get_r_offset(),
					    rela.get_r_addend()));
}

// Return whether there are any relocs to emit.  We don't want to emit
// a reloc if the symbol is no longer defined in a dynamic object.

template<int size, bool big_endian>
bool
Copy_relocs<size, big_endian>::any_to_emit()
{
  for (typename Copy_reloc_entries::iterator p = this->entries_.begin();
       p != this->entries_.end();
       ++p)
    {
      if (p->should_emit())
	return true;
    }
  return false;
}

// Emit relocs.

template<int size, bool big_endian>
template<int sh_type>
void
Copy_relocs<size, big_endian>::emit(
    Output_data_reloc<sh_type, true, size, big_endian>* reloc_data)
{
  for (typename Copy_reloc_entries::iterator p = this->entries_.begin();
       p != this->entries_.end();
       ++p)
    {
      if (p->should_emit())
	p->emit(reloc_data);
    }
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones for implemented targets.

template
void
Sized_relobj<32, false>::do_read_relocs(Read_relocs_data* rd);

template
void
Sized_relobj<32, true>::do_read_relocs(Read_relocs_data* rd);

template
void
Sized_relobj<64, false>::do_read_relocs(Read_relocs_data* rd);

template
void
Sized_relobj<64, true>::do_read_relocs(Read_relocs_data* rd);

template
void
Sized_relobj<32, false>::do_scan_relocs(const General_options& options,
					Symbol_table* symtab,
					Layout* layout,
					Read_relocs_data* rd);

template
void
Sized_relobj<32, true>::do_scan_relocs(const General_options& options,
				       Symbol_table* symtab,
				       Layout* layout,
				       Read_relocs_data* rd);

template
void
Sized_relobj<64, false>::do_scan_relocs(const General_options& options,
					Symbol_table* symtab,
					Layout* layout,
					Read_relocs_data* rd);

template
void
Sized_relobj<64, true>::do_scan_relocs(const General_options& options,
				       Symbol_table* symtab,
				       Layout* layout,
				       Read_relocs_data* rd);

template
void
Sized_relobj<32, false>::do_relocate(const General_options& options,
				     const Symbol_table* symtab,
				     const Layout* layout,
				     Output_file* of);

template
void
Sized_relobj<32, true>::do_relocate(const General_options& options,
				    const Symbol_table* symtab,
				    const Layout* layout,
				    Output_file* of);

template
void
Sized_relobj<64, false>::do_relocate(const General_options& options,
				     const Symbol_table* symtab,
				     const Layout* layout,
				     Output_file* of);

template
void
Sized_relobj<64, true>::do_relocate(const General_options& options,
				    const Symbol_table* symtab,
				    const Layout* layout,
				    Output_file* of);

template
class Copy_relocs<32, false>;

template
class Copy_relocs<32, true>;

template
class Copy_relocs<64, false>;

template
class Copy_relocs<64, true>;

template
void
Copy_relocs<32, false>::emit<elfcpp::SHT_REL>(
    Output_data_reloc<elfcpp::SHT_REL, true, 32, false>*);

template
void
Copy_relocs<32, true>::emit<elfcpp::SHT_REL>(
    Output_data_reloc<elfcpp::SHT_REL, true, 32, true>*);

template
void
Copy_relocs<64, false>::emit<elfcpp::SHT_REL>(
    Output_data_reloc<elfcpp::SHT_REL, true, 64, false>*);

template
void
Copy_relocs<64, true>::emit<elfcpp::SHT_REL>(
    Output_data_reloc<elfcpp::SHT_REL, true, 64, true>*);

template
void
Copy_relocs<32, false>::emit<elfcpp::SHT_RELA>(
    Output_data_reloc<elfcpp::SHT_RELA , true, 32, false>*);

template
void
Copy_relocs<32, true>::emit<elfcpp::SHT_RELA>(
    Output_data_reloc<elfcpp::SHT_RELA, true, 32, true>*);

template
void
Copy_relocs<64, false>::emit<elfcpp::SHT_RELA>(
    Output_data_reloc<elfcpp::SHT_RELA, true, 64, false>*);

template
void
Copy_relocs<64, true>::emit<elfcpp::SHT_RELA>(
    Output_data_reloc<elfcpp::SHT_RELA, true, 64, true>*);

} // End namespace gold.
