---@class API
local API = {}

local Json = require("json")

---@alias RequestHandler fun(request: Request<any>)

---@type table<string, RequestHandler>
local request_handlers = {}

---@param request_name string | string[]
---@param handler RequestHandler
function API.AddRequestHandler(request_name, handler)
    assert(request_name, handler)
    if game or data then error("You can only register request handlers during the control life cycle") end

    if type(request_name) == "table" then
        for i, n in ipairs(request_name) do
            API.AddRequestHandler(n, handler)
        end
    else
        if request_handlers[request_name] ~= nil then
            error("A request handler for the request name " .. request_name .. " has already been registered")
        end
        request_handlers[request_name] = handler
    end

end

---@param request_name string
---@return RequestHandler
function API.GetRequestHandler(request_name)
    return assert(request_name and request_handlers[request_name], "No handler for the request name " .. (request_name and request_name or "nil") .. " has been registered")
end

---@enum RequestError
RequestError = {
    SUCCESS = 0,
    FACTORIO_NOT_RUNNING = 1,
    FACTORIO_EXITED = 2,
    BUSY = 3,                       -- An instruction is already being executed
    NOT_BUSY = 4,                   -- No instruction is being executed
    CANCELED = 5,
    NO_PATH_FOUND = 6,              -- Pathfinder failed to find a path
    EMPTY_PATH = 7,                 -- The given path is empty
    ENTITY_DOESNT_EXIST = 8,        -- No entity prototype with the given name exist
    ITEM_DOESNT_EXIST = 9,          -- No item prototype with the given name exist
    FLUID_DOESNT_EXIST = 10,        -- No fluid prototype with the given name exist
    RECIPE_DOESNT_EXIST = 11,       -- No recipe prototype with the given name exist
    NOT_ENOUGH_ITEM = 12,           -- Not enough item in the inventory
    NOT_ENOUGH_ROOM = 13,           -- The entity cannot be built because another entity is in the way
    NOT_IN_RANGE = 14,              -- The player is not in range
    ITEM_NOT_BUILDABLE = 15,        -- The given item prototype cannot be built in an entity
    NO_ENTITY_FOUND = 16,           -- No entity at the given position was found
    NO_INVENTORY_FOUND = 17,        -- The given entity or player does not have an inventory of the given index
    NOT_ENOUGH_INGREDIENTS = 18,    -- Not enough ingredients to craft
    ENTITY_NOT_ROTATABLE = 19       -- The given entity is not rotatable
}

---@enum TileType
TileType = {
    NORMAL = 0,
    WATER = 1
}

---@alias Request<T> { id: int, name: string, data: T }

---@param prefix string
---@param data string
local function SendData(prefix, data)
    print(prefix .. data:len() .. " " .. data)
end

---@generic T
---@param request Request<T>
---@param data any
function API.Success(request, data)
    local d = Json.encode({
        id = request.id,
        success = true,
        data = data,
        error = RequestError.SUCCESS
    })

    SendData("response", d)
end

---@generic T
---@param request Request<T>
---@param error RequestError
function API.Failed(request, error)
    local d = Json.encode({
        id = request.id,
        success = false,
        error = error
    })

    SendData("response", d)
end

commands.add_command("request", nil, function (data)
    local status, err = pcall(function()
        if data.player_index ~= nil then
            game.get_player(data.player_index).print("You cannot use this command")
            return
        end

        if data.parameter == nil then return end

        local d = helpers.json_to_table(data.parameter)
        if type(d) ~= "table" then error("Invalid json: " .. data.parameter) end

        API.GetRequestHandler(d.name)(d)
    end)

    if not status then
        API.Throw(tostring(err))
    end
end)

---@param name string
---@param data any
function API.InvokeEvent(name, data)
    local d = Json.encode({
        name = name,
        id = API.GetId(),
        tick = game.tick,
        data = data
    })

    SendData("event", d)
end

function API.GetId()
    if not storage.id then
        storage.id = 0
    end

    storage.id = storage.id + 1
    return storage.id
end

---Throws an exception in the cpp side
---@param message string
function API.Throw(message)
    API.InvokeEvent("Throw", debug.traceback(message))
end

return API