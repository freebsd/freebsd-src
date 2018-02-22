#!/bin/sh
# $FreeBSD$

# Run linter on the lua scripts in stand/lua. luacheck, not currently available
# in ports, must be installed as a dependency.

die() {
    echo $*
    exit 1
}

LUACHECK=$(which luacheck)

[ ! -z "${LUACHECK}" ] && [ -e "${LUACHECK}" ] || \
	die "You must have luacheck installed and in your PATH"

cd $(make -V SRCTOP)/stand
${LUACHECK} . --globals loader --globals lfs --globals io.getchar \
	--globals io.ischar --std lua53
