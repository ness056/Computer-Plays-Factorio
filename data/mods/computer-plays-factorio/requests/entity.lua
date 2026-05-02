local Instruction = require("__computer-plays-factorio__.instruction")
local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

---@param inventory LuaInventory
---@return table
local function GetInventoryContents(inventory)
    local items = {}
    local size = 0
    for _, item in pairs(inventory.get_contents()) do
        items[item.name] = item.count
        size = size + 1
    end

    return {
        items = size > 0 and items or nil,
        size = size
    }
end

---@param e LuaEntity
---@return table?
local function GetInventories(e)
    local t = {}

    ---@diagnostic disable param-type-mismatch
    if e.get_inventory(defines.inventory.fuel) then
        t["fuel"] = GetInventoryContents(e.get_inventory(defines.inventory.fuel))
    end

    ---@diagnostic disable param-type-mismatch
    if e.get_inventory(defines.inventory.chest) then
        t["main"] = GetInventoryContents(e.get_inventory(defines.inventory.chest))
    end

    local type = e.type
    if type == "character" then
        t["main"] = GetInventoryContents(e.get_inventory(defines.inventory.character_main))
        t["guns"] = GetInventoryContents(e.get_inventory(defines.inventory.character_guns))
        t["ammo"] = GetInventoryContents(e.get_inventory(defines.inventory.character_ammo))
        t["armor"] = GetInventoryContents(e.get_inventory(defines.inventory.character_armor))

    elseif type == "beacon" then
        t["modules"] = GetInventoryContents(e.get_inventory(defines.inventory.beacon_modules))

    elseif type == "assembling-machine" or type == "furnace" then
        t["input"] = GetInventoryContents(e.get_inventory(defines.inventory.crafter_input))
        t["output"] = GetInventoryContents(e.get_inventory(defines.inventory.crafter_output))
        t["modules"] = GetInventoryContents(e.get_inventory(defines.inventory.crafter_modules))

    elseif type == "mining-drill" then
        t["modules"] = GetInventoryContents(e.get_inventory(defines.inventory.mining_drill_modules))
    end
    ---@diagnostic enable param-type-mismatch

    return table_size(t) > 0 and t or nil
end

---@param e LuaEntity
---@return table
local function ToCppEntity(e)
    local type = e.type
    local t = {
        type = e.type,
        name = e.name,
        position = e.position,
        direction = e.direction,
        mirror = e.mirroring,
        inventories = GetInventories(e)
    }

    if type == "crafting-machine" then
        t.recipe = e.get_recipe().name
        t.crafting_progress = e.crafting_progress
    end

    if type == "underground-belt" then
        t.underground_type = e.belt_to_ground_type
    end

    if type == "lane-splitter" then
        t.input_priority = e.splitter_input_priority
        t.output_priority = e.splitter_output_priority
    end

    if type == "resource" then
        t.resource_amount = e.amount
    end

    if type == "mining-drill" then
        t.crafting_progress = e.mining_progress
    end

    return t
end

---@param event EventData.on_chunk_generated
Event.OnEvent(defines.events.on_chunk_generated, function (event)
    API.InvokeEvent("ChunkGenerated", event.position)

    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local area = event.area
    local entities = surface.find_entities_filtered{area = event.area}
    local filtered_entities = {}
    for k, entity in pairs(entities) do
        local pos = entity.position
        if area.left_top.x - pos.x <= 0 and pos.x - area.right_bottom.x < 0 and
           area.left_top.y - pos.y <= 0 and pos.y - area.right_bottom.y < 0 and
           entity.name ~= "character"
        then
            table.insert(filtered_entities, ToCppEntity(entity))
        end
    end

    API.InvokeEvent("EntityAutoPlace", filtered_entities)

    local tiles = surface.find_tiles_filtered{area = event.area}
    local t = {}
    for k, tile in pairs(tiles) do
        local collides = prototypes.tile[tile.name].collision_mask.layers["player"]
        if not collides then goto continue end

        table.insert(t, {
            tile.position,
            TileType.WATER,
            true
        })
    end

    API.InvokeEvent("SetTiles", t)
    ::continue::
end)

---@param request Request<{ position: MapPosition.0, name: string, direction: defines.direction }>
---@return BoundingBox?, number?, boolean?
local function getAreaBuild(request)
    if not prototypes.item[request.data.name] then
        API.Failed(request, RequestError.ITEM_DOESNT_EXIST)
        return
    end

    if not prototypes.item[request.data.name].place_result then
        API.Failed(request, RequestError.ITEM_NOT_BUILDABLE)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local proto = prototypes.entity[request.data.name]
    local box = proto.collision_box
    return Area.Add(box, request.data.position), player.build_distance, proto.collision_mask.layers["player"]
end

---Do NOT call this request directly. Use the Bot::Build function instead because it keeps the MapData object updated.
---@param request Request<{ position: MapPosition.0, name: string, direction: defines.direction, underground_type: string? }>
Instruction.AddRangedRequest("Build", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local data = request.data

    if player.get_item_count(data.name) == 0 then
        API.Failed(request, RequestError.NOT_ENOUGH_ITEM)
        return
    end

    if not player.can_place_entity{ name = data.name, position = data.position, direction = data.direction } then
        if table_size(storage.mine_requests) > 0 then
            return true
        end
        API.Failed(request, RequestError.NOT_ENOUGH_ROOM)
        return
    end

    local entity = prototypes.item[data.name].place_result --[[@as LuaEntityPrototype]]
    player.surface.create_entity{
        name = entity,
        position = data.position,
        direction = data.direction,
        force = player.force,
        player = player,
        type = data.underground_type ~= "" and data.underground_type or nil,
        raise_built = true
    }
    player.remove_item{ name = data.name, count = 1 }

    API.Success(request)
end, getAreaBuild)

---@param request Request<{name: string, position: MapPosition.0}>
---@return BoundingBox?, number?
local function getAreaMine(request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local entity = surface.find_entity(request.data.name, request.data.position)
    if not entity or not entity.valid then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    player.selected = entity
    return player.selected.bounding_box, player.reach_distance
end

local function MineUpdate()
    local player = game.get_player(1) --[[@as LuaPlayer]]

    if not storage.current_mine_request then
        for request, _ in pairs(storage.mine_requests) do
            local area, range = getAreaMine(request)
            if not area or not range then
                storage.mine_requests[request] = nil
                goto continue
            end

            if Area.SqDistanceTo(area, player.position) <= math.pow(range, 2) then
                storage.current_mine_request = request
            end
            ::continue::
        end
    end

    local request = storage.current_mine_request
    if not request then return end

    local surface = game.get_surface(1) --[[@as LuaSurface]]
    local entity = surface.find_entity(request.data.name, request.data.position)
    if not entity or not entity.valid then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    player.selected = entity
    player.mining_state = { mining = true, position = storage.current_mine_request.data.position }
end

---Do NOT call this request directly. Use the Bot::Mine function instead because it keeps the MapData object updated.
---@param request Request<{name: string, position: MapPosition.0}>
API.AddRequestHandler("Mine", function (request)
    storage.mine_requests[request] = true
    MineUpdate()
end)

Event.OnEvent(defines.events.on_tick, function (event)
    MineUpdate()
end)

---@param event EventData.on_player_mined_entity
Event.OnEvent(defines.events.on_player_mined_entity, function (event)
    if not storage.current_mine_request then return end

    local items = {}
    for k, v in pairs(event.buffer.get_contents()) do
        items[v.name] = v.count
    end

    API.Success(storage.current_mine_request, items)

    storage.mine_requests[storage.current_mine_request] = nil
    storage.current_mine_request = nil

    local player = game.get_player(1) --[[@as LuaPlayer]]
    player.clear_selected_entity()
    player.mining_state = { mining = false }
end)

---@param request Request<{ position: MapPosition.0, entity: string }>
---@return BoundingBox?, number?, boolean?
local function getAreaReachEntity(request)
    if not prototypes.entity[request.data.entity] then
        API.Failed(request, RequestError.ENTITY_DOESNT_EXIST)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local proto = prototypes.entity[request.data.entity]
    local bounding_box = Area.Add(proto.collision_box, request.data.position)
    return bounding_box, player.reach_distance, proto.collision_mask.layers["player"]
end

---@type { [string]: fun(entity: LuaEntity, value: any) }
local entity_property_setters = {
    ["recipe"] = function (entity, value) entity.set_recipe(value) end
}

---Do NOT call this request directly. Use the Bot::SetEntityProperty function instead because it keeps the MapData object updated.
---@param request Request<{ entity: string, position: MapPosition.0, property: string, value: any }>
Instruction.AddRangedRequest("SetEntityProperty", function (request)
    local data = request.data
    local entity = game.get_surface(1).find_entity(data.entity, data.position)
    if not entity or not entity.valid then
        return true
    end
    ---@cast entity - nil

    local setter = entity_property_setters[data.property]
    if setter then
        setter(entity, data.value)
    else
        entity[data.property] = data.value
    end

    API.Success(request)
end, getAreaReachEntity)

---@param request Request<{ entity: string, position: MapPosition.0 }>
API.AddRequestHandler("FetchEntityProperties", function (request)
    local data = request.data
    local entity = game.get_surface(1).find_entity(data.entity, data.position)
    if not entity or not entity.valid then
        API.Failed(request, RequestError.ENTITY_DOESNT_EXIST)
    end
    ---@cast entity - nil

    API.Success(request, ToCppEntity(entity))
end)