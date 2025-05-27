#pragma once

#include "../factorioAPI/factorioAPI.hpp"

namespace ComputerPlaysFactorio {
    class Instruction {
    public:
        // The returned shared_ptr may be nullptr if the request could not be sent.
        // For instance, in case the FactorioInstance is not running.
        virtual std::shared_ptr<Waiter> Send(FactorioInstance&) = 0;
        virtual constexpr std::string Name() = 0;
    };

    // Used to mark the end of a subtask.
    class EndSubtask {
        constexpr std::string Name() { return "EndSubtask"; }
    };

    // Used to mark the end of a task.
    class Endtask {
        constexpr std::string Name() { return "Endtask"; }
    };

    template<typename ReqType, typename ResType>
    class InstructionRes : public Instruction {
    public:
        InstructionRes(ReqType d, RequestCallback<ResType> c = nullptr)
            : data(d), callback(c) {}

        std::shared_ptr<Waiter> Send(FactorioInstance& instance) {
            auto waiter = std::make_shared<Waiter>();
            waiter->Lock();

            bool success = instance.SendRequestDataRes<ReqType, ResType>(Name(), data,
                [callback = callback, waiter](const Response<ResType> &d) {
                    if (callback) callback(d);
                    waiter->Unlock();
                }
            );

            if (success) return waiter;
            else return nullptr;
        }

        ReqType data;
        RequestCallback<ResType> callback;
    };

    template<typename ReqType>
    class InstructionDL : public Instruction {
    public:
        InstructionDL(ReqType d, RequestDatalessCallback c = nullptr)
            : data(d), callback(c) {}

        std::shared_ptr<Waiter> Send(FactorioInstance& instance) {
            auto waiter = std::make_shared<Waiter>();
            waiter->Lock();

            bool success = instance.SendRequestDataResDL(Name(), data,
                [callback = callback, waiter](const ResponseDataless &d) {
                    if (callback) callback(d);
                    waiter->Unlock();
                }
            );

            if (success) return waiter;
            return nullptr;
        }

        ReqType data;
        RequestDatalessCallback callback;
    };

    class Walk : public InstructionDL<Path> {
    public:
        using InstructionDL::InstructionDL;
        constexpr std::string Name() { return "Walk"; }
    };

    class CraftableAmount : public InstructionRes<std::string, uint32_t> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "CraftableAmount"; }
    };

    struct CraftData {
        std::string recipe;
        uint32_t amount;
        bool force;

        SERIALIZABLE(CraftData, recipe, amount, force);
    };

    class Craft : public InstructionRes<CraftData, uint32_t> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "Craft"; }
    };

    // class Cancel : public Instruction {
    // public:
    //     constexpr std::string Name() { return "Cancel"; }
    // };

    struct BuildData {
        MapPosition position;
        std::string item;
        Direction direction;

        SERIALIZABLE(BuildData, position, item, direction);
    };

    class Build : public InstructionDL<BuildData> {
    public:
        using InstructionDL::InstructionDL;
        constexpr std::string Name() { return "Build"; }
    };

    class Mine : public InstructionRes<MapPosition, std::map<std::string, uint32_t>> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "Mine"; }
    };

    struct RotateData {
        MapPosition position;
        std::string entity;
        bool reversed;

        SERIALIZABLE(RotateData, position, entity, reversed);
    };

    class Rotate : public InstructionRes<RotateData, Direction> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "Rotate"; }
    };
    
    struct PutTakeData {
        MapPosition position;
        std::string entity;
        std::string item;
        int32_t amount;
        uint8_t playerInventory;
        uint8_t entityInventory;
        bool force;

        SERIALIZABLE(PutTakeData, position, entity, item, amount, playerInventory, entityInventory, force);
    };

    class Take : public InstructionRes<PutTakeData, uint32_t> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "Take"; }
    };

    class Put : public InstructionRes<PutTakeData, uint32_t> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "Put"; }
    };

    // inline bool Tech(std::shared_ptr<Tech> instruction);
    // inline bool Pickup(std::shared_ptr<Pickup> instruction);
    // inline bool Drop(std::shared_ptr<Drop> instruction);
    // inline bool Shoot(std::shared_ptr<Shoot> instruction);
    // inline bool Use(std::shared_ptr<Use> instruction);
    // inline bool Equip(std::shared_ptr<Equip> instruction);
    // inline bool Recipe(std::shared_ptr<Recipe> instruction);
    // inline bool Limit(std::shared_ptr<Limit> instruction);
    // inline bool Filter(std::shared_ptr<Filter> instruction);
    // inline bool Priority(std::shared_ptr<Priority> instruction);
    // inline bool Launch(std::shared_ptr<Launch> instruction);
}