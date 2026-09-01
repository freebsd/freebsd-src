#!/bin/sh

# Helpers for exercising GEOM classes on top of a gzoned provider.  Source this
# in addition to geom_subr.sh.

# Create a vnode-backed md large enough for four 256m zones.  gzoned reserves
# space at the end of the provider for its metadata block and zone table, so an
# exact multiple of the zone size (1024m) would lose a zone.
zoned_backing_md()
{
	atf_check truncate -s 1025m backing_file
	attach_md md -t vnode -f backing_file
}

# Set up a gzoned provider on a fresh backing md.  Any argument is passed to
# gzoned create as its conventional zone specification.
zoned_attach_md()
{
	local conv=$1

	zoned_backing_md
	atf_check gzoned create -s 256m ${conv:+-c ${conv}} ${md}
}

# Number of zones on $1 matching the report option $2, e.g. "nonwp".
zoned_zone_count()
{
	zonectl -d $1 -c rz -o $2 -P summary | awk '/zones,/ {print $1}'
}
