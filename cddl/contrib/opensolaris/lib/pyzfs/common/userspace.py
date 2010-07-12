#! /usr/bin/python2.4
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

"""This module implements the "zfs userspace" and "zfs groupspace" subcommands.
The only public interface is the zfs.userspace.do_userspace() function."""

import zfs.util
import zfs.ioctl
import zfs.dataset
import optparse
import sys
import pwd
import grp
import errno

_ = zfs.util._

# map from property name prefix -> (field name, isgroup)
props = {
    "userused@": ("used", False),
    "userquota@": ("quota", False),
    "groupused@": ("used", True),
    "groupquota@": ("quota", True),
}

def skiptype(options, prop):
	"""Return True if this property (eg "userquota@") should be skipped."""
	(field, isgroup) = props[prop]
	if field not in options.fields:
		return True
	if isgroup and "posixgroup" not in options.types and \
	    "smbgroup" not in options.types:
		return True
	if not isgroup and "posixuser" not in options.types and \
	    "smbuser" not in options.types:
		return True
	return False

def updatemax(d, k, v):
	d[k] = max(d.get(k, None), v)

def new_entry(options, isgroup, domain, rid):
	"""Return a dict("field": value) for this domain (string) + rid (int)"""

	if domain:
		idstr = "%s-%u" % (domain, rid)
	else:
		idstr = "%u" % rid

	(typename, mapfunc) = {
	    (1, 1): ("SMB Group",   lambda id: zfs.ioctl.sid_to_name(id, 0)),
	    (1, 0): ("POSIX Group", lambda id: grp.getgrgid(int(id)).gr_name),
	    (0, 1): ("SMB User",    lambda id: zfs.ioctl.sid_to_name(id, 1)),
	    (0, 0): ("POSIX User",  lambda id: pwd.getpwuid(int(id)).pw_name)
	}[isgroup, bool(domain)]

	if typename.lower().replace(" ", "") not in options.types:
		return None

	v = dict()
	v["type"] = typename

	# python's getpwuid/getgrgid is confused by ephemeral uids
	if not options.noname and rid < 1<<31:
		try:
			v["name"] = mapfunc(idstr)
		except KeyError:
			pass

	if "name" not in v:
		v["name"] = idstr
		if not domain:
			# it's just a number, so pad it with spaces so
			# that it will sort numerically
			v["name.sort"] = "%20d" % rid
	# fill in default values
	v["used"] = "0"
	v["used.sort"] = 0
	v["quota"] = "none"
	v["quota.sort"] = 0
	return v

def process_one_raw(acct, maxfieldlen, options, prop, elem):
	"""Update the acct and maxfieldlen dicts to incorporate the
	information from this elem from Dataset.userspace(prop)."""

	(domain, rid, value) = elem
	(field, isgroup) = props[prop]

	if options.translate and domain:
		try:
			rid = zfs.ioctl.sid_to_id("%s-%u" % (domain, rid),
			    not isgroup)
			domain = None
		except KeyError:
			pass;
	key = (isgroup, domain, rid)
		
	try:
		v = acct[key]
	except KeyError:
		v = new_entry(options, isgroup, domain, rid)
		if not v:
			return
		acct[key] = v

	# Add our value to an existing value, which may be present if
	# options.translate is set.
	value = v[field + ".sort"] = value + v[field + ".sort"]

	if options.parsable:
		v[field] = str(value)
	else:
		v[field] = zfs.util.nicenum(value)
	for k in v.keys():
		# some of the .sort fields are integers, so have no len()
		if isinstance(v[k], str):
			updatemax(maxfieldlen, k, len(v[k]))

def do_userspace():
	"""Implements the "zfs userspace" and "zfs groupspace" subcommands."""

	def usage(msg=None):
		parser.print_help()
		if msg:
			print
			parser.exit("zfs: error: " + msg)
		else:
			parser.exit()

	if sys.argv[1] == "userspace":
		defaulttypes = "posixuser,smbuser"
	else:
		defaulttypes = "posixgroup,smbgroup"

	fields = ("type", "name", "used", "quota")
	ljustfields = ("type", "name")
	types = ("all", "posixuser", "smbuser", "posixgroup", "smbgroup")

	u = _("%s [-niHp] [-o field[,...]] [-sS field] ... \n") % sys.argv[1]
	u += _("    [-t type[,...]] <filesystem|snapshot>")
	parser = optparse.OptionParser(usage=u, prog="zfs")

	parser.add_option("-n", action="store_true", dest="noname",
	    help=_("Print numeric ID instead of user/group name"))
	parser.add_option("-i", action="store_true", dest="translate",
	    help=_("translate SID to posix (possibly ephemeral) ID"))
	parser.add_option("-H", action="store_true", dest="noheaders",
	    help=_("no headers, tab delimited output"))
	parser.add_option("-p", action="store_true", dest="parsable",
	    help=_("exact (parsable) numeric output"))
	parser.add_option("-o", dest="fields", metavar="field[,...]",
	    default="type,name,used,quota",
	    help=_("print only these fields (eg type,name,used,quota)"))
	parser.add_option("-s", dest="sortfields", metavar="field",
	    type="choice", choices=fields, default=list(),
	    action="callback", callback=zfs.util.append_with_opt,
	    help=_("sort field"))
	parser.add_option("-S", dest="sortfields", metavar="field",
	    type="choice", choices=fields, #-s sets the default
	    action="callback", callback=zfs.util.append_with_opt,
	    help=_("reverse sort field"))
	parser.add_option("-t", dest="types", metavar="type[,...]",
	    default=defaulttypes,
	    help=_("print only these types (eg posixuser,smbuser,posixgroup,smbgroup,all)"))

	(options, args) = parser.parse_args(sys.argv[2:])
	if len(args) != 1:
		usage(_("wrong number of arguments"))
	dsname = args[0]

	options.fields = options.fields.split(",")
	for f in options.fields:
		if f not in fields:
			usage(_("invalid field %s") % f)

	options.types = options.types.split(",")
	for t in options.types:
		if t not in types:
			usage(_("invalid type %s") % t)

	if not options.sortfields:
		options.sortfields = [("-s", "type"), ("-s", "name")]

	if "all" in options.types:
		options.types = types[1:]

	ds = zfs.dataset.Dataset(dsname, types=("filesystem"))

	if ds.getprop("jailed") and zfs.ioctl.isglobalzone():
		options.noname = True

	if not ds.getprop("useraccounting"):
		print(_("Initializing accounting information on old filesystem, please wait..."))
		ds.userspace_upgrade()

	acct = dict()
	maxfieldlen = dict()

	# gather and process accounting information
	for prop in props.keys():
		if skiptype(options, prop):
			continue;
		for elem in ds.userspace(prop):
			process_one_raw(acct, maxfieldlen, options, prop, elem)

	# print out headers
	if not options.noheaders:
		line = str()
		for field in options.fields:
			# make sure the field header will fit
			updatemax(maxfieldlen, field, len(field))

			if field in ljustfields:
				fmt = "%-*s  "
			else:
				fmt = "%*s  "
			line += fmt % (maxfieldlen[field], field.upper())
		print(line)

	# custom sorting func
	def cmpkey(val):
		l = list()
		for (opt, field) in options.sortfields:
			try:
				n = val[field + ".sort"]
			except KeyError:
				n = val[field]
			if opt == "-S":
				# reverse sorting
				try:
					n = -n
				except TypeError:
					# it's a string; decompose it
					# into an array of integers,
					# each one the negative of that
					# character
					n = [-ord(c) for c in n]
			l.append(n)
		return l

	# print out data lines
	for val in sorted(acct.itervalues(), key=cmpkey):
		line = str()
		for field in options.fields:
			if options.noheaders:
				line += val[field]
				line += "\t"
			else:
				if field in ljustfields:
					fmt = "%-*s  "
				else:
					fmt = "%*s  "
				line += fmt % (maxfieldlen[field], val[field])
		print(line)
