#!/bin/sh
#
# $FreeBSD$
# 
# Search for a.out libraries and move them to an aout subdirectory of
# the elf library directory.
#
# The arguments are the directories to search.
#
libdirs="$*"

# Create a temporary tool to get the timestamp of libraries. No, I don't
# want to use perl or whatever.
create_get_time_stamp ( )
{
	echo "#include <stdio.h>" > /tmp/get_time_stamp.c
	echo "#include <sys/stat.h>" >> /tmp/get_time_stamp.c
	echo "int main(int argc, char *argv[]) {" >> /tmp/get_time_stamp.c
	echo "int ierr; struct stat fs;" >> /tmp/get_time_stamp.c
	echo "if ((ierr = stat(argv[1],&fs)) == 0)" >> /tmp/get_time_stamp.c
	echo "printf(\"%ld\n\",(long) fs.st_mtime);" >> /tmp/get_time_stamp.c
	echo "return (ierr); }" >> /tmp/get_time_stamp.c
	gcc -o /tmp/get_time_stamp /tmp/get_time_stamp.c
	rm /tmp/get_time_stamp.c
	return
}

# Move an a.out library to the aout subdirectory of the elf directory.
move_file ( ) 
{
	if test -d $dir/aout; then
	else
		echo "Creating directory $dir/aout"
		mkdir $dir/aout
		ldconfig -m $dir/aout
	fi
	fname=${file#$dir/}
	if test -f $dir/aout/$fname; then
		if test -x /tmp/get_time_stamp; then
		else
			create_get_time_stamp
		fi
		t1=`/tmp/get_time_stamp $dir/aout/$fname`
		t2=`/tmp/get_time_stamp $file`
		if test $t1 -gt $t2; then
			echo $file is older than $dir/aout/$fname
			answer=""
			while test "$answer" != "y" -a "$answer" != "n"; do
				read -p "OK to delete the older file? (y/n) " answer
			done
			if test $answer = "y"; then
				echo Deleting $file
				chflags noschg $file
				rm $file
			else
				echo "You need to move $file out of $dir because that's an elf directory"
			fi
		else
			echo $dir/aout/$fname is older than $file
			answer=""
			while test "$answer" != "y" -a "$answer" != "n"; do
				read -p "OK to overwrite the older file? (y/n) " answer
			done
			if test $answer = "y"; then
				echo Overwriting $dir/aout/$fname with $file
				chflags noschg $file
				mv $file $dir/aout/$fname
				ldconfig -R
			else
				echo "You need to move $file out of $dir because that's an elf directory"
			fi
		fi
	else
		echo Move $fname from $dir to $dir/aout
		chflags noschg $file
		mv $file $dir/aout/$fname
		ldconfig -R
	fi
	return
}

# Given a list of files in a directory, find those that are a.out
# libraries and move them.
move_if_aout ( ) 
{
	# Check each library
	for file in $files
	do
		# Don't touch symbolic links yet. It's not clear how
		# they should be handled.
		if test -h $file; then
		else
			# Check that this is a normal file.
			if test -f $file; then
				# Identify the file by magic
				filemagic=`file $file`

				# Check if the file is an a.out library
				if expr "$filemagic" : ".*$aoutmagic"; then
					# Move the a.out library
					move_file
				fi
			fi
		fi
	done
	return
}

# Only search the directories specified.
for dir in $libdirs
do
	# Make sure the directory exists, or ldconfig will choke later.
	mkdir -p $dir $dir/aout

	echo "Searching library directory $dir for a.out libraries..."

	# Get a list of archive libraries.
	files=`ls $dir/*.a 2> /dev/null`

	# a.out archive libraries look like this:
	aoutmagic="current ar archive random library"

	# Move each a.out archive library:
	move_if_aout

	# Get a list of shared libraries
	files=`ls $dir/*.so.*.* 2> /dev/null`

	# a.out shared libraries look like this:
	aoutmagic="FreeBSD/i386 compact demand paged shared library"

	# Move each a.out shared library:
	move_if_aout
done

# If we created the time stamp program, delete it:
if test -x /tmp/get_time_stamp; then
	rm /tmp/get_time_stamp
fi
