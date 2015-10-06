This is a set of individual patches, which contain all the customizations to
llvm/clang currently in the FreeBSD base system.  These can be applied in
alphabetical order to a pristine llvm/clang 3.7.0 source tree, for example by
doing:

svn co https://llvm.org/svn/llvm-project/llvm/trunk llvm-3.7.0 
svn co https://llvm.org/svn/llvm-project/cfe/trunk llvm-3.7.0/tools/clang
cd llvm-3.7.0
for p in /usr/src/contrib/llvm/patches/patch-*.diff; do
	patch -p0 -f -F0 -E -i $p -s || break
done

A number of these consist of hand-written modifications, specifically for
FreeBSD, while most others are cherry pickings off the llvm and clang trunks.
When a new version of llvm/clang is eventually imported, those latter ones will
largely disappear.
