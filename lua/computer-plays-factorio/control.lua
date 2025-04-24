local API = require("api")

API.AddRequestHandler(0, function (request)
    print(request.data)
end, {name = "Test", template = {"string"}}, "")