---@class API
local API = {}

local Event = require("__computer-plays-factorio__.event")
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
    SUCCESS = "SUCCESS",
    FACTORIO_NOT_RUNNING = "FACTORIO_NOT_RUNNING",
    FACTORIO_EXITED = "FACTORIO_EXITED",
    BUSY = "BUSY",                                      -- An instruction is already being executed
    NO_PATH_FOUND = "NO_PATH_FOUND",                    -- Pathfinder failed to find a path
    EMPTY_PATH = "EMPTY_PATH",                          -- The given path is empty
    ENTITY_DOESNT_EXIST = "ENTITY_DOESNT_EXIST",        -- No entity prototype with the given name exist
    ITEM_DOESNT_EXIST = "ITEM_DOESNT_EXIST",            -- No item prototype with the given name exist
    FLUID_DOESNT_EXIST = "FLUID_DOESNT_EXIST",          -- No fluid prototype with the given name exist
    RECIPE_DOESNT_EXIST = "RECIPE_DOESNT_EXIST",        -- No recipe prototype with the given name exist
    NOT_ENOUGH_ITEM = "NOT_ENOUGH_ITEM",                -- Not enough item in the inventory
    NOT_ENOUGH_ROOM = "NOT_ENOUGH_ROOM",                -- The entity cannot be built because another entity is in the way
    NOT_IN_RANGE = "NOT_IN_RANGE",                      -- The player is not in range
    ITEM_NOT_BUILDABLE = "ITEM_NOT_BUILDABLE",          -- The given item prototype cannot be built in an entity
    NO_ENTITY_FOUND = "NO_ENTITY_FOUND",                -- No entity at the given position was found
    NO_INVENTORY_FOUND = "NO_INVENTORY_FOUND",          -- The given entity or player does not have an inventory of the given index
    NOT_ENOUGH_INGREDIENTS = "NOT_ENOUGH_INGREDIENTS",  -- Not enough ingredients to craft
    ENTITY_NOT_ROTATABLE = "ENTITY_NOT_ROTATABLE",      -- The given entity is not rotatable
}

local direction_to_string = {
    [0] = "NORTH",
    [1] = "NORTH_NORTH_EAST",
    [2] = "NORTH_EAST",
    [3] = "EAST_NORTH_EAST",
    [4] = "EAST",
    [5] = "EAST_SOUTH_EAST",
    [6] = "SOUTH_EAST",
    [7] = "SOUTH_SOUTH_EAST",
    [8] = "SOUTH",
    [9] = "SOUTH_SOUTH_WEST",
    [10] = "SOUTH_WEST",
    [11] = "WEST_SOUTH_WEST",
    [12] = "WEST",
    [13] = "WEST_NORTH_WEST",
    [14] = "NORTH_WEST",
    [15] = "NORTH_NORTH_WEST"
}

---@param direction defines.direction
---@return string
function DirectionToString(direction)
    return direction_to_string[direction]
end

local string_to_direction = {
    ["NORTH"] = 0,
    ["NORTH_NORTH_EAST"] = 1,
    ["NORTH_EAST"] = 2,
    ["EAST_NORTH_EAST"] = 3,
    ["EAST"] = 4,
    ["EAST_SOUTH_EAST"] = 5,
    ["SOUTH_EAST"] = 6,
    ["SOUTH_SOUTH_EAST"] = 7,
    ["SOUTH"] = 8,
    ["SOUTH_SOUTH_WEST"] = 9,
    ["SOUTH_WEST"] = 10,
    ["WEST_SOUTH_WEST"] = 11,
    ["WEST"] = 12,
    ["WEST_NORTH_WEST"] = 13,
    ["NORTH_WEST"] = 14,
    ["NORTH_NORTH_WEST"] = 15
}

---@param direction string
---@return defines.direction
function StringToDirection(direction)
    return string_to_direction[direction] --[[@as defines.direction]]
end

---@alias Request<T> { id: int, name: string, data: T }

---@param prefix string
---@param data string
local function SendData(prefix, data)
    log(data)
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
    log(data.parameter)
    if data.player_index ~= nil then
        game.get_player(data.player_index).print("You cannot use this command")
        return
    end

    if data.parameter == nil then return end

    local d = helpers.json_to_table(data.parameter)
    log(serpent.line(d))
    if type(d) ~= "table" then error("Invalid json: " .. data.parameter) end

    API.GetRequestHandler(d.name)({
        id = d.id,
        name = d.name,
        data = d.data
    } --[[@as Request<any>]])
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

Event.OnInit(function ()
    storage.id = 0
end)

function API.GetId()
    storage.id = storage.id + 1
    return storage.id
end

return API