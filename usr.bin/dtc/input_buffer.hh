/*-
 * Copyright (c) 2013 David Chisnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _INPUT_BUFFER_HH_
#define _INPUT_BUFFER_HH_
#include "util.hh"
#include <assert.h>

namespace dtc
{

/**
 * Class encapsulating the input file.  Can be used as a const char*, but has
 * range checking.  Attempting to access anything out of range will return a 0
 * byte.  The input buffer can be cheaply copied, without copying the
 * underlying memory, however it is the user's responsibility to ensure that
 * such copies do not persist beyond the lifetime of the underlying memory.
 *
 * This also contains methods for reporting errors and for consuming the token
 * stream.
 */
class input_buffer
{
	protected:
	/**
	 * The buffer.  This class doesn't own the buffer, but the
	 * mmap_input_buffer subclass does.
	 */
	const char* buffer;
	/**
	 * The size of the buffer.
	 */
	int size;
	private:
	/**
	 * The current place in the buffer where we are reading.  This class
	 * keeps a separate size, pointer, and cursor so that we can move
	 * forwards and backwards and still have checks that we haven't fallen
	 * off either end.
	 */
	int cursor;
	/**
	 * Private constructor.  This is used to create input buffers that
	 * refer to the same memory, but have different cursors.
	 */
	input_buffer(const char* b, int s, int c) : buffer(b), size(s),
		cursor(c) {}
	/**
	 * Reads forward past any spaces.  The DTS format is not whitespace
	 * sensitive and so we want to scan past whitespace when reading it.
	 */
	void skip_spaces();
	public:
	/**
	 * Return whether all input has been consumed.
	 */
	bool finished() { return cursor >= size; }
	/**
	 * Virtual destructor.  Does nothing, but exists so that subclasses
	 * that own the memory can run cleanup code for deallocating it.
	 */
	virtual ~input_buffer() {};
	/**
	 * Constructs an empty buffer.
	 */
	input_buffer() : buffer(0), size(0), cursor(0) {}
	/**
	 * Constructs a new buffer with a specified memory region and size.
	 */
	input_buffer(const char* b, int s) : buffer(b), size(s), cursor(0){}
	/**
	 * Returns a new input buffer referring into this input, clamped to the
	 * specified size.  If the requested buffer would fall outside the
	 * range of this one, then it returns an empty buffer.
	 *
	 * The returned buffer shares the same underlying storage as the
	 * original.  This is intended to be used for splitting up the various
	 * sections of a device tree blob.  Requesting a size of 0 will give a
	 * buffer that extends to the end of the available memory.
	 */
	input_buffer buffer_from_offset(int offset, int s=0);
	/**
	 * Returns true if this buffer has no unconsumed space in it.
	 */
	inline bool empty()
	{
		return cursor >= size;
	}
	/**
	 * Dereferencing operator, allows the buffer to be treated as a char*
	 * and dereferenced to give a character.  This returns a null byte if
	 * the cursor is out of range.
	 */
	inline char operator*()
	{
		if (cursor >= size) { return '\0'; }
		if (cursor < 0) { return '\0'; }
		return buffer[cursor];
	}
	/**
	 * Array subscripting operator, returns a character at the specified
	 * index offset from the current cursor.  The offset may be negative,
	 * to reread characters that have already been read.  If the current
	 * cursor plus offset is outside of the range, this returns a nul
	 * byte.
	 */
	inline char operator[](int offset)
	{
		if (cursor + offset >= size) { return '\0'; }
		if (cursor + offset < 0) { return '\0'; }
		return buffer[cursor + offset];
	}
	/**
	 * Increments the cursor, iterating forward in the buffer.
	 */
	inline input_buffer &operator++()
	{
		cursor++; 
		return *this;
	}
	/**
	 * Cast to char* operator.  Returns a pointer into the buffer that can
	 * be used for constructing strings.
	 */
	inline operator const char*()
	{
		if (cursor >= size) { return 0; }
		if (cursor < 0) { return 0; }
		return &buffer[cursor];
	}
	/**
	 * Consumes a character.  Moves the cursor one character forward if the
	 * next character matches the argument, returning true.  If the current
	 * character does not match the argument, returns false.
	 */
	inline bool consume(char c)
	{
		if ((*this)[0] == c) 
		{
			++(*this);
			return true;
		}
		return false;
	}
	/**
	 * Consumes a string.  If the (null-terminated) string passed as the
	 * argument appears in the input, advances the cursor to the end and
	 * returns true.  Returns false if the string does not appear at the
	 * current point in the input.
	 */
	bool consume(const char *str);
	/**
	 * Reads an integer in base 8, 10, or 16.  Returns true and advances
	 * the cursor to the end of the integer if the cursor points to an
	 * integer, returns false and does not move the cursor otherwise.
	 *
	 * The parsed value is returned via the argument.
	 */
	bool consume_integer(unsigned long long &outInt);
	/**
	 * Template function that consumes a binary value in big-endian format
	 * from the input stream.  Returns true and advances the cursor if
	 * there is a value of the correct size.  This function assumes that
	 * all values must be natively aligned, and so advances the cursor to
	 * the correct alignment before reading.
	 */
	template<typename T>
	bool consume_binary(T &out)
	{
		int align = 0;
		int type_size = sizeof(T);
		if (cursor % type_size != 0)
		{
			align = type_size - (cursor % type_size);
		}
		if (size < cursor + align + type_size)
		{
			return false;
		}
		cursor += align;
		assert(cursor % type_size == 0);
		out = 0;
		for (int i=0 ; i<type_size ; ++i)
		{
			out <<= 8;
			out |= (((T)buffer[cursor++]) & 0xff);
		}
		return true;
	}
	/**
	 * Consumes two hex digits and return the resulting byte via the first
	 * argument.  If the next two characters are hex digits, returns true
	 * and advances the cursor.  If not, then returns false and leaves the
	 * cursor in place.
	 */
	bool consume_hex_byte(uint8_t &outByte);
	/**
	 * Advances the cursor to the start of the next token, skipping
	 * comments and whitespace.  If the cursor already points to the start
	 * of a token, then this function does nothing.
	 */
	input_buffer &next_token();
	/**
	 * Prints a message indicating the location of a parse error.
	 */
	void parse_error(const char *msg);
	/**
	 * Dumps the current cursor value and the unconsumed values in the
	 * input buffer to the standard error.  This method is intended solely
	 * for debugging.
	 */
	void dump();
};
/**
 * Explicit specialisation for reading a single byte.
 */
template<>
inline bool input_buffer::consume_binary(uint8_t &out)
{
	if (size < cursor + 1)
	{
		return false;
	}
	out = buffer[cursor++];
	return true;
}

/**
 * Subclass of input_buffer that mmap()s a file and owns the resulting memory.
 * When this object is destroyed, the memory is unmapped.
 */
struct mmap_input_buffer : public input_buffer
{
	/**
	 * Constructs a new buffer from the file passed in as a file
	 * descriptor.  
	 */
	mmap_input_buffer(int fd);
	/**
	 * Unmaps the buffer, if one exists.
	 */
	virtual ~mmap_input_buffer();
};
/**
 * Input buffer read from standard input.  This is used for reading device tree
 * blobs and source from standard input.  It reads the entire input into
 * malloc'd memory, so will be very slow for large inputs.  DTS and DTB files
 * are very rarely more than 10KB though, so this is probably not a problem.
 */
struct stream_input_buffer : public input_buffer
{
	/**
	 * The buffer that will store the data read from the standard input.
	 */
	std::vector<char> b;
	/**
	 * Constructs a new buffer from the standard input.
	 */
	stream_input_buffer();
};

} // namespace dtc

#endif // !_INPUT_BUFFER_HH_
