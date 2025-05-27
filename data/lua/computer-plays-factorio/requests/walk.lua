local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector2d
local Area = Math2d.Area

---@param request Request<nil>
API.AddRequestHandler("PathfinderData", function (request)
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local collision_box = Area.new(prototypes.entity["character"].collision_box)

    local limits = Area.Zero()
    for chunk in surface.get_chunks() do
        limits.left_top = Vector.Min(limits.left_top, chunk.area.left_top)
        limits.right_bottom = Vector.Max(limits.right_bottom, chunk.area.right_bottom)
    end

    local collisions = {}
    for x = limits.left_top.x, limits.right_bottom.x do
        for y = limits.left_top.y, limits.right_bottom.y do
            local pos = { x = x, y = y }
            local count = surface.count_entities_filtered{area = collision_box + pos, collision_mask = "player"}
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
    if v:SqLength() <= math.pow(speed, 2) then
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