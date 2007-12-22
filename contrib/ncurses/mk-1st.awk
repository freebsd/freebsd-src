# $Id: mk-1st.awk,v 1.68 2006/10/08 00:14:08 tom Exp $
##############################################################################
# Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.                #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
#
# Author: Thomas E. Dickey
#
# Generate list of objects for a given model library
# Variables:
#	name		  (library name, e.g., "ncurses", "panel", "forms", "menus")
#	traces		  ("all" or "DEBUG", to control whether tracing is compiled in)
#	MODEL		  (e.g., "DEBUG", uppercase; toupper is not portable)
#	model		  (directory into which we compile, e.g., "obj")
#	prefix		  (e.g., "lib", for Unix-style libraries)
#	suffix		  (e.g., "_g.a", for debug libraries)
#	subset		  ("none", "base", "base+ext_funcs" or "termlib")
#	ShlibVer	  ("rel", "abi" or "auto", to augment DoLinks variable)
#	ShlibVerInfix ("yes" or "no", determines location of version #)
#	DoLinks		  ("yes", "reverse" or "no", flag to add symbolic links)
#	rmSoLocs	  ("yes" or "no", flag to add extra clean target)
#	ldconfig	  (path for this tool, if used)
#	overwrite	  ("yes" or "no", flag to add link to libcurses.a
#	depend		  (optional dependencies for all objects, e.g, ncurses_cfg.h)
#	host		  (cross-compile host, if any)
#
# Notes:
#	CLIXs nawk does not like underscores in command-line variable names.
#	Mixed-case variable names are ok.
#	HP/UX requires shared libraries to have executable permissions.
#
function symlink(src,dst) {
		if ( src != dst ) {
			printf "rm -f %s; ", dst
			printf "$(LN_S) %s %s; ", src, dst
		}
	}
function rmlink(directory, dst) {
		printf "\t-rm -f %s/%s\n", directory, dst
	}
function removelinks(directory) {
		rmlink(directory, end_name);
		if ( DoLinks == "reverse" ) {
				if ( ShlibVer == "rel" ) {
					rmlink(directory, abi_name);
					rmlink(directory, rel_name);
				} else if ( ShlibVer == "abi" ) {
					rmlink(directory, abi_name);
				}
		} else {
				if ( ShlibVer == "rel" ) {
					rmlink(directory, abi_name);
					rmlink(directory, lib_name);
				} else if ( ShlibVer == "abi" ) {
					rmlink(directory, lib_name);
				}
		}
	}
function make_shlib(objs, shlib_list) {
		printf "\t$(MK_SHARED_LIB) $(%s_OBJS) $(%s) $(LDFLAGS)\n", objs, shlib_list
	}
function sharedlinks(directory) {
		if ( ShlibVer != "auto" && ShlibVer != "cygdll" ) {
			printf "\tcd %s && (", directory
			if ( DoLinks == "reverse" ) {
				if ( ShlibVer == "rel" ) {
					symlink(lib_name, abi_name);
					symlink(abi_name, rel_name);
				} else if ( ShlibVer == "abi" ) {
					symlink(lib_name, abi_name);
				}
			} else {
				if ( ShlibVer == "rel" ) {
					symlink(rel_name, abi_name);
					symlink(abi_name, lib_name);
				} else if ( ShlibVer == "abi" ) {
					symlink(abi_name, lib_name);
				}
			}
			printf ")\n"
		}
	}
function shlib_rule(directory) {
		if ( ShlibVer == "cygdll" ) {
				dst_libs = sprintf("%s/$(SHARED_LIB) %s/$(IMPORT_LIB)", directory, directory);
		} else {
				dst_libs = sprintf("%s/%s", directory, end_name);
		}
		printf "%s : %s $(%s_OBJS)\n", dst_libs, directory, OBJS
		printf "\t@echo linking $@\n"
		print "\t-@rm -f %s", dst_libs;
		if ( subset == "termlib" || subset == "termlib+ext_tinfo" ) {
				make_shlib(OBJS, "TINFO_LIST")
		} else {
				make_shlib(OBJS, "SHLIB_LIST")
		}
		sharedlinks(directory)
	}
function install_dll(directory,filename) {
		src_name = sprintf("../lib/%s", filename);
		dst_name = sprintf("$(DESTDIR)%s/%s", directory, filename);
		printf "\t@echo installing %s as %s\n", src_name, dst_name
		printf "\t-@rm -f %s\n", dst_name
		if ( directory == "$(bindir)" ) {
			program = "$(INSTALL) -m 755";
		} else {
			program = "$(INSTALL_LIB)";
		}
		printf "\t%s %s %s\n", program, src_name, dst_name
	}
BEGIN	{
		found = 0
		using = 0
	}
	/^@/ {
		using = 0
		if (subset == "none") {
			using = 1
		} else if (index(subset,$2) > 0) {
			if (using == 0) {
				if (found == 0) {
					print  ""
					printf "# generated by mk-1st.awk (subset=%s)\n", subset
					printf "#  name:          %s\n", name 
					printf "#  traces:        %s\n", traces 
					printf "#  MODEL:         %s\n", MODEL 
					printf "#  model:         %s\n", model 
					printf "#  prefix:        %s\n", prefix 
					printf "#  suffix:        %s\n", suffix 
					printf "#  subset:        %s\n", subset 
					printf "#  ShlibVer:      %s\n", ShlibVer 
					printf "#  ShlibVerInfix: %s\n", ShlibVerInfix 
					printf "#  DoLinks:       %s\n", DoLinks 
					printf "#  rmSoLocs:      %s\n", rmSoLocs 
					printf "#  ldconfig:      %s\n", ldconfig 
					printf "#  overwrite:     %s\n", overwrite 
					printf "#  depend:        %s\n", depend 
					printf "#  host:          %s\n", host 
					print  ""
				}
				using = 1
			}
			if ( subset == "termlib" || subset == "termlib+ext_tinfo" ) {
				OBJS  = MODEL "_T"
			} else {
				OBJS  = MODEL
			}
		}
	}
	/^[@#]/ {
		next
	}
	$1 ~ /trace/ {
		if (traces != "all" && traces != MODEL && $1 != "lib_trace")
			next
	}
	{
		if (using \
		 && ( $1 != "link_test" ) \
		 && ( $2 == "lib" \
		   || $2 == "progs" \
		   || $2 == "c++" \
		   || $2 == "tack" ))
		{
			if ( found == 0 )
			{
				printf "%s_OBJS =", OBJS
				if ( $2 == "lib" )
					found = 1
				else
					found = 2
			}
			printf " \\\n\t../%s/%s$o", model, $1
		}
	}
END	{
		print  ""
		if ( found != 0 )
		{
			printf "\n$(%s_OBJS) : %s\n", OBJS, depend
		}
		if ( found == 1 )
		{
			print  ""
			lib_name = sprintf("%s%s%s", prefix, name, suffix)
			if ( MODEL == "SHARED" )
			{
				if (ShlibVerInfix == "cygdll") {
					abi_name = sprintf("%s%s$(ABI_VERSION)%s", "cyg", name, suffix);
					rel_name = sprintf("%s%s$(REL_VERSION)%s", "cyg", name, suffix);
					imp_name = sprintf("%s%s%s.a", prefix, name, suffix);
				} else if (ShlibVerInfix == "yes") {
					abi_name = sprintf("%s%s.$(ABI_VERSION)%s", prefix, name, suffix);
					rel_name = sprintf("%s%s.$(REL_VERSION)%s", prefix, name, suffix);
				} else {
					abi_name = sprintf("%s.$(ABI_VERSION)", lib_name);
					rel_name = sprintf("%s.$(REL_VERSION)", lib_name);
				}
				if ( DoLinks == "reverse") {
					end_name = lib_name;
				} else {
					if ( ShlibVer == "rel" ) {
						end_name = rel_name;
					} else if ( ShlibVer == "abi" || ShlibVer == "cygdll" ) {
						end_name = abi_name;
					} else {
						end_name = lib_name;
					}
				}

				shlib_rule("../lib")

				print  ""
				print  "install \\"
				print  "install.libs \\"

				if ( ShlibVer == "cygdll" ) {

					dst_dirs = "$(DESTDIR)$(bindir) $(DESTDIR)$(libdir)";
					printf "install.%s :: %s $(LIBRARIES)\n", name, dst_dirs
					install_dll("$(bindir)",end_name);
					install_dll("$(libdir)",imp_name);

				} else {

					lib_dir = "$(DESTDIR)$(libdir)";
					printf "install.%s :: %s/%s\n", name, lib_dir, end_name
					print ""
					shlib_rule(lib_dir)
				}

				if ( overwrite == "yes" && name == "ncurses" )
				{
					if ( ShlibVer == "cygdll" ) {
						ovr_name = sprintf("libcurses%s.a", suffix)
						printf "\t@echo linking %s to %s\n", imp_name, ovr_name
						printf "\tcd $(DESTDIR)$(libdir) && (rm -f %s; $(LN_S) %s %s; )\n", ovr_name, imp_name, ovr_name
					} else {
						ovr_name = sprintf("libcurses%s", suffix)
						printf "\t@echo linking %s to %s\n", end_name, ovr_name
						printf "\tcd $(DESTDIR)$(libdir) && (rm -f %s; $(LN_S) %s %s; )\n", ovr_name, end_name, ovr_name
					}
				}
				if ( ldconfig != "" && ldconfig != ":" ) {
					printf "\t- test -z \"$(DESTDIR)\" && %s\n", ldconfig
				}
				print  ""
				print  "uninstall \\"
				print  "uninstall.libs \\"
				printf "uninstall.%s ::\n", name
				if ( ShlibVer == "cygdll" ) {

					printf "\t@echo uninstalling $(DESTDIR)$(bindir)/%s\n", end_name
					printf "\t-@rm -f $(DESTDIR)$(bindir)/%s\n", end_name

					printf "\t@echo uninstalling $(DESTDIR)$(libdir)/%s\n", imp_name
					printf "\t-@rm -f $(DESTDIR)$(libdir)/%s\n", imp_name

				} else {
					printf "\t@echo uninstalling $(DESTDIR)$(libdir)/%s\n", end_name
					removelinks("$(DESTDIR)$(libdir)")
					if ( overwrite == "yes" && name == "ncurses" )
					{
						if ( ShlibVer == "cygdll" ) {
							ovr_name = sprintf("libcurses%s.a", suffix)
						} else {
							ovr_name = sprintf("libcurses%s", suffix)
						}
						printf "\t-@rm -f $(DESTDIR)$(libdir)/%s\n", ovr_name
					}
				}
				if ( rmSoLocs == "yes" ) {
					print  ""
					print  "mostlyclean \\"
					print  "clean ::"
					printf "\t-@rm -f so_locations\n"
				}
			}
			else if ( MODEL == "LIBTOOL" )
			{
				if ( $2 == "c++" ) {
					compile="CXX"
				} else {
					compile="CC"
				}
				end_name = lib_name;
				printf "../lib/%s : $(%s_OBJS)\n", lib_name, OBJS
				printf "\tcd ../lib && $(LIBTOOL_LINK) $(%s) -o %s $(%s_OBJS:$o=.lo) -rpath $(DESTDIR)$(libdir) -version-info $(NCURSES_MAJOR):$(NCURSES_MINOR) $(SHLIB_LIST)\n", compile, lib_name, OBJS
				print  ""
				print  "install \\"
				print  "install.libs \\"
				printf "install.%s :: $(DESTDIR)$(libdir) ../lib/%s\n", name, lib_name
				printf "\t@echo installing ../lib/%s as $(DESTDIR)$(libdir)/%s\n", lib_name, lib_name
				printf "\tcd ../lib; $(LIBTOOL_INSTALL) $(INSTALL) %s $(DESTDIR)$(libdir)\n", lib_name
				print  ""
				print  "uninstall \\"
				print  "uninstall.libs \\"
				printf "uninstall.%s ::\n", name
				printf "\t@echo uninstalling $(DESTDIR)$(libdir)/%s\n", lib_name
				printf "\t-@$(LIBTOOL_UNINSTALL) rm -f $(DESTDIR)$(libdir)/%s\n", lib_name
			}
			else
			{
				end_name = lib_name;
				printf "../lib/%s : $(%s_OBJS)\n", lib_name, OBJS
				printf "\t$(AR) $(AR_OPTS) $@ $?\n"
				printf "\t$(RANLIB) $@\n"
				if ( host == "vxworks" )
				{
					printf "\t$(LD) $(LD_OPTS) $? -o $(@:.a=$o)\n"
				}
				print  ""
				print  "install \\"
				print  "install.libs \\"
				printf "install.%s :: $(DESTDIR)$(libdir) ../lib/%s\n", name, lib_name
				printf "\t@echo installing ../lib/%s as $(DESTDIR)$(libdir)/%s\n", lib_name, lib_name
				printf "\t$(INSTALL_DATA) ../lib/%s $(DESTDIR)$(libdir)/%s\n", lib_name, lib_name
				if ( overwrite == "yes" && lib_name == "libncurses.a" )
				{
					printf "\t@echo linking libcurses.a to libncurses.a\n"
					printf "\t-@rm -f $(DESTDIR)$(libdir)/libcurses.a\n"
					printf "\t(cd $(DESTDIR)$(libdir) && $(LN_S) libncurses.a libcurses.a)\n"
				}
				printf "\t$(RANLIB) $(DESTDIR)$(libdir)/%s\n", lib_name
				if ( host == "vxworks" )
				{
					printf "\t@echo installing ../lib/lib%s$o as $(DESTDIR)$(libdir)/lib%s$o\n", name, name
					printf "\t$(INSTALL_DATA) ../lib/lib%s$o $(DESTDIR)$(libdir)/lib%s$o\n", name, name
				}
				print  ""
				print  "uninstall \\"
				print  "uninstall.libs \\"
				printf "uninstall.%s ::\n", name
				printf "\t@echo uninstalling $(DESTDIR)$(libdir)/%s\n", lib_name
				printf "\t-@rm -f $(DESTDIR)$(libdir)/%s\n", lib_name
				if ( overwrite == "yes" && lib_name == "libncurses.a" )
				{
					printf "\t@echo linking libcurses.a to libncurses.a\n"
					printf "\t-@rm -f $(DESTDIR)$(libdir)/libcurses.a\n"
				}
				if ( host == "vxworks" )
				{
					printf "\t@echo uninstalling $(DESTDIR)$(libdir)/lib%s$o\n", name
					printf "\t-@rm -f $(DESTDIR)$(libdir)/lib%s$o\n", name
				}
			}
			print ""
			print "clean ::"
			removelinks("../lib");
			print ""
			print "mostlyclean::"
			printf "\t-rm -f $(%s_OBJS)\n", OBJS
			if ( MODEL == "LIBTOOL" ) {
				printf "\t-$(LIBTOOL_CLEAN) rm -f $(%s_OBJS:$o=.lo)\n", OBJS
			}
		}
		else if ( found == 2 )
		{
			print ""
			print "mostlyclean::"
			printf "\t-rm -f $(%s_OBJS)\n", OBJS
			if ( MODEL == "LIBTOOL" ) {
				printf "\t-$(LIBTOOL_CLEAN) rm -f $(%s_OBJS:$o=.lo)\n", OBJS
			}
			print ""
			print "clean ::"
			printf "\t-rm -f $(%s_OBJS)\n", OBJS
			if ( MODEL == "LIBTOOL" ) {
				printf "\t-$(LIBTOOL_CLEAN) rm -f $(%s_OBJS:$o=.lo)\n", OBJS
			}
		}
	}
