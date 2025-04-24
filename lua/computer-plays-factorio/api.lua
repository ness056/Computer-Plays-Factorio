local Serializer = require("serializer")

---@class API
local API = {}

---@class RequestHandler
---@field fn fun(request: Request<any>)
---@field dataType CppType | string
---@field responseType CppType | string

---@type table<int, RequestHandler>
local requestHandlers = {}

---@param requestName int
---@param handler fun(request: Request<any>)
---@param dataType CppType | string
---@param responseType CppType | string
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
    return assert(requestName and requestHandlers[requestName], "No handler for the request name " .. (requestName and requestName or "nil") .. " has been registered")
end

---@alias Request<T> { id: int, name: RequestName, data: T, responseType: CppType | string }

---@generic T
---@param request Request<T>
---@param data any
function API.Respond(request, data)
    print("API.Respond TODO")
end

commands.add_command("request", nil, function (data)
    print("testtest")
    if data.player_index ~= nil then
        game.get_player(data.player_index).print("You cannot use this command")
        return
    end

    if data.parameter == nil then return end

    local d = Serializer.Deserialize(data.parameter or "", 0, { name = "Request", template = {""}})
    for k, v in pairs(d) do
        print(k, v)
    end
    print(d.name)
    local handler = API.GetRequestHandler(d.name)

    d = Serializer.Deserialize(data.parameter, 0, {name = "Request", template = {handler.dataType}})
    handler.fn({
        id = d.id,
        name = d.name,
        responseType = handler.responseType,
        data = d.data
    } --[[@as Request<any>]])
end)

function API.InvokeEvent()

end

return API