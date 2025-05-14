local API = require("api")
local Event = require("event")

Event.OnInit(function ()
    ---@type Request<{ start: MapPosition.0, goal: MapPosition.0 }>[]
    storage.pathRequests = {}
end)

---@param request Request<{ start: MapPosition.0, goal: MapPosition.0 }>
local function RequestPath(request)
    local nauvis = game.get_surface(1) --[[@as LuaSurface]]
    log(serpent.line(request))

    local id = nauvis.request_path({
        bounding_box = prototypes.entity["character"].collision_box,
        collision_mask = prototypes.entity["character"].collision_mask,
        force = "player",
        start = request.data.start,
        goal = request.data.goal,
        can_open_gates = true,
        radius = 0.13,
        path_resolution_modifier = 0,
        pathfind_flags = {
            no_break = true,
            cache = true
        }
    })

    storage.pathRequests[id] = request
end

API.AddRequestHandler("RequestPath", function (request)
    RequestPath(request)
end)

---@param event EventData.on_script_path_request_finished
Event.OnEvent(defines.events.on_script_path_request_finished, function (event)
    log(serpent.line(event))
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
        API.Failed(request, "no path found")
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
    local x = waypoint.position.x - player.position.x
    local y = waypoint.position.y - player.position.y

    if x*x + y*y <= math.pow(player.character_running_speed, 2) then
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

    local s = player.character_running_speed
    if x > s and y > s then
        walkingState.direction = defines.direction.southeast

    elseif x > s and y < -s then
        walkingState.direction = defines.direction.northeast

    elseif x < -s and y > s then
        walkingState.direction = defines.direction.southwest

    elseif x < -s and y < -s then
        walkingState.direction = defines.direction.northwest

    elseif x > s and math.abs(y) < s then
        walkingState.direction = defines.direction.east

    elseif x < -s and math.abs(y) < s then
        walkingState.direction = defines.direction.west

    elseif math.abs(x) < s and y > s then
        walkingState.direction = defines.direction.south

    else
        walkingState.direction = defines.direction.north
    end
end

---@param request Request<PathfinderWaypoint[]>
API.AddRequestHandler("Walk", function (request)
    log(serpent.line(request))
    if storage.walkRequest then
        log("aa")
        API.Failed(request, "Already walking")
        return
    end

    if table_size(request.data) == 0 then
        log("bb")
        API.Failed(request, "Empty path")
        return
    end

    storage.walkRequest = request
    storage.currentWaypoint = 1
    EvaluatePath()
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)