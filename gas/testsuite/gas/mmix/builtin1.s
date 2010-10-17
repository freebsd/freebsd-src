# When disallowing built-in names, we have to treat GET and PUT
# specially, so when parsing the special register operand we do
# not use the symbol table.
rJ IS 20
Main GET $5,rJ
 GET $6,:rJ
 PUT rJ,$7
 PUT :rJ,$8
