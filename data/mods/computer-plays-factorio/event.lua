local API = require("__computer-plays-factorio__.api")

---@class Event
local Event = {}

---@type { [defines.events]: fun(event: EventData)[] }
local event_handlers = {}

---@type fun()[]
local init_handlers = {}

---@type { [uint]: fun(event: NthTickEventData)[] }
local nth_tick_handlers = {}

local function ControlStageCheck()
    if game or data then API.Throw("You can only register event handlers during the control life cycle") end
end

script.on_init(function ()
    for k, handler in ipairs(init_handlers) do
        local status, err = pcall(function ()
            handler()
        end)
        if not status then API.Throw(tostring(err)) end
    end
end)

---@param handler fun()
function Event.OnInit(handler)
    ControlStageCheck()
    table.insert(init_handlers, handler)
end

---@param event defines.events
---@param handler fun(event: EventData)
function Event.OnEvent(event, handler)
    ControlStageCheck()
    if event_handlers[event] == nil then
        event_handlers[event] = {}

        script.on_event(event, function (e)
            for k, handler_ in ipairs(event_handlers[event]) do
                local status, err = pcall(function ()
                    handler_(e)
                end)
                if not status then API.Throw(tostring(err)) end
            end
        end)
    end

    table.insert(event_handlers[event], handler)
end

---@param tick uint
---@param handler fun(event: NthTickEventData)
function Event.OnNthTick(tick, handler)
    ControlStageCheck()
    if nth_tick_handlers[tick] == nil then
        nth_tick_handlers[tick] = {}

        script.on_nth_tick(tick, function (e)
            for k, handler_ in ipairs(nth_tick_handlers[tick]) do
                local status, err = pcall(function ()
                    handler_(e)
                end)
                if not status then API.Throw(tostring(err)) end
            end
        end)
    end

    table.insert(nth_tick_handlers[tick], handler)
end

return Event