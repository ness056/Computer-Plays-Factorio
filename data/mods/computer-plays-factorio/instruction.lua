local Instruction = {}

local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

Event.OnInit(function ()
    ---@type Request<{ start: MapPosition.0, goal: MapPosition.0 }>[]
    storage.path_requests = {}
    ---@type Request<MapPosition.0[]>
    storage.walk_request = nil
    ---@type Request<any>?
    storage.ranged_request = nil
    ---@type int
    storage.current_waypoint = 1
    ---@type Request<MapPosition.0>
    storage.mine_request = nil
end)

---@return boolean
function Instruction.IsBusy()
    return not not (storage.ranged_request or storage.mine_request)
end

---Same as API.AddRequestHandler but with additional logic for request that
---requires the player to be in range of some area
---@param request_name string | string[]
---@param handler RequestHandler
---@param get_area fun(request: Request<any>): BoundingBox?, number?
function Instruction.AddRangedRequest(request_name, handler, get_area)
    API.AddRequestHandler(request_name, function (request)
        if Instruction.IsBusy() then
            if storage.ranged_request ~= request then
                API.Failed(request, RequestError.BUSY)
                return
            end
        else
            storage.ranged_request = request
        end

        local area, range = get_area(request)
        if not area or not range then
            storage.ranged_request = nil
            return
        end

        if Area.SqDistanceTo(area, game.get_player(1).position) <= math.pow(range, 2) then
            handler(request)
            storage.ranged_request = nil
        end
    end)
end

---Evaluate pending instruction, see Player.AddInstructionHandler
Event.OnEvent(defines.events.on_tick, function()
    if not storage.ranged_request then return end
    local handler = API.GetRequestHandler(storage.ranged_request.name)

    handler(storage.ranged_request)
end)

return Instruction