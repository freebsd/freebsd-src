# $NetBSD: t_db.sh,v 1.6 2015/11/18 18:35:35 christos Exp $
#
# Copyright (c) 2008 The NetBSD Foundation, Inc.
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

prog_db()
{
	echo $(atf_get_srcdir)/h_db
}

prog_lfsr()
{
	echo $(atf_get_srcdir)/h_lfsr
}

dict()
{
	if [ -f /usr/share/dict/words ]; then
		echo /usr/share/dict/words
	elif [ -f /usr/dict/words ]; then
		echo /usr/dict/words
	else
		echo ""
		atf_fail "no dictionary found"
	fi
}

# Begin FreeBSD
dict()
{
	echo /usr/share/dict/words
}
# End FreeBSD

SEVEN_SEVEN="abcdefg|abcdefg|abcdefg|abcdefg|abcdefg|abcdefg|abcdefg"

atf_test_case small_btree
small_btree_head()
{
	atf_set "descr" \
		"Checks btree database using small keys and small data" \
		"pairs: takes the first hundred entries in the dictionary," \
		"and makes them be key/data pairs."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
small_btree_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	sed 200q $(dict) >exp

	for i in `sed 200q $(dict)`; do
		echo p
		echo k$i
		echo d$i
		echo g
		echo k$i
	done >in

	atf_check -o file:exp "$(prog_db)" btree in
}

atf_test_case small_hash
small_hash_head()
{
	atf_set "descr" \
		"Checks hash database using small keys and small data" \
		"pairs: takes the first hundred entries in the dictionary," \
		"and makes them be key/data pairs."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
small_hash_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	sed 200q $(dict) >exp

	for i in `sed 200q $(dict)`; do
		echo p
		echo k$i
		echo d$i
		echo g
		echo k$i
	done >in

	atf_check -o file:exp "$(prog_db)" hash in
}

atf_test_case small_recno
small_recno_head()
{
	atf_set "descr" \
		"Checks recno database using small keys and small data" \
		"pairs: takes the first hundred entries in the dictionary," \
		"and makes them be key/data pairs."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
small_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	sed 200q $(dict) >exp

	sed 200q $(dict) |
	awk '{ 
		++i;
		printf("p\nk%d\nd%s\ng\nk%d\n", i, $0, i);
	}' >in

	atf_check -o file:exp "$(prog_db)" recno in
}

atf_test_case medium_btree
medium_btree_head()
{
	atf_set "descr" \
		"Checks btree database using small keys and medium" \
		"data pairs: takes the first 200 entries in the" \
		"dictionary, and gives them each a medium size data entry."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
medium_btree_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	mdata=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz
	echo $mdata |
	awk '{ for (i = 1; i < 201; ++i) print $0 }' >exp

	for i in $(sed 200q $(dict)); do
		echo p
		echo k$i
		echo d$mdata
		echo g
		echo k$i
	done >in

	atf_check -o file:exp "$(prog_db)" btree in
}

atf_test_case medium_hash
medium_hash_head()
{
	atf_set "descr" \
		"Checks hash database using small keys and medium" \
		"data pairs: takes the first 200 entries in the" \
		"dictionary, and gives them each a medium size data entry."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
medium_hash_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	mdata=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz
	echo $mdata |
	awk '{ for (i = 1; i < 201; ++i) print $0 }' >exp

	for i in $(sed 200q $(dict)); do
		echo p
		echo k$i
		echo d$mdata
		echo g
		echo k$i
	done >in

	atf_check -o file:exp "$(prog_db)" hash in
}

atf_test_case medium_recno
medium_recno_head()
{
	atf_set "descr" \
		"Checks recno database using small keys and medium" \
		"data pairs: takes the first 200 entries in the" \
		"dictionary, and gives them each a medium size data entry."
}
medium_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	mdata=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz
	echo $mdata |
	awk '{ for (i = 1; i < 201; ++i) print $0 }' >exp

	echo $mdata | 
	awk '{  for (i = 1; i < 201; ++i)
		printf("p\nk%d\nd%s\ng\nk%d\n", i, $0, i);
	}' >in

	atf_check -o file:exp "$(prog_db)" recno in
}

atf_test_case big_btree
big_btree_head()
{
	atf_set "descr" \
		"Checks btree database using small keys and big data" \
		"pairs: inserts the programs in /bin with their paths" \
		"as their keys."
}
big_btree_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	(find /bin -type f -print | xargs cat) >exp

	for psize in 512 16384 65536; do
		echo "checking page size: $psize"

		for i in `find /bin -type f -print`; do
			echo p
			echo k$i
			echo D$i
			echo g
			echo k$i
		done >in

		atf_check "$(prog_db)" -o out btree in
		cmp -s exp out || atf_fail "test failed for page size: $psize"
	done
}

atf_test_case big_hash
big_hash_head()
{
	atf_set "descr" \
		"Checks hash database using small keys and big data" \
		"pairs: inserts the programs in /bin with their paths" \
		"as their keys."
}
big_hash_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	(find /bin -type f -print | xargs cat) >exp

	for i in `find /bin -type f -print`; do
		echo p
		echo k$i
		echo D$i
		echo g
		echo k$i
	done >in

	atf_check "$(prog_db)" -o out hash in
	cmp -s exp out || atf_fail "test failed"
}

atf_test_case big_recno
big_recno_head()
{
	atf_set "descr" \
		"Checks recno database using small keys and big data" \
		"pairs: inserts the programs in /bin with their paths" \
		"as their keys."
}
big_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	(find /bin -type f -print | xargs cat) >exp

	find /bin -type f -print | 
	awk '{
		++i;
		printf("p\nk%d\nD%s\ng\nk%d\n", i, $0, i);
	}' >in

	for psize in 512 16384 65536; do
		echo "checking page size: $psize"

		atf_check "$(prog_db)" -o out recno in
		cmp -s exp out || atf_fail "test failed for page size: $psize"
	done
}

atf_test_case random_recno
random_recno_head()
{
	atf_set "descr" "Checks recno database using random entries"
}
random_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 37; i <= 37 + 88 * 17; i += 17) {
			if (i % 41)
				s = substr($0, 1, i % 41);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		for (i = 1; i <= 15; ++i) {
			if (i % 41)
				s = substr($0, 1, i % 41);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		for (i = 19234; i <= 19234 + 61 * 27; i += 27) {
			if (i % 41)
				s = substr($0, 1, i % 41);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		exit
	}' >exp

	cat exp |
	awk 'BEGIN {
			i = 37;
			incr = 17;
		}
		{
			printf("p\nk%d\nd%s\n", i, $0);
			if (i == 19234 + 61 * 27)
				exit;
			if (i == 37 + 88 * 17) {
				i = 1;
				incr = 1;
			} else if (i == 15) {
				i = 19234;
				incr = 27;
			} else
				i += incr;
		}
		END {
			for (i = 37; i <= 37 + 88 * 17; i += 17)
				printf("g\nk%d\n", i);
			for (i = 1; i <= 15; ++i)
				printf("g\nk%d\n", i);
			for (i = 19234; i <= 19234 + 61 * 27; i += 27)
				printf("g\nk%d\n", i);
		}' >in

	atf_check -o file:exp "$(prog_db)" recno in
}

atf_test_case reverse_recno
reverse_recno_head()
{
	atf_set "descr" "Checks recno database using reverse order entries"
}
reverse_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk ' {
		for (i = 1500; i; --i) {
			if (i % 34)
				s = substr($0, 1, i % 34);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		exit;
	}' >exp

	cat exp |
	awk 'BEGIN {
			i = 1500;
		}
		{
			printf("p\nk%d\nd%s\n", i, $0);
			--i;
		}
		END {
			for (i = 1500; i; --i) 
				printf("g\nk%d\n", i);
		}' >in

	atf_check -o file:exp "$(prog_db)" recno in
}
		
atf_test_case alternate_recno
alternate_recno_head()
{
	atf_set "descr" "Checks recno database using alternating order entries"
}
alternate_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk ' {
		for (i = 1; i < 1200; i += 2) {
			if (i % 34)
				s = substr($0, 1, i % 34);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		for (i = 2; i < 1200; i += 2) {
			if (i % 34)
				s = substr($0, 1, i % 34);
			else
				s = substr($0, 1);
			printf("input key %d: %s\n", i, s);
		}
		exit;
	}' >exp

	cat exp |
	awk 'BEGIN {
			i = 1;
			even = 0;
		}
		{
			printf("p\nk%d\nd%s\n", i, $0);
			i += 2;
			if (i >= 1200) {
				if (even == 1)
					exit;
				even = 1;
				i = 2;
			}
		}
		END {
			for (i = 1; i < 1200; ++i) 
				printf("g\nk%d\n", i);
		}' >in

	atf_check "$(prog_db)" -o out recno in
	
	sort -o exp exp
	sort -o out out

	cmp -s exp out || atf_fail "test failed"
}

h_delete()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	type=$1

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 1; i <= 120; ++i)
			printf("%05d: input key %d: %s\n", i, i, $0);
	}' >exp

	cat exp |
	awk '{
		printf("p\nk%d\nd%s\n", ++i, $0);
	}
	END {
		printf("fR_NEXT\n");
		for (i = 1; i <= 120; ++i)
			printf("s\n");
		printf("fR_CURSOR\ns\nkXX\n");
		printf("r\n");
		printf("fR_NEXT\ns\n");
		printf("fR_CURSOR\ns\nk1\n");
		printf("r\n");
		printf("fR_FIRST\ns\n");
	}' >in

	# For btree, the records are ordered by the string representation
	# of the key value.  So sort the expected output file accordingly,
	# and set the seek_last key to the last expected key value.

	if [ "$type" = "btree" ] ; then
		sed -e 's/kXX/k99/' < in > tmp
		mv tmp in
		sort -d -k4 < exp > tmp
		mv tmp exp
		echo $SEVEN_SEVEN |
		awk '{
			printf("%05d: input key %d: %s\n", 99, 99, $0);
			printf("seq failed, no such key\n");
			printf("%05d: input key %d: %s\n", 1, 1, $0);
			printf("%05d: input key %d: %s\n", 10, 10, $0);
			exit;
		}' >> exp
	else
	# For recno, records are ordered by numerical key value.  No sort
	# is needed, but still need to set proper seek_last key value.
		sed -e 's/kXX/k120/' < in > tmp
		mv tmp in
		echo $SEVEN_SEVEN |
		awk '{
			printf("%05d: input key %d: %s\n", 120, 120, $0);
			printf("seq failed, no such key\n");
			printf("%05d: input key %d: %s\n", 1, 1, $0);
			printf("%05d: input key %d: %s\n", 2, 2, $0);
			exit;
		}' >> exp
	fi

	atf_check "$(prog_db)" -o out $type in
	atf_check -o file:exp cat out
}

atf_test_case delete_btree
delete_btree_head()
{
	atf_set "descr" "Checks removing records in btree database"
}
delete_btree_body()
{
	h_delete btree
}

atf_test_case delete_recno
delete_recno_head()
{
	atf_set "descr" "Checks removing records in recno database"
}
delete_recno_body()
{
	h_delete recno
}

h_repeated()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo "" | 
	awk 'BEGIN {
		for (i = 1; i <= 10; ++i) {
			printf("p\nkkey1\nD/bin/sh\n");
			printf("p\nkkey2\nD/bin/csh\n");
			if (i % 8 == 0) {
				printf("c\nkkey2\nD/bin/csh\n");
				printf("c\nkkey1\nD/bin/sh\n");
				printf("e\t%d of 10 (comparison)\n", i);
			} else
				printf("e\t%d of 10             \n", i);
			printf("r\nkkey1\nr\nkkey2\n");
		}
	}' >in

	$(prog_db) btree in
}

atf_test_case repeated_btree
repeated_btree_head()
{
	atf_set "descr" \
		"Checks btree database with repeated small keys and" \
		"big data pairs. Makes sure that overflow pages are reused"
}
repeated_btree_body()
{
	h_repeated btree
}

atf_test_case repeated_hash
repeated_hash_head()
{
	atf_set "descr" \
		"Checks hash database with repeated small keys and" \
		"big data pairs. Makes sure that overflow pages are reused"
}
repeated_hash_body()
{
	h_repeated hash
}

atf_test_case duplicate_btree
duplicate_btree_head()
{
	atf_set "descr" "Checks btree database with duplicate keys"
}
duplicate_btree_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 1; i <= 543; ++i)
			printf("%05d: input key %d: %s\n", i, i, $0);
		exit;
	}' >exp

	cat exp | 
	awk '{
		if (i++ % 2)
			printf("p\nkduplicatekey\nd%s\n", $0);
		else
			printf("p\nkunique%dkey\nd%s\n", i, $0);
	}
	END {
			printf("o\n");
	}' >in

	atf_check -o file:exp -x "$(prog_db) -iflags=1 btree in | sort"
}

h_cursor_flags()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	type=$1

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 1; i <= 20; ++i)
			printf("%05d: input key %d: %s\n", i, i, $0);
		exit;
	}' >exp

	# Test that R_CURSOR doesn't succeed before cursor initialized
	cat exp |
	awk '{
		if (i == 10)
			exit;
		printf("p\nk%d\nd%s\n", ++i, $0);
	}
	END {
		printf("fR_CURSOR\nr\n");
		printf("eR_CURSOR SHOULD HAVE FAILED\n");
	}' >in

	atf_check -o ignore -e ignore -s ne:0 "$(prog_db)" -o out $type in
	atf_check -s ne:0 test -s out

	cat exp |
	awk '{
		if (i == 10)
			exit;
		printf("p\nk%d\nd%s\n", ++i, $0);
	}
	END {
		printf("fR_CURSOR\np\nk1\ndsome data\n");
		printf("eR_CURSOR SHOULD HAVE FAILED\n");
	}' >in

	atf_check -o ignore -e ignore -s ne:0 "$(prog_db)" -o out $type in
	atf_check -s ne:0 test -s out
}

atf_test_case cursor_flags_btree
cursor_flags_btree_head()
{
	atf_set "descr" \
		"Checks use of cursor flags without initialization in btree database"
}
cursor_flags_btree_body()
{
	h_cursor_flags btree
}

atf_test_case cursor_flags_recno
cursor_flags_recno_head()
{
	atf_set "descr" \
		"Checks use of cursor flags without initialization in recno database"
}
cursor_flags_recno_body()
{
	h_cursor_flags recno
}

atf_test_case reverse_order_recno
reverse_order_recno_head()
{
	atf_set "descr" "Checks reverse order inserts in recno database"
}
reverse_order_recno_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 1; i <= 779; ++i)
			printf("%05d: input key %d: %s\n", i, i, $0);
		exit;
	}' >exp

	cat exp |
	awk '{
		if (i == 0) {
			i = 1;
			printf("p\nk1\nd%s\n", $0);
			printf("%s\n", "fR_IBEFORE");
		} else
			printf("p\nk1\nd%s\n", $0);
	}
	END {
			printf("or\n");
	}' >in

	atf_check -o file:exp "$(prog_db)" recno in
}

atf_test_case small_page_btree
small_page_btree_head()
{
	atf_set "descr" \
		"Checks btree database with lots of keys and small page" \
		"size: takes the first 20000 entries in the dictionary," \
		"reverses them, and gives them each a small size data" \
		"entry. Uses a small page size to make sure the btree" \
		"split code gets hammered."
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
small_page_btree_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	mdata=abcdefghijklmnopqrstuvwxy
	echo $mdata |
	awk '{ for (i = 1; i < 20001; ++i) print $0 }' >exp

	for i in `sed 20000q $(dict) | rev`; do
		echo p
		echo k$i
		echo d$mdata
		echo g
		echo k$i
	done >in

	atf_check -o file:exp "$(prog_db)" -i psize=512 btree in
}

h_byte_orders()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	type=$1

	sed 50q $(dict) >exp
	for order in 1234 4321; do
		for i in `sed 50q $(dict)`; do
			echo p
			echo k$i
			echo d$i
			echo g
			echo k$i
		done >in

		atf_check -o file:exp "$(prog_db)" -ilorder=$order -f byte.file $type in

		for i in `sed 50q $(dict)`; do
			echo g
			echo k$i
		done >in

		atf_check -o file:exp "$(prog_db)" -s -ilorder=$order -f byte.file $type in
	done
}

atf_test_case byte_orders_btree
byte_orders_btree_head()
{
	atf_set "descr" "Checks btree database using differing byte orders"
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
byte_orders_btree_body()
{
	h_byte_orders btree
}

atf_test_case byte_orders_hash
byte_orders_hash_head()
{
	atf_set "descr" "Checks hash database using differing byte orders"
}
byte_orders_hash_body()
{
	h_byte_orders hash
}

h_bsize_ffactor()
{
	bsize=$1
	ffactor=$2

	echo "bucketsize $bsize, fill factor $ffactor"
	atf_check -o file:exp "$(prog_db)" "-ibsize=$bsize,\
ffactor=$ffactor,nelem=25000,cachesize=65536" hash in
}

atf_test_case bsize_ffactor
bsize_ffactor_head()
{
	atf_set "timeout" "1800"
	atf_set "descr" "Checks hash database with various" \
					"bucketsizes and fill factors"
	# Begin FreeBSD
	atf_set "require.files" /usr/share/dict/words
	# End FreeBSD
}
bsize_ffactor_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	echo $SEVEN_SEVEN |
	awk '{
		for (i = 1; i <= 10000; ++i) {
			if (i % 34)
				s = substr($0, 1, i % 34);
			else
				s = substr($0, 1);
			printf("%s\n", s);
		}
		exit;

	}' >exp

	sed 10000q $(dict) |
	awk 'BEGIN {
		ds="'$SEVEN_SEVEN'"
	}
	{
		if (++i % 34)
			s = substr(ds, 1, i % 34);
		else
			s = substr(ds, 1);
		printf("p\nk%s\nd%s\n", $0, s);
	}' >in

	sed 10000q $(dict) |
	awk '{
		++i;
		printf("g\nk%s\n", $0);
	}' >>in

	h_bsize_ffactor 256 11
	h_bsize_ffactor 256 14
	h_bsize_ffactor 256 21

	h_bsize_ffactor 512 21
	h_bsize_ffactor 512 28
	h_bsize_ffactor 512 43

	h_bsize_ffactor 1024 43
	h_bsize_ffactor 1024 57
	h_bsize_ffactor 1024 85

	h_bsize_ffactor 2048 85
	h_bsize_ffactor 2048 114
	h_bsize_ffactor 2048 171

	h_bsize_ffactor 4096 171
	h_bsize_ffactor 4096 228
	h_bsize_ffactor 4096 341

	h_bsize_ffactor 8192 341
	h_bsize_ffactor 8192 455
	h_bsize_ffactor 8192 683

	h_bsize_ffactor 16384 341
	h_bsize_ffactor 16384 455
	h_bsize_ffactor 16384 683

	h_bsize_ffactor 32768 341
	h_bsize_ffactor 32768 455
	h_bsize_ffactor 32768 683

	# Begin FreeBSD
	if false; then
	# End FreeBSD
	h_bsize_ffactor 65536 341
	h_bsize_ffactor 65536 455
	h_bsize_ffactor 65536 683
	# Begin FreeBSD
	fi
	# End FreeBSD
}

# This tests 64K block size addition/removal
atf_test_case four_char_hash
four_char_hash_head()
{
	atf_set "descr" \
		"Checks hash database with 4 char key and" \
		"value insert on a 65536 bucket size"
}
four_char_hash_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}

	cat >in <<EOF
p
k1234
d1234
r
k1234
EOF

	# Begin FreeBSD
	if true; then
		atf_check "$(prog_db)" -i bsize=32768 hash in
	else
	# End FreeBSD
	atf_check "$(prog_db)" -i bsize=65536 hash in
	# Begin FreeBSD
	fi
	# End FreeBSD
}


atf_test_case bsize_torture
bsize_torture_head()
{
	atf_set "timeout" "36000"
	atf_set "descr" "Checks hash database with various bucket sizes"
}
bsize_torture_body()
{
	TMPDIR="$(pwd)/db_dir"; export TMPDIR
	mkdir ${TMPDIR}
	# Begin FreeBSD
	#
	# db(3) doesn't support 64kB bucket sizes
	for i in 2048 4096 8192 16384 32768 # 65536
	# End FreeBSD
	do
		atf_check "$(prog_lfsr)" $i
	done
}

atf_init_test_cases()
{
	atf_add_test_case small_btree
	atf_add_test_case small_hash
	atf_add_test_case small_recno
	atf_add_test_case medium_btree
	atf_add_test_case medium_hash
	atf_add_test_case medium_recno
	atf_add_test_case big_btree
	atf_add_test_case big_hash
	atf_add_test_case big_recno
	atf_add_test_case random_recno
	atf_add_test_case reverse_recno
	atf_add_test_case alternate_recno
	atf_add_test_case delete_btree
	atf_add_test_case delete_recno
	atf_add_test_case repeated_btree
	atf_add_test_case repeated_hash
	atf_add_test_case duplicate_btree
	atf_add_test_case cursor_flags_btree
	atf_add_test_case cursor_flags_recno
	atf_add_test_case reverse_order_recno
	atf_add_test_case small_page_btree
	atf_add_test_case byte_orders_btree
	atf_add_test_case byte_orders_hash
	atf_add_test_case bsize_ffactor
	atf_add_test_case four_char_hash
	atf_add_test_case bsize_torture
}
