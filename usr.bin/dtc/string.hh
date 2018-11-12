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

#ifndef _STRING_HH_
#define _STRING_HH_
#include "input_buffer.hh"
#include <string>
#include <functional>

namespace dtc
{

/**
 * String, referring to a place in the input file.  We don't bother copying
 * strings until we write them to the final output.  These strings should be
 * two words long: a start and a length.  They are intended to be cheap to copy
 * and store in collections.  Copying the string object does not copy the
 * underlying storage.
 *
 * Strings are not nul-terminated.
 */
class string
{
	friend std::hash<string>;
	/** Start address.  Contained within the mmap()'d input file and not
	 * owned by this object. */
	const char *start;
	/** length of the string.  DTS strings are allowed to contain nuls */
	int length;
	/** Generic function for parsing strings matching the character set
	 * defined by the template argument.  */
	template<class T>
	static string parse(input_buffer &s);
	public:
	/**
	 * Constructs a string referring into another buffer.
	 */
	string(const char *s, int l) : start(s), length(l) {}
	/** Constructs a string from a C string.  */
	string(const char *s) : start(s), length(strlen(s)) {}
	/** Default constructor, returns an empty string. */
	string() : start(0), length(0) {}
	/** Construct a from an input buffer, ending with a nul terminator. */
	string(input_buffer &s);
	/**
	 * Returns the longest string in the input buffer starting at the
	 * current cursor and composed entirely of characters that are valid in
	 * node names.
	 */
	static string parse_node_name(input_buffer &s);
	/**
	 * Returns the longest string in the input buffer starting at the
	 * current cursor and composed entirely of characters that are valid in
	 * property names.
	 */
	static string parse_property_name(input_buffer &s);
	/**
	 * Parses either a node or a property name.  If is_property is true on
	 * entry, then only property names are parsed.  If it is false, then it
	 * will be set, on return, to indicate whether the parsed name is only
	 * valid as a property.
	 */
	static string parse_node_or_property_name(input_buffer &s,
	                                          bool &is_property);
	/**
	 * Compares two strings for equality.  Strings are equal if they refer
	 * to identical byte sequences.
	 */
	bool operator==(const string& other) const;
	/**
	 * Compares a string against a C string.  The trailing nul in the C
	 * string is ignored for the purpose of comparison, so this will always
	 * fail if the string contains nul bytes.
	 */
	bool operator==(const char *other) const;
	/**
	 * Inequality operator, defined as the inverse of the equality
	 * operator.
	 */
	template <typename T>
	inline bool operator!=(T other)
	{
		return !(*this == other);
	}
	/**
	 * Comparison operator, defined to allow strings to be used as keys in
	 * maps.
	 */
	bool operator<(const string& other) const;
	/**
	 * Returns true if this is the empty string, false otherwise.
	 */
	inline bool empty() const
	{
		return length == 0;
	}
	/**
	 * Returns the size of the string, in bytes.
	 */
	inline size_t size()
	{
		return length;
	}
	/**
	 * Writes the string to the specified buffer.
	 */
	void push_to_buffer(byte_buffer &buffer, bool escapes=false);
	/**
	 * Prints the string to the specified output stream.
	 */
	void print(FILE *file);
	/**
	 * Dumps the string to the standard error stream.  Intended to be used
	 * for debugging.
	 */
	void dump();
};

} // namespace dtc
namespace std
{
	template<>
	struct hash<dtc::string>
	{
		std::size_t operator()(dtc::string const& s) const
		{
			std::string str(s.start, s.length);
			std::hash<std::string> h;
			return h(str);
		}
	};
}


#endif // !_STRING_HH_
