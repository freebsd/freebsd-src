// reloc.h -- relocate input files for gold   -*- C++ -*-

#ifndef GOLD_RELOC_H
#define GOLD_RELOC_H

#include <byteswap.h>

#include "workqueue.h"

namespace gold
{

class General_options;
class Relobj;
class Read_relocs_data;
class Symbol;
class Layout;

template<int size>
class Sized_symbol;

template<int size, bool big_endian>
class Sized_relobj;

template<int size>
class Symbol_value;

template<int sh_type, bool dynamic, int size, bool big_endian>
class Output_data_reloc;

// A class to read the relocations for an object file, and then queue
// up a task to see if they require any GOT/PLT/COPY relocations in
// the symbol table.

class Read_relocs : public Task
{
 public:
  // SYMTAB_LOCK is used to lock the symbol table.  BLOCKER should be
  // unblocked when the Scan_relocs task completes.
  Read_relocs(const General_options& options, Symbol_table* symtab,
	      Layout* layout, Relobj* object, Task_token* symtab_lock,
	      Task_token* blocker)
    : options_(options), symtab_(symtab), layout_(layout), object_(object),
      symtab_lock_(symtab_lock), blocker_(blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  const General_options& options_;
  Symbol_table* symtab_;
  Layout* layout_;
  Relobj* object_;
  Task_token* symtab_lock_;
  Task_token* blocker_;
};

// Scan the relocations for an object to see if they require any
// GOT/PLT/COPY relocations.

class Scan_relocs : public Task
{
 public:
  // SYMTAB_LOCK is used to lock the symbol table.  BLOCKER should be
  // unblocked when the task completes.
  Scan_relocs(const General_options& options, Symbol_table* symtab,
	      Layout* layout, Relobj* object, Read_relocs_data* rd,
	      Task_token* symtab_lock, Task_token* blocker)
    : options_(options), symtab_(symtab), layout_(layout), object_(object),
      rd_(rd), symtab_lock_(symtab_lock), blocker_(blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  class Scan_relocs_locker;

  const General_options& options_;
  Symbol_table* symtab_;
  Layout* layout_;
  Relobj* object_;
  Read_relocs_data* rd_;
  Task_token* symtab_lock_;
  Task_token* blocker_;
};

// A class to perform all the relocations for an object file.

class Relocate_task : public Task
{
 public:
  Relocate_task(const General_options& options, const Symbol_table* symtab,
		const Layout* layout, Relobj* object, Output_file* of,
		Task_token* final_blocker)
    : options_(options), symtab_(symtab), layout_(layout), object_(object),
      of_(of), final_blocker_(final_blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  class Relocate_locker;

  const General_options& options_;
  const Symbol_table* symtab_;
  const Layout* layout_;
  Relobj* object_;
  Output_file* of_;
  Task_token* final_blocker_;
};

// Standard relocation routines which are used on many targets.  Here
// SIZE and BIG_ENDIAN refer to the target, not the relocation type.

template<int size, bool big_endian>
class Relocate_functions
{
private:
  // Do a simple relocation with the addend in the section contents.
  // VALSIZE is the size of the value.
  template<int valsize>
  static inline void
  rel(unsigned char* view,
      typename elfcpp::Swap<valsize, big_endian>::Valtype value)
  {
    typedef typename elfcpp::Swap<valsize, big_endian>::Valtype Valtype;
    Valtype* wv = reinterpret_cast<Valtype*>(view);
    Valtype x = elfcpp::Swap<valsize, big_endian>::readval(wv);
    elfcpp::Swap<valsize, big_endian>::writeval(wv, x + value);
  }

  // Do a simple relocation using a Symbol_value with the addend in
  // the section contents.  VALSIZE is the size of the value to
  // relocate.
  template<int valsize>
  static inline void
  rel(unsigned char* view,
      const Sized_relobj<size, big_endian>* object,
      const Symbol_value<size>* psymval)
  {
    typedef typename elfcpp::Swap<valsize, big_endian>::Valtype Valtype;
    Valtype* wv = reinterpret_cast<Valtype*>(view);
    Valtype x = elfcpp::Swap<valsize, big_endian>::readval(wv);
    x = psymval->value(object, x);
    elfcpp::Swap<valsize, big_endian>::writeval(wv, x);
  }

  // Do a simple PC relative relocation with the addend in the section
  // contents.  VALSIZE is the size of the value.
  template<int valsize>
  static inline void
  pcrel(unsigned char* view,
	typename elfcpp::Swap<valsize, big_endian>::Valtype value,
	typename elfcpp::Elf_types<size>::Elf_Addr address)
  {
    typedef typename elfcpp::Swap<valsize, big_endian>::Valtype Valtype;
    Valtype* wv = reinterpret_cast<Valtype*>(view);
    Valtype x = elfcpp::Swap<valsize, big_endian>::readval(wv);
    elfcpp::Swap<valsize, big_endian>::writeval(wv, x + value - address);
  }

  // Do a simple PC relative relocation with a Symbol_value with the
  // addend in the section contents.  VALSIZE is the size of the
  // value.
  template<int valsize>
  static inline void
  pcrel(unsigned char* view,
	const Sized_relobj<size, big_endian>* object,
	const Symbol_value<size>* psymval,
	typename elfcpp::Elf_types<size>::Elf_Addr address)
  {
    typedef typename elfcpp::Swap<valsize, big_endian>::Valtype Valtype;
    Valtype* wv = reinterpret_cast<Valtype*>(view);
    Valtype x = elfcpp::Swap<valsize, big_endian>::readval(wv);
    x = psymval->value(object, x);
    elfcpp::Swap<valsize, big_endian>::writeval(wv, x - address);
  }

  typedef Relocate_functions<size, big_endian> This;

public:
  // Do a simple 8-bit REL relocation with the addend in the section
  // contents.
  static inline void
  rel8(unsigned char* view, unsigned char value)
  { This::template rel<8>(view, value); }

  static inline void
  rel8(unsigned char* view,
       const Sized_relobj<size, big_endian>* object,
       const Symbol_value<size>* psymval)
  { This::template rel<8>(view, object, psymval); }

  // Do a simple 8-bit PC relative relocation with the addend in the
  // section contents.
  static inline void
  pcrel8(unsigned char* view, unsigned char value,
	 typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<8>(view, value, address); }

  static inline void
  pcrel8(unsigned char* view,
	 const Sized_relobj<size, big_endian>* object,
	 const Symbol_value<size>* psymval,
	 typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<8>(view, object, psymval, address); }

  // Do a simple 16-bit REL relocation with the addend in the section
  // contents.
  static inline void
  rel16(unsigned char* view, elfcpp::Elf_Half value)
  { This::template rel<16>(view, value); }

  static inline void
  rel16(unsigned char* view,
	const Sized_relobj<size, big_endian>* object,
	const Symbol_value<size>* psymval)
  { This::template rel<16>(view, object, psymval); }

  // Do a simple 32-bit PC relative REL relocation with the addend in
  // the section contents.
  static inline void
  pcrel16(unsigned char* view, elfcpp::Elf_Word value,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<16>(view, value, address); }

  static inline void
  pcrel16(unsigned char* view,
	  const Sized_relobj<size, big_endian>* object,
	  const Symbol_value<size>* psymval,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<16>(view, object, psymval, address); }

  // Do a simple 32-bit REL relocation with the addend in the section
  // contents.
  static inline void
  rel32(unsigned char* view, elfcpp::Elf_Word value)
  { This::template rel<32>(view, value); }

  static inline void
  rel32(unsigned char* view,
	const Sized_relobj<size, big_endian>* object,
	const Symbol_value<size>* psymval)
  { This::template rel<32>(view, object, psymval); }

  // Do a simple 32-bit PC relative REL relocation with the addend in
  // the section contents.
  static inline void
  pcrel32(unsigned char* view, elfcpp::Elf_Word value,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<32>(view, value, address); }

  static inline void
  pcrel32(unsigned char* view,
	  const Sized_relobj<size, big_endian>* object,
	  const Symbol_value<size>* psymval,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<32>(view, object, psymval, address); }

  // Do a simple 64-bit REL relocation with the addend in the section
  // contents.
  static inline void
  rel64(unsigned char* view, elfcpp::Elf_Xword value)
  { This::template rel<64>(view, value); }

  static inline void
  rel64(unsigned char* view,
	const Sized_relobj<size, big_endian>* object,
	const Symbol_value<size>* psymval)
  { This::template rel<64>(view, object, psymval); }

  // Do a simple 64-bit PC relative REL relocation with the addend in
  // the section contents.
  static inline void
  pcrel64(unsigned char* view, elfcpp::Elf_Xword value,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<64>(view, value, address); }

  static inline void
  pcrel64(unsigned char* view,
	  const Sized_relobj<size, big_endian>* object,
	  const Symbol_value<size>* psymval,
	  typename elfcpp::Elf_types<size>::Elf_Addr address)
  { This::template pcrel<64>(view, object, psymval, address); }
};

// We try to avoid COPY relocations when possible.  A COPY relocation
// may be required when an executable refers to a variable defined in
// a shared library.  COPY relocations are problematic because they
// tie the executable to the exact size of the variable in the shared
// library.  We can avoid them if all the references to the variable
// are in a writeable section.  In that case we can simply use dynamic
// relocations.  However, when scanning relocs, we don't know when we
// see the relocation whether we will be forced to use a COPY
// relocation or not.  So we have to save the relocation during the
// reloc scanning, and then emit it as a dynamic relocation if
// necessary.  This class implements that.  It is used by the target
// specific code.

template<int size, bool big_endian>
class Copy_relocs
{
 public:
  Copy_relocs()
    : entries_()
  { }

  // Return whether we need a COPY reloc for a reloc against GSYM,
  // which is being applied to section SHNDX in OBJECT.
  static bool
  need_copy_reloc(const General_options*, Relobj* object, unsigned int shndx,
		  Sized_symbol<size>* gsym);

  // Save a Rel against SYM for possible emission later.  SHNDX is the
  // index of the section to which the reloc is being applied.
  void
  save(Symbol* sym, Relobj*, unsigned int shndx,
       const elfcpp::Rel<size, big_endian>&);

  // Save a Rela against SYM for possible emission later.
  void
  save(Symbol* sym, Relobj*, unsigned int shndx,
       const elfcpp::Rela<size, big_endian>&);

  // Return whether there are any relocs to emit.  This also discards
  // entries which need not be emitted.
  bool
  any_to_emit();

  // Emit relocs for each symbol which did not get a COPY reloc (i.e.,
  // is still defined in the dynamic object).
  template<int sh_type>
  void
  emit(Output_data_reloc<sh_type, true, size, big_endian>*);

 private:
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Address;
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Addend;

  // This POD class holds the entries we are saving.
  class Copy_reloc_entry
  {
   public:
    Copy_reloc_entry(Symbol* sym, unsigned int reloc_type,
		     Relobj* relobj, unsigned int shndx,
		     Address address, Addend addend)
      : sym_(sym), reloc_type_(reloc_type), relobj_(relobj),
	shndx_(shndx), address_(address), addend_(addend)
    { }

    // Return whether we should emit this reloc.  If we should not
    // emit, we clear it.
    bool
    should_emit();

    // Emit this reloc.

    void
    emit(Output_data_reloc<elfcpp::SHT_REL, true, size, big_endian>*);

    void
    emit(Output_data_reloc<elfcpp::SHT_RELA, true, size, big_endian>*);

   private:
    Symbol* sym_;
    unsigned int reloc_type_;
    Relobj* relobj_;
    unsigned int shndx_;
    Address address_;
    Addend addend_;
  };

  // A list of relocs to be saved.
  typedef std::vector<Copy_reloc_entry> Copy_reloc_entries;

  // The list of relocs we are saving.
  Copy_reloc_entries entries_;
};

} // End namespace gold.

#endif // !defined(GOLD_RELOC_H)
