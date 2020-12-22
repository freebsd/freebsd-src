#!/usr/bin/env python3
# PYTHON_ARGCOMPLETE_OKAY
# -
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Alex Richardson <arichardson@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

# This script makes it easier to build on non-FreeBSD systems by bootstrapping
# bmake and inferring required compiler variables.
#
# On FreeBSD you can use it the same way as just calling make:
# `MAKEOBJDIRPREFIX=~/obj ./tools/build/make.py buildworld -DWITH_FOO`
#
# On Linux and MacOS you will either need to set XCC/XCXX/XLD/XCPP or pass
# --cross-bindir to specify the path to the cross-compiler bindir:
# `MAKEOBJDIRPREFIX=~/obj ./tools/build/make.py
# --cross-bindir=/path/to/cross/compiler buildworld -DWITH_FOO TARGET=foo
# TARGET_ARCH=bar`
import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd, **kwargs):
    cmd = list(map(str, cmd))  # convert all Path objects to str
    debug("Running", cmd)
    subprocess.check_call(cmd, **kwargs)


def bootstrap_bmake(source_root, objdir_prefix):
    bmake_source_dir = source_root / "contrib/bmake"
    bmake_build_dir = objdir_prefix / "bmake-build"
    bmake_install_dir = objdir_prefix / "bmake-install"
    bmake_binary = bmake_install_dir / "bin/bmake"

    if (bmake_install_dir / "bin/bmake").exists():
        return bmake_binary
    print("Bootstrapping bmake...")
    # TODO: check if the host system bmake is new enough and use that instead
    if not bmake_build_dir.exists():
        os.makedirs(str(bmake_build_dir))
    env = os.environ.copy()
    global new_env_vars
    env.update(new_env_vars)

    if sys.platform.startswith("linux"):
        # Work around the deleted file bmake/missing/sys/cdefs.h
        # TODO: bmake should keep the compat sys/cdefs.h
        env["CFLAGS"] = "-I{src}/tools/build/cross-build/include/common " \
                        "-I{src}/tools/build/cross-build/include/linux " \
                        "-D_GNU_SOURCE=1".format(src=source_root)
    configure_args = [
        "--with-default-sys-path=" + str(bmake_install_dir / "share/mk"),
        "--with-machine=amd64",  # TODO? "--with-machine-arch=amd64",
        "--without-filemon", "--prefix=" + str(bmake_install_dir)]
    run(["sh", bmake_source_dir / "boot-strap"] + configure_args,
        cwd=str(bmake_build_dir), env=env)

    run(["sh", bmake_source_dir / "boot-strap", "op=install"] + configure_args,
        cwd=str(bmake_build_dir))
    print("Finished bootstrapping bmake...")
    return bmake_binary


def debug(*args, **kwargs):
    global parsed_args
    if parsed_args.debug:
        print(*args, **kwargs)


def is_make_var_set(var):
    return any(
        x.startswith(var + "=") or x == ("-D" + var) for x in sys.argv[1:])


def check_required_make_env_var(varname, binary_name, bindir):
    global new_env_vars
    if os.getenv(varname):
        return
    if not bindir:
        sys.exit("Could not infer value for $" + varname + ". Either set $" +
                 varname + " or pass --cross-bindir=/cross/compiler/dir/bin")
    # try to infer the path to the tool
    guess = os.path.join(bindir, binary_name)
    if not os.path.isfile(guess):
        sys.exit("Could not infer value for $" + varname + ": " + guess +
                 " does not exist")
    new_env_vars[varname] = guess
    debug("Inferred", varname, "as", guess)
    global parsed_args
    if parsed_args.debug:
        run([guess, "--version"])


def default_cross_toolchain():
    # default to homebrew-installed clang on MacOS if available
    if sys.platform.startswith("darwin"):
        if shutil.which("brew"):
            llvm_dir = subprocess.getoutput("brew --prefix llvm")
            if llvm_dir and Path(llvm_dir, "bin").exists():
                return str(Path(llvm_dir, "bin"))
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--host-bindir",
                        help="Directory to look for cc/c++/cpp/ld to build "
                             "host (" + sys.platform + ") binaries",
                        default="/usr/bin")
    parser.add_argument("--cross-bindir", default=default_cross_toolchain(),
                        help="Directory to look for cc/c++/cpp/ld to build "
                             "target binaries (only needed if XCC/XCPP/XLD "
                             "are not set)")
    parser.add_argument("--cross-compiler-type", choices=("clang", "gcc"),
                        default="clang",
                        help="Compiler type to find in --cross-bindir (only "
                             "needed if XCC/XCPP/XLD are not set)"
                             "Note: using CC is currently highly experimental")
    parser.add_argument("--host-compiler-type", choices=("cc", "clang", "gcc"),
                        default="cc",
                        help="Compiler type to find in --host-bindir (only "
                             "needed if CC/CPP/CXX are not set). ")
    parser.add_argument("--debug", action="store_true",
                        help="Print information on inferred env vars")
    parser.add_argument("--clean", action="store_true",
                        help="Do a clean rebuild instead of building with "
                             "-DNO_CLEAN")
    parser.add_argument("--no-clean", action="store_false", dest="clean",
                        help="Do a clean rebuild instead of building with "
                             "-DNO_CLEAN")
    try:
        import argcomplete  # bash completion:

        argcomplete.autocomplete(parser)
    except ImportError:
        pass
    parsed_args, bmake_args = parser.parse_known_args()

    MAKEOBJDIRPREFIX = os.getenv("MAKEOBJDIRPREFIX")
    if not MAKEOBJDIRPREFIX:
        sys.exit("MAKEOBJDIRPREFIX is not set, cannot continue!")
    if not Path(MAKEOBJDIRPREFIX).is_dir():
        sys.exit(
            "Chosen MAKEOBJDIRPREFIX=" + MAKEOBJDIRPREFIX + " doesn't exit!")
    objdir_prefix = Path(MAKEOBJDIRPREFIX).absolute()
    source_root = Path(__file__).absolute().parent.parent.parent

    new_env_vars = {}
    if not sys.platform.startswith("freebsd"):
        if not is_make_var_set("TARGET") or not is_make_var_set("TARGET_ARCH"):
            if "universe" not in sys.argv and "tinderbox" not in sys.argv:
                sys.exit("TARGET= and TARGET_ARCH= must be set explicitly "
                         "when building on non-FreeBSD")
        # infer values for CC/CXX/CPP

        if sys.platform.startswith(
                "linux") and parsed_args.host_compiler_type == "cc":
            # FIXME: bsd.compiler.mk doesn't handle the output of GCC if it
            #  is /usr/bin/cc on Ubuntu since it doesn't contain the GCC string.
            parsed_args.host_compiler_type = "gcc"

        if parsed_args.host_compiler_type == "gcc":
            default_cc, default_cxx, default_cpp = ("gcc", "g++", "cpp")
        # FIXME: this should take values like `clang-9` and then look for
        # clang-cpp-9, etc. Would alleviate the need to set the bindir on
        # ubuntu/debian at least.
        elif parsed_args.host_compiler_type == "clang":
            default_cc, default_cxx, default_cpp = (
                "clang", "clang++", "clang-cpp")
        else:
            default_cc, default_cxx, default_cpp = ("cc", "c++", "cpp")

        check_required_make_env_var("CC", default_cc, parsed_args.host_bindir)
        check_required_make_env_var("CXX", default_cxx,
                                    parsed_args.host_bindir)
        check_required_make_env_var("CPP", default_cpp,
                                    parsed_args.host_bindir)
        # Using the default value for LD is fine (but not for XLD!)

        # On non-FreeBSD we need to explicitly pass XCC/XLD/X_COMPILER_TYPE
        use_cross_gcc = parsed_args.cross_compiler_type == "gcc"
        check_required_make_env_var("XCC", "gcc" if use_cross_gcc else "clang",
                                    parsed_args.cross_bindir)
        check_required_make_env_var("XCXX",
                                    "g++" if use_cross_gcc else "clang++",
                                    parsed_args.cross_bindir)
        check_required_make_env_var("XCPP",
                                    "cpp" if use_cross_gcc else "clang-cpp",
                                    parsed_args.cross_bindir)
        check_required_make_env_var("XLD", "ld" if use_cross_gcc else "ld.lld",
                                    parsed_args.cross_bindir)

        # We also need to set STRIPBIN if there is no working strip binary
        # in $PATH.
        if not shutil.which("strip"):
            if sys.platform.startswith("darwin"):
                # On macOS systems we have to use /usr/bin/strip.
                sys.exit("Cannot find required tool 'strip'. Please install the"
                         " host compiler and command line tools.")
            if parsed_args.host_compiler_type == "clang":
                strip_binary = "llvm-strip"
            else:
                strip_binary = "strip"
            check_required_make_env_var("STRIPBIN", strip_binary,
                                        parsed_args.cross_bindir)
        if os.getenv("STRIPBIN") or "STRIPBIN" in new_env_vars:
            # If we are setting STRIPBIN, we have to set XSTRIPBIN to the
            # default if it is not set otherwise already.
            if not os.getenv("XSTRIPBIN") and not is_make_var_set("XSTRIPBIN"):
                # Use the bootstrapped elftoolchain strip:
                new_env_vars["XSTRIPBIN"] = "strip"

    bmake_binary = bootstrap_bmake(source_root, objdir_prefix)
    # at -j1 cleandir+obj is unbearably slow. AUTO_OBJ helps a lot
    debug("Adding -DWITH_AUTO_OBJ")
    bmake_args.append("-DWITH_AUTO_OBJ")
    if parsed_args.clean is False:
        bmake_args.append("-DWITHOUT_CLEAN")
    if (parsed_args.clean is None and not is_make_var_set("NO_CLEAN")
            and not is_make_var_set("WITHOUT_CLEAN")):
        # Avoid accidentally deleting all of the build tree and wasting lots of
        # time cleaning directories instead of just doing a rm -rf ${.OBJDIR}
        want_clean = input("You did not set -DNO_CLEAN/--clean/--no-clean."
                           " Did you really mean to do a  clean build? y/[N] ")
        if not want_clean.lower().startswith("y"):
            bmake_args.append("-DNO_CLEAN")

    env_cmd_str = " ".join(
        shlex.quote(k + "=" + v) for k, v in new_env_vars.items())
    make_cmd_str = " ".join(
        shlex.quote(s) for s in [str(bmake_binary)] + bmake_args)
    debug("Running `env ", env_cmd_str, " ", make_cmd_str, "`", sep="")
    os.environ.update(new_env_vars)
    os.chdir(str(source_root))
    os.execv(str(bmake_binary), [str(bmake_binary)] + bmake_args)
