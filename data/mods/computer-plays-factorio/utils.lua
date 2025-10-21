local Utils = {}

local API = require("__computer-plays-factorio__.api")
local Math2d = require("__computer-plays-factorio__.math2d")
local Vector = Math2d.Vector

---@generic T
---@param table T[]
---@param value T
---@param cmp fun(a: T, b: T): number -- Comparator function, returns negative if a < b, positive if a > b, zero if a == b
---@return integer, boolean -- Index and whether the value was found
function Utils.BinarySearch(table, value, cmp)
    local low = 1
    local high = #table
    while low <= high do
        local mid = math.floor((low + high) / 2)
        local mid_value = table[mid]
        local comparison = cmp(mid_value, value)
        if comparison < 0 then
            low = mid + 1
        elseif comparison > 0 then
            high = mid - 1
        else
            return mid, true
        end
    end
    return low, false
end

commands.add_command("reload", "", function (c)
    game.reload_mods()
    game.print("Reloaded")
end)

commands.add_command("log-storage", "", function (c)
    log(serpent.block(storage))
    log("ranged requests: ")
    for request, _ in pairs(storage.ranged_requests) do
        log(serpent.line(request))
    end
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

commands.add_command("draw_pathfinder_data", "", function (c)
    API.InvokeEvent("DrawPathfinderData", tonumber(c.parameter));
end)

commands.add_command("draw_patchs", "", function (c)
    API.InvokeEvent("DrawPatchs");
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

---@param request Request<{position: MapPosition.0, radius: number, color: Color, filled: boolean}>
API.AddRequestHandler("DrawCircle", function (request)
    local data = request.data
    rendering.draw_circle{ surface=1, target=data.position, radius=data.radius, color=data.color, filled=data.filled }
    API.Success(request)
end)

---@param request Request<{positions: MapPosition.0[], radius: number, color: Color, filled: boolean}>
API.AddRequestHandler("DrawCircleBulk", function (request)
    local data = request.data
    for k, position in pairs(data.positions) do
        rendering.draw_circle{
            surface=1,
            target=position,
            radius=data.radius,
            color=data.color,
            filled=data.filled
        }
    end
    API.Success(request)
end)

---@param request Request<{area: BoundingBox.0, color: Color, filled: boolean}>
API.AddRequestHandler("DrawRectangle", function (request)
    local data = request.data
    rendering.draw_rectangle{ surface=1, left_top=data.area.left_top, right_bottom=data.area.right_bottom, color=data.color, filled=data.filled }
    API.Success(request)
end)

---@param request Request<{areas: BoundingBox.0[]?, positions: MapPosition.0[]?, side_length: number?, color: Color, filled: boolean}>
API.AddRequestHandler("DrawRectangleBulk", function (request)
    local data = request.data
    data.side_length = data.side_length / 2
    if data.areas then
        for k, area in pairs(data.areas) do
            rendering.draw_rectangle{ surface=1, left_top=area.left_top, right_bottom=area.right_bottom, color=data.color, filled=data.filled }
        end
    else
        for k, position in pairs(data.positions) do
            rendering.draw_rectangle{
                surface=1,
                left_top=Vector.Add(position, {-data.side_length, -data.side_length}),      ---@diagnostic disable-line
                right_bottom=Vector.Add(position, {data.side_length, data.side_length}),    ---@diagnostic disable-line
                color=data.color,
                filled=data.filled
            }
        end
    end
    API.Success(request)
end)

return Utils