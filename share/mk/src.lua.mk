#
# Lua helper file for FreeBSD /usr/src builds.
#
# This file provides any necessary assistance for consumers of Lua in the base
# system.

.if !target(__<src.lua.mk>__)
__<src.lua.mk>__:

.include <bsd.own.mk>

#
# LUA_INSTALL_PATH and LUA_CMD describe where the internal lua has been
# installed to, along with the name of the internal command.  The default
# name is flua.
#
# LUA_CMD can be overwritten to point to a Lua that isn't flua.  This is fine,
# but parts of the src build that use it may have certain expectations that
# may only be fulfilled by the in-tree Lua.  The user overwriting it is expected
# to understand these and provide the expectations.
#
# flua is currently equivalent to Lua 5.3, with the following modules:
# - luafilesystem
# - lua-posix
#
LUA_INSTALL_PATH?=	${LIBEXECDIR}
LUA_CMD?=		flua

#
# Some standalone usage may want a variable that tries to find the lua command,
# and cannot necessarily embed the logic for trying to find it amongst bootstrap
# tools.  For these, we provide the LUA variable.
#
# The LUA variable should point to LUA_CMD on the system, if it exists.
# Otherwise, consumers will have to settle for a PATH search and PATH being
# appropriately set.
#
.if !defined(LUA) && exists(${LUA_INSTALL_PATH}/${LUA_CMD})
LUA=	${LUA_INSTALL_PATH}/${LUA_CMD}
.else
LUA?=	${LUA_CMD}
.endif

.endif #  !target(__<src.lua.mk>__)
