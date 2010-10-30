// elfcpp_file.h -- file access for elfcpp   -*- C++ -*-

// This header file defines the class Elf_file which can be used to
// read useful data from an ELF file.  The functions here are all
// templates which take a file interface object as a parameter.  This
// type must have a subtype View.  This type must support two methods:
//     View view(off_t file_offset, off_t data_size)
// returns a View for the specified part of the file.
//     void error(const char* printf_format, ...)
// prints an error message and does not return.  The subtype View must
// support a method
//     const unsigned char* data()
// which returns a pointer to a buffer containing the requested data.
// This general interface is used to read data from the file.  Objects
// of type View will never survive longer than the elfcpp function.

// Some of these functions must return a reference to part of the
// file.  To use these, the file interface must support a subtype
// Location:
//    Location(off_t file_offset, off_t data_size)
// To use this in conjunction with the accessors types Shdr, etc., the
// file interface should support an overload of view:
//    View view(Location)
// This permits writing
//    elfcpp::Shdr shdr(file, ef.section_header(n));

#ifndef ELFPCP_FILE_H
#define ELFCPP_FILE_H

#include <string>
#include <cstring>

namespace elfcpp
{

// This object is used to read an ELF file.
//   SIZE: The size of file, 32 or 64.
//   BIG_ENDIAN: Whether the file is in big-endian format.
//   FILE: A file reading type as described above.

template<int size, bool big_endian, typename File>
class Elf_file
{
 private:
  typedef Elf_file<size, big_endian, File> This;

 public:
  static const int ehdr_size = Elf_sizes<size>::ehdr_size;
  static const int phdr_size = Elf_sizes<size>::phdr_size;
  static const int shdr_size = Elf_sizes<size>::shdr_size;
  static const int sym_size = Elf_sizes<size>::sym_size;
  static const int rel_size = Elf_sizes<size>::rel_size;
  static const int rela_size = Elf_sizes<size>::rela_size;

  typedef Ehdr<size, big_endian> Ef_ehdr;
  typedef Phdr<size, big_endian> Ef_phdr;
  typedef Shdr<size, big_endian> Ef_shdr;
  typedef Sym<size, big_endian> Ef_sym;

  // Construct an Elf_file given an ELF file header.
  Elf_file(File* file, const Ef_ehdr& ehdr)
  { this->construct(file, ehdr); }

  // Construct an ELF file.
  inline
  Elf_file(File* file);

  // Return the file offset to the section headers.
  off_t
  shoff() const
  { return this->shoff_; }

  // Return the number of sections.
  unsigned int
  shnum()
  {
    this->initialize_shnum();
    return this->shnum_;
  }

  // Return the section index of the section name string table.
  unsigned int
  shstrndx()
  {
    this->initialize_shnum();
    return this->shstrndx_;
  }

  // Return the location of the header of section SHNDX.
  typename File::Location
  section_header(unsigned int shndx)
  {
    return typename File::Location(this->section_header_offset(shndx),
				   shdr_size);
  }

  // Return the name of section SHNDX.
  std::string
  section_name(unsigned int shndx);

  // Return the location of the contents of section SHNDX.
  typename File::Location
  section_contents(unsigned int shndx);

  // Return the flags of section SHNDX.
  typename Elf_types<size>::Elf_WXword
  section_flags(unsigned int shndx);

 private:
  // Shared constructor code.
  void
  construct(File* file, const Ef_ehdr& ehdr);

  // Initialize shnum_ and shstrndx_.
  void
  initialize_shnum();

  // Return the file offset of the header of section SHNDX.
  off_t
  section_header_offset(unsigned int shndx);

  // The file we are reading.
  File* file_;
  // The file offset to the section headers.
  off_t shoff_;
  // The number of sections.
  unsigned int shnum_;
  // The section index of the section name string table.
  unsigned int shstrndx_;
};

// Template function definitions.

// Construct an Elf_file given an ELF file header.

template<int size, bool big_endian, typename File>
void
Elf_file<size, big_endian, File>::construct(File* file, const Ef_ehdr& ehdr)
{
  this->file_ = file;
  this->shoff_ = ehdr.get_e_shoff();
  this->shnum_ = ehdr.get_e_shnum();
  this->shstrndx_ = ehdr.get_e_shstrndx();
  if (ehdr.get_e_ehsize() != This::ehdr_size)
    file->error(_("bad e_ehsize (%d != %d)"),
		ehdr.get_e_ehsize(), This::ehdr_size);
  if (ehdr.get_e_shentsize() != This::shdr_size)
    file->error(_("bad e_shentsize (%d != %d)"),
		ehdr.get_e_shentsize(), This::shdr_size);
}

// Construct an ELF file.

template<int size, bool big_endian, typename File>
inline
Elf_file<size, big_endian, File>::Elf_file(File* file)
{
  typename File::View v(file->view(file_header_offset, This::ehdr_size));
  this->construct(file, Ef_ehdr(v.data()));
}

// Initialize the shnum_ and shstrndx_ fields, handling overflow.

template<int size, bool big_endian, typename File>
void
Elf_file<size, big_endian, File>::initialize_shnum()
{
  if ((this->shnum_ == 0 || this->shstrndx_ == SHN_XINDEX)
      && this->shoff_ != 0)
    {
      typename File::View v(this->file_->view(this->shoff_, This::shdr_size));
      Ef_shdr shdr(v.data());
      if (this->shnum_ == 0)
	this->shnum_ = shdr.get_sh_size();
      if (this->shstrndx_ == SHN_XINDEX)
	this->shstrndx_ = shdr.get_sh_link();
    }
}

// Return the file offset of the section header of section SHNDX.

template<int size, bool big_endian, typename File>
off_t
Elf_file<size, big_endian, File>::section_header_offset(unsigned int shndx)
{
  if (shndx >= this->shnum())
    this->file_->error(_("section_header_offset: bad shndx %u >= %u"),
		       shndx, this->shnum());
  return this->shoff_ + This::shdr_size * shndx;
}

// Return the name of section SHNDX.

template<int size, bool big_endian, typename File>
std::string
Elf_file<size, big_endian, File>::section_name(unsigned int shndx)
{
  File* const file = this->file_;

  // Get the section name offset.
  unsigned int sh_name;
  {
    typename File::View v(file->view(this->section_header_offset(shndx),
				     This::shdr_size));
    Ef_shdr shdr(v.data());
    sh_name = shdr.get_sh_name();
  }

  // Get the file offset for the section name string table data.
  off_t shstr_off;
  off_t shstr_size;
  {
    const unsigned int shstrndx = this->shstrndx_;
    typename File::View v(file->view(this->section_header_offset(shstrndx),
				     This::shdr_size));
    Ef_shdr shstr_shdr(v.data());
    shstr_off = shstr_shdr.get_sh_offset();
    shstr_size = shstr_shdr.get_sh_size();
  }

  if (sh_name >= shstr_size)
    file->error(_("bad section name offset for section %u: %u"),
		shndx, sh_name);

  typename File::View v(file->view(shstr_off, shstr_size));

  const unsigned char* datau = v.data();
  const char* data = reinterpret_cast<const char*>(datau);
  const void* p = ::memchr(data + sh_name, '\0', shstr_size - sh_name);
  if (p == NULL)
    file->error(_("missing null terminator for name of section %u"),
		shndx);

  size_t len = static_cast<const char*>(p) - (data + sh_name);

  return std::string(data + sh_name, len);
}

// Return the contents of section SHNDX.

template<int size, bool big_endian, typename File>
typename File::Location
Elf_file<size, big_endian, File>::section_contents(unsigned int shndx)
{
  File* const file = this->file_;

  if (shndx >= this->shnum())
    file->error(_("section_contents: bad shndx %u >= %u"),
		shndx, this->shnum());

  typename File::View v(file->view(this->section_header_offset(shndx),
				   This::shdr_size));
  Ef_shdr shdr(v.data());
  return typename File::Location(shdr.get_sh_offset(), shdr.get_sh_size());
}

// Return the section flags of section SHNDX.

template<int size, bool big_endian, typename File>
typename Elf_types<size>::Elf_WXword
Elf_file<size, big_endian, File>::section_flags(unsigned int shndx)
{
  File* const file = this->file_;

  if (shndx >= this->shnum())
    file->error(_("section_flags: bad shndx %u >= %u"),
		shndx, this->shnum());

  typename File::View v(file->view(this->section_header_offset(shndx),
				   This::shdr_size));

  Ef_shdr shdr(v.data());
  return shdr.get_sh_flags();
}

} // End namespace elfcpp.

#endif // !defined(ELFCPP_FILE_H)
