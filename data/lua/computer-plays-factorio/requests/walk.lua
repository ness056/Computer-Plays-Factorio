local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector2d

--[[
/sc
local pos = {
    {10, 10},
    {0, 0},
    {10, 0},
    {0, 10},
    {5, 9}
}
local player = game.player
for k, _ in pairs(pos) do
    player.surface.request_path({
        bounding_box = prototypes.entity["character"].collision_box,
        collision_mask = prototypes.entity["character"].collision_mask,
        force = "player",
        start = k == 1 and {0, 0} or pos[k - 1],
        goal = pos[k],
        entity_to_ignore = player and player.character,
        path_resolution_modifier = 0,
        pathfind_flags = {
            cache = false
        }
    })
end

/sc
local player = game.player
player.surface.request_path({
    bounding_box = prototypes.entity["character"].collision_box,
    collision_mask = prototypes.entity["character"].collision_mask,
    force = "player",
    start = {12, 0},
    goal = {12, 10},
    entity_to_ignore = player and player.character,
    path_resolution_modifier = 0,
    pathfind_flags = {
        cache = false
    }
})
]]

---@param request Request<{ start: MapPosition.0, goal: MapPosition.0 }>
local function RequestPath(request)
    local nauvis = game.get_surface(1) --[[@as LuaSurface]]
    local player = game.get_player(1)

    local id = nauvis.request_path({
        bounding_box = prototypes.entity["character"].collision_box,
        collision_mask = prototypes.entity["character"].collision_mask,
        force = "player",
        start = request.data.start,
        goal = request.data.goal,
        can_open_gates = true,
        radius = 0.15,
        entity_to_ignore = player and player.character,
        path_resolution_modifier = 0,
        pathfind_flags = {
            no_break = true,
            cache = false
        }
    })

    storage.pathRequests[id] = request
end

API.AddRequestHandler("RequestPath", function (request)
    log("RequestPath on tick: " .. game.tick)
    RequestPath(request)
end)

---@param event EventData.on_script_path_request_finished
Event.OnEvent(defines.events.on_script_path_request_finished, function (event)
    log("on_script_path_request_finished on tick: " .. game.tick)
    if (not storage.pathRequests[event.id]) then return end

    local request = storage.pathRequests[event.id]
    storage.pathRequests[event.id] = nil
    if (event.try_again_later) then
        RequestPath(request)
        return
    end

    if (event.path) then
        API.Success(request, event.path)
    else
        API.Failed(request, RequestError.NO_PATH_FOUND)
    end
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