---Lua equivalent of serializer.hpp 
---@class Serializer
local Serializer = {}

---@enum RequestName
RequestName = {
    test_request_name = 0
}

---@class CppFieldDescriptor
---@field name string
---@field type string
---@field arraySize number? -- if field is an array of type, otherwise nil
---@field template string[]?

---@type { [string]: { fields: CppFieldDescriptor[], template: string[]? } }
Serializer.classes = {
    ["Request"] = {
        template = {"T"},
        fields = {
            {
                name = "id",
                type = "uint32"
            },
            {
                name = "name",
                type = "uint8"
            },
            {
                name = "data",
                type = "T"
            }
        }
    }
}

---@generic T
---@param class `T`
---@param data T
---@return string
function Serializer.Serialize(class, data)

end

---@generic T
---@param class `T`
---@param data string
---@param i int
---@return T, int
function Serializer.Deserialize(class, data, i)

end

return Serializer