// archive.cc -- archive support for gold

#include "gold.h"

#include <cerrno>
#include <cstring>
#include <climits>
#include <vector>

#include "elfcpp.h"
#include "fileread.h"
#include "readsyms.h"
#include "symtab.h"
#include "object.h"
#include "archive.h"

namespace gold
{

// The header of an entry in the archive.  This is all readable text,
// padded with spaces where necesary.  If the contents of an archive
// are all text file, the entire archive is readable.

struct Archive::Archive_header
{
  // The entry name.
  char ar_name[16];
  // The file modification time.
  char ar_date[12];
  // The user's UID in decimal.
  char ar_uid[6];
  // The user's GID in decimal.
  char ar_gid[6];
  // The file mode in octal.
  char ar_mode[8];
  // The file size in decimal.
  char ar_size[10];
  // The final magic code.
  char ar_fmag[2];
};

// Archive methods.

const char Archive::armag[sarmag] =
{
  '!', '<', 'a', 'r', 'c', 'h', '>', '\n'
};

const char Archive::arfmag[2] = { '`', '\n' };

// Set up the archive: read the symbol map and the extended name
// table.

void
Archive::setup()
{
  // The first member of the archive should be the symbol table.
  std::string armap_name;
  off_t armap_size = this->read_header(sarmag, &armap_name);
  if (!armap_name.empty())
    {
      fprintf(stderr, _("%s: %s: no archive symbol table (run ranlib)\n"),
	      program_name, this->name().c_str());
      gold_exit(false);
    }

  // Read in the entire armap.
  const unsigned char* p = this->get_view(sarmag + sizeof(Archive_header),
					  armap_size);

  // Numbers in the armap are always big-endian.
  const elfcpp::Elf_Word* pword = reinterpret_cast<const elfcpp::Elf_Word*>(p);
  unsigned int nsyms = elfcpp::Swap<32, true>::readval(pword);
  ++pword;

  // Note that the addition is in units of sizeof(elfcpp::Elf_Word).
  const char* pnames = reinterpret_cast<const char*>(pword + nsyms);

  this->armap_.resize(nsyms);

  for (unsigned int i = 0; i < nsyms; ++i)
    {
      this->armap_[i].name = pnames;
      this->armap_[i].offset = elfcpp::Swap<32, true>::readval(pword);
      pnames += strlen(pnames) + 1;
      ++pword;
    }

  if (reinterpret_cast<const unsigned char*>(pnames) - p > armap_size)
    {
      fprintf(stderr, _("%s: %s: bad archive symbol table names\n"),
	      program_name, this->name().c_str());
      gold_exit(false);
    }

  // See if there is an extended name table.
  off_t off = sarmag + sizeof(Archive_header) + armap_size;
  if ((off & 1) != 0)
    ++off;
  std::string xname;
  off_t extended_size = this->read_header(off, &xname);
  if (xname == "/")
    {
      p = this->get_view(off + sizeof(Archive_header), extended_size);
      const char* px = reinterpret_cast<const char*>(p);
      this->extended_names_.assign(px, extended_size);
    }

  // This array keeps track of which symbols are for archive elements
  // which we have already included in the link.
  this->seen_.resize(nsyms);

  // Opening the file locked it.  Unlock it now.
  this->input_file_->file().unlock();
}

// Read the header of an archive member at OFF.  Fail if something
// goes wrong.  Return the size of the member.  Set *PNAME to the name
// of the member.

off_t
Archive::read_header(off_t off, std::string* pname)
{
  const unsigned char* p = this->get_view(off, sizeof(Archive_header));
  const Archive_header* hdr = reinterpret_cast<const Archive_header*>(p);

  if (memcmp(hdr->ar_fmag, arfmag, sizeof arfmag) != 0)
    {
      fprintf(stderr, _("%s; %s: malformed archive header at %ld\n"),
	      program_name, this->name().c_str(),
	      static_cast<long>(off));
      gold_exit(false);
    }

  const int size_string_size = sizeof hdr->ar_size;
  char size_string[size_string_size + 1];
  memcpy(size_string, hdr->ar_size, size_string_size);
  char* ps = size_string + size_string_size;
  while (ps[-1] == ' ')
    --ps;
  *ps = '\0';

  errno = 0;
  char* end;
  off_t member_size = strtol(size_string, &end, 10);
  if (*end != '\0'
      || member_size < 0
      || (member_size == LONG_MAX && errno == ERANGE))
    {
      fprintf(stderr, _("%s: %s: malformed archive header size at %ld\n"),
	      program_name, this->name().c_str(),
	      static_cast<long>(off));
      gold_exit(false);
    }

  if (hdr->ar_name[0] != '/')
    {
      const char* name_end = strchr(hdr->ar_name, '/');
      if (name_end == NULL
	  || name_end - hdr->ar_name >= static_cast<int>(sizeof hdr->ar_name))
	{
	  fprintf(stderr, _("%s: %s: malformed archive header name at %ld\n"),
		  program_name, this->name().c_str(),
		  static_cast<long>(off));
	  gold_exit(false);
	}
      pname->assign(hdr->ar_name, name_end - hdr->ar_name);
    }
  else if (hdr->ar_name[1] == ' ')
    {
      // This is the symbol table.
      pname->clear();
    }
  else if (hdr->ar_name[1] == '/')
    {
      // This is the extended name table.
      pname->assign(1, '/');
    }
  else
    {
      errno = 0;
      long x = strtol(hdr->ar_name + 1, &end, 10);
      if (*end != ' '
	  || x < 0
	  || (x == LONG_MAX && errno == ERANGE)
	  || static_cast<size_t>(x) >= this->extended_names_.size())
	{
	  fprintf(stderr, _("%s: %s: bad extended name index at %ld\n"),
		  program_name, this->name().c_str(),
		  static_cast<long>(off));
	  gold_exit(false);
	}

      const char* name = this->extended_names_.data() + x;
      const char* name_end = strchr(name, '/');
      if (static_cast<size_t>(name_end - name) > this->extended_names_.size()
	  || name_end[1] != '\n')
	{
	  fprintf(stderr, _("%s: %s: bad extended name entry at header %ld\n"),
		  program_name, this->name().c_str(),
		  static_cast<long>(off));
	  gold_exit(false);
	}
      pname->assign(name, name_end - name);
    }

  return member_size;
}

// Select members from the archive and add them to the link.  We walk
// through the elements in the archive map, and look each one up in
// the symbol table.  If it exists as a strong undefined symbol, we
// pull in the corresponding element.  We have to do this in a loop,
// since pulling in one element may create new undefined symbols which
// may be satisfied by other objects in the archive.

void
Archive::add_symbols(const General_options& options, Symbol_table* symtab,
		     Layout* layout, Input_objects* input_objects)
{
  const size_t armap_size = this->armap_.size();

  bool added_new_object;
  do
    {
      added_new_object = false;
      off_t last = -1;
      for (size_t i = 0; i < armap_size; ++i)
	{
	  if (this->seen_[i])
	    continue;
	  if (this->armap_[i].offset == last)
	    {
	      this->seen_[i] = true;
	      continue;
	    }

	  Symbol* sym = symtab->lookup(this->armap_[i].name);
	  if (sym == NULL)
	    continue;
	  else if (!sym->is_undefined())
	    {
	      this->seen_[i] = true;
	      continue;
	    }
	  else if (sym->binding() == elfcpp::STB_WEAK)
	    continue;

	  // We want to include this object in the link.
	  last = this->armap_[i].offset;
	  this->include_member(options, symtab, layout, input_objects, last);
	  this->seen_[i] = true;
	  added_new_object = true;
	}
    }
  while (added_new_object);
}

// Include an archive member in the link.  OFF is the file offset of
// the member header.

void
Archive::include_member(const General_options& options, Symbol_table* symtab,
			Layout* layout, Input_objects* input_objects,
			off_t off)
{
  std::string n;
  this->read_header(off, &n);

  size_t memoff = off + sizeof(Archive_header);

  // Read enough of the file to pick up the entire ELF header.
  int ehdr_size = elfcpp::Elf_sizes<64>::ehdr_size;
  off_t bytes;
  const unsigned char* p = this->input_file_->file().get_view(memoff,
							      ehdr_size,
							      &bytes);
  if (bytes < 4)
    {
      fprintf(stderr, _("%s: %s: member at %ld is not an ELF object"),
	      program_name, this->name().c_str(),
	      static_cast<long>(off));
      gold_exit(false);
    }

  static unsigned char elfmagic[4] =
    {
      elfcpp::ELFMAG0, elfcpp::ELFMAG1,
      elfcpp::ELFMAG2, elfcpp::ELFMAG3
    };
  if (memcmp(p, elfmagic, 4) != 0)
    {
      fprintf(stderr, _("%s: %s: member at %ld is not an ELF object"),
	      program_name, this->name().c_str(),
	      static_cast<long>(off));
      gold_exit(false);
    }

  Object* obj = make_elf_object((std::string(this->input_file_->filename())
				 + "(" + n + ")"),
				this->input_file_, memoff, p, bytes);

  input_objects->add_object(obj);

  Read_symbols_data sd;
  obj->read_symbols(&sd);
  obj->layout(options, symtab, layout, &sd);
  obj->add_symbols(symtab, &sd);
}

// Add_archive_symbols methods.

Add_archive_symbols::~Add_archive_symbols()
{
  if (this->this_blocker_ != NULL)
    delete this->this_blocker_;
  // next_blocker_ is deleted by the task associated with the next
  // input file.
}

// Return whether we can add the archive symbols.  We are blocked by
// this_blocker_.  We block next_blocker_.  We also lock the file.

Task::Is_runnable_type
Add_archive_symbols::is_runnable(Workqueue*)
{
  if (this->this_blocker_ != NULL && this->this_blocker_->is_blocked())
    return IS_BLOCKED;
  return IS_RUNNABLE;
}

class Add_archive_symbols::Add_archive_symbols_locker : public Task_locker
{
 public:
  Add_archive_symbols_locker(Task_token& token, Workqueue* workqueue,
			     File_read& file)
    : blocker_(token, workqueue), filelock_(file)
  { }

 private:
  Task_locker_block blocker_;
  Task_locker_obj<File_read> filelock_;			     
};

Task_locker*
Add_archive_symbols::locks(Workqueue* workqueue)
{
  return new Add_archive_symbols_locker(*this->next_blocker_,
					workqueue,
					this->archive_->file());
}

void
Add_archive_symbols::run(Workqueue*)
{
  this->archive_->add_symbols(this->options_, this->symtab_, this->layout_,
			      this->input_objects_);

  if (this->input_group_ != NULL)
    this->input_group_->add_archive(this->archive_);
  else
    {
      // We no longer need to know about this archive.
      delete this->archive_;
    }
}

} // End namespace gold.
