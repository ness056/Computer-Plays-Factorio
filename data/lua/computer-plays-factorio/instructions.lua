local API = require("api")
local Event = require("event")
local Math2d = require("math2d")
local Vector = Math2d.Vector2d
local Area = Math2d.Area

Event.OnInit(function ()
    ---@type Request<{ start: MapPosition.0, goal: MapPosition.0 }>[]
    storage.pathRequests = {}
    ---@type Request<any>?
    storage.instruction = nil
    ---Used for instruction like build, put, take, etc for which
    ---the player has a certain range.
    ---@type Area?
    storage.instructionArea = nil
    ---@type number?
    storage.instructionSqDistance = nil
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
        API.Failed(request, RequestErrorCode.NO_PATH_FOUND)
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

    if v:SqLength() <= math.pow(player.character_running_speed, 2) then
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
        API.Failed(request, RequestErrorCode.BUSY)
        return
    end

    if table_size(request.data) == 0 then
        log("bb")
        API.Failed(request, RequestErrorCode.EMPTY_PATH)
        return
    end

    storage.walkRequest = request
    storage.currentWaypoint = 1
    EvaluatePath()
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)

---@param request Request<{ position: MapPosition.0, item: string, direction: defines.direction }>
local function Build(request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local data = request.data

    if player.get_item_count(data.item) == 0 then
        API.Failed(request, RequestErrorCode.NOT_ENOUGH_ITEM)
        return
    end

    if not player.can_place_entity{ name = data.item, position = data.position, direction = data.direction } then
        API.Failed(request, RequestErrorCode.NOT_ENOUGH_ROOM)
        return
    end
end

---@param request Request<any>
API.AddRequestHandler("Build", function (request)
    if storage.instruction then
        API.Failed(request, RequestErrorCode.BUSY)
        return
    end
    storage.instruction = request

    local player = game.get_player(1) --[[@as LuaPlayer]]
    box = prototypes.entity[request.data.item].collision_box
    storage.instructionArea = Area.Add(box, request.data.position)
    storage.instructionSqDistance = math.pow(player.build_distance, 2)
end)

Event.OnEvent(defines.events.on_tick, function (event)
    local i = storage.instruction
    if not i then return end

    if storage.instructionArea then
        if storage.instructionArea:SqDistanceTo(i.data.position) > storage.instructionSqDistance then
            return;
        end
    end

    if i.name == "Build" then Build(i) end
end)