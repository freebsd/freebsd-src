`metalog_reader.lua` is a script that reads METALOG file created by pkgbase
(make packages) and generates reports about the installed system
and issues

the script accepts an mtree file in a format that's returned by
`mtree -c | mtree -C`

synopsis:
```
metalog_reader.lua [-h] [-a | -c | -p [-count] [-size] [-f...]] [-W...] [-v] metalog-path
```

options:

* `-a` prints all scan results. this is the default option if no option is
  provided.
* `-c` lints the file and gives warnings/errors, including duplication and
  conflicting metadata
  *  `-Wcheck-notagdir` entries with dir type and no tags will be also included
     the first time they appear (1)
* `-p` list all package names found in the file as exactly specified by
  `tags=package=...`
  * `-count` display the number of files of the package
  * `-size` display the size of the package
  * `-fsetgid` only include packages with setgid files
  * `-fsetuid` only include packages with setuid files
  * `-fsetid` only include packages with setgid or setuid files
* `-v` verbose mode
* `-h` help page

some examples:

* `metalog_reader.lua -a METALOG`
  prints all scan results described below. this is the default option
* `metalog_reader.lua -c METALOG`
  only prints errors and warnings found in the file
* `metalog_reader.lua -c -Wcheck-notagdir METALOG`
  prints errors and warnings found in the file, including directories with no
  tags
* `metalog_reader.lua -p METALOG`
  only prints all the package names found in the file
* `metalog_reader.lua -p -count -size METALOG`
  prints all the package names, followed by number of files, followed by total
  size
* `metalog_reader.lua -p -size -fsetid METALOG`
  prints packages that has either setuid/setgid files, followed by the total
  size
* `metalog_reader.lua -p -fsetuid -fsetgid METALOG`
  prints packages that has both setuid and setgid files (if more than one
  filters are specified, they are composed using logic and)
* `metalog_reader.lua -p -count -size -fsetuid METALOG`
  prints packages that has setuid files, followed by number of files and total
  size

(1) if we have two entries
```
./bin type=dir uname=root gname=wheel mode=0755
./bin type=dir uname=root gname=wheel mode=0755 tags=...
```
by default, this is not warned. if the option is enabled, this will be warned
as the second line sufficiently covers the first line.
