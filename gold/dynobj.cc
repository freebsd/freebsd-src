// dynobj.cc -- dynamic object support for gold

#include "gold.h"

#include <vector>
#include <cstring>

#include "elfcpp.h"
#include "symtab.h"
#include "dynobj.h"

namespace gold
{

// Class Dynobj.

// Return the string to use in a DT_NEEDED entry.

const char*
Dynobj::soname() const
{
  if (!this->soname_.empty())
    return this->soname_.c_str();
  return this->name().c_str();
}

// Class Sized_dynobj.

template<int size, bool big_endian>
Sized_dynobj<size, big_endian>::Sized_dynobj(
    const std::string& name,
    Input_file* input_file,
    off_t offset,
    const elfcpp::Ehdr<size, big_endian>& ehdr)
  : Dynobj(name, input_file, offset),
    elf_file_(this, ehdr)
{
}

// Set up the object.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::setup(
    const elfcpp::Ehdr<size, big_endian>& ehdr)
{
  this->set_target(ehdr.get_e_machine(), size, big_endian,
		   ehdr.get_e_ident()[elfcpp::EI_OSABI],
		   ehdr.get_e_ident()[elfcpp::EI_ABIVERSION]);

  const unsigned int shnum = this->elf_file_.shnum();
  this->set_shnum(shnum);
}

// Find the SHT_DYNSYM section and the various version sections, and
// the dynamic section, given the section headers.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::find_dynsym_sections(
    const unsigned char* pshdrs,
    unsigned int* pdynsym_shndx,
    unsigned int* pversym_shndx,
    unsigned int* pverdef_shndx,
    unsigned int* pverneed_shndx,
    unsigned int* pdynamic_shndx)
{
  *pdynsym_shndx = -1U;
  *pversym_shndx = -1U;
  *pverdef_shndx = -1U;
  *pverneed_shndx = -1U;
  *pdynamic_shndx = -1U;

  const unsigned int shnum = this->shnum();
  const unsigned char* p = pshdrs;
  for (unsigned int i = 0; i < shnum; ++i, p += This::shdr_size)
    {
      typename This::Shdr shdr(p);

      unsigned int* pi;
      switch (shdr.get_sh_type())
	{
	case elfcpp::SHT_DYNSYM:
	  pi = pdynsym_shndx;
	  break;
	case elfcpp::SHT_GNU_versym:
	  pi = pversym_shndx;
	  break;
	case elfcpp::SHT_GNU_verdef:
	  pi = pverdef_shndx;
	  break;
	case elfcpp::SHT_GNU_verneed:
	  pi = pverneed_shndx;
	  break;
	case elfcpp::SHT_DYNAMIC:
	  pi = pdynamic_shndx;
	  break;
	default:
	  pi = NULL;
	  break;
	}

      if (pi == NULL)
	continue;

      if (*pi != -1U)
	{
	  fprintf(stderr,
		  _("%s: %s: unexpected duplicate type %u section: %u, %u\n"),
		  program_name, this->name().c_str(), shdr.get_sh_type(),
		  *pi, i);
	  gold_exit(false);
	}

      *pi = i;
    }
}

// Read the contents of section SHNDX.  PSHDRS points to the section
// headers.  TYPE is the expected section type.  LINK is the expected
// section link.  Store the data in *VIEW and *VIEW_SIZE.  The
// section's sh_info field is stored in *VIEW_INFO.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::read_dynsym_section(
    const unsigned char* pshdrs,
    unsigned int shndx,
    elfcpp::SHT type,
    unsigned int link,
    File_view** view,
    off_t* view_size,
    unsigned int* view_info)
{
  if (shndx == -1U)
    {
      *view = NULL;
      *view_size = 0;
      *view_info = 0;
      return;
    }

  typename This::Shdr shdr(pshdrs + shndx * This::shdr_size);

  gold_assert(shdr.get_sh_type() == type);

  if (shdr.get_sh_link() != link)
    {
      fprintf(stderr,
	      _("%s: %s: unexpected link in section %u header: %u != %u\n"),
	      program_name, this->name().c_str(), shndx,
	      shdr.get_sh_link(), link);
      gold_exit(false);
    }

  *view = this->get_lasting_view(shdr.get_sh_offset(), shdr.get_sh_size());
  *view_size = shdr.get_sh_size();
  *view_info = shdr.get_sh_info();
}

// Set the soname field if this shared object has a DT_SONAME tag.
// PSHDRS points to the section headers.  DYNAMIC_SHNDX is the section
// index of the SHT_DYNAMIC section.  STRTAB_SHNDX, STRTAB, and
// STRTAB_SIZE are the section index and contents of a string table
// which may be the one associated with the SHT_DYNAMIC section.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::set_soname(const unsigned char* pshdrs,
					   unsigned int dynamic_shndx,
					   unsigned int strtab_shndx,
					   const unsigned char* strtabu,
					   off_t strtab_size)
{
  typename This::Shdr dynamicshdr(pshdrs + dynamic_shndx * This::shdr_size);
  gold_assert(dynamicshdr.get_sh_type() == elfcpp::SHT_DYNAMIC);

  const off_t dynamic_size = dynamicshdr.get_sh_size();
  const unsigned char* pdynamic = this->get_view(dynamicshdr.get_sh_offset(),
						 dynamic_size);

  const unsigned int link = dynamicshdr.get_sh_link();
  if (link != strtab_shndx)
    {
      if (link >= this->shnum())
	{
	  fprintf(stderr,
		  _("%s: %s: DYNAMIC section %u link out of range: %u\n"),
		  program_name, this->name().c_str(),
		  dynamic_shndx, link);
	  gold_exit(false);
	}

      typename This::Shdr strtabshdr(pshdrs + link * This::shdr_size);
      if (strtabshdr.get_sh_type() != elfcpp::SHT_STRTAB)
	{
	  fprintf(stderr,
		  _("%s: %s: DYNAMIC section %u link %u is not a strtab\n"),
		  program_name, this->name().c_str(),
		  dynamic_shndx, link);
	  gold_exit(false);
	}

      strtab_size = strtabshdr.get_sh_size();
      strtabu = this->get_view(strtabshdr.get_sh_offset(), strtab_size);
    }

  for (const unsigned char* p = pdynamic;
       p < pdynamic + dynamic_size;
       p += This::dyn_size)
    {
      typename This::Dyn dyn(p);

      if (dyn.get_d_tag() == elfcpp::DT_SONAME)
	{
	  off_t val = dyn.get_d_val();
	  if (val >= strtab_size)
	    {
	      fprintf(stderr,
		      _("%s: %s: DT_SONAME value out of range: "
			"%lld >= %lld\n"),
		      program_name, this->name().c_str(),
		      static_cast<long long>(val),
		      static_cast<long long>(strtab_size));
	      gold_exit(false);
	    }

	  const char* strtab = reinterpret_cast<const char*>(strtabu);
	  this->set_soname_string(strtab + val);
	  return;
	}

      if (dyn.get_d_tag() == elfcpp::DT_NULL)
	return;
    }

  fprintf(stderr, _("%s: %s: missing DT_NULL in dynamic segment\n"),
	  program_name, this->name().c_str());
  gold_exit(false);
}

// Read the symbols and sections from a dynamic object.  We read the
// dynamic symbols, not the normal symbols.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::do_read_symbols(Read_symbols_data* sd)
{
  this->read_section_data(&this->elf_file_, sd);

  const unsigned char* const pshdrs = sd->section_headers->data();

  unsigned int dynsym_shndx;
  unsigned int versym_shndx;
  unsigned int verdef_shndx;
  unsigned int verneed_shndx;
  unsigned int dynamic_shndx;
  this->find_dynsym_sections(pshdrs, &dynsym_shndx, &versym_shndx,
			     &verdef_shndx, &verneed_shndx, &dynamic_shndx);

  unsigned int strtab_shndx = -1U;

  if (dynsym_shndx == -1U)
    {
      sd->symbols = NULL;
      sd->symbols_size = 0;
      sd->symbol_names = NULL;
      sd->symbol_names_size = 0;
    }
  else
    {
      // Get the dynamic symbols.
      typename This::Shdr dynsymshdr(pshdrs + dynsym_shndx * This::shdr_size);
      gold_assert(dynsymshdr.get_sh_type() == elfcpp::SHT_DYNSYM);

      sd->symbols = this->get_lasting_view(dynsymshdr.get_sh_offset(),
					   dynsymshdr.get_sh_size());
      sd->symbols_size = dynsymshdr.get_sh_size();

      // Get the symbol names.
      strtab_shndx = dynsymshdr.get_sh_link();
      if (strtab_shndx >= this->shnum())
	{
	  fprintf(stderr,
		  _("%s: %s: invalid dynamic symbol table name index: %u\n"),
		  program_name, this->name().c_str(), strtab_shndx);
	  gold_exit(false);
	}
      typename This::Shdr strtabshdr(pshdrs + strtab_shndx * This::shdr_size);
      if (strtabshdr.get_sh_type() != elfcpp::SHT_STRTAB)
	{
	  fprintf(stderr,
		  _("%s: %s: dynamic symbol table name section "
		    "has wrong type: %u\n"),
		  program_name, this->name().c_str(),
		  static_cast<unsigned int>(strtabshdr.get_sh_type()));
	  gold_exit(false);
	}

      sd->symbol_names = this->get_lasting_view(strtabshdr.get_sh_offset(),
						strtabshdr.get_sh_size());
      sd->symbol_names_size = strtabshdr.get_sh_size();

      // Get the version information.

      unsigned int dummy;
      this->read_dynsym_section(pshdrs, versym_shndx, elfcpp::SHT_GNU_versym,
				dynsym_shndx, &sd->versym, &sd->versym_size,
				&dummy);

      // We require that the version definition and need section link
      // to the same string table as the dynamic symbol table.  This
      // is not a technical requirement, but it always happens in
      // practice.  We could change this if necessary.

      this->read_dynsym_section(pshdrs, verdef_shndx, elfcpp::SHT_GNU_verdef,
				strtab_shndx, &sd->verdef, &sd->verdef_size,
				&sd->verdef_info);

      this->read_dynsym_section(pshdrs, verneed_shndx, elfcpp::SHT_GNU_verneed,
				strtab_shndx, &sd->verneed, &sd->verneed_size,
				&sd->verneed_info);
    }

  // Read the SHT_DYNAMIC section to find whether this shared object
  // has a DT_SONAME tag.  This doesn't really have anything to do
  // with reading the symbols, but this is a convenient place to do
  // it.
  if (dynamic_shndx != -1U)
    this->set_soname(pshdrs, dynamic_shndx, strtab_shndx,
		     (sd->symbol_names == NULL
		      ? NULL
		      : sd->symbol_names->data()),
		     sd->symbol_names_size);
}

// Lay out the input sections for a dynamic object.  We don't want to
// include sections from a dynamic object, so all that we actually do
// here is check for .gnu.warning sections.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::do_layout(const General_options&,
					  Symbol_table* symtab,
					  Layout*,
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

      this->handle_gnu_warning_section(name, i, symtab);
    }

  delete sd->section_headers;
  sd->section_headers = NULL;
  delete sd->section_names;
  sd->section_names = NULL;
}

// Add an entry to the vector mapping version numbers to version
// strings.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::set_version_map(
    Version_map* version_map,
    unsigned int ndx,
    const char* name) const
{
  if (ndx >= version_map->size())
    version_map->resize(ndx + 1);
  if ((*version_map)[ndx] != NULL)
    {
      fprintf(stderr, _("%s: %s: duplicate definition for version %u\n"),
	      program_name, this->name().c_str(), ndx);
      gold_exit(false);
    }
  (*version_map)[ndx] = name;
}

// Add mappings for the version definitions to VERSION_MAP.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::make_verdef_map(
    Read_symbols_data* sd,
    Version_map* version_map) const
{
  if (sd->verdef == NULL)
    return;

  const char* names = reinterpret_cast<const char*>(sd->symbol_names->data());
  off_t names_size = sd->symbol_names_size;

  const unsigned char* pverdef = sd->verdef->data();
  off_t verdef_size = sd->verdef_size;
  const unsigned int count = sd->verdef_info;

  const unsigned char* p = pverdef;
  for (unsigned int i = 0; i < count; ++i)
    {
      elfcpp::Verdef<size, big_endian> verdef(p);

      if (verdef.get_vd_version() != elfcpp::VER_DEF_CURRENT)
	{
	  fprintf(stderr, _("%s: %s: unexpected verdef version %u\n"),
		  program_name, this->name().c_str(), verdef.get_vd_version());
	  gold_exit(false);
	}

      const unsigned int vd_ndx = verdef.get_vd_ndx();

      // The GNU linker clears the VERSYM_HIDDEN bit.  I'm not
      // sure why.

      // The first Verdaux holds the name of this version.  Subsequent
      // ones are versions that this one depends upon, which we don't
      // care about here.
      const unsigned int vd_cnt = verdef.get_vd_cnt();
      if (vd_cnt < 1)
	{
	  fprintf(stderr, _("%s: %s: verdef vd_cnt field too small: %u\n"),
		  program_name, this->name().c_str(), vd_cnt);
	  gold_exit(false);
	}

      const unsigned int vd_aux = verdef.get_vd_aux();
      if ((p - pverdef) + vd_aux >= verdef_size)
	{
	  fprintf(stderr,
		  _("%s: %s: verdef vd_aux field out of range: %u\n"),
		  program_name, this->name().c_str(), vd_aux);
	  gold_exit(false);
	}

      const unsigned char* pvda = p + vd_aux;
      elfcpp::Verdaux<size, big_endian> verdaux(pvda);

      const unsigned int vda_name = verdaux.get_vda_name();
      if (vda_name >= names_size)
	{
	  fprintf(stderr,
		  _("%s: %s: verdaux vda_name field out of range: %u\n"),
		  program_name, this->name().c_str(), vda_name);
	  gold_exit(false);
	}

      this->set_version_map(version_map, vd_ndx, names + vda_name);

      const unsigned int vd_next = verdef.get_vd_next();
      if ((p - pverdef) + vd_next >= verdef_size)
	{
	  fprintf(stderr,
		  _("%s: %s: verdef vd_next field out of range: %u\n"),
		  program_name, this->name().c_str(), vd_next);
	  gold_exit(false);
	}

      p += vd_next;
    }
}

// Add mappings for the required versions to VERSION_MAP.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::make_verneed_map(
    Read_symbols_data* sd,
    Version_map* version_map) const
{
  if (sd->verneed == NULL)
    return;

  const char* names = reinterpret_cast<const char*>(sd->symbol_names->data());
  off_t names_size = sd->symbol_names_size;

  const unsigned char* pverneed = sd->verneed->data();
  const off_t verneed_size = sd->verneed_size;
  const unsigned int count = sd->verneed_info;

  const unsigned char* p = pverneed;
  for (unsigned int i = 0; i < count; ++i)
    {
      elfcpp::Verneed<size, big_endian> verneed(p);

      if (verneed.get_vn_version() != elfcpp::VER_NEED_CURRENT)
	{
	  fprintf(stderr, _("%s: %s: unexpected verneed version %u\n"),
		  program_name, this->name().c_str(),
		  verneed.get_vn_version());
	  gold_exit(false);
	}

      const unsigned int vn_aux = verneed.get_vn_aux();

      if ((p - pverneed) + vn_aux >= verneed_size)
	{
	  fprintf(stderr,
		  _("%s: %s: verneed vn_aux field out of range: %u\n"),
		  program_name, this->name().c_str(), vn_aux);
	  gold_exit(false);
	}

      const unsigned int vn_cnt = verneed.get_vn_cnt();
      const unsigned char* pvna = p + vn_aux;
      for (unsigned int j = 0; j < vn_cnt; ++j)
	{
	  elfcpp::Vernaux<size, big_endian> vernaux(pvna);

	  const unsigned int vna_name = vernaux.get_vna_name();
	  if (vna_name >= names_size)
	    {
	      fprintf(stderr,
		      _("%s: %s: vernaux vna_name field "
			"out of range: %u\n"),
		      program_name, this->name().c_str(), vna_name);
	      gold_exit(false);
	    }

	  this->set_version_map(version_map, vernaux.get_vna_other(),
				names + vna_name);

	  const unsigned int vna_next = vernaux.get_vna_next();
	  if ((pvna - pverneed) + vna_next >= verneed_size)
	    {
	      fprintf(stderr,
		      _("%s: %s: verneed vna_next field "
			"out of range: %u\n"),
		      program_name, this->name().c_str(), vna_next);
	      gold_exit(false);
	    }

	  pvna += vna_next;
	}

      const unsigned int vn_next = verneed.get_vn_next();
      if ((p - pverneed) + vn_next >= verneed_size)
	{
	  fprintf(stderr,
		  _("%s: %s: verneed vn_next field out of range: %u\n"),
		  program_name, this->name().c_str(), vn_next);
	  gold_exit(false);
	}

      p += vn_next;
    }
}

// Create a vector mapping version numbers to version strings.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::make_version_map(
    Read_symbols_data* sd,
    Version_map* version_map) const
{
  if (sd->verdef == NULL && sd->verneed == NULL)
    return;

  // A guess at the maximum version number we will see.  If this is
  // wrong we will be less efficient but still correct.
  version_map->reserve(sd->verdef_info + sd->verneed_info * 10);

  this->make_verdef_map(sd, version_map);
  this->make_verneed_map(sd, version_map);
}

// Add the dynamic symbols to the symbol table.

template<int size, bool big_endian>
void
Sized_dynobj<size, big_endian>::do_add_symbols(Symbol_table* symtab,
					       Read_symbols_data* sd)
{
  if (sd->symbols == NULL)
    {
      gold_assert(sd->symbol_names == NULL);
      gold_assert(sd->versym == NULL && sd->verdef == NULL
		  && sd->verneed == NULL);
      return;
    }

  const int sym_size = This::sym_size;
  const size_t symcount = sd->symbols_size / sym_size;
  if (symcount * sym_size != sd->symbols_size)
    {
      fprintf(stderr,
	      _("%s: %s: size of dynamic symbols is not "
		"multiple of symbol size\n"),
	      program_name, this->name().c_str());
      gold_exit(false);
    }

  Version_map version_map;
  this->make_version_map(sd, &version_map);

  const char* sym_names =
    reinterpret_cast<const char*>(sd->symbol_names->data());
  symtab->add_from_dynobj(this, sd->symbols->data(), symcount,
			  sym_names, sd->symbol_names_size,
			  (sd->versym == NULL
			   ? NULL
			   : sd->versym->data()),
			  sd->versym_size,
			  &version_map);

  delete sd->symbols;
  sd->symbols = NULL;
  delete sd->symbol_names;
  sd->symbol_names = NULL;
  if (sd->versym != NULL)
    {
      delete sd->versym;
      sd->versym = NULL;
    }
  if (sd->verdef != NULL)
    {
      delete sd->verdef;
      sd->verdef = NULL;
    }
  if (sd->verneed != NULL)
    {
      delete sd->verneed;
      sd->verneed = NULL;
    }
}

// Given a vector of hash codes, compute the number of hash buckets to
// use.

unsigned int
Dynobj::compute_bucket_count(const std::vector<uint32_t>& hashcodes,
			     bool for_gnu_hash_table)
{
  // FIXME: Implement optional hash table optimization.

  // Array used to determine the number of hash table buckets to use
  // based on the number of symbols there are.  If there are fewer
  // than 3 symbols we use 1 bucket, fewer than 17 symbols we use 3
  // buckets, fewer than 37 we use 17 buckets, and so forth.  We never
  // use more than 32771 buckets.  This is straight from the old GNU
  // linker.
  static const unsigned int buckets[] =
  {
    1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
    16411, 32771
  };
  const int buckets_count = sizeof buckets / sizeof buckets[0];

  unsigned int symcount = hashcodes.size();
  unsigned int ret = 1;
  for (int i = 0; i < buckets_count; ++i)
    {
      if (symcount < buckets[i])
	break;
      ret = buckets[i];
    }

  if (for_gnu_hash_table && ret < 2)
    ret = 2;

  return ret;
}

// The standard ELF hash function.  This hash function must not
// change, as the dynamic linker uses it also.

uint32_t
Dynobj::elf_hash(const char* name)
{
  const unsigned char* nameu = reinterpret_cast<const unsigned char*>(name);
  uint32_t h = 0;
  unsigned char c;
  while ((c = *nameu++) != '\0')
    {
      h = (h << 4) + c;
      uint32_t g = h & 0xf0000000;
      if (g != 0)
	{
	  h ^= g >> 24;
	  // The ELF ABI says h &= ~g, but using xor is equivalent in
	  // this case (since g was set from h) and may save one
	  // instruction.
	  h ^= g;
	}
    }
  return h;
}

// Create a standard ELF hash table, setting *PPHASH and *PHASHLEN.
// DYNSYMS is a vector with all the global dynamic symbols.
// LOCAL_DYNSYM_COUNT is the number of local symbols in the dynamic
// symbol table.

void
Dynobj::create_elf_hash_table(const Target* target,
			      const std::vector<Symbol*>& dynsyms,
			      unsigned int local_dynsym_count,
			      unsigned char** pphash,
			      unsigned int* phashlen)
{
  unsigned int dynsym_count = dynsyms.size();

  // Get the hash values for all the symbols.
  std::vector<uint32_t> dynsym_hashvals(dynsym_count);
  for (unsigned int i = 0; i < dynsym_count; ++i)
    dynsym_hashvals[i] = Dynobj::elf_hash(dynsyms[i]->name());

  const unsigned int bucketcount =
    Dynobj::compute_bucket_count(dynsym_hashvals, false);

  std::vector<uint32_t> bucket(bucketcount);
  std::vector<uint32_t> chain(local_dynsym_count + dynsym_count);

  for (unsigned int i = 0; i < dynsym_count; ++i)
    {
      unsigned int dynsym_index = dynsyms[i]->dynsym_index();
      unsigned int bucketpos = dynsym_hashvals[i] % bucketcount;
      chain[dynsym_index] = bucket[bucketpos];
      bucket[bucketpos] = dynsym_index;
    }

  unsigned int hashlen = ((2
			   + bucketcount
			   + local_dynsym_count
			   + dynsym_count)
			  * 4);
  unsigned char* phash = new unsigned char[hashlen];

  if (target->is_big_endian())
    Dynobj::sized_create_elf_hash_table<true>(bucket, chain, phash, hashlen);
  else
    Dynobj::sized_create_elf_hash_table<false>(bucket, chain, phash, hashlen);

  *pphash = phash;
  *phashlen = hashlen;
}

// Fill in an ELF hash table.

template<bool big_endian>
void
Dynobj::sized_create_elf_hash_table(const std::vector<uint32_t>& bucket,
				    const std::vector<uint32_t>& chain,
				    unsigned char* phash,
				    unsigned int hashlen)
{
  unsigned char* p = phash;

  const unsigned int bucketcount = bucket.size();
  const unsigned int chaincount = chain.size();

  elfcpp::Swap<32, big_endian>::writeval(p, bucketcount);
  p += 4;
  elfcpp::Swap<32, big_endian>::writeval(p, chaincount);
  p += 4;

  for (unsigned int i = 0; i < bucketcount; ++i)
    {
      elfcpp::Swap<32, big_endian>::writeval(p, bucket[i]);
      p += 4;
    }

  for (unsigned int i = 0; i < chaincount; ++i)
    {
      elfcpp::Swap<32, big_endian>::writeval(p, chain[i]);
      p += 4;
    }

  gold_assert(static_cast<unsigned int>(p - phash) == hashlen);
}

// The hash function used for the GNU hash table.  This hash function
// must not change, as the dynamic linker uses it also.

uint32_t
Dynobj::gnu_hash(const char* name)
{
  const unsigned char* nameu = reinterpret_cast<const unsigned char*>(name);
  uint32_t h = 5381;
  unsigned char c;
  while ((c = *nameu++) != '\0')
    h = (h << 5) + h + c;
  return h;
}

// Create a GNU hash table, setting *PPHASH and *PHASHLEN.  GNU hash
// tables are an extension to ELF which are recognized by the GNU
// dynamic linker.  They are referenced using dynamic tag DT_GNU_HASH.
// TARGET is the target.  DYNSYMS is a vector with all the global
// symbols which will be going into the dynamic symbol table.
// LOCAL_DYNSYM_COUNT is the number of local symbols in the dynamic
// symbol table.

void
Dynobj::create_gnu_hash_table(const Target* target,
			      const std::vector<Symbol*>& dynsyms,
			      unsigned int local_dynsym_count,
			      unsigned char** pphash,
			      unsigned int* phashlen)
{
  const unsigned int count = dynsyms.size();

  // Sort the dynamic symbols into two vectors.  Symbols which we do
  // not want to put into the hash table we store into
  // UNHASHED_DYNSYMS.  Symbols which we do want to store we put into
  // HASHED_DYNSYMS.  DYNSYM_HASHVALS is parallel to HASHED_DYNSYMS,
  // and records the hash codes.

  std::vector<Symbol*> unhashed_dynsyms;
  unhashed_dynsyms.reserve(count);

  std::vector<Symbol*> hashed_dynsyms;
  hashed_dynsyms.reserve(count);

  std::vector<uint32_t> dynsym_hashvals;
  dynsym_hashvals.reserve(count);
  
  for (unsigned int i = 0; i < count; ++i)
    {
      Symbol* sym = dynsyms[i];

      // FIXME: Should put on unhashed_dynsyms if the symbol is
      // hidden.
      if (sym->is_undefined())
	unhashed_dynsyms.push_back(sym);
      else
	{
	  hashed_dynsyms.push_back(sym);
	  dynsym_hashvals.push_back(Dynobj::gnu_hash(sym->name()));
	}
    }

  // Put the unhashed symbols at the start of the global portion of
  // the dynamic symbol table.
  const unsigned int unhashed_count = unhashed_dynsyms.size();
  unsigned int unhashed_dynsym_index = local_dynsym_count;
  for (unsigned int i = 0; i < unhashed_count; ++i)
    {
      unhashed_dynsyms[i]->set_dynsym_index(unhashed_dynsym_index);
      ++unhashed_dynsym_index;
    }

  // For the actual data generation we call out to a templatized
  // function.
  int size = target->get_size();
  bool big_endian = target->is_big_endian();
  if (size == 32)
    {
      if (big_endian)
	Dynobj::sized_create_gnu_hash_table<32, true>(hashed_dynsyms,
						      dynsym_hashvals,
						      unhashed_dynsym_index,
						      pphash,
						      phashlen);
      else
	Dynobj::sized_create_gnu_hash_table<32, false>(hashed_dynsyms,
						       dynsym_hashvals,
						       unhashed_dynsym_index,
						       pphash,
						       phashlen);
    }
  else if (size == 64)
    {
      if (big_endian)
	Dynobj::sized_create_gnu_hash_table<64, true>(hashed_dynsyms,
						      dynsym_hashvals,
						      unhashed_dynsym_index,
						      pphash,
						      phashlen);
      else
	Dynobj::sized_create_gnu_hash_table<64, false>(hashed_dynsyms,
						       dynsym_hashvals,
						       unhashed_dynsym_index,
						       pphash,
						       phashlen);
    }
  else
    gold_unreachable();
}

// Create the actual data for a GNU hash table.  This is just a copy
// of the code from the old GNU linker.

template<int size, bool big_endian>
void
Dynobj::sized_create_gnu_hash_table(
    const std::vector<Symbol*>& hashed_dynsyms,
    const std::vector<uint32_t>& dynsym_hashvals,
    unsigned int unhashed_dynsym_count,
    unsigned char** pphash,
    unsigned int* phashlen)
{
  if (hashed_dynsyms.empty())
    {
      // Special case for the empty hash table.
      unsigned int hashlen = 5 * 4 + size / 8;
      unsigned char* phash = new unsigned char[hashlen];
      // One empty bucket.
      elfcpp::Swap<32, big_endian>::writeval(phash, 1);
      // Symbol index above unhashed symbols.
      elfcpp::Swap<32, big_endian>::writeval(phash + 4, unhashed_dynsym_count);
      // One word for bitmask.
      elfcpp::Swap<32, big_endian>::writeval(phash + 8, 1);
      // Only bloom filter.
      elfcpp::Swap<32, big_endian>::writeval(phash + 12, 0);
      // No valid hashes.
      elfcpp::Swap<size, big_endian>::writeval(phash + 16, 0);
      // No hashes in only bucket.
      elfcpp::Swap<32, big_endian>::writeval(phash + 16 + size / 8, 0);

      *phashlen = hashlen;
      *pphash = phash;

      return;
    }

  const unsigned int bucketcount =
    Dynobj::compute_bucket_count(dynsym_hashvals, true);

  const unsigned int nsyms = hashed_dynsyms.size();

  uint32_t maskbitslog2 = 1;
  uint32_t x = nsyms >> 1;
  while (x != 0)
    {
      ++maskbitslog2;
      x >>= 1;
    }
  if (maskbitslog2 < 3)
    maskbitslog2 = 5;
  else if (((1U << (maskbitslog2 - 2)) & nsyms) != 0)
    maskbitslog2 += 3;
  else
    maskbitslog2 += 2;

  uint32_t shift1;
  if (size == 32)
    shift1 = 5;
  else
    {
      if (maskbitslog2 == 5)
	maskbitslog2 = 6;
      shift1 = 6;
    }
  uint32_t mask = (1U << shift1) - 1U;
  uint32_t shift2 = maskbitslog2;
  uint32_t maskbits = 1U << maskbitslog2;
  uint32_t maskwords = 1U << (maskbitslog2 - shift1);

  typedef typename elfcpp::Elf_types<size>::Elf_WXword Word;
  std::vector<Word> bitmask(maskwords);
  std::vector<uint32_t> counts(bucketcount);
  std::vector<uint32_t> indx(bucketcount);
  uint32_t symindx = unhashed_dynsym_count;

  // Count the number of times each hash bucket is used.
  for (unsigned int i = 0; i < nsyms; ++i)
    ++counts[dynsym_hashvals[i] % bucketcount];

  unsigned int cnt = symindx;
  for (unsigned int i = 0; i < bucketcount; ++i)
    {
      indx[i] = cnt;
      cnt += counts[i];
    }

  unsigned int hashlen = (4 + bucketcount + nsyms) * 4;
  hashlen += maskbits / 8;
  unsigned char* phash = new unsigned char[hashlen];

  elfcpp::Swap<32, big_endian>::writeval(phash, bucketcount);
  elfcpp::Swap<32, big_endian>::writeval(phash + 4, symindx);
  elfcpp::Swap<32, big_endian>::writeval(phash + 8, maskwords);
  elfcpp::Swap<32, big_endian>::writeval(phash + 12, shift2);

  unsigned char* p = phash + 16 + maskbits / 8;
  for (unsigned int i = 0; i < bucketcount; ++i)
    {
      if (counts[i] == 0)
	elfcpp::Swap<32, big_endian>::writeval(p, 0);
      else
	elfcpp::Swap<32, big_endian>::writeval(p, indx[i]);
      p += 4;
    }

  for (unsigned int i = 0; i < nsyms; ++i)
    {
      Symbol* sym = hashed_dynsyms[i];
      uint32_t hashval = dynsym_hashvals[i];

      unsigned int bucket = hashval % bucketcount;
      unsigned int val = ((hashval >> shift1)
			  & ((maskbits >> shift1) - 1));
      bitmask[val] |= (static_cast<Word>(1U)) << (hashval & mask);
      bitmask[val] |= (static_cast<Word>(1U)) << ((hashval >> shift2) & mask);
      val = hashval & ~ 1U;
      if (counts[bucket] == 1)
	{
	  // Last element terminates the chain.
	  val |= 1;
	}
      elfcpp::Swap<32, big_endian>::writeval(p + (indx[bucket] - symindx) * 4,
					     val);
      --counts[bucket];

      sym->set_dynsym_index(indx[bucket]);
      ++indx[bucket];
    }

  p = phash + 16;
  for (unsigned int i = 0; i < maskwords; ++i)
    {
      elfcpp::Swap<size, big_endian>::writeval(p, bitmask[i]);
      p += size / 8;
    }

  *phashlen = hashlen;
  *pphash = phash;
}

// Verdef methods.

// Write this definition to a buffer for the output section.

template<int size, bool big_endian>
unsigned char*
Verdef::write(const Stringpool* dynpool, bool is_last, unsigned char* pb
              ACCEPT_SIZE_ENDIAN) const
{
  const int verdef_size = elfcpp::Elf_sizes<size>::verdef_size;
  const int verdaux_size = elfcpp::Elf_sizes<size>::verdaux_size;

  elfcpp::Verdef_write<size, big_endian> vd(pb);
  vd.set_vd_version(elfcpp::VER_DEF_CURRENT);
  vd.set_vd_flags((this->is_base_ ? elfcpp::VER_FLG_BASE : 0)
		  | (this->is_weak_ ? elfcpp::VER_FLG_WEAK : 0));
  vd.set_vd_ndx(this->index());
  vd.set_vd_cnt(1 + this->deps_.size());
  vd.set_vd_hash(Dynobj::elf_hash(this->name()));
  vd.set_vd_aux(verdef_size);
  vd.set_vd_next(is_last
		 ? 0
		 : verdef_size + (1 + this->deps_.size()) * verdaux_size);
  pb += verdef_size;

  elfcpp::Verdaux_write<size, big_endian> vda(pb);
  vda.set_vda_name(dynpool->get_offset(this->name()));
  vda.set_vda_next(this->deps_.empty() ? 0 : verdaux_size);
  pb += verdaux_size;

  Deps::const_iterator p;
  unsigned int i;
  for (p = this->deps_.begin(), i = 0;
       p != this->deps_.end();
       ++p, ++i)
    {
      elfcpp::Verdaux_write<size, big_endian> vda(pb);
      vda.set_vda_name(dynpool->get_offset(*p));
      vda.set_vda_next(i + 1 >= this->deps_.size() ? 0 : verdaux_size);
      pb += verdaux_size;
    }

  return pb;
}

// Verneed methods.

Verneed::~Verneed()
{
  for (Need_versions::iterator p = this->need_versions_.begin();
       p != this->need_versions_.end();
       ++p)
    delete *p;
}

// Add a new version to this file reference.

Verneed_version*
Verneed::add_name(const char* name)
{
  Verneed_version* vv = new Verneed_version(name);
  this->need_versions_.push_back(vv);
  return vv;
}

// Set the version indexes starting at INDEX.

unsigned int
Verneed::finalize(unsigned int index)
{
  for (Need_versions::iterator p = this->need_versions_.begin();
       p != this->need_versions_.end();
       ++p)
    {
      (*p)->set_index(index);
      ++index;
    }
  return index;
}

// Write this list of referenced versions to a buffer for the output
// section.

template<int size, bool big_endian>
unsigned char*
Verneed::write(const Stringpool* dynpool, bool is_last,
	       unsigned char* pb ACCEPT_SIZE_ENDIAN) const
{
  const int verneed_size = elfcpp::Elf_sizes<size>::verneed_size;
  const int vernaux_size = elfcpp::Elf_sizes<size>::vernaux_size;

  elfcpp::Verneed_write<size, big_endian> vn(pb);
  vn.set_vn_version(elfcpp::VER_NEED_CURRENT);
  vn.set_vn_cnt(this->need_versions_.size());
  vn.set_vn_file(dynpool->get_offset(this->filename()));
  vn.set_vn_aux(verneed_size);
  vn.set_vn_next(is_last
		 ? 0
		 : verneed_size + this->need_versions_.size() * vernaux_size);
  pb += verneed_size;

  Need_versions::const_iterator p;
  unsigned int i;
  for (p = this->need_versions_.begin(), i = 0;
       p != this->need_versions_.end();
       ++p, ++i)
    {
      elfcpp::Vernaux_write<size, big_endian> vna(pb);
      vna.set_vna_hash(Dynobj::elf_hash((*p)->version()));
      // FIXME: We need to sometimes set VER_FLG_WEAK here.
      vna.set_vna_flags(0);
      vna.set_vna_other((*p)->index());
      vna.set_vna_name(dynpool->get_offset((*p)->version()));
      vna.set_vna_next(i + 1 >= this->need_versions_.size()
		       ? 0
		       : vernaux_size);
      pb += vernaux_size;
    }

  return pb;
}

// Versions methods.

Versions::~Versions()
{
  for (Defs::iterator p = this->defs_.begin();
       p != this->defs_.end();
       ++p)
    delete *p;

  for (Needs::iterator p = this->needs_.begin();
       p != this->needs_.end();
       ++p)
    delete *p;
}

// Record version information for a symbol going into the dynamic
// symbol table.

void
Versions::record_version(const General_options* options,
			 Stringpool* dynpool, const Symbol* sym)
{
  gold_assert(!this->is_finalized_);
  gold_assert(sym->version() != NULL);

  Stringpool::Key version_key;
  const char* version = dynpool->add(sym->version(), &version_key);

  if (!sym->is_from_dynobj())
    this->add_def(options, sym, version, version_key);
  else
    {
      // This is a version reference.

      Object* object = sym->object();
      gold_assert(object->is_dynamic());
      Dynobj* dynobj = static_cast<Dynobj*>(object);

      this->add_need(dynpool, dynobj->soname(), version, version_key);
    }
}

// We've found a symbol SYM defined in version VERSION.

void
Versions::add_def(const General_options* options, const Symbol* sym,
		  const char* version, Stringpool::Key version_key)
{
  Key k(version_key, 0);
  Version_base* const vbnull = NULL;
  std::pair<Version_table::iterator, bool> ins =
    this->version_table_.insert(std::make_pair(k, vbnull));

  if (!ins.second)
    {
      // We already have an entry for this version.
      Version_base* vb = ins.first->second;

      // We have now seen a symbol in this version, so it is not
      // weak.
      vb->clear_weak();

      // FIXME: When we support version scripts, we will need to
      // check whether this symbol should be forced local.
    }
  else
    {
      // If we are creating a shared object, it is an error to
      // find a definition of a symbol with a version which is not
      // in the version script.
      if (options->is_shared())
	{
	  fprintf(stderr, _("%s: symbol %s has undefined version %s\n"),
		  program_name, sym->name(), version);
	  gold_exit(false);
	}

      // If this is the first version we are defining, first define
      // the base version.  FIXME: Should use soname here when
      // creating a shared object.
      Verdef* vdbase = new Verdef(options->output_file_name(), true, false,
				  true);
      this->defs_.push_back(vdbase);

      // When creating a regular executable, automatically define
      // a new version.
      Verdef* vd = new Verdef(version, false, false, false);
      this->defs_.push_back(vd);
      ins.first->second = vd;
    }
}

// Add a reference to version NAME in file FILENAME.

void
Versions::add_need(Stringpool* dynpool, const char* filename, const char* name,
		   Stringpool::Key name_key)
{
  Stringpool::Key filename_key;
  filename = dynpool->add(filename, &filename_key);

  Key k(name_key, filename_key);
  Version_base* const vbnull = NULL;
  std::pair<Version_table::iterator, bool> ins =
    this->version_table_.insert(std::make_pair(k, vbnull));

  if (!ins.second)
    {
      // We already have an entry for this filename/version.
      return;
    }

  // See whether we already have this filename.  We don't expect many
  // version references, so we just do a linear search.  This could be
  // replaced by a hash table.
  Verneed* vn = NULL;
  for (Needs::iterator p = this->needs_.begin();
       p != this->needs_.end();
       ++p)
    {
      if ((*p)->filename() == filename)
	{
	  vn = *p;
	  break;
	}
    }

  if (vn == NULL)
    {
      // We have a new filename.
      vn = new Verneed(filename);
      this->needs_.push_back(vn);
    }

  ins.first->second = vn->add_name(name);
}

// Set the version indexes.  Create a new dynamic version symbol for
// each new version definition.

unsigned int
Versions::finalize(const Target* target, Symbol_table* symtab,
		   unsigned int dynsym_index, std::vector<Symbol*>* syms)
{
  gold_assert(!this->is_finalized_);

  unsigned int vi = 1;

  for (Defs::iterator p = this->defs_.begin();
       p != this->defs_.end();
       ++p)
    {
      (*p)->set_index(vi);
      ++vi;

      // Create a version symbol if necessary.
      if (!(*p)->is_symbol_created())
	{
	  Symbol* vsym = symtab->define_as_constant(target, (*p)->name(),
						    (*p)->name(), 0, 0,
						    elfcpp::STT_OBJECT,
						    elfcpp::STB_GLOBAL,
						    elfcpp::STV_DEFAULT, 0,
						    false);
	  vsym->set_needs_dynsym_entry();
	  ++dynsym_index;
	  syms->push_back(vsym);
	  // The name is already in the dynamic pool.
	}
    }

  // Index 1 is used for global symbols.
  if (vi == 1)
    {
      gold_assert(this->defs_.empty());
      vi = 2;
    }

  for (Needs::iterator p = this->needs_.begin();
       p != this->needs_.end();
       ++p)
    vi = (*p)->finalize(vi);

  this->is_finalized_ = true;

  return dynsym_index;
}

// Return the version index to use for a symbol.  This does two hash
// table lookups: one in DYNPOOL and one in this->version_table_.
// Another approach alternative would be store a pointer in SYM, which
// would increase the size of the symbol table.  Or perhaps we could
// use a hash table from dynamic symbol pointer values to Version_base
// pointers.

unsigned int
Versions::version_index(const Stringpool* dynpool, const Symbol* sym) const
{
  Stringpool::Key version_key;
  const char* version = dynpool->find(sym->version(), &version_key);
  gold_assert(version != NULL);

  Key k;
  if (!sym->is_from_dynobj())
    k = Key(version_key, 0);
  else
    {
      Object* object = sym->object();
      gold_assert(object->is_dynamic());
      Dynobj* dynobj = static_cast<Dynobj*>(object);

      Stringpool::Key filename_key;
      const char* filename = dynpool->find(dynobj->soname(), &filename_key);
      gold_assert(filename != NULL);

      k = Key(version_key, filename_key);
    }

  Version_table::const_iterator p = this->version_table_.find(k);
  gold_assert(p != this->version_table_.end());

  return p->second->index();
}

// Return an allocated buffer holding the contents of the symbol
// version section.

template<int size, bool big_endian>
void
Versions::symbol_section_contents(const Stringpool* dynpool,
				  unsigned int local_symcount,
				  const std::vector<Symbol*>& syms,
				  unsigned char** pp,
				  unsigned int* psize
                                  ACCEPT_SIZE_ENDIAN) const
{
  gold_assert(this->is_finalized_);

  unsigned int sz = (local_symcount + syms.size()) * 2;
  unsigned char* pbuf = new unsigned char[sz];

  for (unsigned int i = 0; i < local_symcount; ++i)
    elfcpp::Swap<16, big_endian>::writeval(pbuf + i * 2,
					   elfcpp::VER_NDX_LOCAL);

  for (std::vector<Symbol*>::const_iterator p = syms.begin();
       p != syms.end();
       ++p)
    {
      unsigned int version_index;
      const char* version = (*p)->version();
      if (version == NULL)
	version_index = elfcpp::VER_NDX_GLOBAL;
      else
	version_index = this->version_index(dynpool, *p);
      elfcpp::Swap<16, big_endian>::writeval(pbuf + (*p)->dynsym_index() * 2,
					     version_index);
    }

  *pp = pbuf;
  *psize = sz;
}

// Return an allocated buffer holding the contents of the version
// definition section.

template<int size, bool big_endian>
void
Versions::def_section_contents(const Stringpool* dynpool,
			       unsigned char** pp, unsigned int* psize,
			       unsigned int* pentries
                               ACCEPT_SIZE_ENDIAN) const
{
  gold_assert(this->is_finalized_);
  gold_assert(!this->defs_.empty());

  const int verdef_size = elfcpp::Elf_sizes<size>::verdef_size;
  const int verdaux_size = elfcpp::Elf_sizes<size>::verdaux_size;

  unsigned int sz = 0;
  for (Defs::const_iterator p = this->defs_.begin();
       p != this->defs_.end();
       ++p)
    {
      sz += verdef_size + verdaux_size;
      sz += (*p)->count_dependencies() * verdaux_size;
    }

  unsigned char* pbuf = new unsigned char[sz];

  unsigned char* pb = pbuf;
  Defs::const_iterator p;
  unsigned int i;
  for (p = this->defs_.begin(), i = 0;
       p != this->defs_.end();
       ++p, ++i)
    pb = (*p)->write SELECT_SIZE_ENDIAN_NAME(size, big_endian)(
            dynpool, i + 1 >= this->defs_.size(), pb
            SELECT_SIZE_ENDIAN(size, big_endian));

  gold_assert(static_cast<unsigned int>(pb - pbuf) == sz);

  *pp = pbuf;
  *psize = sz;
  *pentries = this->defs_.size();
}

// Return an allocated buffer holding the contents of the version
// reference section.

template<int size, bool big_endian>
void
Versions::need_section_contents(const Stringpool* dynpool,
				unsigned char** pp, unsigned int *psize,
				unsigned int *pentries
                                ACCEPT_SIZE_ENDIAN) const
{
  gold_assert(this->is_finalized_);
  gold_assert(!this->needs_.empty());

  const int verneed_size = elfcpp::Elf_sizes<size>::verneed_size;
  const int vernaux_size = elfcpp::Elf_sizes<size>::vernaux_size;

  unsigned int sz = 0;
  for (Needs::const_iterator p = this->needs_.begin();
       p != this->needs_.end();
       ++p)
    {
      sz += verneed_size;
      sz += (*p)->count_versions() * vernaux_size;
    }

  unsigned char* pbuf = new unsigned char[sz];

  unsigned char* pb = pbuf;
  Needs::const_iterator p;
  unsigned int i;
  for (p = this->needs_.begin(), i = 0;
       p != this->needs_.end();
       ++p, ++i)
    pb = (*p)->write SELECT_SIZE_ENDIAN_NAME(size, big_endian)(
	    dynpool, i + 1 >= this->needs_.size(), pb
            SELECT_SIZE_ENDIAN(size, big_endian));

  gold_assert(static_cast<unsigned int>(pb - pbuf) == sz);

  *pp = pbuf;
  *psize = sz;
  *pentries = this->needs_.size();
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones for implemented targets.

template
class Sized_dynobj<32, false>;

template
class Sized_dynobj<32, true>;

template
class Sized_dynobj<64, false>;

template
class Sized_dynobj<64, true>;

template
void
Versions::symbol_section_contents<32, false>(
    const Stringpool*,
    unsigned int,
    const std::vector<Symbol*>&,
    unsigned char**,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, false)) const;

template
void
Versions::symbol_section_contents<32, true>(
    const Stringpool*,
    unsigned int,
    const std::vector<Symbol*>&,
    unsigned char**,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, true)) const;

template
void
Versions::symbol_section_contents<64, false>(
    const Stringpool*,
    unsigned int,
    const std::vector<Symbol*>&,
    unsigned char**,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, false)) const;

template
void
Versions::symbol_section_contents<64, true>(
    const Stringpool*,
    unsigned int,
    const std::vector<Symbol*>&,
    unsigned char**,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, true)) const;

template
void
Versions::def_section_contents<32, false>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, false)) const;

template
void
Versions::def_section_contents<32, true>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, true)) const;

template
void
Versions::def_section_contents<64, false>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, false)) const;

template
void
Versions::def_section_contents<64, true>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, true)) const;

template
void
Versions::need_section_contents<32, false>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, false)) const;

template
void
Versions::need_section_contents<32, true>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(32, true)) const;

template
void
Versions::need_section_contents<64, false>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, false)) const;

template
void
Versions::need_section_contents<64, true>(
    const Stringpool*,
    unsigned char**,
    unsigned int*,
    unsigned int*
    ACCEPT_SIZE_ENDIAN_EXPLICIT(64, true)) const;

} // End namespace gold.
