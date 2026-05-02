local Instruction = {}

local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

Event.OnInit(function ()
    ---@type Request<MapPosition.0[]>
    storage.walk_request = nil
    ---@type boolean
    storage.is_walk_until = false
    ---@type { [Request<any>]: true }
    storage.ranged_requests = {}
    ---@type { [Request<any>]: true }
    storage.mine_requests = {}
    ---@type { [Request<any>]: true }
    storage.wait_entity_requests = {}
    ---@type int
    storage.current_waypoint = 1
end)

---Same as API.AddRequestHandler but with additional logic for request that
---requires the player to be in range of some area
---@param request_name string | string[]
---@param handler RequestHandler
---@param get_area fun(request: Request<any>): BoundingBox?, number?, boolean?
function Instruction.AddRangedRequest(request_name, handler, get_area)
    API.AddRequestHandler(request_name, function (request)
        if not storage.ranged_requests[request] then
            storage.ranged_requests[request] = true
            return false
        end

        local area, range, collides_with_player = get_area(request)
        if not area or not range then
            storage.ranged_requests[request] = nil
            return false
        end

        local player = game.get_player(1) --[[@as LuaPlayer]]
        if Area.SqDistanceTo(area, player.position) <= math.pow(range, 2) then
            if (collides_with_player and Area.Contains(area, player.character.bounding_box)) then
                return false
            end

            if (handler(request)) then
                return false
            end
            storage.ranged_requests[request] = nil
            return true
        end
        return false
    end)
end

---Evaluate pending ranged instructions, see Instruction.AddRangedRequest
Event.OnEvent(defines.events.on_tick, function()
    for request, _ in pairs(storage.ranged_requests) do
        local handler = API.GetRequestHandler(request.name)
        if handler(request) then break end
    end

    if table_size(storage.ranged_requests) == 0 and table_size(storage.mine_requests) == 0 then
        for request, _ in pairs(storage.wait_entity_requests) do
            API.Success(request)
            storage.wait_entity_requests[request] = nil
        end
    end
end)

API.AddRequestHandler("WaitAllEntityRequests", function (request)
    storage.wait_entity_requests[request] = true
end)

return Instruction