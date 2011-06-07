#!/usr/local/bin/python
#
# This script generates a master INDEX file for the CD images built by the
# FreeBSD release engineers.  Each disc is given a list of desired packages.
# Dependencies of these packages are placed on either the same disc or an
# earlier disc.  The resulting master INDEX file is then written out.
#
# Usage: package-split.py <INDEX> <master INDEX>
#
# $FreeBSD$

import os
import sys

try:
    arch = os.environ["PKG_ARCH"]
except:
    arch = os.uname()[4]
print "Using arch %s" % (arch)

if 'PKG_VERBOSE' in os.environ:
    verbose = 1
else:
    verbose = 0

if 'PKG_DVD' in os.environ:
    doing_dvd = 1
else:
    doing_dvd = 0

# List of packages for disc1.
def disc1_packages():
    pkgs = ['misc/freebsd-doc-bn', 
	    'misc/freebsd-doc-da',
	    'misc/freebsd-doc-de',
	    'misc/freebsd-doc-el',
	    'misc/freebsd-doc-en',
	    'misc/freebsd-doc-es',
	    'misc/freebsd-doc-fr',
	    'misc/freebsd-doc-hu',
	    'misc/freebsd-doc-it',
	    'misc/freebsd-doc-ja',
	    'misc/freebsd-doc-mn',
	    'misc/freebsd-doc-nl',
	    'misc/freebsd-doc-pl',
	    'misc/freebsd-doc-pt',
	    'misc/freebsd-doc-ru',
	    'misc/freebsd-doc-sr',
	    'misc/freebsd-doc-tr',
	    'misc/freebsd-doc-zh_cn',
	    'misc/freebsd-doc-zh_tw']

    if doing_dvd:
	pkgs.extend(['archivers/unzip',
	    'astro/xearth',
	    'devel/gmake',
	    'devel/imake',
	    'editors/emacs',
	    'editors/vim-lite',
	    'emulators/linux_base-f10',
	    'emulators/mtools',
	    'graphics/png',
	    'graphics/xv',
	    'irc/xchat',
	    'lang/perl5.8',
	    'mail/alpine',
	    'mail/exim',
	    'mail/fetchmail',
	    'mail/mutt',
	    'mail/popd',
	    'mail/postfix',
	    'mail/xfmail',
	    'net/cvsup-without-gui',
	    'net/rsync',
	    'net/samba3',
	    'news/slrn',
	    'news/tin',
	    'ports-mgmt/p5-FreeBSD-Portindex',
	    'ports-mgmt/portaudit',
	    'ports-mgmt/portmaster',
	    'ports-mgmt/portupgrade',
	    'print/a2ps-letter',
	    'print/apsfilter',
	    'print/ghostscript7-nox11',
	    'print/psutils-letter',
	    'print/gv',
	    'shells/bash',
	    'shells/pdksh',
	    'shells/zsh',
	    'security/sudo',
	    'sysutils/screen',
	    'www/links',
	    'www/lynx',
	    'x11/gnome2',
	    'x11/kde4',
	    'x11/rxvt',
	    'x11/xorg',
	    'x11-wm/afterstep',
	    'x11-wm/fvwm2',
	    'x11-wm/windowmaker'])
    return pkgs

# The list of desired packages
def desired_packages():
    disc1 = disc1_packages()
    return [disc1]

# Suck the entire INDEX file into a two different dictionaries.  The first
# dictionary maps port names (origins) to package names.  The second
# dictionary maps a package name to a list of its dependent packages.
PACKAGE_COL=0
ORIGIN_COL=1
DEPENDS_COL=8

def load_index(index):
    deps = {}
    pkgs = {}
    line_num = 1
    for line in index:
        fields = line.split('|')
        name = fields[PACKAGE_COL]
        if name in deps:
            sys.stderr.write('%d: Duplicate package %s\n' % (line_num, name))
            sys.exit(1)
        origin = fields[ORIGIN_COL].replace('/usr/ports/', '', 1)
        if origin in pkgs:
            sys.stderr.write('%d: Duplicate port %s\n' % (line_num, origin))
            sys.exit(1)
        deps[name] = fields[DEPENDS_COL].split()
        pkgs[origin] = name
        line_num = line_num + 1
    return (deps, pkgs)

# Layout the packages on the various CD images.  Here's how it works.  We walk
# each disc in the list of discs.  Within each disc we walk the list of ports.
# For each port, we add the package name to a dictionary with the value being
# the current disc number.  We also add all of the dependent packages.  If
# a package is already in the dictionary when we go to add it, we just leave
# the dictionary as it is.  This means that each package ends up on the first
# disc that either lists it or contains it as a dependency.
def layout_discs(discs, pkgs, deps):
    disc_num = 1
    layout = {}
    for disc in discs:
        for port in disc:
            if port not in pkgs:
                sys.stderr.write('Disc %d: Unable to find package for %s\n' %
                                 (disc_num, port))
                continue
            pkg = pkgs[port]
            pkg_list = [pkg] + deps[pkg]
            for pkg in pkg_list:
                if pkg not in layout:
                    if verbose:
                        print "--> Adding %s to Disc %d" % (pkg, disc_num)
                    layout[pkg] = disc_num
        disc_num = disc_num + 1
    return layout

# Generate a master INDEX file based on the generated layout.  The way this
# works is that for each INDEX line, we check to see if the package is in the
# layout.  If it is, we put that INDEX line into the master INDEX and append
# a new field with the disc number to the line.
def generate_index(index, layout, master_index):
    for line in index:
        pkg = line.split('|')[PACKAGE_COL]
        if pkg in layout:
            new_line = '%s|%d\n' % (line.splitlines()[0], layout[pkg])
            master_index.write(new_line)

# Verify the command line arguments
if len(sys.argv) != 3:
    sys.stderr.write('Invalid number of arguments\n')
    sys.stderr.write('Usage: package-split.py <source INDEX> <master INDEX>\n')
    sys.exit(1)

print "Loading %s..." % (sys.argv[1])
index = file(sys.argv[1])
(deps, pkgs) = load_index(index)
discs = desired_packages()
layout = layout_discs(discs, pkgs, deps)
index.seek(0)
print "Generating %s..." % (sys.argv[2])
master_index = file(sys.argv[2], 'w')
generate_index(index, layout, master_index)
index.close()
master_index.close()
