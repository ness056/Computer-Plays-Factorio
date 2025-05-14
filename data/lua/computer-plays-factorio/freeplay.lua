local Event = require("event")
local util = require("util")
local crash_site = require("crash-site")

Event.OnInit(function ()
    if not remote.interfaces.freeplay then return end

    remote.call("freeplay", "set_disable_crashsite", true)
    remote.call("freeplay", "set_skip_intro", true)
end)

---@param event EventData.on_player_created
Event.OnEvent(defines.events.on_player_created, function (event)
    if event.player_index ~= 1 or not remote.interfaces.freeplay then return end
    local player = game.get_player(event.player_index) --[[@as LuaPlayer]]

    local crashed_ship_items = remote.call("freeplay", "get_ship_items")
    local crashed_debris_items = remote.call("freeplay", "get_debris_items")

    local surface = player.surface
    surface.daytime = 0.7
    crash_site.create_crash_site(surface, { -5, -6 }, util.copy(crashed_ship_items),
        util.copy(crashed_debris_items))
    util.remove_safe(player, crashed_ship_items)
    util.remove_safe(player, crashed_debris_items)
    player.get_main_inventory().sort_and_merge()
end)