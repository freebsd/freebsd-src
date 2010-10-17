% Check that changing prefixes between the GREG definition, its use and
% the end of the assembly file does not change the GREG definition.
a1 GREG someplace
 PREFIX b
a2 GREG 567890
 PREFIX c:
h LDB $255,:a1
:Main LDB $254,:ba2
:someplace IS @
