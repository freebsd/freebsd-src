// Copyright 2010 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "utils/passwd.hpp"

extern "C" {
#include <sys/types.h>

#include <pwd.h>
#include <unistd.h>
}

#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace passwd_ns = utils::passwd;


namespace {


/// If defined, replaces the value returned by current_user().
static utils::optional< passwd_ns::user > fake_current_user;


/// If not empty, defines the current set of mock users.
static std::vector< passwd_ns::user > mock_users;


/// Formats a user for logging purposes.
///
/// \param user The user to format.
///
/// \return The user as a string.
static std::string
format_user(const passwd_ns::user& user)
{
    return F("name=%s, uid=%s, gid=%s") % user.name % user.uid % user.gid;
}


}  // anonymous namespace


/// Constructs a new user.
///
/// \param name_ The name of the user.
/// \param uid_ The user identifier.
/// \param gid_ The login group identifier.
passwd_ns::user::user(const std::string& name_, const unsigned int uid_,
                      const unsigned int gid_) :
    name(name_),
    uid(uid_),
    gid(gid_)
{
}


/// Checks if the user has superpowers or not.
///
/// \return True if the user is root, false otherwise.
bool
passwd_ns::user::is_root(void) const
{
    return uid == 0;
}


/// Gets the current user.
///
/// \return The current user.
passwd_ns::user
passwd_ns::current_user(void)
{
    if (fake_current_user) {
        const user u = fake_current_user.get();
        LD(F("Current user is fake: %s") % format_user(u));
        return u;
    } else {
        const user u = find_user_by_uid(::getuid());
        LD(F("Current user is: %s") % format_user(u));
        return u;
    }
}


/// Gets information about a user by its name.
///
/// \param name The name of the user to query.
///
/// \return The information about the user.
///
/// \throw std::runtime_error If the user does not exist.
passwd_ns::user
passwd_ns::find_user_by_name(const std::string& name)
{
    if (mock_users.empty()) {
        const struct ::passwd* pw = ::getpwnam(name.c_str());
        if (pw == NULL)
            throw std::runtime_error(F("Failed to get information about the "
                                       "user '%s'") % name);
        INV(pw->pw_name == name);
        return user(pw->pw_name, pw->pw_uid, pw->pw_gid);
    } else {
        for (std::vector< user >::const_iterator iter = mock_users.begin();
             iter != mock_users.end(); iter++) {
            if ((*iter).name == name)
                return *iter;
        }
        throw std::runtime_error(F("Failed to get information about the "
                                   "user '%s'") % name);
    }
}


/// Gets information about a user by its identifier.
///
/// \param uid The identifier of the user to query.
///
/// \return The information about the user.
///
/// \throw std::runtime_error If the user does not exist.
passwd_ns::user
passwd_ns::find_user_by_uid(const unsigned int uid)
{
    if (mock_users.empty()) {
        const struct ::passwd* pw = ::getpwuid(uid);
        if (pw == NULL)
            throw std::runtime_error(F("Failed to get information about the "
                                       "user with UID %s") % uid);
        INV(pw->pw_uid == uid);
        return user(pw->pw_name, pw->pw_uid, pw->pw_gid);
    } else {
        for (std::vector< user >::const_iterator iter = mock_users.begin();
             iter != mock_users.end(); iter++) {
            if ((*iter).uid == uid)
                return *iter;
        }
        throw std::runtime_error(F("Failed to get information about the "
                                   "user with UID %s") % uid);
    }
}


/// Overrides the current user for testing purposes.
///
/// This DOES NOT change the current privileges!
///
/// \param new_current_user The new current user.
void
passwd_ns::set_current_user_for_testing(const user& new_current_user)
{
    fake_current_user = new_current_user;
}


/// Overrides the current set of users for testing purposes.
///
/// \param users The new users set.  Cannot be empty.
void
passwd_ns::set_mock_users_for_testing(const std::vector< user >& users)
{
    PRE(!users.empty());
    mock_users = users;
}
