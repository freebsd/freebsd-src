parser_fuzzer_exe = executable(
  'parser-fuzzer',
  'parser-fuzzer.c',
  dependencies: dep_libpkgconf,
  include_directories: include_directories('..'),
  install: false,
  c_args: ['-fsanitize=fuzzer'],
  link_args: ['-fsanitize=fuzzer'],
)

fuzz_root = join_paths(meson.project_build_root(), 'fuzz')
corpus_dir = join_paths(fuzz_root, 'corpus')
seed_dir = join_paths(meson.project_source_root(), 'tests', 'lib1')

run_target(
  'fuzz-prepare',
  command: ['mkdir', '-p', corpus_dir]
)

run_target(
  'fuzz-parser',
  command: [parser_fuzzer_exe, corpus_dir, seed_dir]
)
