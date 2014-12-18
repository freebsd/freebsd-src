sinclude(libtool.m4/libtool.m4)dnl
sinclude(libtool.m4/ltoptions.m4)dnl
sinclude(libtool.m4/ltsugar.m4)dnl
sinclude(libtool.m4/ltversion.m4)dnl
sinclude(libtool.m4/lt~obsolete.m4)dnl

m4_divert_text(HELP_CANON, [[
  NOTE: If PREFIX is not set, then the default values for --sysconfdir
  and --localstatedir are /etc and /var, respectively.]])
m4_divert_text(HELP_END, [[
Professional support for BIND is provided by Internet Systems Consortium,
Inc.  Information about paid support and training options is available at
https://www.isc.org/support.

Help can also often be found on the BIND Users mailing list
(https://lists.isc.org/mailman/listinfo/bind-users) or in the #bind
channel of the Freenode IRC service.]])
