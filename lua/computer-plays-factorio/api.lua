local Serializer = require("serializer")

---@class API
local API = {}

---@class RequestHandler
---@field fn fun(request: Request)
---@field dataType string
---@field responseType string

---@type table<int, RequestHandler>
local requestHandlers = {}

---@param requestName int
---@param handler fun(request: Request)
function API.AddRequestHandler(requestName, handler, dataType, responseType)
    assert(requestName, handler)
    if game or data then error("You can only register request handlers during the control life cycle") end

    if requestHandlers[requestName] ~= nil then
        error("A request handler for the request name " .. requestName .. " has already been registered")
    end

    requestHandlers[requestName] = {
        fn = handler,
        dataType = dataType,
        responseType = responseType
    }
end

---@param requestName int
---@return RequestHandler
function API.GetRequestHandler(requestName)
    return assert(requestName and requestHandlers[requestName], "No handler for the request name " .. requestName .. " has been registered")
end

---@class Request<T, R> : { id: int, name: RequestName, data: T, responseType: string, Response: fun(self:Request<T, R>, data:R)}

---@generic T, R
---@param request Request<T, R>
---@param data R
function API.Respond(request, data)
    TypeCheck(data, self.responseType)
    d = helpers.table_to_json(data)

    r = { id = self.id, data = d}
end

commands.add_command("request", nil, function (data)
    if data.player_index ~= nil then
        game.get_player(data.player_index).print("You cannot use this command")
        return
    end

    if data.parameter == nil then return end

    local d = helpers.json_to_table(data.parameter)
    if not d then
        error("JSON could not be parsed into table: " .. data.parameter)
    end

    TypeCheck(d) -- name
    local handler = API.GetRequestHandler(d.name)

    TypeCheck(d, handler.dataType)
    handler.fn(Request.new(d.id, d.type, d.data))
end)

function API.InvokeEvent()

end

return API