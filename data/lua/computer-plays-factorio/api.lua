---@class API
local API = {}

local Event = require("__computer-plays-factorio__.event")

---@alias RequestHandler fun(request: Request<any>)

---@type table<string, RequestHandler>
local requestHandlers = {}

---@param requestName string | string[]
---@param handler RequestHandler
function API.AddRequestHandler(requestName, handler)
    assert(requestName, handler)
    if game or data then error("You can only register request handlers during the control life cycle") end

    if type(requestName) == "table" then
        for i, n in ipairs(requestName) do
            API.AddRequestHandler(n, handler)
        end
    else
        if requestHandlers[requestName] ~= nil then
            error("A request handler for the request name " .. requestName .. " has already been registered")
        end
        requestHandlers[requestName] = handler
    end

end

---@param requestName string
---@return RequestHandler
function API.GetRequestHandler(requestName)
    return assert(requestName and requestHandlers[requestName], "No handler for the request name " .. (requestName and requestName or "nil") .. " has been registered")
end

---@enum RequestError
RequestError = {
    BUSY = 1,                       -- An instruction is already being executed
    NO_PATH_FOUND = 2,              -- Pathfinder failed to find a path
    EMPTY_PATH = 3,                 -- The given path is empty
    ENTITY_DOESNT_EXIST = 4,        -- No entity prototype with the given name exist
    ITEM_DOESNT_EXIST = 5,          -- No item prototype with the given name exist
    FLUID_DOESNT_EXIST = 6,         -- No fluid prototype with the given name exist
    RECIPE_DOESNT_EXIST = 7,        -- No recipe prototype with the given name exist
    NOT_ENOUGH_ITEM = 8,            -- Not enough item in the inventory
    NOT_ENOUGH_ROOM = 9,            -- The entity cannot be built because another entity is in the way
    NOT_IN_RANGE = 10,              -- The player is not in range
    ITEM_NOT_BUILDABLE = 11,        -- The given item prototype cannot be built in an entity
    NO_ENTITY_FOUND = 12,           -- No entity at the given position was found
    NO_INVENTORY_FOUND = 13,        -- The given entity or player does not have an inventory of the given index
    NOT_ENOUGH_INGREDIENTS = 14,    -- Not enough ingredients to craft
    ENTITY_NOT_ROTATABLE = 15,      -- The given entity is not rotatable
}

---@alias Request<T> { id: int, name: string, data: T }

---@generic T
---@param request Request<T>
---@param data any
function API.Success(request, data)
    local d = helpers.table_to_json({
        id = request.id,
        success = true,
        data = data
    })
    print("response" .. d:len() .. " " .. d)
    log(d)
end

---@generic T
---@param request Request<T>
---@param error RequestError
function API.Failed(request, error)
    local d = helpers.table_to_json({
        id = request.id,
        success = false,
        error = error
    })
    print("response" .. d:len() .. " " .. d)
    log(d)
end

commands.add_command("request", nil, function (data)
    if data.player_index ~= nil then
        game.get_player(data.player_index).print("You cannot use this command")
        return
    end

    if data.parameter == nil then return end

    local d = helpers.json_to_table(data.parameter)
    if type(d) ~= "table" then error("Invalid json: " .. data.parameter) end

    API.GetRequestHandler(d.name)({
        id = d.id,
        name = d.name,
        data = d.data
    } --[[@as Request<any>]])
end)

function API.InvokeEvent()

end

Event.OnInit(function ()
    storage.id = 0
end)

function API.GetId()
    storage.id = storage.id + 1
    return storage.id
end

return API