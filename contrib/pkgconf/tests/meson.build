configure_file(input: 'Kyuafile.in', output: 'Kyuafile', configuration: cdata)
configure_file(input: 'test_env.sh.in', output: 'test_env.sh', configuration: cdata)


tests = [
  'parser',
  'personality',
  'regress',
  'requires',
  'spdxtool',
  'symlink',
]


# yuck
foreach test : tests
  test_file = configure_file(input: test + '.sh', output: test, copy: true)
endforeach
