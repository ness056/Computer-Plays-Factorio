#include "bot.hpp"

#include "../factorioAPI/factorioData.hpp"

namespace ComputerPlaysFactorio {

    Bot::Bot() : m_instance(FactorioInstance::GRAPHICAL) {
        m_instance.SetExitedCallback([this](int) {
            Stop();
        });

        // This event is sent once every tick to notify the bot what entities have
        // been created or destroyed during that tick. If the "valid" member of an
        // entity is set to true, that entity was created, otherwise it was destroyed. 
        m_instance.RegisterEvent<std::vector<Entity>>("UpdateEntities", [this](const Event<std::vector<Entity>> &event) {
            for (const auto &entity : event.data) {
                if (entity.valid) {
                    m_mapData.AddEntity(entity);
                } else {
                    m_mapData.RemoveEntity(entity.name, entity.position);
                }
            }
        });

        m_instance.RegisterEvent<MapPosition>("UpdateChunkColliders", [this](const Event<MapPosition> &event) {
            m_mapData.ChunkGenerated(event.data);
        });

        m_lua = luaL_newstate();
        luaL_openlibs(m_lua);
        LuaCreateLightUDataMeta(m_lua);

        Exec(GetLuaPath() / "serpent.lua", false);
        lua_setglobal(m_lua, "serpent");

        lua_pushvalue(m_lua, LUA_REGISTRYINDEX);
        lua_setglobal(m_lua, "registry");
        
        LuaPushLightUserdata(m_lua, &m_eventManager);
        lua_setglobal(m_lua, "event");

        LuaPushLightUserdata(m_lua, this);
        lua_setglobal(m_lua, "bot");

        LuaPushLightUserdata(m_lua, &m_mapData);
        lua_setglobal(m_lua, "map");
    }

    Bot::~Bot() {
        lua_close(m_lua);
    }
   
    void Bot::Start() {
        std::string message;
        if (Running()) return;
        if (!m_instance.Start(123, &message)) return;

        m_exit = false;
        Loop();
    }
   
    void Bot::Stop() {
        ClearInstructions();
        m_exit = true;
        m_instance.Stop();
    }

    void Bot::Exec(std::string script, bool ignoreReturns) {
        auto top = lua_gettop(m_lua);
        if (luaL_dostring(m_lua, script.c_str()) != LUA_OK) {
            if (lua_isstring(m_lua, -1)) {
                Warn("Exec string error: {}", lua_tostring(m_lua, -1));
            } else {
                Warn("Exec string error.");
            }
        }
        if (ignoreReturns) lua_settop(m_lua, top);
    }

    void Bot::Exec(std::filesystem::path file, bool ignoreReturns) {
        if (!std::filesystem::exists(file)) {
            Warn("Bot::Exec(file): path is not a file.");
        }

        auto top = lua_gettop(m_lua);
        if (luaL_dofile(m_lua, file.string().c_str()) != LUA_OK) {
            if (lua_isstring(m_lua, -1)) {
                Warn("Exec file error: {}", lua_tostring(m_lua, -1));
            } else {
                Warn("Exec file error.");
            }
        }
        if (ignoreReturns) lua_settop(m_lua, top);
    }

    size_t Bot::InstructionCount() {
        std::scoped_lock lock(m_instructionMutex);
        size_t sum = 0;
        for (auto &task : m_tasks) {
            sum += task.InstructionCount();
        }
        return sum;
    }

    Instruction *Bot::GetInstruction() {
        std::unique_lock lock(m_instructionMutex);
        while (true) {
            if (!m_tasks.empty()) {
                auto instruction = m_tasks.front().GetInstruction();
                if (instruction) return instruction;
            }
    
            m_instructionCond.wait(lock);
        }
    }

    void Bot::PopInstruction() {
        std::scoped_lock lock(m_instructionMutex);
        if (m_tasks.empty()) return;
        m_tasks.front().PopInstruction();
    }

    void Bot::ClearInstructions() {
        std::scoped_lock lock(m_instructionMutex);
        m_tasks.clear();
    }

    Task &Bot::QueueTask() {
        std::scoped_lock lock(m_instructionMutex);
        m_instructionCond.notify_all();
        return m_tasks.emplace_back();
    }

    void Bot::PopTask() {
        std::scoped_lock lock(m_instructionMutex);
        m_tasks.pop_front();
    }

    bool Bot::Join() {
        if (m_loopThread.joinable()) m_loopThread.join();
        return m_instance.Join();
    }

    bool Bot::Running() {
        return m_instance.Running();
    }

    void Bot::Loop() {
        while(!m_exit) {
            Instruction *instruction = GetInstruction();
            if (instruction->GetType() == Instruction::TASK_END) {
                PopTask();
                continue;
            }

            instruction->Call(m_instance);
            PopInstruction();
        }
    }

    void Bot::QueueMineEntities(Task &task, const std::vector<Entity> &entities) {
        std::vector<std::tuple<MapPosition, double>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.position, 5);
        }

        auto pathfinderData = m_mapData.GetPathfinderData();
        auto paths = FindMultiPath(pathfinderData, {0, 0}, points);

        task.QueueInstruction([paths](FactorioInstance &instance) {
            for (auto &path : paths) {
                instance.RequestNoRes("Walk", std::get<Path>(path)).wait();
            }
        });
    }

    int Bot::QueueBuildBurnerCity(lua_State *L) {
        auto parameters = LuaGetParameters<Bot*,int,int,int,int>(L);
        Bot *bot = std::get<0>(parameters);

        auto &task = bot->QueueTask();

        auto entitiesFuture = bot->m_instance.Request<EntitySearchFilters, std::vector<Entity>>(
            "FindEntitiesFiltered", EntitySearchFilters{.area = {{-100, -100}, {100, 100}}, .name = {"huge-rock"}});
        entitiesFuture.wait();
        auto entities = entitiesFuture.get();
        bot->QueueMineEntities(task, entities.data);

        return 0;
    }
}