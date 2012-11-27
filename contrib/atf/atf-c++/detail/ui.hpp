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

#if !defined(_ATF_CXX_UI_HPP_)
#define _ATF_CXX_UI_HPP_

#include <string>

namespace atf {
namespace ui {

//!
//! \brief Formats an error message to fit on screen.
//!
//! Given the program's name and an error message, properly formats it to
//! fit on screen.
//!
//! The program's name is not stored globally to prevent the usage of this
//! function from inside the library.  Making it a explicit parameter
//! restricts its usage to the frontend.
//!
std::string format_error(const std::string&, const std::string&);

//!
//! \brief Formats an informational message to fit on screen.
//!
//! Given the program's name and an informational message, properly formats
//! it to fit on screen.
//!
//! The program's name is not stored globally to prevent the usage of this
//! function from inside the library.  Making it a explicit parameter
//! restricts its usage to the frontend.
//!
std::string format_info(const std::string&, const std::string&);

//!
//! \brief Formats a block of text to fit nicely on screen.
//!
//! Given a text, which is composed of multiple paragraphs separated by
//! a single '\n' character, reformats it to fill on the current screen's
//! width with proper line wrapping.
//!
//! This is just a special case of format_text_with_tag, provided for
//! simplicity.
//!
std::string format_text(const std::string&);

//!
//! \brief Formats a block of text to fit nicely on screen, prepending a
//! tag to it.
//!
//! Given a text, which is composed of multiple paragraphs separated by
//! a single '\n' character, reformats it to fill on the current screen's
//! width with proper line wrapping.  The text is prepended with a tag;
//! i.e. a word that is printed at the beginning of the first paragraph and
//! optionally repeated at the beginning of each word.  The last parameter
//! specifies the column on which the text should start, and that position
//! must be greater than the tag's length or 0, in which case it
//! automatically takes the correct value.
//!
std::string format_text_with_tag(const std::string&, const std::string&,
                                 bool, size_t = 0);

//!
//! \brief Formats a warning message to fit on screen.
//!
//! Given the program's name and a warning message, properly formats it to
//! fit on screen.
//!
//! The program's name is not stored globally to prevent the usage of this
//! function from inside the library.  Making it a explicit parameter
//! restricts its usage to the frontend.
//!
std::string format_warning(const std::string&, const std::string&);

} // namespace ui
} // namespace atf

#endif // !defined(_ATF_CXX_UI_HPP_)
