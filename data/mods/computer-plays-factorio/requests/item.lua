local Instruction = require("__computer-plays-factorio__.instruction")
local API = require("__computer-plays-factorio__.api")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

---@class PutTakeRequestData
---@field position MapPosition.0
---@field entity string
---@field item string
---@field amount int
---@field player_inventory defines.inventory
---@field entity_inventory defines.inventory
---@field force boolean

---@param request Request<PutTakeRequestData>
---@return BoundingBox?, number?
local function getAreaReachEntity(request)
    if not prototypes.entity[request.data.entity] then
        API.Failed(request, RequestError.ENTITY_DOESNT_EXIST)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local entity = player.surface.find_entity(request.data.entity, request.data.position)
    if not entity then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    return Area.Add(entity.bounding_box, request.data.position), player.reach_distance
end

---@param request Request<PutTakeRequestData>
Instruction.AddRangedRequest({"Put", "Take"}, function (request)
    local data = request.data

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local entity = player.surface.find_entity(request.data.entity, request.data.position)
    if not entity then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    local from_inventory = player.get_inventory(data.player_inventory)
    local to_inventory = entity.get_inventory(data.entity_inventory)
    if request.name == "Take" then
        from_inventory, to_inventory = to_inventory, from_inventory
    end

    if not from_inventory or not to_inventory then
        API.Failed(request, RequestError.NO_INVENTORY_FOUND)
        return
    end

    local item_count = from_inventory.get_item_count(data.item)
    local amount = data.amount
    if amount < 0 then
        amount = item_count + amount
    end

    if math.abs(data.amount) > item_count then
        if data.force then
            amount = item_count
        else
            API.Failed(request, RequestError.NOT_ENOUGH_ITEM)
            return
        end
    end

    local inserted = to_inventory.insert({ name = data.item, count = amount })
    from_inventory.remove({ name = data.item, count = inserted })

    API.Success(request, inserted)
end, getAreaReachEntity)