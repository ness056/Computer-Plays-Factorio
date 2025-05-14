---@class API
local API = {}

---@alias RequestHandler fun(request: Request<any>)

---@type table<string, RequestHandler>
local requestHandlers = {}

---@enum RequestName
API.RequestName = {
    ["TEST_REQUEST"] = 0
}

---@param requestName string
---@param handler RequestHandler
function API.AddRequestHandler(requestName, handler)
    assert(requestName, handler)
    if game or data then error("You can only register request handlers during the control life cycle") end

    if requestHandlers[requestName] ~= nil then
        error("A request handler for the request name " .. requestName .. " has already been registered")
    end

    requestHandlers[requestName] = handler
end

---@param requestName string
---@return RequestHandler
function API.GetRequestHandler(requestName)
    return assert(requestName and requestHandlers[requestName], "No handler for the request name " .. (requestName and requestName or "nil") .. " has been registered")
end

---@alias Request<T> { id: int, name: RequestName, data: T }

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
---@param message string
function API.Failed(request, message)
    local d = helpers.table_to_json({
        id = request.id,
        success = false,
        message = message
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