local API = require("api")
local Event = require("event")

API.AddRequestHandler("test", function (request)
    API.Success(request, request.data)
end)

API.AddRequestHandler("Broadcast", function (request)
    game.print(request.data)
end)

API.AddRequestHandler("GameSpeed", function (request)
    game.speed = request.data
end)

API.AddRequestHandler("Pause", function (request)
    game.tick_paused = not game.tick_paused
end)

API.AddRequestHandler("Save", function (request)
    game.auto_save(request.data)
end)