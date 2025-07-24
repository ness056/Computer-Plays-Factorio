local Instruction = require("__computer-plays-factorio__.instruction")
local API = require("__computer-plays-factorio__.api")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

---@class PutTakeRequestData
---@field position MapPosition.0
---@field entity string
---@field item string
---@field amount int
---@field playerInventory defines.inventory
---@field entityInventory defines.inventory
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

    local fromInventory = player.get_inventory(data.playerInventory)
    local toInventory = entity.get_inventory(data.entityInventory)
    if request.name == "Take" then
        fromInventory, toInventory = toInventory, fromInventory
    end

    if not fromInventory or not toInventory then
        API.Failed(request, RequestError.NO_INVENTORY_FOUND)
        return
    end

    local itemCount = fromInventory.get_item_count(data.item)
    local amount = data.amount
    if amount < 0 then
        amount = itemCount + amount
    end

    if math.abs(data.amount) > itemCount then
        if data.force then
            amount = itemCount
        else
            API.Failed(request, RequestError.NOT_ENOUGH_ITEM)
            return
        end
    end

    local inserted = toInventory.insert({ name = data.item, count = amount })
    fromInventory.remove({ name = data.item, count = inserted })

    API.Success(request, inserted)
end, getAreaReachEntity)