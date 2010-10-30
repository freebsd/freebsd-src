// archive.h -- archive support for gold      -*- C++ -*-

#ifndef GOLD_ARCHIVE_H
#define GOLD_ARCHIVE_H

#include <string>
#include <vector>

#include "workqueue.h"

namespace gold
{

class General_options;
class Input_file;
class Input_objects;
class Input_group;
class Layout;
class Symbol_table;

// This class represents an archive--generally a libNAME.a file.
// Archives have a symbol table and a list of objects.

class Archive
{
 public:
  Archive(const std::string& name, Input_file* input_file)
    : name_(name), input_file_(input_file), armap_(), extended_names_()
  { }

  // The length of the magic string at the start of an archive.
  static const int sarmag = 8;

  // The magic string at the start of an archive.
  static const char armag[sarmag];

  // The string expected at the end of an archive member header.
  static const char arfmag[2];

  // The name of the object.
  const std::string&
  name() const
  { return this->name_; }

  // Set up the archive: read the symbol map.
  void
  setup();

  // Get a reference to the underlying file.
  File_read&
  file()
  { return this->input_file_->file(); }

  // Lock the underlying file.
  void
  lock()
  { this->input_file_->file().lock(); }

  // Unlock the underlying file.
  void
  unlock()
  { this->input_file_->file().unlock(); }

  // Return whether the underlying file is locked.
  bool
  is_locked() const
  { return this->input_file_->file().is_locked(); }

  // Select members from the archive as needed and add them to the
  // link.
  void
  add_symbols(const General_options&, Symbol_table*, Layout*, Input_objects*);

 private:
  Archive(const Archive&);
  Archive& operator=(const Archive&);

  struct Archive_header;

  // Get a view into the underlying file.
  const unsigned char*
  get_view(off_t start, off_t size)
  { return this->input_file_->file().get_view(start, size); }

  // Read an archive member header at OFF.  Return the size of the
  // member, and set *PNAME to the name.
  off_t
  read_header(off_t off, std::string* pname);

  // Include an archive member in the link.
  void
  include_member(const General_options&, Symbol_table*, Layout*,
		 Input_objects*, off_t off);

  // An entry in the archive map of symbols to object files.
  struct Armap_entry
  {
    // The symbol name.
    const char* name;
    // The offset to the file.
    off_t offset;
  };

  // Name of object as printed to user.
  std::string name_;
  // For reading the file.
  Input_file* input_file_;
  // The archive map.
  std::vector<Armap_entry> armap_;
  // The extended name table.
  std::string extended_names_;
  // Track which symbols in the archive map are for elements which
  // have already been included in the link.
  std::vector<bool> seen_;
};

// This class is used to read an archive and pick out the desired
// elements and add them to the link.

class Add_archive_symbols : public Task
{
 public:
  Add_archive_symbols(const General_options& options, Symbol_table* symtab,
		      Layout* layout, Input_objects* input_objects,
		      Archive* archive, Input_group* input_group,
		      Task_token* this_blocker,
		      Task_token* next_blocker)
    : options_(options), symtab_(symtab), layout_(layout),
      input_objects_(input_objects), archive_(archive),
      input_group_(input_group), this_blocker_(this_blocker),
      next_blocker_(next_blocker)
  { }

  ~Add_archive_symbols();

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  class Add_archive_symbols_locker;

  const General_options& options_;
  Symbol_table* symtab_;
  Layout* layout_;
  Input_objects* input_objects_;
  Archive* archive_;
  Input_group* input_group_;
  Task_token* this_blocker_;
  Task_token* next_blocker_;
};

} // End namespace gold.

#endif // !defined(GOLD_ARCHIVE_H)
