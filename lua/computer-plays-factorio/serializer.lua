---Lua equivalent of serializer.hpp 
---@class Serializer
local Serializer = {}

---@enum RequestName
RequestName = {
    test_request_name = 0
}

---@class CppType
---@field type string
---@field arraySize int?
---@field template CppType[]?

---@type { [string]: { [string]: (CppType | string) }}
Serializer.classes = {
    ["Request"] = {
        ["id"] = "u32",
        ["name"] = "u8",
        ["data"] = "1"
    }
}

---@generic T
---@param class `T`
---@param data T
---@return string
function Serializer.Serialize(class, data)
    return ""
end

---@generic T
---@param data string
---@param i int
---@param type string
---@param arraySize int?
---@param template CppType[]?
---@return T, int
function Serializer.Deserialize(data, i, type, arraySize, template)
    assert(type and string.len(type) > 0 and data and i)

    if type == "vector" then
        if template ~= nil and table_size(template) ~= 1 then
            error("Wrong number of template for vector type")
        end

        arraySize, i = Serializer.Deserialize(data, i, "u64")
        type = template[]
    end

    if arraySize ~= nil then
        local r = {}
        for _ = 1, arraySize do
            local o, j = Serializer.Deserialize(data, i, type, 0, template)
            table.insert(r, o)
            i = j
        end
        return r, i
    end

    if type == "map" then

    end

    if type == "string" then
        local length, j = Serializer.Deserialize(data, i, "u64")
        i = j
        s = data.sub(i, i + length - 1)
        return s, i + length
    end

    if type == "float" or type == "double" then
        return nil, 10
    end

    numberType = nil
    numberSize = nil
    if type[1] == "u" then numberType = "u"
    elseif type[1] == "i" then numberType = "i" end

    local s = string.sub(type, 2)
    if s == "8" then numberSize = 1
    elseif s == "16" then numberSize = 2
    elseif s == "32" then numberSize = 4
    elseif s == "64" then numberSize = 8 end
    if type == "bool" then
        numberType = "u"
        numberSize = 1
    end

    if numberSize ~= nil and numberType ~= nil then
        local n = 0
        local bytes = table.pack(data.byte(i, i + numberSize - 1))
        for j, v in ipairs(bytes) do
            n = n + v * math.pow(2, j - 1)
        end

        if numberType == "i" and bytes[1] >= 128 then
            n = n - 2 * math.pow(2, numberSize * 8)
        end

        return n, i + numberSize
    end
end

return Serializer