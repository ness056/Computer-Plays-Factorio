---@type boolean
local metatableRegistered = false

---@type { Vector2d: Vector2d, Area: Area }
local Math2d = { Vector2d = {}, Area = {} } ---@diagnostic disable-line missing-fields

---A MapPosition with which you can do all 2d vector operations.
---
---Since it extends MapPosition, any method that takes a MapPosition can also take a Vector (including all vanilla functions).
---The opposite is not true.
---
---Please note that the multiplication and division metamethod, as well as the Mul and Div methods
---takes either a number (in which case there isn't any ambiguity) or another vector, in which case
---the component-wise product/quotient will be computed.
---@class Vector2d : MapPosition.0
---@operator add(MapPosition): Vector2d
---@operator sub(MapPosition): Vector2d
---@operator mul(number | MapPosition): Vector2d
---@operator div(number | MapPosition): Vector2d
---@operator unm(Vector2d): Vector2d
local Vector2d = Math2d.Vector2d

---@param first number | MapPosition
---@param second number
---@return Vector2d
---@overload fun(pos: MapPosition): Vector2d
---@overload fun(x: number, y: number): Vector2d
function Vector2d.new(first, second)
    if Vector2d.IsMapPosition(first) then
        second, first = first.y, first.x
    end

    if not metatableRegistered then
        error("Math2d metatables were not registered.")
    end
    return setmetatable({ x = first, y = second }, Vector2d)
end

---@param ... MapPosition
function Vector2d.FormatMapPosition(...)
    for k, o in ipairs(table.pack(...)) do
        if not o.x then o.x = o[1] end
        if not o.y then o.y = o[2] end
    end
end

---@param object any
---@return boolean
function Vector2d.IsVector(object)
    return getmetatable(object) == Vector2d
end

---@param object any
---@return boolean
function Vector2d.IsMapPosition(object)
    if type(object) == "table" and object.x and object.y then
        return true
    elseif type(object) == "table" and object[1] and object[2] then
        Vector2d.FormatMapPosition(object)
        return true
    end
    return false
end

---@private
---@param ... any
function Vector2d.ScalarCheck(...)
    for k, o in ipairs(table.pack(...)) do
        if not type(o) == "number" then error("A scalar was expected, got " .. type(o) .. ".", 2) end
    end
end

---@private
---@param ... any
function Vector2d.VectorsCheck(...)
    for k, o in ipairs(table.pack(...)) do
        if not Vector2d.IsVector(o) then error("A Vector was expected, got " .. type(o) .. ".", 2) end
    end
end

---@private
---@param ... any
function Vector2d.MapPositionsCheck(...)
    for k, o in ipairs(table.pack(...)) do
        if not Vector2d.IsMapPosition(o) then error("A MapPosition was expected, got " .. type(o) .. ".", 2) end
    end
end

---@return Vector2d
function Vector2d.Zero()
    return Vector2d.new(0, 0)
end

---@return Vector2d
function Vector2d.One()
    return Vector2d.new(1, 1)
end

---@private
---@param self Vector2d
---@param key any
function Vector2d.__index(self, key)
    if key == 1 then
        return self.x
    elseif key == 2 then
        return self.y
    else
        return Vector2d[key]
    end
end

---@private
---@param self Vector2d
---@param key any
---@param value any
function Vector2d.__newindex(self, key, value)
    if key == 1 then
        self.x = value
    elseif key == 2 then
        self.y = value
    end
end

---@private
---@param v1 MapPosition
---@param v2 MapPosition
---@return Vector2d
function Vector2d.__add(v1, v2)
    Vector2d.MapPositionsCheck(v1, v2)
    return Vector2d.new(v1.x + v2.x, v1.y + v2.y)
end
Vector2d.Add = Vector2d.__add

---@private
---@param v1 MapPosition
---@param v2 MapPosition
---@return Vector2d
function Vector2d.__sub(v1, v2)
    Vector2d.MapPositionsCheck(v1, v2)
    return Vector2d.new(v1.x - v2.x, v1.y - v2.y)
end
Vector2d.Sub = Vector2d.__sub

---@private
---If 1 vector v and 1 number n are provided: r = (v.x * n, v.y * n) (scalar multiplication)
---If 2 vector v1, v2 are provided: r = (v1.x * v2.x, v1.y * v2.y) (Hadamard product)
---@param v1 MapPosition | number
---@param v2 MapPosition | number
---@return Vector2d
---@overload fun(v1: MapPosition, v2: MapPosition): Vector2d
---@overload fun(v: MapPosition, n: number): Vector2d
---@overload fun(n: number, v: MapPosition): Vector2d
function Vector2d.__mul(v1, v2)
    if Vector2d.IsMapPosition(v1) and Vector2d.IsMapPosition(v2) then
        return Vector2d.new(v1.x * v2.x, v1.y * v2.y)
    elseif Vector2d.IsMapPosition(v1) and type(v2) == "number" then
        return Vector2d.new(v1.x * v2, v1.y * v2)
    elseif Vector2d.IsMapPosition(v2) and type(v1) == "number" then
        return Vector2d.new(v2.x * v1, v2.y * v1)
    else
        error("1 vector and 1 number or 2 vector was expected.")
    end
end
Vector2d.Mul = Vector2d.__mul

---@private
---If 1 vector v and 1 number n are provided: r = (v.x / n, v.y / n) (scalar division)
---If 2 vector v1, v2 are provided: r = (v1.x / v2.x, v1.y / v2.y) (Hadamard quotient)
---@param v1 MapPosition | number
---@param v2 MapPosition | number
---@return Vector2d
---@overload fun(v1: MapPosition, v2: MapPosition): Vector2d
---@overload fun(v: MapPosition, n: number): Vector2d
---@overload fun(n: number, v: MapPosition): Vector2d
function Vector2d.__div(v1, v2)
    if Vector2d.IsMapPosition(v1) and Vector2d.IsMapPosition(v2) then
        return Vector2d.new(v1.x / v2.x, v1.y / v2.y)
    elseif Vector2d.IsMapPosition(v1) and type(v2) == "number" then
        return Vector2d.new(v1.x / v2, v1.y / v2)
    elseif Vector2d.IsMapPosition(v2) and type(v1) == "number" then
        return Vector2d.new(v2.x / v1, v2.y / v1)
    else
        error("1 vector and 1 number or 2 vector was expected.")
    end
end
Vector2d.Div = Vector2d.__div

---@private
---@param v MapPosition
---@return Vector2d
function Vector2d.__unm(v)
    return Vector2d.new(-v.x, -v.y)
end
Vector2d.Opposite = Vector2d.__unm

---@private
---@param v1 MapPosition
---@param v2 MapPosition
---@return boolean
function Vector2d.__eq(v1, v2)
    Vector2d.MapPositionsCheck(v1, v2)
    return v1.x == v2.x and v1.y == v2.y
end
Vector2d.Equal = Vector2d.__eq

---@private
---@param v Vector2d
---@return string
function Vector2d.__tostring(v)
    return "Vector2d: (" .. v.x .. "; " .. v.y .. ")"
end
Vector2d.ToString = Vector2d.__tostring


---@return number, number
function Vector2d:Unpack()
    return self.x, self.y
end

---@return Vector2d
function Vector2d:Copy()
    return Vector2d.new(self.x, self.y)
end

---Applies a callback to each components.
---@param fun fun(scalar: number): number
---@return Vector2d
function Vector2d:Map(fun)
    local c = self:Copy()
    c.x = fun(c.x)
    c.y = fun(c.y)
    return c
end

---@return Vector2d
function Vector2d:FromChunkToMap()
    return (self:Copy() / 32):Map(math.floor)
end

---@return Area
function Vector2d:FromMapToChunk()
    local c = self:Copy() * 32
    return Math2d.Area.new(c, c + {31, 31})
end

---@return number
function Vector2d:SquaredLength()
    return self.x * self.x + self.y * self.y
end

---@return number
function Vector2d:Length()
    return math.sqrt(self:SquaredLength())
end

---@return number
function Vector2d:AngleToAbscise()
    return math.acos(self.y / self:Length())
end

---@return boolean
function Vector2d:IsNormalized()
    return self:SquaredLength() == 1
end

function Vector2d:Normalize()
    local length = self:Length()
    if length == 0 or length == 1 then return end

    self = self / length
end

---@return Vector2d
function Vector2d:Normalized()
    local v = self:Copy()
    v:Normalize()
    return v
end

---All angles are expressed in radians.
---@param a number
function Vector2d:Rotate(a)
    local temp = self.x
    self.x = math.cos(a) * self.x - math.sin(a) * self.y
    self.y = math.sin(a) * temp + math.cos(a) * self.y
end

---All angles are expressed in radians.
---@param a number
---@return Vector2d
function Vector2d:Rotated(a)
    local v = self:Copy()
    v:Rotate(a)
    return v
end

---For 2 vectors v1 and v2: r = (v1.x * v2.x) + (v1.y * v2.y) (Dot product)
---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector2d.Dot(v1, v2)
    Vector2d.MapPositionsCheck(v1, v2)
    return v1.x * v2.x + v1.y * v2.y
end

---For 2 vectors v1 and v2: r = (v1.x * v2.y) - (v1.y * v2.x) (Cross product)
---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector2d.Cross(v1, v2)
    Vector2d.MapPositionsCheck(v1, v2)
    return v1.x * v2.y - v1.y * v2.x
end

---@param v1 Vector2d
---@param v2 Vector2d
---@return number
function Vector2d.SquaredDistance(v1, v2)
    Vector2d.VectorsCheck(v1, v2)
    return (v1 - v2):SquaredLength()
end

---@param v1 Vector2d
---@param v2 Vector2d
---@return number
function Vector2d.Distance(v1, v2)
    Vector2d.VectorsCheck(v1, v2)
    return (v1 - v2):Length()
end

---@param v1 Vector2d
---@param v2 Vector2d
---@return number
function Vector2d.Angle(v1, v2)
    Vector2d.VectorsCheck(v1, v2)
    return math.acos(Vector2d.Dot(v1, v2) / (v1:Length() * v2:Length()))
end

---@param v1 Vector2d
---@param v2 Vector2d
---@return Vector2d
function Vector2d.Max(v1, v2)
    Vector2d.VectorsCheck(v1, v2)
    return Vector2d.new(math.max(v1.x, v2.x), math.max(v1.y, v2.y))
end

---@param v1 Vector2d
---@param v2 Vector2d
---@return Vector2d
function Vector2d.Min(v1, v2)
    Vector2d.VectorsCheck(v1, v2)
    return Vector2d.new(math.min(v1.x, v2.x), math.min(v1.y, v2.y))
end

---The equivalent of the Factorio BoundingBox but with which you can do math.
---@class Area : BoundingBox.0
---@operator add(MapPosition): Area
---@operator sub(MapPosition): Area
---@operator mul(number): Vector2d
---@operator div(number): Vector2d
local Area = Math2d.Area

---@param boundingBox BoundingBox
---@return BoundingBox.0
function Area.Format(boundingBox)
    Area.BoundingBoxCheck(boundingBox)
    return {
        left_top = boundingBox.left_top or boundingBox[1],
        right_bottom = boundingBox.right_bottom or boundingBox[2]
    }
end

---@param first BoundingBox | MapPosition | Vector2d
---@param second MapPosition | Vector2d
---@return Area
---@overload fun(boundingBox: BoundingBox): Area
---@overload fun(x: MapPosition, y: MapPosition): Area
---@overload fun(x: Vector2d, y: Vector2d): Area
function Area.new(first, second)
    ---@diagnostic disable
    if Area.IsBoundingBox(first) then
        first = Area.Format(first)
        first = { left_top = Vector2d.new(first.left_top), right_bottom = Vector2d.new(first.right_bottom) }

    elseif Vector2d.IsMapPosition(first) and Vector2d.IsMapPosition(second) then
        first = { left_top = Vector2d.new(first), right_bottom = Vector2d.new(second) }
    end

    if not metatableRegistered then
        error("Math2d metatables were not registered.")
    end
    return setmetatable(first --[[@as Area]], Area)
    ---@diagnostic enable
end

---@param object any
---@return boolean
function Area.IsBoundingBox(object)
    if type(object) == "table" and Vector2d.IsMapPosition(object.left_top) and Vector2d.IsMapPosition(object.right_bottom) then
        return true
    elseif type(object) == "table" and Vector2d.IsMapPosition(object[1]) and Vector2d.IsMapPosition(object[2]) then
        if not object.x then object.x = object[1] end
        if not object.y then object.y = object[2] end
        Vector2d.FormatMapPosition(object.x)
        Vector2d.FormatMapPosition(object.y)
        return true
    end
    return false
end

---@param object any
---@return boolean
function Area.IsArea(object)
    return getmetatable(object) == Area
end

---@private
---@param ... any
function Area.BoundingBoxCheck(...)
    for k, o in ipairs(table.pack(...)) do
        if not Area.IsBoundingBox(o) then error("A BoundingBox was expected, got " .. type(o) .. ".", 2) end
    end
end

---@return Area
function Area.Zero()
    return Area.new({{0,0}, {0,0}})
end

---@return Area
function Area.One()
    return Area.new({{1,1}, {1,1}})
end

---@private
---@param self Area
---@param key any
function Area.__index(self, key)
    if key == 1 then
        return self.left_top
    elseif key == 2 then
        return self.right_bottom
    else
        return Area[key]
    end
end

---@private
---@param self Area
---@param key any
---@param value any
function Area.__newindex(self, key, value)
    if key == 1 then
        self.left_top = value
    elseif key == 2 then
        self.right_bottom = value
    end
end

---@private
---@param v1 BoundingBox
---@param v2 MapPosition
---@return Area
function Area.__add(v1, v2)
    Area.BoundingBoxCheck(v1)
    Vector2d.MapPositionsCheck(v2)
    return Area.new({{ x = v1.left_top.x + v2.x, y = v1.left_top.y + v2.y }, { x = v1.right_bottom.x + v2.x, y = v1.right_bottom.y + v2.y }})
end
Area.Add = Area.__add

---@private
---@param v1 BoundingBox
---@param v2 MapPosition
---@return Area
function Area.__sub(v1, v2)
    Area.BoundingBoxCheck(v1)
    Vector2d.MapPositionsCheck(v2)
    return Area.new({{ x = v1.left_top.x - v2.x, y = v1.left_top.y - v2.y }, { x = v1.right_bottom.x - v2.x, y = v1.right_bottom.y - v2.y }})
end
Area.Sub = Area.__sub

---@private
---@param v1 BoundingBox
---@param v2 number
---@return Area
function Area.__mul(v1, v2)
    Area.BoundingBoxCheck(v1)
    Vector2d.ScalarCheck(v2)
    return Area.new({{ x = v1.left_top.x * v2, y = v1.left_top.y * v2 }, { x = v1.right_bottom.x * v2, y = v1.right_bottom.y * v2 }})
end
Area.Mul = Area.__mul

---@private
---@param v1 BoundingBox
---@param v2 number
---@return Area
function Area.__div(v1, v2)
    Area.BoundingBoxCheck(v1)
    Vector2d.ScalarCheck(v2)
    return Area.new({{ x = v1.left_top.x / v2, y = v1.left_top.y / v2 }, { x = v1.right_bottom.x / v2, y = v1.right_bottom.y / v2 }})
end
Area.Div = Area.__div

---@private
---@param v1 BoundingBox
---@param v2 BoundingBox
---@return boolean
function Area.__eq(v1, v2)
    Area.BoundingBoxCheck(v1, v2)
    return v1.left_top == v2.left_top and v1.right_bottom == v2.right_bottom
end
Area.Equal = Area.__eq

---@private
---@param v BoundingBox
---@return string
function Area.__tostring(v)
    return "{" .. Vector2d.new(v.left_top):ToString() .. "; " .. Vector2d.new(v.right_bottom):ToString() .. "}"
end
Area.ToString = Area.__tostring

---@param position MapPosition
---@return number
function Area:SquaredDistanceToPosition(position)
    Vector2d.MapPositionsCheck(position)
    local x = math.max(self.left_top.x - position.x, 0, position.x - self.right_bottom.x)
    local y = math.max(self.left_top.y - position.y, 0, position.y - self.right_bottom.y)
    return x * x + y * y
end

---@param position MapPosition
---@return number
function Area:DistanceToPosition(position)
    return math.sqrt(self:SquaredDistanceToPosition(position))
end

---@return Vector2d
function Area:CenterOfMass()
    return Vector2d.new((self.left_top.x + self.right_bottom.x) / 2, (self.left_top.y + self.right_bottom.y) / 2)
end

---@param position MapPosition
function Area:Contains(position)
    Vector2d.MapPositionsCheck(position)
    return self:SquaredDistanceToPosition(position) == 0
end

script.register_metatable("Vector2d", Vector2d)
script.register_metatable("Area", Area)
metatableRegistered = true

return Math2d