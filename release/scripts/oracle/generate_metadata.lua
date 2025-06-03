#!/usr/libexec/flua

local ucl = require("ucl")

-- read from environment variables
local os_type = os.getenv("TYPE")
local os_version = os.getenv("OSRELEASE")
-- the raw file
local capability_file = os.getenv("ORACLE_CAPABILITY")
-- the platform-specific file
local shapes_file = os.getenv("ORACLE_SHAPES")
-- base template
local template_file = os.getenv("ORACLE_TEMPLATE")
local output_file = os.getenv("ORACLE_OUTPUT")

if not os_type or not os_version or not capability_file or
   not shapes_file or not template_file or not output_file then
    io.stderr:write("Error: Oracle metadata script is missing required environment variables:\n")
    io.stderr:write("TYPE, OSRELEASE, ORACLE_CAPABILITY, ORACLE_SHAPES, ORACLE_TEMPLATE, ORACLE_OUTPUT\n")
    os.exit(1)
end

-- read files
local function read_file(path)
    local f = io.open(path, "r")
    if not f then
        io.stderr:write("Error: Oracle metadata script cannot open file: " .. path .. "\n")
        os.exit(1)
    end
    local content = f:read("*a")
    f:close()
    return content
end

-- parse the template
local template = read_file(template_file)
local metadata = ucl.parser()
metadata:parse_string(template)
local data = metadata:get_object()

-- update the simple fields
data.operatingSystem = os_type
data.operatingSystemVersion = os_version

-- capability data is actually JSON, but needs to be inserted as a raw blob
local caps = read_file(capability_file)
-- remove all newlines and preceding spaces to match Oracle's format
caps = caps:gsub("\n", "")
caps = caps:gsub("%s+", "")
-- is it still valid JSON?
local caps_parser = ucl.parser()
if not caps_parser:parse_string(caps) then
    io.stderr:write("Error: Oracle metadata script found invalid JSON in capability file\n")
    os.exit(1)
end
-- insert as a raw blob
data.imageCapabilityData = caps

-- parse and insert architecture-dependent shape compatibilities data
local shapes_data = read_file(shapes_file)
local shapes = ucl.parser()
shapes:parse_string(shapes_data)
data.additionalMetadata.shapeCompatibilities = shapes:get_object()

-- save the metadata file
local dir = os.getenv("PWD")
local out = io.open(output_file, "w")
if not out then
    io.stderr:write("Error: Oracle metadata script cannot create output file: "
        .. dir .. "/" .. output_file .. "\n")
    os.exit(1)
end
out:write(ucl.to_format(data, "json", {pretty = true}))
out:close()
