---@type { Vector: Vector, Area: Area }
local Math2d = { Vector = {}, Area = {} } ---@diagnostic disable-line missing-fields

---@class Vector
local Vector = Math2d.Vector

---@param pos MapPosition
---@return MapPosition
local function FormatMapPosition(pos)
    local r = {}

    if pos.x then
        r.x, r[1] = pos.x, pos.x
    else
        r.x, r[1] = pos[1], pos[1]
    end

    if pos.y then
        r.y, r[2] = pos.y, pos.y
    else
        r.y, r[2] = pos[2], pos[2]
    end

    return r
end

---@param pos MapPosition
local function FormatMapPositionOutArg(pos)
    if pos.x then
        pos[1] = pos.x
    else
        pos.x = pos[1]
    end

    if pos.y then
        pos[2] = pos.y
    else
        pos.y = pos[2]
    end
end

---@param object any
---@return boolean
local function IsMapPosition(object)
    if type(object) == "table" and (object.x or object[1]) and (object.y or object[2]) then
        FormatMapPositionOutArg(object)
        return true
    end
    return false
end

---@param ... any
local function ScalarCheck(...)
    for k, o in ipairs({...}) do
        if not type(o) == "number" then error("A scalar was expected, got " .. type(o) .. ".", 2) end
    end
end

---@param ... any
local function MapPositionCheck(...)
    for k, o in ipairs({...}) do
        if not IsMapPosition(o) then error("A MapPosition was expected, got " .. type(o) .. ".", 2) end
    end
end

---@param boundingBox BoundingBox
---@return BoundingBox
local function FormatBoundingBox(boundingBox)
    local r = {}

    local pos
    if boundingBox.left_top then
        pos = FormatMapPosition(boundingBox.left_top)
    else
        pos = FormatMapPosition(boundingBox[1])
    end
    r.left_top, r[1] = pos, pos

    if boundingBox.right_bottom then
        pos = FormatMapPosition(boundingBox.right_bottom)
    else
        pos = FormatMapPosition(boundingBox[2])
    end
    r.right_bottom, r[2] = pos

    return r
end

---@param object any
---@return boolean
local function IsBoundingBox(object)
    if type(object) == "table" and
        (IsMapPosition(object.left_top) or IsMapPosition(object[1])) and
        (IsMapPosition(object.right_bottom) or IsMapPosition(object[2])) then
        FormatBoundingBox(object)
        return true
    end
    return false
end

---@return MapPosition
function Vector.Zero()
    return { x = 0, y = 0 }
end

---@return MapPosition
function Vector.One()
    return { x = 1, y = 1 }
end

local directionVectors
do
    directionVectors = {
        [defines.direction.north] = {  0, -1 },
        [defines.direction.south] = {  0,  1 },
        [defines.direction.west]  = { -1,  0 },
        [defines.direction.east]  = {  1,  0 },

        [defines.direction.northwest] = { -1, -1 },
        [defines.direction.northeast] = {  1, -1 },
        [defines.direction.southwest] = { -1,  1 },
        [defines.direction.southeast] = {  1,  1 },

        [defines.direction.northnorthwest] = { -0.5, -1 },
        [defines.direction.westnorthwest]  = { -1,   -0.5 },
        [defines.direction.northnortheast] = {  0.5, -1 },
        [defines.direction.eastnortheast]  = {  1,   -0.5 },

        [defines.direction.southsouthwest] = { -0.5,  1 },
        [defines.direction.westsouthwest]  = { -1,    0.5 },
        [defines.direction.southsoutheast] = {  0.5,  1 },
        [defines.direction.eastsoutheast]  = {  1,    0.5 }
    }
end

for k, vec in pairs(directionVectors) do
    FormatMapPositionOutArg(vec)
end

---Returns a vector that points towards the given direction.
---@param direction defines.direction
---@return MapPosition
function Vector.FromDirection(direction)
    return table.deepcopy(directionVectors[direction])
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return MapPosition
function Vector.Add(v1, v2)
    MapPositionCheck(v1, v2)
    return FormatMapPosition({v1.x + v2.x, v1.y + v2.y})
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return MapPosition
function Vector.Sub(v1, v2)
    MapPositionCheck(v1, v2)
    return FormatMapPosition({v1.x - v2.x, v1.y - v2.y})
end

---If 1 vector v and 1 number n are provided: r = (v.x * n, v.y * n) (scalar multiplication)
---If 2 vector v1, v2 are provided: r = (v1.x * v2.x, v1.y * v2.y) (Hadamard product)
---@param v1 MapPosition | number
---@param v2 MapPosition | number
---@return MapPosition
---@overload fun(v1: MapPosition, v2: MapPosition): MapPosition
---@overload fun(v: MapPosition, n: number): MapPosition
---@overload fun(n: number, v: MapPosition): MapPosition
function Vector.Mul(v1, v2)
    if IsMapPosition(v1) and IsMapPosition(v2) then
        return FormatMapPosition({v1.x * v2.x, v1.y * v2.y})
    elseif IsMapPosition(v1) and type(v2) == "number" then
        return FormatMapPosition({v1.x * v2, v1.y * v2})
    elseif IsMapPosition(v2) and type(v1) == "number" then
        return FormatMapPosition({v2.x * v1, v2.y * v1})
    else
        error("1 vector and 1 number or 2 vector was expected.")
    end
end

---If 1 vector v and 1 number n are provided: r = (v.x / n, v.y / n) (scalar division)
---If 2 vector v1, v2 are provided: r = (v1.x / v2.x, v1.y / v2.y) (Hadamard quotient)
---@param v1 MapPosition | number
---@param v2 MapPosition | number
---@return MapPosition
---@overload fun(v1: MapPosition, v2: MapPosition): MapPosition
---@overload fun(v: MapPosition, n: number): MapPosition
---@overload fun(n: number, v: MapPosition): MapPosition
function Vector.Div(v1, v2)
    if IsMapPosition(v1) and IsMapPosition(v2) then
        return FormatMapPosition({v1.x / v2.x, v1.y / v2.y})
    elseif IsMapPosition(v1) and type(v2) == "number" then
        return FormatMapPosition({v1.x / v2, v1.y / v2})
    elseif IsMapPosition(v2) and type(v1) == "number" then
        return FormatMapPosition({v2.x / v1, v2.y / v1})
    else
        error("1 vector and 1 number or 2 vector was expected.")
    end
end

---@param v MapPosition
---@return MapPosition
function Vector.Opposite(v)
    MapPositionCheck(v)
    return FormatMapPosition({-v.x, -v.y})
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return boolean
function Vector.Equal(v1, v2)
    MapPositionCheck(v1, v2)
    return v1.x == v2.x and v1.y == v2.y
end

---@param v MapPosition
---@return string
function Vector.ToString(v)
    MapPositionCheck(v)
    return "MapPosition: (" .. v.x .. "; " .. v.y .. ")"
end

---Applies a callback to each components.
---@param vec MapPosition
---@param fun fun(scalar: number): number
---@return MapPosition
function Vector.Map(vec, fun)
    MapPositionCheck(vec)
    local r = { fun(vec.x), fun(vec.y) }
    return FormatMapPosition(r)
end

---@param vec MapPosition
---@return MapPosition
function Vector.Abs(vec)
    MapPositionCheck(vec)
    return Vector.Map(vec, math.abs)
end

---@param vec MapPosition
---@return MapPosition
function Vector.FromChunkToMap(vec)
    MapPositionCheck(vec)
    return Vector.Map(Vector.Div(vec, 32), math.floor)
end

---@param vec MapPosition
---@return BoundingBox
function Vector.FromMapToChunk(vec)
    MapPositionCheck(vec)
    local p = Vector.Mul(vec, 32)
    return FormatBoundingBox({p, Vector.Add(p, {31, 31})})
end

---@param vec MapPosition
---@return number
function Vector.SqLength(vec)
    MapPositionCheck(vec)
    return vec.x * vec.x + vec.y * vec.y
end

---@param vec MapPosition
---@return number
function Vector.Length(vec)
    MapPositionCheck(vec)
    return math.sqrt(Vector.SqLength(vec))
end

---@param vec MapPosition
---@return number
function Vector.AngleToAbscise(vec)
    MapPositionCheck(vec)
    return math.acos(vec.y / Vector.Length(vec))
end

---@param vec MapPosition
---@return boolean
function Vector.IsNormalized(vec)
    MapPositionCheck(vec)
    return Vector.SqLength(vec) == 1
end

---@param vec MapPosition
---@return MapPosition
function Vector.Normalize(vec)
    MapPositionCheck(vec)
    local length = Vector.Length(vec)
    if length == 0 or length == 1 then return vec end

    return Vector.Div(vec, length)
end

---All angles are expressed in radians.
---@param vec MapPosition
---@param angle number
---@return MapPosition
function Vector.Rotate(vec, angle)
    MapPositionCheck(vec)
    return FormatMapPosition({
        math.cos(angle) * vec.x - math.sin(angle) * vec.y,
        math.sin(angle) * vec.x + math.cos(angle) * vec.y
    })
end

---For 2 vectors v1 and v2: r = (v1.x * v2.x) + (v1.y * v2.y) (Dot product)
---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector.Dot(v1, v2)
    MapPositionCheck(v1, v2)
    return v1.x * v2.x + v1.y * v2.y
end

---For 2 vectors v1 and v2: r = (v1.x * v2.y) - (v1.y * v2.x) (Cross product)
---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector.Cross(v1, v2)
    MapPositionCheck(v1, v2)
    return v1.x * v2.y - v1.y * v2.x
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector.SqDistance(v1, v2)
    MapPositionCheck(v1, v2)
    return Vector.SqLength(Vector.Sub(v1, v2))
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector.Distance(v1, v2)
    MapPositionCheck(v1, v2)
    return Vector.Length(Vector.Sub(v1, v2))
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return number
function Vector.Angle(v1, v2)
    MapPositionCheck(v1, v2)
    return math.acos(Vector.Dot(v1, v2) / (Vector.Length(v1) * Vector.Length(v2)))
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return MapPosition
function Vector.Max(v1, v2)
    MapPositionCheck(v1, v2)
    return FormatMapPosition({math.max(v1.x, v2.x), math.max(v1.y, v2.y)})
end

---@param v1 MapPosition
---@param v2 MapPosition
---@return MapPosition
function Vector.Min(v1, v2)
    MapPositionCheck(v1, v2)
    return FormatMapPosition({math.min(v1.x, v2.x), math.min(v1.y, v2.y)})
end

---@class Area
local Area = Math2d.Area

---@param ... any
local function BoundingBoxCheck(...)
    for k, o in ipairs({...}) do
        if not IsBoundingBox(o) then error("A BoundingBox was expected, got " .. type(o) .. ".", 2) end
    end
end

---@return BoundingBox
function Area.Zero()
    return FormatBoundingBox({{0,0}, {0,0}})
end

---@return BoundingBox
function Area.One()
    return FormatBoundingBox({{1,1}, {1,1}})
end

---@param v1 BoundingBox
---@param v2 MapPosition
---@return BoundingBox
function Area.Add(v1, v2)
    BoundingBoxCheck(v1)
    MapPositionCheck(v2)
    return FormatBoundingBox({
        { x = v1.left_top.x + v2.x, y = v1.left_top.y + v2.y },
        { x = v1.right_bottom.x + v2.x, y = v1.right_bottom.y + v2.y }
    })
end

---@param v1 BoundingBox
---@param v2 MapPosition
---@return BoundingBox
function Area.Sub(v1, v2)
    BoundingBoxCheck(v1)
    MapPositionCheck(v2)
    return FormatBoundingBox({
        { x = v1.left_top.x - v2.x, y = v1.left_top.y - v2.y },
        { x = v1.right_bottom.x - v2.x, y = v1.right_bottom.y - v2.y }
    })
end

---@param v1 BoundingBox
---@param v2 number
---@return BoundingBox
function Area.Mul(v1, v2)
    BoundingBoxCheck(v1)
    ScalarCheck(v2)
    return FormatBoundingBox({
        { x = v1.left_top.x * v2, y = v1.left_top.y * v2 },
        { x = v1.right_bottom.x * v2, y = v1.right_bottom.y * v2 }
    })
end

---@param v1 BoundingBox
---@param v2 number
---@return BoundingBox
function Area.Div(v1, v2)
    BoundingBoxCheck(v1)
    ScalarCheck(v2)
    return FormatBoundingBox({
        { x = v1.left_top.x / v2, y = v1.left_top.y / v2 },
        { x = v1.right_bottom.x / v2, y = v1.right_bottom.y / v2 }
    })
end

---@param v1 BoundingBox
---@param v2 BoundingBox
---@return boolean
function Area.Equal(v1, v2)
    BoundingBoxCheck(v1, v2)
    return v1.left_top == v2.left_top and v1.right_bottom == v2.right_bottom
end

---@param v BoundingBox
---@return string
function Area.ToString(v)
    BoundingBoxCheck(v)
    return "{" .. Vector.ToString(v.left_top) .. "; " .. Vector.ToString(v.right_bottom) .. "}"
end

---@param box BoundingBox
---@param position MapPosition
---@return number
function Area.SqDistanceTo(box, position)
    BoundingBoxCheck(box)
    MapPositionCheck(position)
    local x = math.max(box.left_top.x - position.x, 0, position.x - box.right_bottom.x)
    local y = math.max(box.left_top.y - position.y, 0, position.y - box.right_bottom.y)
    return x * x + y * y
end

---@param box BoundingBox
---@param position MapPosition
---@return number
function Area.DistanceTo(box, position)
    BoundingBoxCheck(box)
    return math.sqrt(Area.SqDistanceTo(box, position))
end

---@param box BoundingBox
---@return MapPosition
function Area.CenterOfMass(box)
    BoundingBoxCheck(box)
    return FormatMapPosition({
        (box.left_top.x + box.right_bottom.x) / 2,
        (box.left_top.y + box.right_bottom.y) / 2
    })
end

---@param box BoundingBox
---@param position MapPosition
function Area.Contains(box, position)
    BoundingBoxCheck(box)
    MapPositionCheck(position)
    return Area.SqDistanceTo(box, position) == 0
end

return Math2d