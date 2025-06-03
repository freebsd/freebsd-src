# SPDX-License-Identifier: BSD-2-Clause
#
# RCSid:
#	$Id: rust.mk,v 1.37 2025/01/11 03:17:36 sjg Exp $
#
#	@(#) Copyright (c) 2024, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

##
# This makefile is used when a build includes one or more Rust projects.
#
# We first include local.rust.mk to allow for customization.
# You can get very fancy - the logic/functionality here is minimal but
# can be extended via local.rust.mk
#
# If RUST_PROJECT_DIR (where we find Cargo.toml) is not set, we will
# make it ${.CURDIR:C,/src.*,,} actually we use
# ${SRCTOP}/${RELDIR:C,/src.*,,} to ensure we don't confuse ${SRCTOP}
# with ${RUST_PROJECT_DIR}/src.
#
# If ${.OBJDIR} is not ${.CURDIR} we will default CARGO_TARGET_DIR
# to ${.OBJDIR}.
#
# First, if ${.CURDIR} is a subdir of ${RUST_PROJECT_DIR} (will happen
# if an Emacs user does 'M-x compile' while visiting a src file) we
# will need to adjust ${.OBJDIR} (and hence CARGO_TARGET_DIR).
#
# We assume that RUST_CARGO will be used to build Rust projects,
# so we default RUST_CARGO_PROJECT_DIR to ${RUST_PROJECT_DIR} and
# provide a _CARGO_USE that we automatically associate with
# targets named 'cargo.*' the default is 'cargo.build'.
#
# _CARGO_USE will chdir to ${RUST_CARGO_PROJECT_DIR} and run
# ${RUST_CARGO} with ENV, FLAGS and ARGS variables derived from
# ${.TARGET:E:tu} so in the case of 'cargo.build' we get:
# RUST_CARGO_BUILD_ENV, RUST_CARGO_BUILD_FLAGS and RUST_CARGO_BUILD_ARGS
#
# _CARGO_USE will "just work" for additional targets like
# 'cargo.test', 'cargo.clippy', ... which will run '${RUST_CARGO} test',
# '${RUST_CARGO} clippy' etc.
#
# If MK_META_MODE is "yes" 'cargo.build' will touch ${.TARGET}
# so the default make rules will not consider it always out-of-date.
# In META MODE, 'bmake' will know if anything changed that should
# cause the target to be re-built.
#
# If MK_STAGING_RUST is "yes" we will stage the binary we
# built to a suitable location under ${STAGE_OBJTOP}.
#

all:
.MAIN: all

# allow for customization
.-include <local.rust.mk>

RUST_CARGO ?= cargo
RUSTC ?= rustc
.if ${.CURDIR} == ${SRCTOP}
RELDIR ?= .
.else
RELDIR ?= ${.CURDIR:S,${SRCTOP}/,,}
.endif
.if empty(RUST_PROJECT_DIR)
# we want this set correctly from anywhere within
# using RELDIR avoids confusing ${SRCTOP} with ${RUST_PROJECT_DIR}/src
RUST_PROJECT_DIR := ${SRCTOP}/${RELDIR:C,/src.*,,}
.if ${RUST_PROJECT_DIR:T:Nsrc:N.} == ""
RUST_PROJECT_DIR := ${RUST_PROJECT_DIR:H}
.endif
.endif

.if ${.OBJDIR} != ${.CURDIR}
.if ${.CURDIR:M${RUST_PROJECT_DIR}/*} != ""
# Our .CURDIR is below RUST_PROJECT_DIR and thus our
# .OBJDIR is likely not what we want either.
# This can easily happen if in Emacs we do 'M-x compile' while
# visiting a src file.
# It is easily fixed.
__objdir := ${.OBJDIR:S,${.CURDIR:S,${RUST_PROJECT_DIR},,},,}
.OBJDIR: ${__objdir}
.endif
# tell cargo where to drop build artifacts
CARGO_TARGET_DIR ?= ${.OBJDIR}
.if !empty(OBJROOT) && exists(${OBJROOT})
CARGO_HOME_RELDIR ?= rust/cargo_home
CARGO_HOME ?= ${OBJROOT}/common/${RUST_CARGO_HOME_RELDIR}
.endif
.elif ${.CURDIR} != ${RUST_PROJECT_DIR}
.OBJDIR: ${RUST_PROJECT_DIR}
.endif
CARGO_TARGET_DIR ?= target

.if ${MK_DIRDEPS_BUILD:Uno} == "no" || ${.MAKE.LEVEL} > 0
.export CARGO_HOME CARGO_TARGET_DIR RUST_PROJECT_DIR RUSTC

all: cargo.build

.if empty(RUST_PROJECT_FILES)
RUST_PROJECT_FILES != find ${RUST_PROJECT_DIR} -type f \( \
	-name '*.rs' -o \
	-name Cargo.lock -o \
	-name Cargo.toml \) | sort
.endif
RUST_CARGO_BUILD_DEPS += ${RUST_PROJECT_FILES:U}
.endif

RUST_CARGO_PROJECT_DIR ?= ${RUST_PROJECT_DIR}

.if ${RUSTC:M/*}
# make sure we find all the other toolchain bits in the same place
RUST_CARGO_ENV += PATH=${RUSTC:H}:${PATH}

# cargo clippy needs extra help finding the sysroot
# https://github.com/rust-lang/rust-clippy/issues/3523
RUST_CARGO_CLIPPY_ENV += RUSTC_SYSROOT=${${RUSTC} --print sysroot:L:sh}
.endif

.if ${LDFLAGS:U:M-[BL]*} != ""
# we may need to tell rustc where to find the native libs needed
# rustc documents a space after -L so put it back
RUST_LDFLAGS := ${LDFLAGS:C/(-[BL]) /\1/gW:M-[BL]*:S/-L/& /:S/-B/-C link-arg=&/}
.endif
.if !empty(RUST_LDFLAGS)
RUSTFLAGS += ${RUST_LDFLAGS}
.endif
.if !empty(RUSTFLAGS)
RUST_CARGO_BUILD_ENV += RUSTFLAGS="${RUSTFLAGS}"
.endif

_CARGO_USE:	.USEBEFORE
	@(cd ${RUST_CARGO_PROJECT_DIR} && ${RUST_CARGO_ENV} \
	${RUST_CARGO_${.TARGET:E:tu}_ENV} \
	${RUST_CARGO} ${RUST_CARGO_${.TARGET:E:tu}_FLAGS:U${RUST_CARGO_FLAGS}} \
	${.TARGET:E} ${RUST_CARGO_${.TARGET:E:tu}_ARGS})

RUST_CARGO_TARGETS += cargo.build
cargo.build: ${RUST_CARGO_BUILD_DEPS}
.if ${.OBJDIR} != ${RUST_PROJECT_DIR}
	test ! -s Cargo.lock || cp -p Cargo.lock ${RUST_CARGO_PROJECT_DIR}
.endif
	@${META_COOKIE_TOUCH}

# handle cargo.{run,test,...}
RUST_CARGO_TARGETS += ${.TARGETS:Mcargo.*}
${RUST_CARGO_TARGETS:O:u}: _CARGO_USE

.if ${MK_DEBUG_RUST:Uno} == "no" && \
	${DEBUG_RUST_DIRS:Unone:@x@${RELDIR:M$x}@} == ""
RUST_CARGO_BUILD_ARGS += --release
.endif

.if ${RUST_CARGO_BUILD_ARGS:U:M--release} != ""
RUST_CARGO_TARGET = release
.else
RUST_CARGO_TARGET = debug
.endif

# do we want cargo.build to depend on cargo.fmt --check ?
# if user did make cargo.fmt the target would exist by now
.if ${MK_RUST_CARGO_FMT_CHECK:Uno} == "yes" && !target(cargo.fmt)
RUST_CARGO_FMT_CHECK_ARGS ?= --check
RUST_CARGO_FMT_ARGS += ${RUST_CARGO_FMT_CHECK_ARGS}
cargo.fmt: _CARGO_USE
cargo.build: cargo.fmt
.endif

# useful? defaults
RUST_CARGO_CLIPPY_ARGS ?= -- -D warnings --no-deps

# do we want cargo.clippy to be run after cargo.build?
.if ${MK_RUST_CARGO_CLIPPY:Uno} == "yes" && !target(cargo.clippy)
cargo.clippy: _CARGO_USE
cargo.clippy: cargo.build
all: cargo.clippy
.endif

.if !defined(RUST_LIBS)
RUST_PROGS ?= ${RUST_PROJECT_DIR:T}
.endif
.if !empty(RUST_PROGS)
BINDIR ?= ${prefix}/bin
# there could be a target triple involved
RUST_CARGO_TARGET_DIR ?= ${CARGO_TARGET_DIR}
RUST_CARGO_OUTPUT_DIR ?= ${RUST_CARGO_TARGET_DIR}/${RUST_CARGO_TARGET}

RUST_CARGO_BUILD_OUTPUT_LIST := ${RUST_PROGS:S,^,${RUST_CARGO_OUTPUT_DIR}/,}

${RUST_CARGO_BUILD_OUTPUT_LIST}: cargo.build
.endif

# for late customizations
.-include <local.rust.build.mk>
