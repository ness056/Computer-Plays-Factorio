local json = require("json")

data = json.encode(data.raw)
log("DATA" .. data:len() .. " " .. data)
error("Data scraper")