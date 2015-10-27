# Version history

## Libucl 0.5

- Streamline emitter has been added, so it is now possible to output partial `ucl` objects
- Emitter now is more flexible due to emitter_context structure

### 0.5.1
- Fixed number of bugs and memory leaks

### 0.5.2

- Allow userdata objects to be emitted and destructed
- Use userdata objects to store lua function references

### Libucl 0.6

- Reworked macro interface

### Libucl 0.6.1

- Various utilities fixes

### Libucl 0.7.0

- Move to klib library from uthash to reduce memory overhead and increase performance

### Libucl 0.7.1

- Added safe iterators API

### Libucl 0.7.2

- Fixed serious bugs in schema and arrays iteration

### Libucl 0.7.3

- Fixed a bug with macroes that come after an empty object
- Fixed a bug in include processing when an incorrect variable has been destroyed (use-after-free)
