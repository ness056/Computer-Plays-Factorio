local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector

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
            table.insert(filtered_entities, {
                type = entity.type,
                name = entity.name,
                position = entity.position,
                direction = entity.direction,
                bounding_box = entity.bounding_box,
            })
        end
    end

    API.InvokeEvent("AddEntities", filtered_entities)

    local tiles = surface.find_tiles_filtered{area = event.area}
    local t = {}
    for k, tile in pairs(tiles) do
        table.insert(t, {
            tile.position,
            prototypes.tile[tile.name].collision_mask.layers["player"] and TileType.WATER or TileType.NORMAL
        })
    end

    API.InvokeEvent("SetTiles", t)
end)

---@param entity LuaEntity
local function EntityBuilt(entity)
    API.InvokeEvent("AddEntities", {{
        name = entity.name,
        position = entity.position,
        direction = entity.direction,
        bounding_box = entity.bounding_box,
        collides_with_player = entity.prototype.collision_mask.layers["player"] or false,
        placeable_off_grid = entity.has_flag("placeable-off-grid")
    }})
end

---@param event EventData.on_built_entity
Event.OnEvent(defines.events.on_built_entity, function (event)
    EntityBuilt(event.entity)
end)

---@param event EventData.on_entity_cloned
Event.OnEvent(defines.events.on_entity_cloned, function (event)
    EntityBuilt(event.destination)
end)

---@param event EventData.on_robot_built_entity
Event.OnEvent(defines.events.on_robot_built_entity, function (event)
    EntityBuilt(event.entity)
end)

---@param entity LuaEntity
local function EntityDestroyed(entity)
    API.InvokeEvent("RemoveEntities", {{
        entity.name,
        entity.position
    }})
end

---@param event EventData.on_player_mined_entity
Event.OnEvent(defines.events.on_player_mined_entity, function (event)
    EntityDestroyed(event.entity)
end)

---@param event EventData.on_robot_mined_entity
Event.OnEvent(defines.events.on_robot_mined_entity, function (event)
    EntityDestroyed(event.entity)
end)

---@param event EventData.on_entity_died
Event.OnEvent(defines.events.on_entity_died, function (event)
    EntityDestroyed(event.entity)
end)

---@param event EventData.script_raised_set_tiles
Event.OnEvent(defines.events.script_raised_set_tiles, function (event)
    local t = {}
    for k, tile in pairs(event.tiles) do
        table.insert(t, {
            tile.position,
            prototypes.tile[tile.name].collision_mask.layers["player"] and TileType.WATER or TileType.NORMAL
        })
    end

    API.InvokeEvent("SetTiles", t)
end)

---@param pos MapPosition.0
---@param tileProto LuaTilePrototype
local function SetTile(pos, tileProto)
    API.InvokeEvent("SetTiles", {
        {
            pos,
            tileProto.collision_mask.layers["player"] and TileType.WATER or TileType.NORMAL
        }
    })
end

---@param event EventData.on_player_built_tile
Event.OnEvent(defines.events.on_player_built_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tile)
end)

---@param event EventData.on_robot_built_tile
Event.OnEvent(defines.events.on_robot_built_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tile)
end)

---@param event EventData.on_space_platform_built_tile
Event.OnEvent(defines.events.on_space_platform_built_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tile)
end)

---@param event EventData.on_player_mined_tile
Event.OnEvent(defines.events.on_player_mined_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tiles[1].old_tile)
end)

---@param event EventData.on_robot_mined_tile
Event.OnEvent(defines.events.on_robot_mined_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tiles[1].old_tile)
end)

---@param event EventData.on_space_platform_mined_tile
Event.OnEvent(defines.events.on_space_platform_mined_tile, function (event)
    SetTile(event.tiles[1].position --[[@as MapPosition.0]], event.tiles[1].old_tile)
end)

local function EvaluatePath()
    local player = game.get_player(1) --[[@as LuaPlayer]]

    local walking_state = player.walking_state
    if not storage.walk_request then
        walking_state.walking = false
        player.walking_state = walking_state
        return
    end

    local path = storage.walk_request.data
    local waypoint = path[storage.current_waypoint]
    local v = Vector.Sub(waypoint, player.character.position)

    local speed = player.character.character_running_speed
    if Vector.SqLength(v) <= math.pow(speed, 2) then
        storage.current_waypoint = storage.current_waypoint + 1

        if storage.current_waypoint <= table_size(path) then
            EvaluatePath()
        else
            API.Success(storage.walk_request)
            storage.walk_request = nil
            walking_state.walking = false
            player.walking_state = walking_state
        end

        return
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

    storage.walk_request = request
    storage.current_waypoint = 1
    EvaluatePath()
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)