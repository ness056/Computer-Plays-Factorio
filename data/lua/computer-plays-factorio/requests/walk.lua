local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector
local Area = Math2d.Area

local halfPi = math.pi / 2

Event.OnInit(function ()
    ---@type { entity: true[][], chunk: true[][] }
    storage.pathfinderData = { entity = {}, chunk = {} }
end)

---@param event EventData.on_chunk_generated
Event.OnEvent(defines.events.on_chunk_generated, function (event)
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local chunkData = storage.pathfinderData.chunk
    local leftTop = event.area.left_top
    local rightBottom = event.area.right_bottom
    local leftBottom = { leftTop.x, rightBottom.y }
    local rightTop = { rightBottom.x, leftTop.y }

    ---@param vec MapPosition
    ---@param firstTile MapPosition
    ---@param lastTile MapPosition
    local function Chunk(vec, firstTile, lastTile)
        local chunk = Vector.Add(event.position, vec)
        local collide = not surface.is_chunk_generated(chunk)

        if not collide and vec.x ~= 0 and vec.y ~= 0 then
            local chunk1 = Vector.Add(event.position, {vec.x, 0})
            local chunk2 = Vector.Add(event.position, {0, vec.y})
            collide = not (surface.is_chunk_generated(chunk1) and
                          surface.is_chunk_generated(chunk2))
        end

        local tile = firstTile
        local lastTileCopy = lastTile

        local increment = Vector.Rotate(vec, halfPi)
        increment = Vector.Abs(increment)
        -- infinite loop if vec is not orthogonal to the vector firstTile -> lastTile
        while true do
            local t = chunkData[tile.x]
            if not t then
                t = {}
                chunkData[tile.x] = t
            end

            if collide then
                t[tile.y] = true
            else
                t[tile.y] = nil
            end

            if Vector.Equal(tile, lastTileCopy) then break end
            tile = Vector.Add(tile, increment)
        end
    end

    local fd = Vector.FromDirection
    local n, s, w, e = fd(defines.direction.north), fd(defines.direction.south),
                       fd(defines.direction.west), fd(defines.direction.east)
    Chunk(n, Vector.Add(leftTop, e), Vector.Add(rightTop, w))
    Chunk(s, Vector.Add(leftBottom, e), Vector.Add(rightBottom, w))
    Chunk(w, Vector.Add(leftTop, s), Vector.Add(leftBottom, n))
    Chunk(e, Vector.Add(rightTop, s), Vector.Add(rightBottom, n))

    Chunk(fd(defines.direction.northwest), leftTop, leftTop)
    Chunk(fd(defines.direction.northeast), rightTop, rightTop)
    Chunk(fd(defines.direction.southwest), leftBottom, leftBottom)
    Chunk(fd(defines.direction.southeast), rightBottom, rightBottom)

    local entityData = storage.pathfinderData.entity
    for x = leftTop.x, rightBottom.x do
        for y = leftTop.y, rightBottom.y do
            local t = entityData[x]
            if not t then
                t = {}
                entityData[x] = t
            end

            local pos = {x, y}
            if surface.entity_prototype_collides("character", pos, false) then
                t[y] = true
            else
                t[y] = nil
            end
        end
    end

    if (event.position.x == 0 and event.position.y == 0) or
       (event.position.x == -1 and event.position.y == 0) or
       (event.position.x == 0 and event.position.y == -1) or
       (event.position.x == -1 and event.position.y == -1) then

        entityData[0][0] = nil
    end
end)

Event.OnNthTick(120, function (event)
    rendering.clear()
    for x, t in pairs(storage.pathfinderData.chunk) do
        for y, valid in pairs(t) do
            if valid then
                rendering.draw_circle{
                    color = { g = 1 },
                    radius = 0.45,
                    filled = true,
                    target = {x, y},
                    surface = 1
                }
            end
        end
    end

    for x, t in pairs(storage.pathfinderData.entity) do
        for y, valid in pairs(t) do
            if valid then
                rendering.draw_circle{
                    color = { r = 1 },
                    radius = 0.45,
                    filled = true,
                    target = {x, y},
                    surface = 1
                }
            end
        end
    end
end)

---@param entity LuaEntity
local function UpdateEntityDataBuilt(entity)
    
end

---@param event EventData.on_built_entity
Event.OnEvent(defines.events.on_built_entity, function (event)
    UpdateEntityDataBuilt(event.entity)
end)

---@param event EventData.on_robot_built_entity
Event.OnEvent(defines.events.on_robot_built_entity, function (event)
    UpdateEntityDataBuilt(event.entity)
end)

---@param request Request<nil>
API.AddRequestHandler("PathfinderData", function (request)
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local collision_box = prototypes.entity["character"].collision_box

    local limits = Area.Zero()
    for chunk in surface.get_chunks() do
        limits.left_top = Vector.Min(limits.left_top, chunk.area.left_top)
        limits.right_bottom = Vector.Max(limits.right_bottom, chunk.area.right_bottom)
    end

    local collisions = {}
    for x = limits.left_top.x, limits.right_bottom.x do
        for y = limits.left_top.y, limits.right_bottom.y do
            local pos = { x = x, y = y }
            local count = surface.count_entities_filtered{area = Area.Add(collision_box, pos), collision_mask = "player"}
            if count > 0 then
                table.insert(collisions, pos)
            end
        end
    end

    API.Success(request, { limits = limits, collisions = collisions })
end)

local function EvaluatePath()
    if true then return end
    local player = game.get_player(1) --[[@as LuaPlayer]]

    local walkingState = player.walking_state
    if not storage.walkRequest then
        walkingState.walking = false
        player.walking_state = walkingState
        return
    end

    local path = storage.walkRequest.data
    local waypoint = path[storage.currentWaypoint]
    local v = Vector.Sub(waypoint.position, player.character.position)

    local speed = player.character.character_running_speed
    if Vector.SqLength(v) <= math.pow(speed, 2) then
        storage.currentWaypoint = storage.currentWaypoint + 1

        if storage.currentWaypoint < table_size(path) then
            EvaluatePath()
        else
            API.Success(storage.walkRequest)
            storage.walkRequest = nil
            walkingState.walking = false
            player.walking_state = walkingState
        end

        return
    end

    local halfSpeed = speed / 2
    if v.x > halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.southeast

    elseif v.x > halfSpeed and v.y < -halfSpeed then
        walkingState.direction = defines.direction.northeast

    elseif v.x < -halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.southwest

    elseif v.x < -halfSpeed and v.y < -halfSpeed then
        walkingState.direction = defines.direction.northwest

    elseif v.x > halfSpeed and math.abs(v.y) < halfSpeed then
        walkingState.direction = defines.direction.east

    elseif v.x < -halfSpeed and math.abs(v.y) < halfSpeed then
        walkingState.direction = defines.direction.west

    elseif math.abs(v.x) < halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.south

    else
        walkingState.direction = defines.direction.north
    end

    walkingState.walking = true
    player.walking_state = walkingState
end

---@param request Request<PathfinderWaypoint[]>
API.AddRequestHandler("Walk", function (request)
    if storage.walkRequest then
        API.Failed(request, RequestError.BUSY)
        return
    end

    if table_size(request.data) == 0 then
        API.Failed(request, RequestError.EMPTY_PATH)
        return
    end

    storage.walkRequest = request
    storage.currentWaypoint = 1
    EvaluatePath()
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)