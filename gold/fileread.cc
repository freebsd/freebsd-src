// fileread.cc -- read files for gold

#include "gold.h"

#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#include "options.h"
#include "dirsearch.h"
#include "fileread.h"

namespace gold
{

// Class File_read::View.

File_read::View::~View()
{
  gold_assert(!this->is_locked());
  delete[] this->data_;
}

void
File_read::View::lock()
{
  ++this->lock_count_;
}

void
File_read::View::unlock()
{
  gold_assert(this->lock_count_ > 0);
  --this->lock_count_;
}

bool
File_read::View::is_locked()
{
  return this->lock_count_ > 0;
}

// Class File_read.

// The File_read class is designed to support file descriptor caching,
// but this is not currently implemented.

File_read::~File_read()
{
  gold_assert(this->lock_count_ == 0);
  if (this->descriptor_ >= 0)
    {
      if (close(this->descriptor_) < 0)
	fprintf(stderr, _("%s: warning: close(%s) failed: %s"),
		program_name, this->name_.c_str(), strerror(errno));
      this->descriptor_ = -1;
    }
  this->name_.clear();
  this->clear_views(true);
}

// Open the file.

bool
File_read::open(const std::string& name)
{
  gold_assert(this->lock_count_ == 0
	      && this->descriptor_ < 0
	      && this->name_.empty());
  this->name_ = name;
  this->descriptor_ = ::open(this->name_.c_str(), O_RDONLY);
  ++this->lock_count_;
  return this->descriptor_ >= 0;
}

// Open the file for testing purposes.

bool
File_read::open(const std::string& name, const unsigned char* contents,
		off_t contents_size)
{
  gold_assert(this->lock_count_ == 0
	      && this->descriptor_ < 0
	      && this->name_.empty());
  this->name_ = name;
  this->contents_ = contents;
  this->contents_size_ = contents_size;
  ++this->lock_count_;
  return true;
}

void
File_read::lock()
{
  ++this->lock_count_;
}

void
File_read::unlock()
{
  gold_assert(this->lock_count_ > 0);
  --this->lock_count_;
}

bool
File_read::is_locked()
{
  return this->lock_count_ > 0;
}

// See if we have a view which covers the file starting at START for
// SIZE bytes.  Return a pointer to the View if found, NULL if not.

inline File_read::View*
File_read::find_view(off_t start, off_t size)
{
  off_t page = File_read::page_offset(start);
  Views::iterator p = this->views_.find(page);
  if (p == this->views_.end())
    return NULL;
  if (p->second->size() - (start - page) < size)
    return NULL;
  return p->second;
}

// Read data from the file.  Return the number of bytes read.  If
// PBYTES is not NULL, store the number of bytes in *PBYTES, otherwise
// require that we read exactly the number of bytes requested.

off_t
File_read::do_read(off_t start, off_t size, void* p, off_t* pbytes)
{
  gold_assert(this->lock_count_ > 0);

  off_t bytes;
  if (this->contents_ == NULL)
    {
      int o = this->descriptor_;

      if (lseek(o, start, SEEK_SET) < 0)
	{
	  fprintf(stderr, _("%s: %s: lseek to %lld failed: %s"),
		  program_name, this->filename().c_str(),
		  static_cast<long long>(start),
		  strerror(errno));
	  gold_exit(false);
	}

      bytes = ::read(o, p, size);
      if (bytes < 0)
	{
	  fprintf(stderr, _("%s: %s: read failed: %s\n"),
		  program_name, this->filename().c_str(), strerror(errno));
	  gold_exit(false);
	}
    }
  else
    {
      bytes = this->contents_size_ - start;
      if (bytes < 0)
	bytes = 0;
      else if (bytes > size)
	bytes = size;
      memcpy(p, this->contents_ + start, bytes);
    }

  if (pbytes != NULL)
    *pbytes = bytes;
  else if (bytes != size)
    {
      fprintf(stderr,
	      _("%s: %s: file too short: read only %lld of %lld "
		"bytes at %lld\n"),
	      program_name, this->filename().c_str(),
	      static_cast<long long>(bytes),
	      static_cast<long long>(size),
	      static_cast<long long>(start));
      gold_exit(false);
    }

  return bytes;
}

void
File_read::read(off_t start, off_t size, void* p, off_t* pbytes)
{
  gold_assert(this->lock_count_ > 0);

  File_read::View* pv = this->find_view(start, size);
  if (pv != NULL)
    {
      memcpy(p, pv->data() + (start - pv->start()), size);
      if (pbytes != NULL)
	*pbytes = size;
      return;
    }

  this->do_read(start, size, p, pbytes);
}

// Find an existing view or make a new one.

File_read::View*
File_read::find_or_make_view(off_t start, off_t size, off_t* pbytes)
{
  gold_assert(this->lock_count_ > 0);

  off_t poff = File_read::page_offset(start);

  File_read::View* const vnull = NULL;
  std::pair<Views::iterator, bool> ins =
    this->views_.insert(std::make_pair(poff, vnull));

  if (!ins.second)
    {
      // There was an existing view at this offset.
      File_read::View* v = ins.first->second;
      if (v->size() - (start - v->start()) >= size)
	{
	  if (pbytes != NULL)
	    *pbytes = size;
	  return v;
	}

      // This view is not large enough.
      this->saved_views_.push_back(v);
    }

  // We need to read data from the file.

  off_t psize = File_read::pages(size + (start - poff));
  unsigned char* p = new unsigned char[psize];

  off_t got_bytes;
  off_t bytes = this->do_read(poff, psize, p, &got_bytes);

  File_read::View* v = new File_read::View(poff, bytes, p);

  ins.first->second = v;

  if (bytes - (start - poff) >= size)
    {
      if (pbytes != NULL)
	*pbytes = size;
      return v;
    }

  if (pbytes != NULL)
    {
      *pbytes = bytes - (start - poff);
      return v;
    }

  fprintf(stderr,
	  _("%s: %s: file too short: read only %lld of %lld bytes at %lld\n"),
	  program_name, this->filename().c_str(),
	  static_cast<long long>(bytes - (start - poff)),
	  static_cast<long long>(size),
	  static_cast<long long>(start));
  gold_exit(false);
}

// This implementation of get_view just reads into a memory buffer,
// which we store on view_list_.  At some point we should support
// mmap.

const unsigned char*
File_read::get_view(off_t start, off_t size, off_t* pbytes)
{
  gold_assert(this->lock_count_ > 0);
  File_read::View* pv = this->find_or_make_view(start, size, pbytes);
  return pv->data() + (start - pv->start());
}

File_view*
File_read::get_lasting_view(off_t start, off_t size, off_t* pbytes)
{
  gold_assert(this->lock_count_ > 0);
  File_read::View* pv = this->find_or_make_view(start, size, pbytes);
  pv->lock();
  return new File_view(*this, pv, pv->data() + (start - pv->start()));
}

// Remove all the file views.

void
File_read::clear_views(bool destroying)
{
  for (Views::iterator p = this->views_.begin();
       p != this->views_.end();
       ++p)
    {
      if (!p->second->is_locked())
	delete p->second;
      else
	{
	  gold_assert(!destroying);
	  this->saved_views_.push_back(p->second);
	}
    }
  this->views_.clear();

  Saved_views::iterator p = this->saved_views_.begin();
  while (p != this->saved_views_.end())
    {
      if (!(*p)->is_locked())
	{
	  delete *p;
	  p = this->saved_views_.erase(p);
	}
      else
	{
	  gold_assert(!destroying);
	  ++p;
	}
    }
}

// Class File_view.

File_view::~File_view()
{
  gold_assert(this->file_.is_locked());
  this->view_->unlock();
}

// Class Input_file.

// Create a file for testing.

Input_file::Input_file(const char* name, const unsigned char* contents,
		       off_t size)
  : file_()
{
  this->input_argument_ =
    new Input_file_argument(name, false, Position_dependent_options());
  bool ok = file_.open(name, contents, size);
  gold_assert(ok);
}

// Open the file.

void
Input_file::open(const General_options& options, const Dirsearch& dirpath)
{
  std::string name;
  if (!this->input_argument_->is_lib())
    name = this->input_argument_->name();
  else
    {
      std::string n1("lib");
      n1 += this->input_argument_->name();
      std::string n2;
      if (options.is_static())
	n1 += ".a";
      else
	{
	  n2 = n1 + ".a";
	  n1 += ".so";
	}
      name = dirpath.find(n1, n2);
      if (name.empty())
	{
	  fprintf(stderr, _("%s: cannot find %s\n"), program_name,
		  this->input_argument_->name());
	  gold_exit(false);
	}
    }

  if (!this->file_.open(name))
    {
      fprintf(stderr, _("%s: cannot open %s: %s\n"), program_name,
	      name.c_str(), strerror(errno));
      gold_exit(false);
    }
}

} // End namespace gold.
