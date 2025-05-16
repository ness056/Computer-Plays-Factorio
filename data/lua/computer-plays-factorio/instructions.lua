local API = require("api")
local Event = require("event")
local Math2d = require("math2d")
local Vector = Math2d.Vector2d
local Area = Math2d.Area

Event.OnInit(function ()
    ---@type Request<{ start: MapPosition.0, goal: MapPosition.0 }>[]
    storage.pathRequests = {}
    ---@type Request<any>
    storage.instruction = nil
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
    local v = Vector.Sub(waypoint.position, player.position)

    if v:SquaredLength() <= math.pow(player.character_running_speed, 2) then
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
    if v.x > s and v.y > s then
        walkingState.direction = defines.direction.southeast

    elseif v.x > s and v.y < -s then
        walkingState.direction = defines.direction.northeast

    elseif v.x < -s and v.y > s then
        walkingState.direction = defines.direction.southwest

    elseif v.x < -s and v.y < -s then
        walkingState.direction = defines.direction.northwest

    elseif v.x > s and math.abs(v.y) < s then
        walkingState.direction = defines.direction.east

    elseif v.x < -s and math.abs(v.y) < s then
        walkingState.direction = defines.direction.west

    elseif math.abs(v.x) < s and v.y > s then
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

---@param request Request<{ position: MapPosition.0, item: string, direction: defines.direction }>
API.AddRequestHandler("Build", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local data = request.data

    if player.get_item_count(data.item) == 0 then
        API.Failed(request, "Not enough item")
        return
    end

    if not player.can_place_entity{ name = data.item, position = data.position, direction = data.direction } then
        API.Failed(request, "Cannot place item")
        return
    end
end)

Event.OnEvent(defines.events.on_tick, function (event)
    if not storage.instruction then return end
    local i = storage.instruction
    local player = game.get_player(1) --[[@as LuaPlayer]]

    local distance, area
    if i.name == "Build" then
        distance = player.build_distance
        area = Area.Add(prototypes.entity[i.data.item].collision_box, i.data.position)
    end

    if i.name == "Mine" or i.name == "Put" or i.name == "Take" then
        distance = player.reach_distance
    end

    if distance then
    end
end)