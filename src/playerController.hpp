#pragma once

#include <deque>
#include <thread>

#include "factorioAPI.hpp"
#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    /**
     * This class is a wrapper around a graphical FactorioInstance that is used to control
     *  a player character in that instance
     */
    class PlayerController {
    public:
        PlayerController();

        inline const FactorioInstance &GetFactorioInstance() {
            return m_gInstance;
        }

        bool Start(uint32_t seed, std::string *message = nullptr);
        void ClearQueue();
        void Stop();
        bool Join(int ms = -1);
        bool Running();

        bool QueueWalk(MapPosition goal, std::function<void(const ResponseDataless&)> callback);
        // bool QueueCraft(std::shared_ptr<Craft> instruction);
        // bool QueueCancel(std::shared_ptr<Cancel> instruction);
        // bool QueueTech(std::shared_ptr<Tech> instruction);
        // bool QueuePickup(std::shared_ptr<Pickup> instruction);
        // bool QueueDrop(std::shared_ptr<Drop> instruction);
        // bool QueueMine(std::shared_ptr<Mine> instruction);
        // bool QueueShoot(std::shared_ptr<Shoot> instruction);
        // bool QueueUse(std::shared_ptr<Use> instruction);
        // bool QueueEquip(std::shared_ptr<Equip> instruction);
        // bool QueueTake(std::shared_ptr<Take> instruction);
        // bool QueuePut(std::shared_ptr<Put> instruction);
        // bool QueueBuild(std::shared_ptr<Build> instruction);
        // bool QueueRecipe(std::shared_ptr<Recipe> instruction);
        // bool QueueLimit(std::shared_ptr<Limit> instruction);
        // bool QueueFilter(std::shared_ptr<Filter> instruction);
        // bool QueuePriority(std::shared_ptr<Priority> instruction);
        // bool QueueLaunch(std::shared_ptr<Launch> instruction);
        // bool QueueRotate(std::shared_ptr<Rotate> instruction);
    
    private:
        void Loop();
        std::thread m_loopThread;
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_exit;

        std::deque<std::unique_ptr<Instruction>> m_instructions;
        std::unique_ptr<Instruction> m_currentInstruction = nullptr;

        FactorioInstance m_gInstance;
        FactorioInstance m_hInstance;

        MapPosition m_playerPosition;
    };
}