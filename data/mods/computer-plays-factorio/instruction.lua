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
    storage.wait_ranged_requests = {}
    ---@type int
    storage.current_waypoint = 1
end)

---Same as API.AddRequestHandler but with additional logic for request that
---requires the player to be in range of some area
---@param request_name string | string[]
---@param handler RequestHandler
---@param get_area fun(request: Request<any>): BoundingBox?, number?
function Instruction.AddRangedRequest(request_name, handler, get_area)
    API.AddRequestHandler(request_name, function (request)
        if not storage.ranged_requests[request] then
            storage.ranged_requests[request] = true
            return false
        end

        local area, range = get_area(request)
        if not area or not range then
            storage.ranged_requests[request] = nil
            return false
        end

        if Area.SqDistanceTo(area, game.get_player(1).position) <= math.pow(range, 2) then
            handler(request)
            storage.ranged_requests[request] = nil
            return true
        end
        return false
    end)
end

---Evaluate pending instruction, see Player.AddInstructionHandler
Event.OnEvent(defines.events.on_tick, function()
    for request, _ in pairs(storage.ranged_requests) do
        local handler = API.GetRequestHandler(request.name)
        if handler(request) then break end
    end

    if table_size(storage.ranged_requests) == 0 then
        for request, _ in pairs(storage.wait_ranged_requests) do
            API.Success(request)
            storage.wait_ranged_requests[request] = nil
        end
    end
end)

API.AddRequestHandler("WaitAllRangedRequests", function (request)
    storage.wait_ranged_requests[request] = true
end)

return Instruction