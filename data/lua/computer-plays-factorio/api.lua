---@class API
local API = {}

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

---@enum RequestErrorCode
RequestErrorCode = {
    BUSY = 1,
    NO_PATH_FOUND = 2,
    EMPTY_PATH = 3,
    NOT_ENOUGH_ITEM = 4,
    NOT_ENOUGH_ROOM = 5,
    ENTITY_DOESNT_EXIST = 6,
    NOT_IN_RANGE = 7
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
---@param error RequestErrorCode
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

return API