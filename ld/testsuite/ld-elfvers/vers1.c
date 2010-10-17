/*
 * Basic test of versioning.  The idea with this is that we define
 * a bunch of definitions of the same symbol, and we can theoretically
 * then link applications against varying sets of these.
 */
const char * show_bar1 = "asdf";
const char * show_bar2 = "asdf";

extern int new2_foo();
extern int bar33();

int
bar()
{
	return 3;
}

/*
 * The 'hide' prefix is something so that we can automatically search the
 * symbol table and verify that none of these symbols were actually exported.
 */
int
hide_original_foo()
{
	return 1+bar();

}

int
hide_old_foo()
{
	return 10+bar();

}

int
hide_old_foo1()
{
	return 100+bar();

}

int
hide_new_foo()
{
	return 1000+bar();

}

__asm__(".symver hide_original_foo,show_foo@");
__asm__(".symver hide_old_foo,show_foo@VERS_1.1");
__asm__(".symver hide_old_foo1,show_foo@VERS_1.2");
__asm__(".symver hide_new_foo,show_foo@@VERS_2.0");



#ifdef DO_TEST10
/* In test 10, we try and define a non-existant version node.  The linker
 * should catch this and complain. */
int
hide_new_bogus_foo()
{
	return 1000+bar();

}
__asm__(".symver hide_new_bogus_foo,show_foo@VERS_2.2");
#endif




#ifdef DO_TEST11
/*
 * This test is designed to catch a couple of syntactic errors.  The assembler
 * should complain about both of the directives below.
 */
void
xyzzz()
{
  new2_foo();
  bar33();
}

__asm__(".symver new2_foo,fooVERS_2.0");
__asm__(".symver bar33,bar@@VERS_2.0");
#endif

#ifdef DO_TEST12
/*
 * This test is designed to catch a couple of syntactic errors.  The assembler
 * should complain about both of the directives below.
 */
void
xyzzz()
{
  new2_foo();
  bar33();
}

__asm__(".symver bar33,bar@@VERS_2.0");
#endif
