fuzzer_link_args = [
  '-fsanitize=fuzzer',
  '-Wl,--wrap=malloc',
  '-Wl,--wrap=calloc',
  '-Wl,--wrap=realloc',
  '-Wl,--wrap=reallocarray',
  '-Wl,--wrap=strdup',
  '-Wl,--wrap=strndup',
]

parser_fuzzer_exe = executable(
  'parser-fuzzer',
  'parser-fuzzer.c',
  'alloc-inject.c',
  dependencies: dep_libpkgconf,
  include_directories: include_directories('..'),
  install: false,
  c_args: ['-fsanitize=fuzzer'],
  link_args: fuzzer_link_args,
)

solver_fuzzer_exe = executable(
  'solver-fuzzer',
  'solver-fuzzer.c',
  'alloc-inject.c',
  dependencies: dep_libpkgconf,
  include_directories: include_directories('..'),
  install: false,
  c_args: ['-fsanitize=fuzzer'],
  link_args: fuzzer_link_args,
)

fuzz_root = join_paths(meson.project_build_root(), 'fuzz')
corpus_dir = join_paths(fuzz_root, 'corpus')
solver_corpus_dir = join_paths(fuzz_root, 'solver-corpus')
seed_dir = join_paths(meson.project_source_root(), 'tests', 'lib1')
solver_seed_dir = join_paths(meson.project_source_root(), 'fuzzer', 'solver-corpus')

run_target(
  'fuzz-prepare',
  command: ['mkdir', '-p', corpus_dir]
)

run_target(
  'fuzz-parser',
  command: [parser_fuzzer_exe, corpus_dir, seed_dir]
)

run_target(
  'fuzz-solver',
  command: [solver_fuzzer_exe, solver_corpus_dir, solver_seed_dir]
)
