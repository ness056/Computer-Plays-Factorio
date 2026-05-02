require("__computer-plays-factorio__.freeplay")
require("__computer-plays-factorio__.utils")
require("__computer-plays-factorio__.instruction")
require("__computer-plays-factorio__.requests.player")
require("__computer-plays-factorio__.requests.walk")
require("__computer-plays-factorio__.requests.craft")
require("__computer-plays-factorio__.requests.entity")
require("__computer-plays-factorio__.requests.item")

local Event = require("__computer-plays-factorio__.event")
local API = require("__computer-plays-factorio__.api")

Event.OnEvent(defines.events.on_tick, function (event)
    if game.tick == 1 then
        API.InvokeEvent("Ready")
    end
end)

---@param event EventData.script_raised_built
Event.OnEvent(defines.events.script_raised_built, function (event)
    local inventory = event.entity.get_inventory(defines.inventory.fuel)
    if inventory then
        inventory.insert("coal")
    end
end)