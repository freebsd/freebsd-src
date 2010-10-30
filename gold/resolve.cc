// resolve.cc -- symbol resolution for gold

#include "gold.h"

#include "elfcpp.h"
#include "target.h"
#include "object.h"
#include "symtab.h"

namespace gold
{

// Symbol methods used in this file.

// Override the fields in Symbol.

template<int size, bool big_endian>
void
Symbol::override_base(const elfcpp::Sym<size, big_endian>& sym,
		      Object* object, const char* version)
{
  gold_assert(this->source_ == FROM_OBJECT);
  this->u_.from_object.object = object;
  if (version != NULL && this->version() != version)
    {
      gold_assert(this->version() == NULL);
      this->version_ = version;
    }
  // FIXME: Handle SHN_XINDEX.
  this->u_.from_object.shndx = sym.get_st_shndx();
  this->type_ = sym.get_st_type();
  this->binding_ = sym.get_st_bind();
  this->visibility_ = sym.get_st_visibility();
  this->nonvis_ = sym.get_st_nonvis();
}

// Override the fields in Sized_symbol.

template<int size>
template<bool big_endian>
void
Sized_symbol<size>::override(const elfcpp::Sym<size, big_endian>& sym,
			     Object* object, const char* version)
{
  this->override_base(sym, object, version);
  this->value_ = sym.get_st_value();
  this->symsize_ = sym.get_st_size();
}

// Resolve a symbol.  This is called the second and subsequent times
// we see a symbol.  TO is the pre-existing symbol.  SYM is the new
// symbol, seen in OBJECT.  VERSION of the version of SYM.

template<int size, bool big_endian>
void
Symbol_table::resolve(Sized_symbol<size>* to,
		      const elfcpp::Sym<size, big_endian>& sym,
		      Object* object, const char* version)
{
  if (object->target()->has_resolve())
    {
      Sized_target<size, big_endian>* sized_target;
      sized_target = object->sized_target
                     SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
                         SELECT_SIZE_ENDIAN_ONLY(size, big_endian));
      sized_target->resolve(to, sym, object, version);
      return;
    }

  // Build a little code for each symbol.
  // Bit 0: 0 for global, 1 for weak.
  // Bit 1: 0 for regular object, 1 for shared object
  // Bits 2-3: 0 for normal, 1 for undefined, 2 for common
  // This gives us values from 0 to 11:

  enum
  {
    DEF = 0,
    WEAK_DEF = 1,
    DYN_DEF = 2,
    DYN_WEAK_DEF = 3,
    UNDEF = 4,
    WEAK_UNDEF = 5,
    DYN_UNDEF = 6,
    DYN_WEAK_UNDEF = 7,
    COMMON = 8,
    WEAK_COMMON = 9,
    DYN_COMMON = 10,
    DYN_WEAK_COMMON = 11
  };

  int tobits;
  switch (to->binding())
    {
    case elfcpp::STB_GLOBAL:
      tobits = 0;
      break;

    case elfcpp::STB_WEAK:
      tobits = 1;
      break;

    case elfcpp::STB_LOCAL:
      // We should only see externally visible symbols in the symbol
      // table.
      gold_unreachable();

    default:
      // Any target which wants to handle STB_LOOS, etc., needs to
      // define a resolve method.
      gold_unreachable();
    }

  if (to->source() == Symbol::FROM_OBJECT
      && to->object()->is_dynamic())
    tobits |= (1 << 1);

  switch (to->shndx())
    {
    case elfcpp::SHN_UNDEF:
      tobits |= (1 << 2);
      break;

    case elfcpp::SHN_COMMON:
      tobits |= (2 << 2);
      break;

    default:
      if (to->type() == elfcpp::STT_COMMON)
	tobits |= (2 << 2);
      break;
    }

  int frombits;
  switch (sym.get_st_bind())
    {
    case elfcpp::STB_GLOBAL:
      frombits = 0;
      break;

    case elfcpp::STB_WEAK:
      frombits = 1;
      break;

    case elfcpp::STB_LOCAL:
      fprintf(stderr,
	      _("%s: %s: invalid STB_LOCAL symbol %s in external symbols\n"),
	      program_name, object->name().c_str(), to->name());
      gold_exit(false);

    default:
      fprintf(stderr,
	      _("%s: %s: unsupported symbol binding %d for symbol %s\n"),
	      program_name, object->name().c_str(),
	      static_cast<int>(sym.get_st_bind()), to->name());
      gold_exit(false);
    }

  if (!object->is_dynamic())
    {
      // Record that we've seen this symbol in a regular object.
      to->set_in_reg();
    }
  else
    {
      frombits |= (1 << 1);

      // Record that we've seen this symbol in a dynamic object.
      to->set_in_dyn();
    }

  switch (sym.get_st_shndx())
    {
    case elfcpp::SHN_UNDEF:
      frombits |= (1 << 2);
      break;

    case elfcpp::SHN_COMMON:
      frombits |= (2 << 2);
      break;

    default:
      if (sym.get_st_type() == elfcpp::STT_COMMON)
	frombits |= (2 << 2);
      break;
    }

  if ((tobits & (1 << 1)) != (frombits & (1 << 1)))
    {
      // This symbol is seen in both a dynamic object and a regular
      // object.  That means that we need the symbol to go into the
      // dynamic symbol table, so that the dynamic linker can use the
      // regular symbol to override or define the dynamic symbol.
      to->set_needs_dynsym_entry();
    }

  // FIXME: Warn if either but not both of TO and SYM are STT_TLS.

  // We use a giant switch table for symbol resolution.  This code is
  // unwieldy, but: 1) it is efficient; 2) we definitely handle all
  // cases; 3) it is easy to change the handling of a particular case.
  // The alternative would be a series of conditionals, but it is easy
  // to get the ordering wrong.  This could also be done as a table,
  // but that is no easier to understand than this large switch
  // statement.

  switch (tobits * 16 + frombits)
    {
    case DEF * 16 + DEF:
      // Two definitions of the same symbol.
      fprintf(stderr, "%s: %s: multiple definition of %s\n",
	      program_name, object->name().c_str(), to->name());
      // FIXME: Report locations.  Record that we have seen an error.
      return;

    case WEAK_DEF * 16 + DEF:
      // We've seen a weak definition, and now we see a strong
      // definition.  In the original SVR4 linker, this was treated as
      // a multiple definition error.  In the Solaris linker and the
      // GNU linker, a weak definition followed by a regular
      // definition causes the weak definition to be overridden.  We
      // are currently compatible with the GNU linker.  In the future
      // we should add a target specific option to change this.
      // FIXME.
      to->override(sym, object, version);
      return;

    case DYN_DEF * 16 + DEF:
    case DYN_WEAK_DEF * 16 + DEF:
      // We've seen a definition in a dynamic object, and now we see a
      // definition in a regular object.  The definition in the
      // regular object overrides the definition in the dynamic
      // object.
      to->override(sym, object, version);
      return;

    case UNDEF * 16 + DEF:
    case WEAK_UNDEF * 16 + DEF:
    case DYN_UNDEF * 16 + DEF:
    case DYN_WEAK_UNDEF * 16 + DEF:
      // We've seen an undefined reference, and now we see a
      // definition.  We use the definition.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + DEF:
    case WEAK_COMMON * 16 + DEF:
    case DYN_COMMON * 16 + DEF:
    case DYN_WEAK_COMMON * 16 + DEF:
      // We've seen a common symbol and now we see a definition.  The
      // definition overrides.  FIXME: We should optionally issue, version a
      // warning.
      to->override(sym, object, version);
      return;

    case DEF * 16 + WEAK_DEF:
    case WEAK_DEF * 16 + WEAK_DEF:
      // We've seen a definition and now we see a weak definition.  We
      // ignore the new weak definition.
      return;

    case DYN_DEF * 16 + WEAK_DEF:
    case DYN_WEAK_DEF * 16 + WEAK_DEF:
      // We've seen a dynamic definition and now we see a regular weak
      // definition.  The regular weak definition overrides.
      to->override(sym, object, version);
      return;

    case UNDEF * 16 + WEAK_DEF:
    case WEAK_UNDEF * 16 + WEAK_DEF:
    case DYN_UNDEF * 16 + WEAK_DEF:
    case DYN_WEAK_UNDEF * 16 + WEAK_DEF:
      // A weak definition of a currently undefined symbol.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + WEAK_DEF:
    case WEAK_COMMON * 16 + WEAK_DEF:
      // A weak definition does not override a common definition.
      return;

    case DYN_COMMON * 16 + WEAK_DEF:
    case DYN_WEAK_COMMON * 16 + WEAK_DEF:
      // A weak definition does override a definition in a dynamic
      // object.  FIXME: We should optionally issue a warning.
      to->override(sym, object, version);
      return;

    case DEF * 16 + DYN_DEF:
    case WEAK_DEF * 16 + DYN_DEF:
    case DYN_DEF * 16 + DYN_DEF:
    case DYN_WEAK_DEF * 16 + DYN_DEF:
      // Ignore a dynamic definition if we already have a definition.
      return;

    case UNDEF * 16 + DYN_DEF:
    case WEAK_UNDEF * 16 + DYN_DEF:
    case DYN_UNDEF * 16 + DYN_DEF:
    case DYN_WEAK_UNDEF * 16 + DYN_DEF:
      // Use a dynamic definition if we have a reference.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + DYN_DEF:
    case WEAK_COMMON * 16 + DYN_DEF:
    case DYN_COMMON * 16 + DYN_DEF:
    case DYN_WEAK_COMMON * 16 + DYN_DEF:
      // Ignore a dynamic definition if we already have a common
      // definition.
      return;

    case DEF * 16 + DYN_WEAK_DEF:
    case WEAK_DEF * 16 + DYN_WEAK_DEF:
    case DYN_DEF * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_DEF:
      // Ignore a weak dynamic definition if we already have a
      // definition.
      return;

    case UNDEF * 16 + DYN_WEAK_DEF:
    case WEAK_UNDEF * 16 + DYN_WEAK_DEF:
    case DYN_UNDEF * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_DEF:
      // Use a weak dynamic definition if we have a reference.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + DYN_WEAK_DEF:
    case WEAK_COMMON * 16 + DYN_WEAK_DEF:
    case DYN_COMMON * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_DEF:
      // Ignore a weak dynamic definition if we already have a common
      // definition.
      return;

    case DEF * 16 + UNDEF:
    case WEAK_DEF * 16 + UNDEF:
    case DYN_DEF * 16 + UNDEF:
    case DYN_WEAK_DEF * 16 + UNDEF:
    case UNDEF * 16 + UNDEF:
      // A new undefined reference tells us nothing.
      return;

    case WEAK_UNDEF * 16 + UNDEF:
    case DYN_UNDEF * 16 + UNDEF:
    case DYN_WEAK_UNDEF * 16 + UNDEF:
      // A strong undef overrides a dynamic or weak undef.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + UNDEF:
    case WEAK_COMMON * 16 + UNDEF:
    case DYN_COMMON * 16 + UNDEF:
    case DYN_WEAK_COMMON * 16 + UNDEF:
      // A new undefined reference tells us nothing.
      return;

    case DEF * 16 + WEAK_UNDEF:
    case WEAK_DEF * 16 + WEAK_UNDEF:
    case DYN_DEF * 16 + WEAK_UNDEF:
    case DYN_WEAK_DEF * 16 + WEAK_UNDEF:
    case UNDEF * 16 + WEAK_UNDEF:
    case WEAK_UNDEF * 16 + WEAK_UNDEF:
    case DYN_UNDEF * 16 + WEAK_UNDEF:
    case DYN_WEAK_UNDEF * 16 + WEAK_UNDEF:
    case COMMON * 16 + WEAK_UNDEF:
    case WEAK_COMMON * 16 + WEAK_UNDEF:
    case DYN_COMMON * 16 + WEAK_UNDEF:
    case DYN_WEAK_COMMON * 16 + WEAK_UNDEF:
      // A new weak undefined reference tells us nothing.
      return;

    case DEF * 16 + DYN_UNDEF:
    case WEAK_DEF * 16 + DYN_UNDEF:
    case DYN_DEF * 16 + DYN_UNDEF:
    case DYN_WEAK_DEF * 16 + DYN_UNDEF:
    case UNDEF * 16 + DYN_UNDEF:
    case WEAK_UNDEF * 16 + DYN_UNDEF:
    case DYN_UNDEF * 16 + DYN_UNDEF:
    case DYN_WEAK_UNDEF * 16 + DYN_UNDEF:
    case COMMON * 16 + DYN_UNDEF:
    case WEAK_COMMON * 16 + DYN_UNDEF:
    case DYN_COMMON * 16 + DYN_UNDEF:
    case DYN_WEAK_COMMON * 16 + DYN_UNDEF:
      // A new dynamic undefined reference tells us nothing.
      return;

    case DEF * 16 + DYN_WEAK_UNDEF:
    case WEAK_DEF * 16 + DYN_WEAK_UNDEF:
    case DYN_DEF * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_UNDEF:
    case UNDEF * 16 + DYN_WEAK_UNDEF:
    case WEAK_UNDEF * 16 + DYN_WEAK_UNDEF:
    case DYN_UNDEF * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_UNDEF:
    case COMMON * 16 + DYN_WEAK_UNDEF:
    case WEAK_COMMON * 16 + DYN_WEAK_UNDEF:
    case DYN_COMMON * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_UNDEF:
      // A new weak dynamic undefined reference tells us nothing.
      return;

    case DEF * 16 + COMMON:
      // A common symbol does not override a definition.
      return;

    case WEAK_DEF * 16 + COMMON:
    case DYN_DEF * 16 + COMMON:
    case DYN_WEAK_DEF * 16 + COMMON:
      // A common symbol does override a weak definition or a dynamic
      // definition.
      to->override(sym, object, version);
      return;

    case UNDEF * 16 + COMMON:
    case WEAK_UNDEF * 16 + COMMON:
    case DYN_UNDEF * 16 + COMMON:
    case DYN_WEAK_UNDEF * 16 + COMMON:
      // A common symbol is a definition for a reference.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + COMMON:
      // Set the size to the maximum.
      if (sym.get_st_size() > to->symsize())
	to->set_symsize(sym.get_st_size());
      return;

    case WEAK_COMMON * 16 + COMMON:
      // I'm not sure just what a weak common symbol means, but
      // presumably it can be overridden by a regular common symbol.
      to->override(sym, object, version);
      return;

    case DYN_COMMON * 16 + COMMON:
    case DYN_WEAK_COMMON * 16 + COMMON:
      {
	// Use the real common symbol, but adjust the size if necessary.
	typename Sized_symbol<size>::Size_type symsize = to->symsize();
	to->override(sym, object, version);
	if (to->symsize() < symsize)
	  to->set_symsize(symsize);
      }
      return;

    case DEF * 16 + WEAK_COMMON:
    case WEAK_DEF * 16 + WEAK_COMMON:
    case DYN_DEF * 16 + WEAK_COMMON:
    case DYN_WEAK_DEF * 16 + WEAK_COMMON:
      // Whatever a weak common symbol is, it won't override a
      // definition.
      return;

    case UNDEF * 16 + WEAK_COMMON:
    case WEAK_UNDEF * 16 + WEAK_COMMON:
    case DYN_UNDEF * 16 + WEAK_COMMON:
    case DYN_WEAK_UNDEF * 16 + WEAK_COMMON:
      // A weak common symbol is better than an undefined symbol.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + WEAK_COMMON:
    case WEAK_COMMON * 16 + WEAK_COMMON:
    case DYN_COMMON * 16 + WEAK_COMMON:
    case DYN_WEAK_COMMON * 16 + WEAK_COMMON:
      // Ignore a weak common symbol in the presence of a real common
      // symbol.
      return;

    case DEF * 16 + DYN_COMMON:
    case WEAK_DEF * 16 + DYN_COMMON:
    case DYN_DEF * 16 + DYN_COMMON:
    case DYN_WEAK_DEF * 16 + DYN_COMMON:
      // Ignore a dynamic common symbol in the presence of a
      // definition.
      return;

    case UNDEF * 16 + DYN_COMMON:
    case WEAK_UNDEF * 16 + DYN_COMMON:
    case DYN_UNDEF * 16 + DYN_COMMON:
    case DYN_WEAK_UNDEF * 16 + DYN_COMMON:
      // A dynamic common symbol is a definition of sorts.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + DYN_COMMON:
    case WEAK_COMMON * 16 + DYN_COMMON:
    case DYN_COMMON * 16 + DYN_COMMON:
    case DYN_WEAK_COMMON * 16 + DYN_COMMON:
      // Set the size to the maximum.
      if (sym.get_st_size() > to->symsize())
	to->set_symsize(sym.get_st_size());
      return;

    case DEF * 16 + DYN_WEAK_COMMON:
    case WEAK_DEF * 16 + DYN_WEAK_COMMON:
    case DYN_DEF * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_COMMON:
      // A common symbol is ignored in the face of a definition.
      return;

    case UNDEF * 16 + DYN_WEAK_COMMON:
    case WEAK_UNDEF * 16 + DYN_WEAK_COMMON:
    case DYN_UNDEF * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_COMMON:
      // I guess a weak common symbol is better than a definition.
      to->override(sym, object, version);
      return;

    case COMMON * 16 + DYN_WEAK_COMMON:
    case WEAK_COMMON * 16 + DYN_WEAK_COMMON:
    case DYN_COMMON * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_COMMON:
      // Set the size to the maximum.
      if (sym.get_st_size() > to->symsize())
	to->set_symsize(sym.get_st_size());
      return;

    default:
      gold_unreachable();
    }
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones needed for implemented
// targets.

template
void
Symbol_table::resolve<32, true>(
    Sized_symbol<32>* to,
    const elfcpp::Sym<32, true>& sym,
    Object* object,
    const char* version);

template
void
Symbol_table::resolve<32, false>(
    Sized_symbol<32>* to,
    const elfcpp::Sym<32, false>& sym,
    Object* object,
    const char* version);

template
void
Symbol_table::resolve<64, true>(
    Sized_symbol<64>* to,
    const elfcpp::Sym<64, true>& sym,
    Object* object,
    const char* version);

template
void
Symbol_table::resolve<64, false>(
    Sized_symbol<64>* to,
    const elfcpp::Sym<64, false>& sym,
    Object* object,
    const char* version);

} // End namespace gold.
