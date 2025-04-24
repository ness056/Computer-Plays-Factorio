---Lua equivalent of serializer.hpp 
---@class Serializer
local Serializer = {}

---@enum RequestName
RequestName = {
    test_request_name = 0
}

---@class CppType
---@field name string
---@field arraySize int?
---@field template (CppType | string)[]?

---@class CppClass
---@field name string
---@field arraySize int?
---@field template (CppType | string | int)[]?

---@type { [string]: { properties: { [string]: (CppClass | string | int) }, order: string[]}}
Serializer.classes = {
    ["Request"] = {
        properties = {
            ["id"] = "u32",
            ["name"] = "u8",
            ["data"] = 1
        },
        order = {"id", "name", "data"}
    },
    ["Test"] = {
        properties = {
            ["a"] = "i32",
            ["b"] = "bool",
            ["c"] = "u32",
            ["d"] = {name = "u16", arraySize = 4},
            ["e"] = "string",
            ["f"] = {name = "vector", template = {1}},
            ["g"] = {name = "map", template = {"string", "u64"}},
            ["h"] = "float",
            ["i"] = "double",
        },
        order = {"a", "b", "c", "d", "e", "f", "g", "h", "i"}
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
---@param cppType CppType | string
---@return T, int
function Serializer.Deserialize(data, i, cppType)
    if type(cppType) == "string" then
        cppType = {
            name = cppType
        }
    end
    ---@cast cppType -string

    assert(cppType and string.len(cppType.name) > 0 and data and i)

    if cppType.name == "vector" then
        if cppType.template == nil or table_size(cppType.template) ~= 1 then
            error("Wrong number of template for vector type")
        end

        cppType = cppType.template[1]
        cppType.arraySize, i = Serializer.Deserialize(data, i, "u64")
    end

    if cppType.arraySize ~= nil then
        local r = {}
        for _ = 1, cppType.arraySize do
            local o, j = Serializer.Deserialize(data, i, cppType)
            table.insert(r, o)
            i = j
        end
        return r, i
    end

    if cppType.name == "map" then
        print("Deserialize map TODO")
        return nil, i
    end

    if cppType.name == "string" then
        local length, j = Serializer.Deserialize(data, i, "u64")
        i = j
        s = data.sub(i, i + length - 1)
        return s, i + length
    end

    if cppType.name == "float" or cppType.name == "double" then
        print("Deserialize float TODO")
        return nil, 10
    end

    numberType = string.sub(cppType.name, 1, 1)
    numberSize = nil
    if numberType ~= "u" and numberType ~= "i" then numberType = nil end

    local s = string.sub(cppType.name, 2)
    if s == "8" then numberSize = 1
    elseif s == "16" then numberSize = 2
    elseif s == "32" then numberSize = 4
    elseif s == "64" then numberSize = 8 end
    if cppType == "bool" then
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

    local t = Serializer.classes[cppType.name]
    if t == nil then return nil, i end

    local o = {}
    for k, n in ipairs(t.order) do
        local p = t.properties[n]
        if not p then
            error("Invalid serializer class " .. cppType.name .. ": has no property named " .. n)
        end

        if type(p) == "number" then
            if table_size(cppType.template) < p then
                return nil, i
            end

            p = cppType.template[p] --[[@as CppClass]]
            if p == "" then goto continue end
        end
        ---@cast p -integer

        if type(p) == "string" then
            p = {
                name = p
            }
        end
        ---@cast p -string

        if p.template then
            for j in ipairs(p.template) do
                if type(p.template[j]) == "number" then
                    if table_size(cppType.template) < j then
                        return nil, i
                    end
                    p.template[j] = cppType.template[j]
                end
            end
        end

        print("testetetoakrtoakrfoakreofk")
        print(n, p.name)
        o[n], i = Serializer.Deserialize(data, i, p --[[@as CppType]])
        print(o[n])
        ::continue::
    end

    return o, i
end

return Serializer