local lacpi = require("lacpi")

local nodes = lacpi.walk.dump_namespace()  -- returns table of tables

-- Print them in Lua
for _, node in ipairs(nodes) do
    print(node.level, node.path, node.HID or "", node.UID or "")
end
