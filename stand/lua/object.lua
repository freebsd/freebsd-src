local lacpi = require("lacpi")

local function test_get_handle()
	print("----VERIFYING HANDLE----")
	local handle = lacpi.object.get_handle("\\_SB")
	print(tostring(handle))
	local verify = lacpi.object.verify(handle)
	if verify then
		print(tostring(handle) .. " is a lacpi_node")
	
	else
		print(tostring(handle) .. " is not a lacpi_node")
	end
end

print("---TEST BEGIN: OBJECT---")

test_get_handle()

print("-----TESTS COMPLETE-----")
