/*
 * Copyright (c) 2015, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <strstream>

#include "ucl.h"

// C++11 API inspired by json11: https://github.com/dropbox/json11/

namespace ucl {

struct ucl_map_construct_t { };
constexpr ucl_map_construct_t ucl_map_construct = ucl_map_construct_t();
struct ucl_array_construct_t { };
constexpr ucl_array_construct_t ucl_array_construct = ucl_array_construct_t();

class Ucl final {
private:

	struct ucl_deleter {
		void operator() (ucl_object_t *obj) {
			ucl_object_unref (obj);
		}
	};

	static int
	append_char (unsigned char c, size_t nchars, void *ud)
	{
		std::string *out = reinterpret_cast<std::string *>(ud);

		out->append (nchars, (char)c);

		return nchars;
	}
	static int
	append_len (unsigned const char *str, size_t len, void *ud)
	{
		std::string *out = reinterpret_cast<std::string *>(ud);

		out->append ((const char *)str, len);

		return len;
	}
	static int
	append_int (int64_t elt, void *ud)
	{
		std::string *out = reinterpret_cast<std::string *>(ud);
		auto nstr = std::to_string (elt);

		out->append (nstr);

		return nstr.size ();
	}
	static int
	append_double (double elt, void *ud)
	{
		std::string *out = reinterpret_cast<std::string *>(ud);
		auto nstr = std::to_string (elt);

		out->append (nstr);

		return nstr.size ();
	}

	static struct ucl_emitter_functions default_emit_funcs()
	{
		struct ucl_emitter_functions func = {
			Ucl::append_char,
			Ucl::append_len,
			Ucl::append_int,
			Ucl::append_double,
			nullptr,
			nullptr
		};

		return func;
	};

	std::unique_ptr<ucl_object_t, ucl_deleter> obj;

public:
	class const_iterator {
	private:
		struct ucl_iter_deleter {
			void operator() (ucl_object_iter_t it) {
				ucl_object_iterate_free (it);
			}
		};
		std::shared_ptr<void> it;
		std::unique_ptr<Ucl> cur;
	public:
		typedef std::forward_iterator_tag iterator_category;

		const_iterator(const Ucl &obj) {
			it = std::shared_ptr<void>(ucl_object_iterate_new (obj.obj.get()),
					ucl_iter_deleter());
			cur.reset (new Ucl(ucl_object_iterate_safe (it.get(), true)));
		}

		const_iterator() {}
		const_iterator(const const_iterator &other) {
			it = other.it;
		}
		~const_iterator() {}

		const_iterator& operator=(const const_iterator &other) {
			it = other.it;
			return *this;
		}

		bool operator==(const const_iterator &other) const
		{
			if (cur && other.cur) {
				return cur->obj.get() == other.cur->obj.get();
			}

			return !cur && !other.cur;
		}

		bool operator!=(const const_iterator &other) const
		{
			return !(*this == other);
		}

		const_iterator& operator++()
		{
			if (it) {
				cur.reset (new Ucl(ucl_object_iterate_safe (it.get(), true)));
			}

			if (!*cur) {
				it.reset ();
				cur.reset ();
			}

			return *this;
		}

		const Ucl& operator*() const
		{
			return *cur;
		}
		const Ucl* operator->() const
		{
			return cur.get();
		}
	};

	// We grab ownership if get non-const ucl_object_t
	Ucl(ucl_object_t *other) {
		obj.reset (other);
	}

	// Shared ownership
	Ucl(const ucl_object_t *other) {
		obj.reset (ucl_object_ref (other));
	}

	Ucl(const Ucl &other) {
		obj.reset (ucl_object_ref (other.obj.get()));
	}

	Ucl(Ucl &&other) {
		obj.swap (other.obj);
	}

	Ucl() noexcept {
		obj.reset (ucl_object_typed_new (UCL_NULL));
	}
	Ucl(std::nullptr_t) noexcept {
		obj.reset (ucl_object_typed_new (UCL_NULL));
	}
	Ucl(double value) {
		obj.reset (ucl_object_typed_new (UCL_FLOAT));
		obj->value.dv = value;
	}
	Ucl(int64_t value) {
		obj.reset (ucl_object_typed_new (UCL_INT));
		obj->value.iv = value;
	}
	Ucl(bool value) {
		obj.reset (ucl_object_typed_new (UCL_BOOLEAN));
		obj->value.iv = static_cast<int64_t>(value);
	}
	Ucl(const std::string &value) {
		obj.reset (ucl_object_fromstring_common (value.data (), value.size (),
				UCL_STRING_RAW));
	}
	Ucl(const char * value) {
		obj.reset (ucl_object_fromstring_common (value, 0, UCL_STRING_RAW));
	}

	// Implicit constructor: anything with a to_json() function.
	template <class T, class = decltype(&T::to_ucl)>
	Ucl(const T & t) : Ucl(t.to_ucl()) {}

	// Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
	template <class M, typename std::enable_if<
		std::is_constructible<std::string, typename M::key_type>::value
		&& std::is_constructible<Ucl, typename M::mapped_type>::value,
		int>::type = 0>
	Ucl(const M & m) {
		obj.reset (ucl_object_typed_new (UCL_OBJECT));
		auto cobj = obj.get ();

		for (const auto &e : m) {
			ucl_object_insert_key (cobj, ucl_object_ref (e.second.obj.get()),
					e.first.data (), e.first.size (), true);
		}
	}

	// Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
	template <class V, typename std::enable_if<
		std::is_constructible<Ucl, typename V::value_type>::value,
		int>::type = 0>
	Ucl(const V & v) {
		obj.reset (ucl_object_typed_new (UCL_ARRAY));
		auto cobj = obj.get ();

		for (const auto &e : v) {
			ucl_array_append (cobj, ucl_object_ref (e.obj.get()));
		}
	}

	ucl_type_t type () const {
		if (obj) {
			return ucl_object_type (obj.get ());
		}
		return UCL_NULL;
	}

	const std::string key () const {
		std::string res;

		if (obj->key) {
			res.assign (obj->key, obj->keylen);
		}

		return res;
	}

	double number_value () const
	{
		if (obj) {
			return ucl_object_todouble (obj.get());
		}

		return 0.0;
	}

	int64_t int_value () const
	{
		if (obj) {
			return ucl_object_toint (obj.get());
		}

		return 0;
	}

	bool bool_value () const
	{
		if (obj) {
			return ucl_object_toboolean (obj.get());
		}

		return false;
	}

	const std::string string_value () const
	{
		std::string res;

		if (obj) {
			res.assign (ucl_object_tostring (obj.get()));
		}

		return res;
	}

	const Ucl operator[] (size_t i) const
	{
		if (type () == UCL_ARRAY) {
			return Ucl (ucl_array_find_index (obj.get(), i));
		}

		return Ucl (nullptr);
	}

	const Ucl operator[](const std::string &key) const
	{
		if (type () == UCL_OBJECT) {
			return Ucl (ucl_object_find_keyl (obj.get(),
					key.data (), key.size ()));
		}

		return Ucl (nullptr);
	}
	// Serialize.
	void dump (std::string &out, ucl_emitter_t type = UCL_EMIT_JSON) const
	{
		struct ucl_emitter_functions cbdata;

		cbdata = Ucl::default_emit_funcs();
		cbdata.ud = reinterpret_cast<void *>(&out);

		ucl_object_emit_full (obj.get(), type, &cbdata);
	}

	std::string dump (ucl_emitter_t type = UCL_EMIT_JSON) const
	{
		std::string out;

		dump (out, type);

		return out;
	}

	static Ucl parse (const std::string & in, std::string & err)
	{
		auto parser = ucl_parser_new (UCL_PARSER_DEFAULT);

		if (!ucl_parser_add_chunk (parser, (const unsigned char *)in.data (),
				in.size ())) {
			err.assign (ucl_parser_get_error (parser));
			ucl_parser_free (parser);

			return nullptr;
		}

		auto obj = ucl_parser_get_object (parser);
		ucl_parser_free (parser);

		// Obj will handle ownership
		return Ucl (obj);
	}

	static Ucl parse (const char * in, std::string & err)
	{
		if (in) {
			return parse (std::string(in), err);
		} else {
			err = "null input";
			return nullptr;
		}
	}

	static Ucl parse (std::istream &ifs, std::string &err)
	{
		return Ucl::parse (std::string(std::istreambuf_iterator<char>(ifs),
				std::istreambuf_iterator<char>()), err);
	}

	bool operator== (const Ucl &rhs) const
	{
		return ucl_object_compare (obj.get(), rhs.obj.get ()) == 0;
	}
	bool operator< (const Ucl &rhs) const
	{
		return ucl_object_compare (obj.get(), rhs.obj.get ()) < 0;
	}
	bool operator!= (const Ucl &rhs) const { return !(*this == rhs); }
	bool operator<= (const Ucl &rhs) const { return !(rhs < *this); }
	bool operator> (const Ucl &rhs) const { return (rhs < *this); }
	bool operator>= (const Ucl &rhs) const { return !(*this < rhs); }

	operator bool () const
	{
		if (!obj || type() == UCL_NULL) {
			return false;
		}

		if (type () == UCL_BOOLEAN) {
			return bool_value ();
		}

		return true;
	}

	const_iterator begin() const
	{
		return const_iterator(*this);
	}
	const_iterator cbegin() const
	{
		return const_iterator(*this);
	}
	const_iterator end() const
	{
		return const_iterator();
	}
	const_iterator cend() const
	{
		return const_iterator();
	}
};

};
