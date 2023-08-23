#!/usr/bin/env python3
# PYTHON_ARGCOMPLETE_OKAY
# -
# SPDX-License-Identifier: BSD-2-Clause
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


# List of targets that are independent of TARGET/TARGET_ARCH and thus do not
# need them to be set. Keep in the same order as Makefile documents them (if
# they are documented).
mach_indep_targets = [
    "cleanuniverse",
    "universe",
    "universe-toolchain",
    "tinderbox"
    "worlds",
    "kernels",
    "kernel-toolchains",
    "targets",
    "toolchains",
    "makeman",
    "sysent",
]


def run(cmd, **kwargs):
    cmd = list(map(str, cmd))  # convert all Path objects to str
    debug("Running", cmd)
    subprocess.check_call(cmd, **kwargs)


# Always bootstraps in order to control bmake's config to ensure compatibility
def bootstrap_bmake(source_root, objdir_prefix):
    bmake_source_dir = source_root / "contrib/bmake"
    bmake_build_dir = objdir_prefix / "bmake-build"
    bmake_install_dir = objdir_prefix / "bmake-install"
    bmake_binary = bmake_install_dir / "bin/bmake"
    bmake_config = bmake_install_dir / ".make-py-config"

    bmake_source_version = subprocess.run([
        "sh", "-c", ". \"$0\"/VERSION; echo $_MAKE_VERSION",
        bmake_source_dir], capture_output=True).stdout.strip()
    try:
        bmake_source_version = int(bmake_source_version)
    except ValueError:
        sys.exit("Invalid source bmake version '" + bmake_source_version + "'")

    bmake_installed_version = 0
    if bmake_binary.exists():
        bmake_installed_version = subprocess.run([
            bmake_binary, "-r", "-f", "/dev/null", "-V", "MAKE_VERSION"],
            capture_output=True).stdout.strip()
        try:
            bmake_installed_version = int(bmake_installed_version.strip())
        except ValueError:
            print("Invalid installed bmake version '" +
                  bmake_installed_version + "', treating as not present")

    configure_args = [
        "--with-default-sys-path=.../share/mk:" +
        str(bmake_install_dir / "share/mk"),
        "--with-machine=amd64",  # TODO? "--with-machine-arch=amd64",
        "--without-filemon", "--prefix=" + str(bmake_install_dir)]

    configure_args_str = ' '.join([shlex.quote(x) for x in configure_args])
    if bmake_config.exists():
        last_configure_args_str = bmake_config.read_text()
    else:
        last_configure_args_str = ""

    debug("Source bmake version: " + str(bmake_source_version))
    debug("Installed bmake version: " + str(bmake_installed_version))
    debug("Configure args: " + configure_args_str)
    debug("Last configure args: " + last_configure_args_str)

    if bmake_installed_version == bmake_source_version and \
       configure_args_str == last_configure_args_str:
        return bmake_binary

    print("Bootstrapping bmake...")
    if bmake_build_dir.exists():
        shutil.rmtree(str(bmake_build_dir))
    if bmake_install_dir.exists():
        shutil.rmtree(str(bmake_install_dir))

    os.makedirs(str(bmake_build_dir))

    env = os.environ.copy()
    global new_env_vars
    env.update(new_env_vars)

    run(["sh", bmake_source_dir / "boot-strap"] + configure_args,
        cwd=str(bmake_build_dir), env=env)
    run(["sh", bmake_source_dir / "boot-strap", "op=install"] + configure_args,
        cwd=str(bmake_build_dir))
    bmake_config.write_text(configure_args_str)

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


def check_xtool_make_env_var(varname, binary_name):
    # Avoid calling brew --prefix on macOS if all variables are already set:
    if os.getenv(varname):
        return
    global parsed_args
    if parsed_args.cross_bindir is None:
        parsed_args.cross_bindir = default_cross_toolchain()
    return check_required_make_env_var(varname, binary_name,
                                       parsed_args.cross_bindir)


def default_cross_toolchain():
    # default to homebrew-installed clang on MacOS if available
    if sys.platform.startswith("darwin"):
        if shutil.which("brew"):
            llvm_dir = subprocess.run(["brew", "--prefix", "llvm"],
                                      capture_output=True).stdout.strip()
            debug("Inferred LLVM dir as", llvm_dir)
            try:
                if llvm_dir and Path(llvm_dir.decode("utf-8"), "bin").exists():
                    return str(Path(llvm_dir.decode("utf-8"), "bin"))
            except OSError:
                return None
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--host-bindir",
                        help="Directory to look for cc/c++/cpp/ld to build "
                             "host (" + sys.platform + ") binaries",
                        default="/usr/bin")
    parser.add_argument("--cross-bindir", default=None,
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
    parser.add_argument("--bootstrap-toolchain", action="store_true",
                        help="Bootstrap the toolchain instead of using an "
                             "external one (experimental and not recommended)")
    parser.add_argument("--clean", action="store_true",
                        help="Do a clean rebuild instead of building with "
                             "-DWITHOUT_CLEAN")
    parser.add_argument("--no-clean", action="store_false", dest="clean",
                        help="Do a clean rebuild instead of building with "
                             "-DWITHOUT_CLEAN")
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
            if not set(sys.argv).intersection(set(mach_indep_targets)):
                sys.exit("TARGET= and TARGET_ARCH= must be set explicitly "
                         "when building on non-FreeBSD")
    if not parsed_args.bootstrap_toolchain:
        # infer values for CC/CXX/CPP
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
        check_xtool_make_env_var("XCC", "gcc" if use_cross_gcc else "clang")
        check_xtool_make_env_var("XCXX", "g++" if use_cross_gcc else "clang++")
        check_xtool_make_env_var("XCPP",
                                 "cpp" if use_cross_gcc else "clang-cpp")
        check_xtool_make_env_var("XLD", "ld" if use_cross_gcc else "ld.lld")

        # We also need to set STRIPBIN if there is no working strip binary
        # in $PATH.
        if not shutil.which("strip"):
            if sys.platform.startswith("darwin"):
                # On macOS systems we have to use /usr/bin/strip.
                sys.exit("Cannot find required tool 'strip'. Please install "
                         "the host compiler and command line tools.")
            if parsed_args.host_compiler_type == "clang":
                strip_binary = "llvm-strip"
            else:
                strip_binary = "strip"
            check_required_make_env_var("STRIPBIN", strip_binary,
                                        parsed_args.host_bindir)
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
        want_clean = input("You did not set -DWITHOUT_CLEAN/--(no-)clean."
                           " Did you really mean to do a clean build? y/[N] ")
        if not want_clean.lower().startswith("y"):
            bmake_args.append("-DWITHOUT_CLEAN")

    env_cmd_str = " ".join(
        shlex.quote(k + "=" + v) for k, v in new_env_vars.items())
    make_cmd_str = " ".join(
        shlex.quote(s) for s in [str(bmake_binary)] + bmake_args)
    debug("Running `env ", env_cmd_str, " ", make_cmd_str, "`", sep="")
    os.environ.update(new_env_vars)

    # Fedora defines bash function wrapper for some shell commands and this
    # makes 'which <command>' return the function's source code instead of
    # the binary path. Undefine it to restore the original behavior.
    os.unsetenv("BASH_FUNC_which%%")
    os.unsetenv("BASH_FUNC_ml%%")
    os.unsetenv("BASH_FUNC_module%%")

    os.chdir(str(source_root))
    os.execv(str(bmake_binary), [str(bmake_binary)] + bmake_args)
