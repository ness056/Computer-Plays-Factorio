local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector

---@param event EventData.on_chunk_generated
Event.OnEvent(defines.events.on_chunk_generated, function (event)
    API.InvokeEvent("UpdateChunkColliders", event.position)

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
                name = entity.name,
                position = entity.position,
                boundingBox = entity.bounding_box,
                collidesWithPlayer = entity.prototype.collision_mask.layers["player"] or false,
                valid = true
            })
        end
    end

    API.InvokeEvent("UpdateEntities", filtered_entities)
end)

---@param entity LuaEntity
local function UpdateEntityDataBuilt(entity)
    API.InvokeEvent("UpdateEntities", {{
        name = entity.name,
        position = entity.position,
        boundingBox = entity.bounding_box,
        collidesWithPlayer = entity.prototype.collision_mask.layers["player"] or false,
        valid = true
    }})
end

---@param event EventData.on_built_entity
Event.OnEvent(defines.events.on_built_entity, function (event)
    UpdateEntityDataBuilt(event.entity)
end)

---@param event EventData.on_entity_cloned
Event.OnEvent(defines.events.on_entity_cloned, function (event)
    UpdateEntityDataBuilt(event.destination)
end)

---@param event EventData.on_robot_built_entity
Event.OnEvent(defines.events.on_robot_built_entity, function (event)
    UpdateEntityDataBuilt(event.entity)
end)

---@param entity LuaEntity
local function UpdateEntityDataDestroy(entity)
    API.InvokeEvent("UpdateEntities", {{
        name = entity.name,
        position = entity.position,
        boundingBox = entity.bounding_box,
        collidesWithPlayer = entity.prototype.collision_mask.layers["player"] or false,
        valid = false
    }})
end

---@param event EventData.on_player_mined_entity
Event.OnEvent(defines.events.on_player_mined_entity, function (event)
    UpdateEntityDataDestroy(event.entity)
end)

---@param event EventData.on_robot_mined_entity
Event.OnEvent(defines.events.on_robot_mined_entity, function (event)
    UpdateEntityDataDestroy(event.entity)
end)

---@param event EventData.on_entity_died
Event.OnEvent(defines.events.on_entity_died, function (event)
    UpdateEntityDataDestroy(event.entity)
end)

local function EvaluatePath()
    local player = game.get_player(1) --[[@as LuaPlayer]]

    local walkingState = player.walking_state
    if not storage.walkRequest then
        walkingState.walking = false
        player.walking_state = walkingState
        return
    end

    local path = storage.walkRequest.data
    local waypoint = path[storage.currentWaypoint]
    local v = Vector.Sub(waypoint.position, player.character.position)

    local speed = player.character.character_running_speed
    if Vector.SqLength(v) <= math.pow(speed, 2) then
        storage.currentWaypoint = storage.currentWaypoint + 1

        if storage.currentWaypoint < table_size(path) then
            EvaluatePath()
        else
            API.Success(storage.walkRequest)
            storage.walkRequest = nil
            walkingState.walking = false
            player.walking_state = walkingState
        end

        return
    end

    local halfSpeed = speed / 2
    if v.x > halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.southeast

    elseif v.x > halfSpeed and v.y < -halfSpeed then
        walkingState.direction = defines.direction.northeast

    elseif v.x < -halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.southwest

    elseif v.x < -halfSpeed and v.y < -halfSpeed then
        walkingState.direction = defines.direction.northwest

    elseif v.x > halfSpeed and math.abs(v.y) < halfSpeed then
        walkingState.direction = defines.direction.east

    elseif v.x < -halfSpeed and math.abs(v.y) < halfSpeed then
        walkingState.direction = defines.direction.west

    elseif math.abs(v.x) < halfSpeed and v.y > halfSpeed then
        walkingState.direction = defines.direction.south

    else
        walkingState.direction = defines.direction.north
    end

    walkingState.walking = true
    player.walking_state = walkingState
end

---@param request Request<PathfinderWaypoint[]>
API.AddRequestHandler("Walk", function (request)
    if storage.walkRequest then
        API.Failed(request, RequestError.BUSY)
        return
    end

    if table_size(request.data) == 0 then
        API.Failed(request, RequestError.EMPTY_PATH)
        return
    end

    storage.walkRequest = request
    storage.currentWaypoint = 1
    EvaluatePath()
end)

Event.OnEvent(defines.events.on_tick, EvaluatePath)