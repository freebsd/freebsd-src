local n = require("nuage")
print(n.dirname("/my/path/path1"))
if n.dirname("path") then
	nuage.err("Expecting nil for n.dirname(\"path\")")
end
if n.dirname() then
	nuage.err("Expecting nil for n.dirname")
end
