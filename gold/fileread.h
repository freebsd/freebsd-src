// fileread.h -- read files for gold   -*- C++ -*-

// Classes used to read data from binary input files.

#ifndef GOLD_FILEREAD_H
#define GOLD_FILEREAD_H

#include <list>
#include <map>
#include <string>

#include "options.h"

namespace gold
{

class Dirsearch;
class File_view;

// File_read manages a file descriptor for a file we are reading.  We
// close file descriptors if we run out of them, so this class reopens
// the file as needed.

class File_read
{
 public:
  File_read()
    : name_(), descriptor_(-1), lock_count_(0), views_(),
      saved_views_(), contents_(NULL), contents_size_(0)
  { }

  ~File_read();

  // Open a file.
  bool
  open(const std::string& name);

  // Pretend to open the file, but provide the file contents.  No
  // actual file system activity will occur.  This is used for
  // testing.
  bool
  open(const std::string& name, const unsigned char* contents, off_t size);

  // Return the file name.
  const std::string&
  filename() const
  { return this->name_; }

  // Lock the file for access within a particular Task::run execution.
  // This means that the descriptor can not be closed.  This routine
  // may only be called from the main thread.
  void
  lock();

  // Unlock the descriptor, permitting it to be closed if necessary.
  void
  unlock();
  
  // Test whether the object is locked.
  bool
  is_locked();

  // Return a view into the file.  The pointer will remain valid until
  // the File_read is unlocked.  If PBYTES is NULL, it is an error if
  // we can not read enough data.  Otherwise *PBYTES is set to the
  // number of bytes read.
  const unsigned char*
  get_view(off_t start, off_t size, off_t *pbytes = NULL);

  // Read data from the file into the buffer P.  PBYTES is as in
  // get_view.
  void
  read(off_t start, off_t size, void* p, off_t *pbytes = NULL);

  // Return a lasting view into the file.  This is allocated with new,
  // and the caller is responsible for deleting it when done.  The
  // data associated with this view will remain valid until the view
  // is deleted.  PBYTES is handled as with get_view.
  File_view*
  get_lasting_view(off_t start, off_t size, off_t *pbytes = NULL);

 private:
  // This class may not be copied.
  File_read(const File_read&);
  File_read& operator=(const File_read&);

  // A view into the file when not using mmap.
  class View
  {
   public:
    View(off_t start, off_t size, unsigned char* data)
      : start_(start), size_(size), data_(data), lock_count_(0)
    { }

    ~View();

    off_t
    start() const
    { return this->start_; }

    off_t
    size() const
    { return this->size_; }

    unsigned char*
    data() const
    { return this->data_; }

    void
    lock();

    void
    unlock();

    bool
    is_locked();

   private:
    View(const View&);
    View& operator=(const View&);

    off_t start_;
    off_t size_;
    unsigned char* data_;
    int lock_count_;
  };

  friend class File_view;

  // Find a view into the file.
  View*
  find_view(off_t start, off_t size);

  // Read data from the file into a buffer.
  off_t
  do_read(off_t start, off_t size, void* p, off_t* pbytes);

  // Find or make a view into the file.
  View*
  find_or_make_view(off_t start, off_t size, off_t* pbytes);

  // Clear the file views.
  void
  clear_views(bool);

  // The size of a file page for buffering data.
  static const off_t page_size = 8192;

  // Given a file offset, return the page offset.
  static off_t
  page_offset(off_t file_offset)
  { return file_offset & ~ (page_size - 1); }

  // Given a file size, return the size to read integral pages.
  static off_t
  pages(off_t file_size)
  { return (file_size + (page_size - 1)) & ~ (page_size - 1); }

  // The type of a mapping from page start to views.
  typedef std::map<off_t, View*> Views;

  // A simple list of Views.
  typedef std::list<View*> Saved_views;

  // File name.
  std::string name_;
  // File descriptor.
  int descriptor_;
  // Number of locks on the file.
  int lock_count_;
  // Buffered views into the file.
  Views views_;
  // List of views which were locked but had to be removed from views_
  // because they were not large enough.
  Saved_views saved_views_;
  // Specified file contents.  Used only for testing purposes.
  const unsigned char* contents_;
  // Specified file size.  Used only for testing purposes.
  off_t contents_size_;
};

// A view of file data that persists even when the file is unlocked.
// Callers should destroy these when no longer required.  These are
// obtained form File_read::get_lasting_view.  They may only be
// destroyed when the underlying File_read is locked.

class File_view
{
 public:
  // This may only be called when the underlying File_read is locked.
  ~File_view();

  // Return a pointer to the data associated with this view.
  const unsigned char*
  data() const
  { return this->data_; }

 private:
  File_view(const File_view&);
  File_view& operator=(const File_view&);

  friend class File_read;

  // Callers have to get these via File_read::get_lasting_view.
  File_view(File_read& file, File_read::View* view, const unsigned char* data)
    : file_(file), view_(view), data_(data)
  { }

  File_read& file_;
  File_read::View* view_;
  const unsigned char* data_;
};

// All the information we hold for a single input file.  This can be
// an object file, a shared library, or an archive.

class Input_file
{
 public:
  Input_file(const Input_file_argument* input_argument)
    : input_argument_(input_argument), file_()
  { }

  // Create an input file with the contents already provided.  This is
  // only used for testing.  With this path, don't call the open
  // method.
  Input_file(const char* name, const unsigned char* contents, off_t size);

  // Open the file.
  void
  open(const General_options&, const Dirsearch&);

  // Return the name given by the user.
  const char*
  name() const
  { return this->input_argument_->name(); }

  // Return the file name.
  const std::string&
  filename() const
  { return this->file_.filename(); }

  File_read&
  file()
  { return this->file_; }

 private:
  Input_file(const Input_file&);
  Input_file& operator=(const Input_file&);

  const Input_file_argument* input_argument_;
  File_read file_;
};

} // end namespace gold

#endif // !defined(GOLD_FILEREAD_H)
