local function test_walk_namespace()
	local nodes = lacpi.walk.namespace()  -- returns table of tables
	
	if nodes == nil then
		return "FAILURE: Nodes were nil"
	end

	-- Print them in Lua
	for _, node in ipairs(nodes) do
	    print(node.level, node.path, node.HID or "", node.UID or "")
	end

	return "SUCCESS"
end

print("--- TEST BEGIN: WALK NS ---")

status = test_walk_namespace()

print("--- TEST END: WALK NS ---")
print(status)
