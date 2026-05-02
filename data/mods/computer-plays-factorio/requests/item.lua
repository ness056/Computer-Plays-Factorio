local Instruction = require("__computer-plays-factorio__.instruction")
local API = require("__computer-plays-factorio__.api")
local Utils = require("__computer-plays-factorio__.utils")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

---@param entity LuaEntity
---@param inventory string
---@return defines.inventory?
local function ToGameInventory(entity, inventory)
    if inventory == "fuel" then
        return defines.inventory.fuel
    end

    if inventory == "main" and entity.get_inventory(defines.inventory.chest) then
        return defines.inventory.chest
    end

    local type = entity.type
    if type == "character" then
        if inventory == "main" then return defines.inventory.character_main end
        if inventory == "guns" then return defines.inventory.character_guns end
        if inventory == "ammo" then return defines.inventory.character_ammo end
        if inventory == "armor" then return defines.inventory.character_armor end

    elseif type == "beacon" and inventory == "modules" then
        return defines.inventory.beacon_modules

    elseif type == "assembling-machine" or type == "furnace" then
        if inventory == "input" then return defines.inventory.crafter_input end
        if inventory == "output" then return defines.inventory.crafter_output end
        if inventory == "modules" then return defines.inventory.crafter_modules end

    elseif type == "mining-drill" and inventory == "modules" then
        return defines.inventory.mining_drill_modules
    end

    return nil
end

-- ---@class PutTakeRequestData
-- ---@field position MapPosition.0
-- ---@field entity string
-- ---@field item string
-- ---@field amount int
-- ---@field player_inventory defines.inventory
-- ---@field entity_inventory defines.inventory
-- ---@field force boolean

---@param request Request<{ entity: string, position: MapPosition }>
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

    return entity.bounding_box, player.reach_distance
end

-- ---@param request Request<PutTakeRequestData>
-- Instruction.AddRangedRequest({"Put", "Take"}, function (request)
--     local data = request.data

--     local player = game.get_player(1) --[[@as LuaPlayer]]
--     local entity = player.surface.find_entity(request.data.entity, request.data.position)
--     if not entity then
--         API.Failed(request, RequestError.NO_ENTITY_FOUND)
--         return
--     end

--     local from_inventory = player.get_inventory(data.player_inventory)
--     local to_inventory = entity.get_inventory(data.entity_inventory)
--     if request.name == "Take" then
--         from_inventory, to_inventory = to_inventory, from_inventory
--     end

--     if not from_inventory or not to_inventory then
--         API.Failed(request, RequestError.NO_INVENTORY_FOUND)
--         return
--     end

--     local item_count = from_inventory.get_item_count(data.item)
--     local amount = data.amount
--     if amount < 0 then
--         amount = item_count + amount
--     end

--     if math.abs(data.amount) > item_count then
--         if data.force then
--             amount = item_count
--         else
--             API.Failed(request, RequestError.NOT_ENOUGH_ITEM)
--             return
--         end
--     end

--     local inserted = to_inventory.insert({ name = data.item, count = amount })
--     from_inventory.remove({ name = data.item, count = inserted })

--     API.Success(request, inserted)
-- end, getAreaReachEntity)

---@param request Request<{ entity: string, position: MapPosition, inventory: string }>
Instruction.AddRangedRequest({"PutAll", "TakeAll"}, function (request)
    local data = request.data

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local entity = player.surface.find_entity(request.data.entity, request.data.position)
    if not entity then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    local inventory = ToGameInventory(entity, data.inventory)
    if not inventory then
        API.Failed(request, RequestError.NO_INVENTORY_FOUND)
    end
    ---@cast inventory - nil

    local from_inventory = player.get_main_inventory()
    local to_inventory = entity.get_inventory(inventory)
    local prefix = "-"
    if request.name == "TakeAll" then
        from_inventory, to_inventory = to_inventory, from_inventory
        prefix = "+"
    end

    if not from_inventory or not to_inventory then
        API.Failed(request, RequestError.NO_INVENTORY_FOUND)
        return
    end

    local inserted = {}

    for _, item in pairs(from_inventory.get_contents()) do
        local c = to_inventory.insert(item --[[@as ItemStackDefinition]])
        inserted[item.name] = c
        item.count = c
        from_inventory.remove(item --[[@as ItemStackDefinition]])
    end

    local text = {""}
    for name, count in pairs(inserted) do
        local total_count = player.get_main_inventory().get_item_count(name)
        table.insert(text, {"", prefix, count, " [item=", name, "] ", {"item-name." .. name}, " (", total_count, ")\n"})
    end
    Utils.CreateFlyingText(data.position, text)

    API.Success(request, inserted and inserted or nil)
end, getAreaReachEntity)