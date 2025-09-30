local Instruction = require("__computer-plays-factorio__.instruction")
local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Area = Math2d.Area

---@param request Request<{ position: MapPosition.0, name: string, direction: defines.direction }>
---@return BoundingBox?, number?
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
    local box = prototypes.entity[request.data.name].collision_box
    return Area.Add(box, request.data.position), player.build_distance
end

---@param request Request<{ position: MapPosition.0, name: string, direction: defines.direction, underground_type: string? }>
Instruction.AddRangedRequest("Build", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local data = request.data

    if player.get_item_count(data.name) == 0 then
        API.Failed(request, RequestError.NOT_ENOUGH_ITEM)
        return
    end

    if not player.can_place_entity{ name = data.name, position = data.position, direction = data.direction } then
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
        type = data.underground_type ~= "" and data.underground_type or nil
    }
    player.remove_item{ name = data.name, count = 1 }

    API.Success(request)
end, getAreaBuild)

---@param request Request<MapPosition.0>
---@return BoundingBox?, number?
local function getAreaMine(request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    player.update_selected_entity(request.data)
    if not player.selected or not player.selected.valid then
        API.Failed(request, RequestError.NO_ENTITY_FOUND)
        return
    end

    return player.selected.bounding_box, player.reach_distance
end

local function MineUpdate()
    if not storage.mine_request then return end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    player.update_selected_entity(storage.mine_request.data)

    player.mining_state = { mining = true, position = storage.mine_request.data }
end

---@param request Request<MapPosition.0>
Instruction.AddRangedRequest("Mine", function (request)
    storage.mine_request = request
    MineUpdate()
end, getAreaMine)

Event.OnEvent(defines.events.on_tick, function (event)
    MineUpdate()
end)

---@param event EventData.on_player_mined_entity
Event.OnEvent(defines.events.on_player_mined_entity, function (event)
    if not storage.mine_request then return end

    local items = {}
    for k, v in pairs(event.buffer.get_contents()) do
        items[v.name] = v.count
    end

    API.Success(storage.mine_request, items)
    storage.mine_request = nil
    local player = game.get_player(1) --[[@as LuaPlayer]]
    player.selected = nil
    player.mining_state = { mining = false }
end)

---@param request Request<{ position: MapPosition.0, entity: string }>
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

    if not entity.rotatable then
        API.Failed(request, RequestError.ENTITY_NOT_ROTATABLE)
        return
    end

    return Area.Add(entity.bounding_box, request.data.position), player.reach_distance
end

---@param request Request<{ position: MapPosition.0, entity: string, reversed: boolean }>
Instruction.AddRangedRequest("Rotate", function (request)

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local entity = player.surface.find_entity(request.data.entity, request.data.position) --[[@as LuaEntity]]

    entity.rotate{ reverse = request.data.reversed, by_player = player }

    API.Success(request, entity.direction)
end, getAreaReachEntity)

---@param request Request<EntitySearchFilters>
API.AddRequestHandler("FindEntitiesFiltered", function (request)
    local surface = game.get_surface(1) --[[@as LuaSurface]]
    if #request.data.type == 0 then request.data.type = nil end
    if #request.data.name == 0 then request.data.name = nil end
    local entities = surface.find_entities_filtered(request.data)
    local r = {}

    for k, entity in pairs(entities) do
        table.insert(r, {
            type = entity.type,
            name = entity.name,
            position = entity.position,
            direction = entity.direction
        })
    end

    API.Success(request, r)
end)