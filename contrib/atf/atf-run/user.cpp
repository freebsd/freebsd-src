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

extern "C" {
#include <sys/types.h>

#include <pwd.h>
#include <unistd.h>

#include "../atf-c/detail/user.h"
}

#include <stdexcept>
#include <string>

#include "../atf-c++/detail/sanity.hpp"

#include "user.hpp"

namespace impl = atf::atf_run;
#define IMPL_NAME "atf::atf_run"

uid_t
impl::euid(void)
{
    return atf_user_euid();
}

void
impl::drop_privileges(const std::pair< int, int > ids)
{
    if (::setgid(ids.second) == -1)
        throw std::runtime_error("Failed to drop group privileges");
    if (::setuid(ids.first) == -1)
        throw std::runtime_error("Failed to drop user privileges");
}

std::pair< int, int >
impl::get_user_ids(const std::string& user)
{
    const struct passwd* pw = ::getpwnam(user.c_str());
    if (pw == NULL)
        throw std::runtime_error("Failed to get information for user " + user);
    return std::make_pair(pw->pw_uid, pw->pw_gid);
}

bool
impl::is_member_of_group(gid_t gid)
{
    return atf_user_is_member_of_group(gid);
}

bool
impl::is_root(void)
{
    return atf_user_is_root();
}

bool
impl::is_unprivileged(void)
{
    return atf_user_is_unprivileged();
}
