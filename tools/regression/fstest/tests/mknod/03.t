#!/bin/sh
# $FreeBSD$

desc="mknod returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..11"

expect 0 mkdir ${name255} 0755				# 1
expect 0 mkdir ${name255}/${name255} 0755		# 2
expect 0 mkdir ${name255}/${name255}/${name255} 0755	# 3
expect 0 mkdir ${path1021} 0755				# 4
expect 0 mknod ${path1023} f 0644 0 0			# 5
expect 0 unlink ${path1023}				# 6
expect ENAMETOOLONG mknod ${path1024} f 0644 0 0	# 7
expect 0 rmdir ${path1021}				# 8
expect 0 rmdir ${name255}/${name255}/${name255}		# 9
expect 0 rmdir ${name255}/${name255}			# 10
expect 0 rmdir ${name255}				# 11
