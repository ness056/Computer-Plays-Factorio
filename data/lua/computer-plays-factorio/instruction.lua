local Instruction = {}

local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

Event.OnInit(function ()
    ---@type Request<{ start: MapPosition.0, goal: MapPosition.0 }>[]
    storage.pathRequests = {}
    ---@type Request<PathfinderWaypoint[]>
    storage.walkRequest = nil
    ---@type Request<any>?
    storage.rangedRequest = nil
    ---@type int
    storage.currentWaypoint = 1
    ---@type Request<MapPosition.0>
    storage.mineRequest = nil
end)

---@return boolean
function Instruction.IsBusy()
    return not not (storage.rangedRequest or storage.mineRequest)
end

---Same as API.AddRequestHandler but with additional logic for request that
---requires the player to be in range of some area
---@param requestName string | string[]
---@param handler RequestHandler
---@param getArea fun(request: Request<any>): BoundingBox?, number?
function Instruction.AddRangedRequest(requestName, handler, getArea)
    API.AddRequestHandler(requestName, function (request)
        if Instruction.IsBusy() then
            if storage.rangedRequest ~= request then
                API.Failed(request, RequestError.BUSY)
                return
            end
        else
            storage.rangedRequest = request
        end

        local area, range = getArea(request)
        if not area or not range then
            storage.rangedRequest = nil
            return
        end

        if Area.SqDistanceTo(area, game.get_player(1).position) > math.pow(range, 2) then
            handler(request)
            storage.rangedRequest = nil
        end
    end)
end

---Evaluate pending instruction, see Player.AddInstructionHandler
Event.OnEvent(defines.events.on_tick, function()
    if not storage.rangedRequest then return end
    local handler = API.GetRequestHandler(storage.rangedRequest.name)

    handler(storage.rangedRequest)
end)

return Instruction