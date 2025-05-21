local API = require("__computer-plays-factorio__.api")

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