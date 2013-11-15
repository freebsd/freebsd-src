//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if !defined(_ATF_RUN_IO_HPP_)
#define _ATF_RUN_IO_HPP_

#include <istream>
#include <ostream>
#include <streambuf>

#include "fs.hpp"

#include "../atf-c++/detail/auto_array.hpp"
#include "../atf-c++/noncopyable.hpp"

namespace atf {
namespace atf_run {

// ------------------------------------------------------------------------
// The "file_handle" class.
// ------------------------------------------------------------------------

//!
//! \brief Simple RAII model for system file handles.
//!
//! The \a file_handle class is a simple RAII model for native system file
//! handles.  This class wraps one of such handles grabbing its ownership,
//! and automaticaly closes it upon destruction.  It is basically used
//! inside the library to avoid leaking open file handles, shall an
//! unexpected execution trace occur.
//!
//! A \a file_handle object can be copied but doing so invalidates the
//! source object.  There can only be a single valid \a file_handle object
//! for a given system file handle.  This is similar to std::auto_ptr\<\>'s
//! semantics.
//!
//! This class also provides some convenience methods to issue special file
//! operations under their respective platforms.
//!
class file_handle
{
public:
    //!
    //! \brief Opaque name for the native handle type.
    //!
    //! Each operating system identifies file handles using a specific type.
    //! The \a handle_type type is used to transparently refer to file
    //! handles regarless of the operating system in which this class is
    //! used.
    //!
    //! If this class is used in a POSIX system, \a NativeSystemHandle is
    //! an integer type while it is a \a HANDLE in a Win32 system.
    //!
    typedef int handle_type;

    //!
    //! \brief Constructs an invalid file handle.
    //!
    //! This constructor creates a new \a file_handle object that represents
    //! an invalid file handle.  An invalid file handle can be copied but
    //! cannot be manipulated in any way (except checking for its validity).
    //!
    //! \see is_valid()
    //!
    file_handle(void);

    //!
    //! \brief Constructs a new file handle from a native file handle.
    //!
    //! This constructor creates a new \a file_handle object that takes
    //! ownership of the given \a h native file handle.  The user must not
    //! close \a h on his own during the lifetime of the new object.
    //! Ownership can be reclaimed using disown().
    //!
    //! \pre The native file handle must be valid; a close operation must
    //!      succeed on it.
    //!
    //! \see disown()
    //!
    file_handle(handle_type h);

    //!
    //! \brief Copy constructor; invalidates the source handle.
    //!
    //! This copy constructor creates a new file handle from a given one.
    //! Ownership of the native file handle is transferred to the new
    //! object, effectively invalidating the source file handle.  This
    //! avoids having two live \a file_handle objects referring to the
    //! same native file handle.  The source file handle need not be
    //! valid in the name of simplicity.
    //!
    //! \post The source file handle is invalid.
    //! \post The new file handle owns the source's native file handle.
    //!
    file_handle(const file_handle& fh);

    //!
    //! \brief Releases resources if the handle is valid.
    //!
    //! If the file handle is valid, the destructor closes it.
    //!
    //! \see is_valid()
    //!
    ~file_handle(void);

    //!
    //! \brief Assignment operator; invalidates the source handle.
    //!
    //! This assignment operator transfers ownership of the RHS file
    //! handle to the LHS one, effectively invalidating the source file
    //! handle.  This avoids having two live \a file_handle objects
    //! referring to the same native file handle.  The source file
    //! handle need not be valid in the name of simplicity.
    //!
    //! \post The RHS file handle is invalid.
    //! \post The LHS file handle owns RHS' native file handle.
    //! \return A reference to the LHS file handle.
    //!
    file_handle& operator=(const file_handle& fh);

    //!
    //! \brief Checks whether the file handle is valid or not.
    //!
    //! Returns a boolean indicating whether the file handle is valid or
    //! not.  If the file handle is invalid, no other applications can be
    //! executed other than the destructor.
    //!
    //! \return True if the file handle is valid; false otherwise.
    //!
    bool is_valid(void) const;

    //!
    //! \brief Closes the file handle.
    //!
    //! Explicitly closes the file handle, which must be valid.  Upon
    //! exit, the handle is not valid any more.
    //!
    //! \pre The file handle is valid.
    //! \post The file handle is invalid.
    //! \post The native file handle is closed.
    //!
    void close(void);

    //!
    //! \brief Reclaims ownership of the native file handle.
    //!
    //! Explicitly reclaims ownership of the native file handle contained
    //! in the \a file_handle object, returning the native file handle.
    //! The caller is responsible of closing it later on.
    //!
    //! \pre The file handle is valid.
    //! \post The file handle is invalid.
    //! \return The native file handle.
    //!
    handle_type disown(void);

    //!
    //! \brief Gets the native file handle.
    //!
    //! Returns the native file handle for the \a file_handle object.
    //! The caller can issue any operation on it except closing it.
    //! If closing is required, disown() shall be used.
    //!
    //! \pre The file handle is valid.
    //! \return The native file handle.
    //!
    handle_type get(void) const;

    //!
    //! \brief Changes the native file handle to the given one.
    //!
    //! Given a new native file handle \a h, this operation assigns this
    //! handle to the current object, closing its old native file handle.
    //! In other words, it first calls dup2() to remap the old handle to
    //! the new one and then closes the old handle.
    //!
    //! If \a h matches the current value of the handle, this is a no-op.
    //! This is done for simplicity, to avoid the caller having to check
    //! this condition on its own.
    //!
    //! If \a h is open, it is automatically closed by dup2().
    //!
    //! This operation is only available in POSIX systems.
    //!
    //! \pre The file handle is valid.
    //! \pre The native file handle \a h is valid; i.e., it must be
    //!      closeable.
    //! \post The file handle's native file handle is \a h.
    //! \throw system_error If the internal remapping operation fails.
    //!
    void posix_remap(handle_type h);

private:
    //!
    //! \brief Internal handle value.
    //!
    //! This variable holds the native handle value for the file handle
    //! hold by this object.  It is interesting to note that this needs
    //! to be mutable because the copy constructor and the assignment
    //! operator invalidate the source object.
    //!
    mutable handle_type m_handle;

    //!
    //! \brief Constant function representing an invalid handle value.
    //!
    //! Returns the platform-specific handle value that represents an
    //! invalid handle.  This is a constant function rather than a regular
    //! constant because, in the latter case, we cannot define it under
    //! Win32 due to the value being of a complex type.
    //!
    static handle_type invalid_value(void);
};

// ------------------------------------------------------------------------
// The "systembuf" class.
// ------------------------------------------------------------------------

//!
//! \brief std::streambuf implementation for system file handles.
//!
//! systembuf provides a std::streambuf implementation for system file
//! handles.  Contrarywise to file_handle, this class does \b not take
//! ownership of the native file handle; this should be taken care of
//! somewhere else.
//!
//! This class follows the expected semantics of a std::streambuf object.
//! However, it is not copyable to avoid introducing inconsistences with
//! the on-disk file and the in-memory buffers.
//!
class systembuf : public std::streambuf, atf::noncopyable
{
public:
    typedef int handle_type;

    //!
    //! \brief Constructs a new systembuf for the given file handle.
    //!
    //! This constructor creates a new systembuf object that reads or
    //! writes data from/to the \a h native file handle.  This handle
    //! is \b not owned by the created systembuf object; the code
    //! should take care of it externally.
    //!
    //! This class buffers input and output; the buffer size may be
    //! tuned through the \a bufsize parameter, which defaults to 8192
    //! bytes.
    //!
    //! \see pistream.
    //!
    explicit systembuf(handle_type h, std::size_t bufsize = 8192);
    ~systembuf(void);

private:
    //!
    //! \brief Native file handle used by the systembuf object.
    //!
    handle_type m_handle;

    //!
    //! \brief Internal buffer size used during read and write operations.
    //!
    std::size_t m_bufsize;

    //!
    //! \brief Internal buffer used during read operations.
    //!
    char* m_read_buf;

    //!
    //! \brief Internal buffer used during write operations.
    //!
    char* m_write_buf;

protected:
    //!
    //! \brief Reads new data from the native file handle.
    //!
    //! This operation is called by input methods when there are no more
    //! data in the input buffer.  The function fills the buffer with new
    //! data, if available.
    //!
    //! \pre All input positions are exhausted (gptr() >= egptr()).
    //! \post The input buffer has new data, if available.
    //! \returns traits_type::eof() if a read error occurrs or there are
    //!          no more data to be read.  Otherwise returns
    //!          traits_type::to_int_type(*gptr()).
    //!
    virtual int_type underflow(void);

    //!
    //! \brief Makes room in the write buffer for additional data.
    //!
    //! This operation is called by output methods when there is no more
    //! space in the output buffer to hold a new element.  The function
    //! first flushes the buffer's contents to disk and then clears it to
    //! leave room for more characters.  The given \a c character is
    //! stored at the beginning of the new space.
    //!
    //! \pre All output positions are exhausted (pptr() >= epptr()).
    //! \post The output buffer has more space if no errors occurred
    //!       during the write to disk.
    //! \post *(pptr() - 1) is \a c.
    //! \returns traits_type::eof() if a write error occurrs.  Otherwise
    //!          returns traits_type::not_eof(c).
    //!
    virtual int_type overflow(int c);

    //!
    //! \brief Flushes the output buffer to disk.
    //!
    //! Synchronizes the systembuf buffers with the contents of the file
    //! associated to this object through the native file handle.  The
    //! output buffer is flushed to disk and cleared to leave new room
    //! for more data.
    //!
    //! \returns 0 on success, -1 if an error occurred.
    //!
    virtual int sync(void);
};

// ------------------------------------------------------------------------
// The "pistream" class.
// ------------------------------------------------------------------------

//!
//! \brief Child process' output stream.
//!
//! The pistream class represents an output communication channel with the
//! child process.  The child process writes data to this stream and the
//! parent process can read it through the pistream object.  In other
//! words, from the child's point of view, the communication channel is an
//! output one, but from the parent's point of view it is an input one;
//! hence the confusing pistream name.
//!
//! pistream objects cannot be copied because they own the file handle
//! they use to communicate with the child and because they buffer data
//! that flows through the communication channel.
//!
//! A pistream object behaves as a std::istream stream in all senses.
//! The class is only provided because it must provide a method to let
//! the caller explicitly close the communication channel.
//!
//! \remark <b>Blocking remarks</b>: Functions that read data from this
//! stream can block if the associated file handle blocks during the read.
//! As this class is used to communicate with child processes through
//! anonymous pipes, the most typical blocking condition happens when the
//! child has no more data to send to the pipe's system buffer.  When
//! this happens, the buffer eventually empties and the system blocks
//! until the writer generates some data.
//!
class pistream : public std::istream, noncopyable
{
    //!
    //! \brief The systembuf object used to manage this stream's data.
    //!
    systembuf m_systembuf;

public:
    //!
    //! \brief Creates a new process' output stream.
    //!
    //! Given a file handle, this constructor creates a new pistream
    //! object that owns the given file handle \a fh.  Ownership of
    //! \a fh is transferred to the created pistream object.
    //!
    //! \pre \a fh is valid.
    //! \post \a fh is invalid.
    //! \post The new pistream object owns \a fh.
    //!
    explicit pistream(const int);
};

// ------------------------------------------------------------------------
// The "muxer" class.
// ------------------------------------------------------------------------

class muxer : noncopyable {
    const int* m_fds;
    const size_t m_nfds;

    const size_t m_bufsize;
    atf::auto_array< std::string > m_buffers;

protected:
    virtual void line_callback(const size_t, const std::string&) = 0;

    size_t read_one(const size_t, const int, std::string&, const bool);

public:
    muxer(const int*, const size_t, const size_t bufsize = 1024);
    virtual ~muxer(void);

    void mux(volatile const bool&);
    void flush(void);
};

} // namespace atf_run
} // namespace atf

#endif // !defined(_ATF_RUN_IO_HPP_)
