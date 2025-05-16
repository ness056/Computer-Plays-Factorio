---@class Event
local Event = {}

---@type { [defines.events]: fun(event: EventData)[] }
local eventHandlers = {}

---@type fun()[]
local initHandlers = {}

---@type { [uint]: fun(event: NthTickEventData)[] }
local nthTickHandlers = {}

local function ControlStageCheck()
    if game or data then error("You can only register event handlers during the control life cycle") end
end

script.on_init(function ()
    for k, handler in ipairs(initHandlers) do
        handler()
    end
end)

---@param handler fun()
function Event.OnInit(handler)
    ControlStageCheck()
    table.insert(initHandlers, handler)
end

---@param event defines.events
---@param handler fun(event: EventData)
function Event.OnEvent(event, handler)
    ControlStageCheck()
    if eventHandlers[event] == nil then
        eventHandlers[event] = {}

        script.on_event(event, function (e)
            for k, handler_ in ipairs(eventHandlers[event]) do
                handler_(e)
            end
        end)
    end

    table.insert(eventHandlers[event], handler)
end

---@param tick uint
---@param handler fun(event: NthTickEventData)
function Event.OnNthTick(tick, handler)
    ControlStageCheck()
    if nthTickHandlers[tick] == nil then
        nthTickHandlers[tick] = {}

        script.on_nth_tick(tick, function (e)
            for k, handler_ in ipairs(nthTickHandlers[tick]) do
                handler_(e)
            end
        end)
    end

    table.insert(nthTickHandlers[tick], handler)
end

return Event