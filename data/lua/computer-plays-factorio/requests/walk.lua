local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector
local Area = Math2d.Area

local halfPi = math.pi / 2

Event.OnInit(function ()
    ---@type { entity: { pos: MapPosition.0, action: boolean }[], chunk: { pos: MapPosition.0, action: boolean }[] }
    storage.pathfinderDataUpdates = { entity = {}, chunk = {} }
end)

---@param event EventData.on_chunk_generated
Event.OnEvent(defines.events.on_chunk_generated, function (event)
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local updates = storage.pathfinderDataUpdates.chunk
    local leftTop = event.area.left_top
    local rightBottom = event.area.right_bottom
    local leftBottom = { leftTop.x, rightBottom.y }
    local rightTop = { rightBottom.x, leftTop.y }

    ---@param vec MapPosition
    ---@param firstTile MapPosition
    ---@param lastTile MapPosition
    local function Chunk(vec, firstTile, lastTile)
        local chunk = Vector.Add(event.position, vec)
        local action = not surface.is_chunk_generated(chunk)
        local tile, lastTileCopy

        if action then
            tile = Vector.Add(firstTile, vec)
            lastTileCopy = Vector.Add(lastTile, vec)
        else
            tile = firstTile
            lastTileCopy = lastTile
        end

        local increment = Vector.Rotate(vec, halfPi)
        increment = Vector.Abs(increment)
        for _ = 0, 32 do
            local index
            for i, t in pairs(updates) do
                if Vector.Equal(t.pos, tile) then
                    index = i
                    break
                end
            end

            if index then
                updates[index].action = action
            else
                table.insert(updates, { pos = tile, action = action })
            end

            if Vector.Equal(tile, lastTileCopy) then break end
            tile = Vector.Add(tile, increment)
        end
    end

    local fd = Vector.FromDirection
    Chunk(fd(defines.direction.north), leftTop, rightTop)
    Chunk(fd(defines.direction.south), leftBottom, rightBottom)
    Chunk(fd(defines.direction.west), leftTop, leftBottom)
    Chunk(fd(defines.direction.east), rightTop, rightBottom)

    Chunk(fd(defines.direction.northwest), leftTop, leftTop)
    Chunk(fd(defines.direction.northeast), rightTop, rightTop)
    Chunk(fd(defines.direction.southwest), leftBottom, leftBottom)
    Chunk(fd(defines.direction.southeast), rightBottom, rightBottom)
end)

-- Event.OnInit(function ()
--     local surface = game.get_surface(1) --[[@as LuaSurface]]
--     local d = {}

--     for chunk in surface.get_chunks() do
--         local area = chunk.area
--         for x = area.left_top.x, area.right_bottom.x do
--             for y = area.left_top.y, area.right_bottom.y do
--                 local pos = { x = x, y = y }
--                 local count = surface.entity_prototype_collides("character", pos, false)
--                 if count > 0 then
--                     table.insert(d, pos)
--                 end
--             end
--         end
--     end

--     API.InvokeEvent("SetPathfinderData", d)
-- end)

commands.add_command("draw", "", function (c)
    rendering.clear()
    for k, t in pairs(storage.pathfinderDataUpdates.chunk) do
        local c = { r = 1 }
        if t.action then c = { g = 1 } end
        rendering.draw_circle{
            color = c,
            radius = 0.5,
            filled = true,
            target = t.pos,
            surface = 1
        }
    end
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