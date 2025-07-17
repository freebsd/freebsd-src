# [libcbor](https://github.com/PJK/libcbor)

[![CircleCI](https://circleci.com/gh/PJK/libcbor/tree/master.svg?style=svg)](https://circleci.com/gh/PJK/libcbor/tree/master)
[![Documentation Status](https://readthedocs.org/projects/libcbor/badge/?version=latest)](https://readthedocs.org/projects/libcbor/?badge=latest)
[![latest packaged version(s)](https://repology.org/badge/latest-versions/libcbor.svg)](https://repology.org/project/libcbor/versions)
[![codecov](https://codecov.io/gh/PJK/libcbor/branch/master/graph/badge.svg)](https://codecov.io/gh/PJK/libcbor)

**libcbor** is a C library for parsing and generating [CBOR](https://cbor.io/), the general-purpose schema-less binary data format.

## Main features
 - Complete [IETF RFC 8949 (STD 94)](https://www.rfc-editor.org/info/std94) conformance
 - Robust platform-independent C99 implementation
 - Layered architecture offers both control and convenience
 - Flexible memory management
 - No shared global state - threading friendly
 - Proper handling of UTF-8
 - Full support for streams & incremental processing
 - Extensive documentation and test suite
 - No runtime dependencies, small footprint
 
## Getting started

### Compile from source

```bash
git clone https://github.com/PJK/libcbor
cmake -DCMAKE_BUILD_TYPE=Release libcbor
make
make install
```

### Homebrew

```bash
brew install libcbor
```

### Ubuntu 18.04 and above

```bash
sudo add-apt-repository universe
sudo apt-get install libcbor-dev
```

### Fedora & RPM friends

```bash
yum install libcbor-devel
```

### Others 

<details>
  <summary>Packaged libcbor is available from 15+ major repositories. Click here for more detail</summary>
  
  [![Packaging status](https://repology.org/badge/vertical-allrepos/libcbor.svg)](https://repology.org/project/libcbor/versions)
</details>

## Usage example

```c
#include <cbor.h>
#include <stdio.h>

int main(void) {
  /* Preallocate the map structure */
  cbor_item_t* root = cbor_new_definite_map(2);
  /* Add the content */
  bool success = cbor_map_add(
      root, (struct cbor_pair){
                .key = cbor_move(cbor_build_string("Is CBOR awesome?")),
                .value = cbor_move(cbor_build_bool(true))});
  success &= cbor_map_add(
      root, (struct cbor_pair){
                .key = cbor_move(cbor_build_uint8(42)),
                .value = cbor_move(cbor_build_string("Is the answer"))});
  if (!success) return 1;
  /* Output: `length` bytes of data in the `buffer` */
  unsigned char* buffer;
  size_t buffer_size;
  cbor_serialize_alloc(root, &buffer, &buffer_size);

  fwrite(buffer, 1, buffer_size, stdout);
  free(buffer);

  fflush(stdout);
  cbor_decref(&root);
}
```

## Documentation
Get the latest documentation at [libcbor.readthedocs.org](http://libcbor.readthedocs.org/)

## Contributions

Bug reports and contributions are welcome. Please see [CONTRIBUTING.md](https://github.com/PJK/libcbor/blob/master/CONTRIBUTING.md) for more info.

Kudos to all the [contributors](https://github.com/PJK/libcbor/graphs/contributors)!

## License
The MIT License (MIT)

Copyright (c) Pavel Kalvoda, 2014-2020

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
