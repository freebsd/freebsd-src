#!/usr/libexec/flua
--[[
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
]]--

jail = require("jail")
ucl = require("ucl")

name = "demo"

local has_demo = false

-- Make sure we don't have a demo jail to start with; "jid" and "name" are
-- always present.
for jparams in jail.list() do
    if jparams["name"] == name then
        has_demo = true
        break
    end
end

if not has_demo then
    -- Create a persistent jail named "demo" with all other parameters default.
    jid, err = jail.setparams(name, {persist = "true"}, jail.CREATE)
    if not jid then
        error(err)
    end
end

-- Get a list of all known jail parameter names.
allparams = jail.allparams()

-- Get all the parameters of the jail we created.
jid, res = jail.getparams(name, allparams)
if not jid then
    error(res)
end

-- Display the jail's parameters as a pretty-printed JSON object.
print(ucl.to_json(res))

-- Confirm that we still have it for now.
has_demo = false
for jparams in jail.list() do
    if jparams["name"] == name then
        has_demo = true
        break
    end
end

if not has_demo then
    print("demo does not exist")
end

-- Update the "persist" parameter to "false" to remove the jail.
jid, err = jail.setparams(name, {persist = "false"}, jail.UPDATE)
if not jid then
    error(err)
end

-- Verify that the jail is no longer on the system.
local is_persistent = false
has_demo = false
for jparams in jail.list({"persist"}) do
    if jparams["name"] == name then
        has_demo = true
        jid = jparams["jid"]
        is_persistent = jparams["persist"] ~= "false"
    end
end

-- In fact, it does remain until this process ends -- c'est la vie.
if has_demo then
    io.write("demo still exists, jid " .. jid .. ", ")
    if is_persistent then
        io.write("persistent\n")
    else
        io.write("not persistent\n")
    end
end
