require("__computer-plays-factorio__.freeplay")
require("__computer-plays-factorio__.instruction")
require("__computer-plays-factorio__.requests.walk")
require("__computer-plays-factorio__.requests.craft")
require("__computer-plays-factorio__.requests.entity")
require("__computer-plays-factorio__.requests.item")
local API = require("__computer-plays-factorio__.api")

commands.add_command("reload", "", function (c)
    game.reload_mods()
    game.print("Reloaded")
end)

commands.add_command("log-storage", "", function (c)
    log(serpent.block(storage))
end)

---@param request Request<string>
API.AddRequestHandler("Broadcast", function (request)
    game.print(request.data)
end)

---@param request Request<float>
API.AddRequestHandler("GameSpeed", function (request)
    game.speed = request.data
end)

API.AddRequestHandler("PauseToggle", function (request)
    game.tick_paused = not game.tick_paused
end)

API.AddRequestHandler("Save", function (request)
    if (game.is_multiplayer()) then game.auto_save(request.data)
    else game.server_save(request.data) end
end)