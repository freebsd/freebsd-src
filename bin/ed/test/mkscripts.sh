#!/bin/sh -
# This script generates ed test scripts (.ed) from .t files
#	

PATH="/bin:/usr/bin:/usr/local/bin/:."
ED=$1
[ ! -x "$ED" ] && { echo "$ED: cannot execute"; exit 1; }

for i in *.t; do
	base=${i%.*}
	if [ -z "$base" ]; then
		echo "Error extracting base name from $i"
		continue
	fi
	cat <<-EOF >"$base.ed"
	#!/bin/sh -
	$ED - <<\EOT
	H
	r $base.d
	w $base.o
	EOT
	EOF
	cat "$i" >>"$base.ed"
	chmod +x "$base.ed"
done

for i in *.err; do
	base=${i%.*}
	if [ -z "$base" ]; then
		echo "Error extracting base name from $i"
		continue
	fi
	cat <<-EOF >"${base}.red"
	#!/bin/sh -
	$ED - <<\EOT
	H
	r $base.err
	w $base.o
	EOT
	EOF
	chmod +x "${base}.red"
done
