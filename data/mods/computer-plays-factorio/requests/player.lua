local API = require("__computer-plays-factorio__.api")

API.AddRequestHandler("PlayerPosition", function (request)
    local player = game.get_player(1)
    API.Success(request, player and player.position or nil)
end)

API.AddRequestHandler("GetInventory", function (request)
    local player = game.get_player(1) --[[@as LuaPlayer]]
    local inventory = player.get_main_inventory() --[[@as LuaInventory]]

    local items = {}
    for _, item in pairs(inventory.get_contents()) do
        items[item.name] = item.count
    end
    API.Success(request, {
        items = items,
        size = #inventory
    })
end)