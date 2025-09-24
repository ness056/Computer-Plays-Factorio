local Utils = {}

local API = require("__computer-plays-factorio__.api")

commands.add_command("reload", "", function (c)
    game.reload_mods()
    game.print("Reloaded")
end)

commands.add_command("log-storage", "", function (c)
    log(serpent.block(storage))
end)

---@param fn fun(...)
function Utils.Profile(fn, ...)
    local profiler = helpers.create_profiler()
    local success, callError = pcall(fn, ...)
    profiler.stop()
    if success then
        log({"", "Measured time: ", profiler})
        game.print({"", "Measured time: ", profiler})
    else
        log({"", "Run time error: ", callError})
        game.print({"", "Run time error: ", callError})
    end
end

commands.add_command("profile_command", "Same as /c but measures the time to execute the lua command", function (c)
    local fun, compile_error = load(c.parameter, nil, "t")
    if fun then
        Utils.Profile(fun)
    else
        log({"", "Compiling error: ", compile_error})
        game.print({"", "Compiling error: ", compile_error})
    end
end)

commands.add_command("export_pathfinder_data", "", function (c)
    API.InvokeEvent("ExportPathfinderData");
end)

---@param request Request<string>
API.AddRequestHandler("Broadcast", function (request)
    game.print(request.data)
    API.Success(request)
end)

---@param request Request<float>
API.AddRequestHandler("GameSpeed", function (request)
    game.speed = request.data
    API.Success(request)
end)

API.AddRequestHandler("PauseToggle", function (request)
    game.tick_paused = not game.tick_paused
    API.Success(request)
end)

API.AddRequestHandler("Save", function (request)
    game.auto_save(request.data)
    API.Success(request)
end)

API.AddRequestHandler("PlayerPosition", function (request)
    local player = game.get_player(1)
    API.Success(request, player and player.position or nil)
end)

return Utils