#
# Copyright (c) 2024 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case noargs
noargs_head() {
	atf_set descr "No arguments"
}
noargs_body() {
	atf_check -s exit:1 -e match:"^usage:" \
		  lorder
}

atf_test_case onearg
onearg_head() {
	atf_set descr "One argument"
}
onearg_body() {
	echo "void a(void) { }" >a.c
	cc -o a.o -c a.c
	echo "a.o a.o" >output
	atf_check -o file:output \
		  lorder *.o
}

atf_test_case dashdash
dashdash_head() {
	atf_set descr "One argument"
}
dashdash_body() {
	echo "void a(void) { }" >a.c
	cc -o a.o -c a.c
	echo "a.o a.o" >output
	atf_check -o file:output \
		  lorder -- *.o
}

atf_test_case nonexistent
nonexistent_head() {
	atf_set descr "Nonexistent file"
}
nonexistent_body() {
	atf_check -s not-exit:0 -e match:"No such file" -o empty \
		  lorder nonexistent.o
}

atf_test_case invalid
invalid_head() {
	atf_set descr "Invalid file"
}
invalid_body() {
	echo "not an object file" >invalid.o
	atf_check -s not-exit:0 -e match:"File format not" -o empty \
		  lorder invalid.o
}

atf_test_case objects
objects_head() {
	atf_set descr "Order objects"
}
objects_body() {
	echo "void a(void) { }" >a.c
	echo "void a(void); void b(void) { a(); }" >b.c
	echo "void b(void); void c(void) { b(); }" >c.c
	for n in a b c ; do
		cc -o $n.o -c $n.c
		echo "$n.o $n.o"
	done >output
	echo "b.o a.o" >>output
	echo "c.o b.o" >>output
	atf_check -o file:output \
		  lorder *.o
}

atf_test_case archives
archives_head() {
	atf_set descr "Order archives"
}
archives_body() {
	echo "void a(void) { }" >a.c
	echo "void a(void); void b(void) { a(); }" >b.c
	echo "void b(void); void c(void) { b(); }" >c.c
	echo "void e(void); void d(void) { e(); }" >d.c
	echo "void d(void); void e(void) { d(); }" >e.c
	for n in a b c d e ; do
		cc -o $n.o -c $n.c
	done
	for n in a b c ; do
		ar -crs $n.a $n.o
		echo "$n.a $n.a"
	done >output
	ar -crs z.a d.o e.o
	echo "z.a z.a" >>output
	echo "b.a a.a" >>output
	echo "c.a b.a" >>output
	atf_check -o file:output \
		  lorder *.a
}

atf_init_test_cases()
{
	atf_add_test_case noargs
	atf_add_test_case onearg
	atf_add_test_case dashdash
	atf_add_test_case nonexistent
	atf_add_test_case invalid
	atf_add_test_case objects
	atf_add_test_case archives
}
