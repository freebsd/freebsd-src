# build the project
./build/autogen.sh
./configure
make -j$(nproc) all

# build seed
cp $SRC/libarchive/contrib/oss-fuzz/corpus.zip\
       	$OUT/libarchive_fuzzer_seed_corpus.zip

# build fuzzer(s)
$CXX $CXXFLAGS -Ilibarchive \
    $SRC/libarchive/contrib/oss-fuzz/libarchive_fuzzer.cc \
     -o $OUT/libarchive_fuzzer $LIB_FUZZING_ENGINE \
    .libs/libarchive.a -Wl,-Bstatic -lbz2 -llzo2  \
    -lxml2 -llzma -lz -lcrypto -llz4 -licuuc \
    -licudata -Wl,-Bdynamic
