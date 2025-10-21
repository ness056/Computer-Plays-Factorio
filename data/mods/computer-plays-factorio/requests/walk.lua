local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector

---@param event EventData.on_player_changed_position
Event.OnEvent(defines.events.on_player_changed_position, function (event)
    if event.player_index ~= 1 then return end
    API.InvokeEvent("PlayerMoved", game.get_player(1).position)
end)

local function EvaluatePath()
    local player = game.get_player(1) --[[@as LuaPlayer]]

    local walking_state = player.walking_state
    if not storage.walk_request or player.controller_type ~= defines.controllers.character then
        walking_state.walking = false
        player.walking_state = walking_state
        return
    end

    local path = storage.walk_request.data
    local waypoint = path[storage.current_waypoint]
    local v = Vector.Sub(waypoint, player.character.position)

    local speed = player.character.character_running_speed
    if Vector.SqLength(v) <= math.pow(speed, 2) then
        if storage.current_waypoint + 1 <= table_size(path) then
            storage.current_waypoint = storage.current_waypoint + 1
            waypoint = path[storage.current_waypoint]
            v = Vector.Sub(waypoint, player.character.position)

        elseif not storage.is_walk_until then
            API.Success(storage.walk_request)
            storage.walk_request = nil
            walking_state.walking = false
            player.walking_state = walking_state
            return
        end
    end

    local half_speed = speed / 2
    if v.x > half_speed and v.y > half_speed then
        walking_state.direction = defines.direction.southeast

    elseif v.x > half_speed and v.y < -half_speed then
        walking_state.direction = defines.direction.northeast

    elseif v.x < -half_speed and v.y > half_speed then
        walking_state.direction = defines.direction.southwest

    elseif v.x < -half_speed and v.y < -half_speed then
        walking_state.direction = defines.direction.northwest

    elseif v.x > half_speed and math.abs(v.y) < half_speed then
        walking_state.direction = defines.direction.east

    elseif v.x < -half_speed and math.abs(v.y) < half_speed then
        walking_state.direction = defines.direction.west

    elseif math.abs(v.x) < half_speed and v.y > half_speed then
        walking_state.direction = defines.direction.south

    else
        walking_state.direction = defines.direction.north
    end

    walking_state.walking = true
    player.walking_state = walking_state
end

---@param request Request<MapPosition.0[]>
API.AddRequestHandler("Walk", function (request)
    if storage.walk_request then
        API.Failed(request, RequestError.BUSY)
        return
    end

    if table_size(request.data) == 0 then
        API.Failed(request, RequestError.EMPTY_PATH)
        return
    end

    storage.is_walk_until = false
    storage.walk_request = request
    storage.current_waypoint = 1
    EvaluatePath()
end)

---@param request Request<MapPosition.0[]>
API.AddRequestHandler("WalkUntil", function (request)
    if storage.walk_request then
        API.Failed(request, RequestError.BUSY)
        return
    end

    if table_size(request.data) == 0 then
        API.Failed(request, RequestError.EMPTY_PATH)
        return
    end

    storage.is_walk_until = true
    storage.walk_request = request
    storage.current_waypoint = 1
    EvaluatePath()
end)

API.AddRequestHandler("WalkFinishAndStop", function (request)
    if not storage.walk_request then
        API.Failed(request, RequestError.NOT_BUSY)
        return
    end

    storage.is_walk_until = false
    API.Success(request)
end)

---@param request Request<any>
API.AddRequestHandler("WalkStop", function (request)
    if not storage.walk_request then
        API.Failed(request, RequestError.NOT_BUSY)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local walking_state = player.walking_state
    walking_state.walking = false
    player.walking_state = walking_state

    if storage.is_walk_until then
        API.Success(storage.walk_request)
    else
        API.Failed(storage.walk_request, RequestError.CANCELED)
    end
    storage.walk_request = nil
    API.Success(request)
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)