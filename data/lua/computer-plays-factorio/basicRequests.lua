local API = require("api")

API.AddRequestHandler("Broadcast", function (request)
    game.print(request.data)
end)

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