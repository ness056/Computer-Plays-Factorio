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

commands.add_command("profile_command", "Same as /c but measures the time to execute the lua command", function (c)
    local printFun = game.print
    if game.player then
        printFun = game.player.print
    end

    local fun, compileError = load(c.parameter, nil, "t")
    if fun then
        local profiler = helpers.create_profiler()
        local success, callError = pcall(fun)
        profiler.stop()
        if success then
            printFun({"", "Measured time: ", profiler})
        else
            printFun({"", "Run time error: ", callError})
        end
    else
        printFun({"", "Compiling error: ", compileError})
    end
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