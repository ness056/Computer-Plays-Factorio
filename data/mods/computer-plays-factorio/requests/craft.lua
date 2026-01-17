local API = require("__computer-plays-factorio__.api")
local Event = require("__computer-plays-factorio__.event")

---@alias CraftTree { recipe: string, amount: int, id: int, subCrafts: { [string]: CraftTree } }

---@param request Request<string>
API.AddRequestHandler("CraftableAmount", function (request)
    local data = request.data

    local prototype = prototypes.recipe[data]
    if not prototype then
        API.Failed(request, RequestError.RECIPE_DOESNT_EXIST)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    API.Success(request, player.get_craftable_count(data))
end)

---@param request Request<{ recipe: string, amount: int, force: boolean }>
API.AddRequestHandler("Craft", function(request)
    log(request.data.recipe)
    local data = request.data

    local prototype = prototypes.recipe[data.recipe]
    if not prototype then
        API.Failed(request, RequestError.RECIPE_DOESNT_EXIST)
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    local craftable = player.get_craftable_count(data.recipe)
    local amount = data.amount
    if amount > craftable then
        if data.force then
            amount = craftable
        else
            API.Failed(request, RequestError.NOT_ENOUGH_INGREDIENTS)
            return
        end
    end

    API.Success(request, player.begin_crafting{ recipe = data.recipe, count = amount, silent = true })
end)

-- API.AddRequestHandler("WaitCraft")
-- API.AddRequestHandler("Cancel")

---@param request Request<nil>
API.AddRequestHandler("CraftingTimeRemaining", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local duration = 0

    if player.crafting_queue then
        for i, item in pairs(player.crafting_queue) do
            local craft_time = prototypes.recipe[item.recipe].energy * 60

            duration = duration + craft_time * item.count

            if i == 1 then
                duration = duration - craft_time * player.crafting_queue_progress
            end
        end
    end

    API.Success(request, duration)
end)

Event.OnInit(function ()
    ---@type Request<nil>[]
    storage.wait_crafting_queue_requests = {}
    storage.wait_crafting_queue_done = false
end)

---@param event EventData.on_player_crafted_item
Event.OnEvent(defines.events.on_player_crafted_item, function (event)
    if event.player_index ~= 1 then
        return
    end

    local player = game.get_player(1) --[[@as LuaPlayer]]
    if player.crafting_queue_size ~= 1 or player.crafting_queue[1].count ~= 1 then
        return
    end

    storage.wait_crafting_queue_done = true
end)

Event.OnEvent(defines.events.on_tick, function (event)
    if (storage.wait_crafting_queue_done) then
        for _, request in pairs(storage.wait_crafting_queue_requests) do
            API.Success(request)
        end

        storage.wait_crafting_queue_done = false
        storage.wait_crafting_queue_requests = {}
    end
end)

---@param request Request<nil>
API.AddRequestHandler("WaitCraftingQueue", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    if player.crafting_queue_size == 0 then
        API.Success(request)
    else
        table.insert(storage.wait_crafting_queue_requests, request)
    end
end)