#!/bin/sh
# $FreeBSD$

# A directory in a device different from that where the tests are run
TMPDIR=/tmp/regress.$$
COUNT=0

# Begin an individual test
begin()
{
	COUNT=`expr $COUNT + 1`
	OK=1
	if [ -z "$FS" ]
	then
		NAME="$1"
	else
		NAME="$1 (cross device)"
	fi
	rm -rf testdir $TMPDIR/testdir
	mkdir -p testdir $TMPDIR/testdir
	cd testdir
}

# End an individual test
end()
{
	if [ $OK = 1 ]
	then
		echo -n 'ok '
	else
		echo -n 'not ok '
	fi
	echo "$COUNT - $NAME"
	cd ..
	rm -rf testdir $TMPDIR/testdir
}

# Make a file that can later be verified
mkf()
{
	CN=`basename $1`
	echo "$CN-$CN" >$1
}

# Verify that the file specified is correct
ckf()
{
	if [ -f $2 ] && echo "$1-$1" | diff -q - $2
	then
		ok
	else
		notok
	fi
}

# Make a fifo that can later be verified
mkp()
{
	mkfifo $1
}

# Verify that the file specified is correct
ckp()
{
	if [ -p $2 ]
	then
		ok
	else
		notok
	fi
}

# Make a directory that can later be verified
mkd()
{
	CN=`basename $1`
	mkdir -p $1/"$CN-$CN"
}

# Verify that the directory specified is correct
ckd()
{
	if [ -d $2/$1-$1 ]
	then
		ok
	else
		notok
	fi
}

# Verify that the specified file does not exist
# (is not there)
cknt()
{
	if [ -e $1 ]
	then
		notok
	else
		ok
	fi
}

# A part of a test succeeds
ok()
{
	:
}

# A part of a test fails
notok()
{
	OK=0
}

# Run all tests locally and across devices
echo 1..24
for FS in '' $TMPDIR/testdir/
do
	begin 'Rename file'
	mkf fa
	mv fa ${FS}fb
	ckf fa ${FS}fb
	cknt fa
	end

	begin 'Move files into directory'
	mkf fa
	mkf fb
	mkdir -p ${FS}1/2/3
	mv fa fb ${FS}1/2/3
	ckf fa ${FS}1/2/3/fa
	ckf fb ${FS}1/2/3/fb
	cknt fa
	cknt fb
	end

	begin 'Move file from directory to file'
	mkdir -p 1/2/3
	mkf 1/2/3/fa
	mv 1/2/3/fa ${FS}fb
	ckf fa ${FS}fb
	cknt 1/2/3/fa
	end

	begin 'Move file from directory to directory'
	mkdir -p da1/da2/da3
	mkdir -p ${FS}db1/db2/db3
	mkf da1/da2/da3/fa
	mv da1/da2/da3/fa ${FS}db1/db2/db3/fb
	ckf fa ${FS}db1/db2/db3/fb
	cknt da1/da2/da3/fa
	end

	begin 'Rename directory'
	mkd da
	mv da ${FS}db
	ckd da ${FS}db
	cknt da
	end

	begin 'Move directory to directory name'
	mkd da1/da2/da3/da
	mkdir -p ${FS}db1/db2/db3
	mv da1/da2/da3/da ${FS}db1/db2/db3/db
	ckd da ${FS}db1/db2/db3/db
	cknt da1/da2/da3/da
	end

	begin 'Move directory to directory'
	mkd da1/da2/da3/da
	mkdir -p ${FS}db1/db2/db3
	mv da1/da2/da3/da ${FS}db1/db2/db3
	ckd da ${FS}db1/db2/db3/da
	cknt da1/da2/da3/da
	end

	begin 'Move directory to existing directory'
	mkd da1/da2/da3/da
	mkdir -p ${FS}db1/db2/db3/da
	mv da1/da2/da3/da ${FS}db1/db2/db3
	ckd da ${FS}db1/db2/db3/da
	cknt da1/da2/da3/da
	end

	begin 'Rename fifo'
	mkp fa
	mv fa ${FS}fb
	ckp fa ${FS}fb
	cknt fa
	end

	begin 'Move fifos into directory'
	mkp fa
	mkp fb
	mkdir -p ${FS}1/2/3
	mv fa fb ${FS}1/2/3
	ckp fa ${FS}1/2/3/fa
	ckp fb ${FS}1/2/3/fb
	cknt fa
	cknt fb
	end

	begin 'Move fifo from directory to fifo'
	mkdir -p 1/2/3
	mkp 1/2/3/fa
	mv 1/2/3/fa ${FS}fb
	ckp fa ${FS}fb
	cknt 1/2/3/fa
	end

	begin 'Move fifo from directory to directory'
	mkdir -p da1/da2/da3
	mkdir -p ${FS}db1/db2/db3
	mkp da1/da2/da3/fa
	mv da1/da2/da3/fa ${FS}db1/db2/db3/fb
	ckp fa ${FS}db1/db2/db3/fb
	cknt da1/da2/da3/fa
	end
done
