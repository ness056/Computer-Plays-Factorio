#pragma once

#include "factorioAPI.hpp"
#include "types.hpp"

namespace ComputerPlaysFactorio {
    class Instruction {
    public:
        // The returned shared_ptr may be nullptr if the request could not be sent.
        // For instance, in case the FactorioInstance is not running.
        virtual std::shared_ptr<Waiter> Send(FactorioInstance&) = 0;
        // This function is used by some Instructions that need to compute some stuff
        // before Send is called. For instance the Walk instruction may be faster
        // if the path is calculated beforehand. If Precompute was never called,
        // the Send function will call it.
        virtual void Precompute(FactorioInstance&) {
            m_precomputed = true;
        }
        virtual constexpr std::string Name() = 0;

    protected:
        std::mutex m_precomputeMutex;
        Waiter m_precomputeWaiter;
        bool m_precomputed = false;
    };

    template<typename ReqType, typename ResType>
    class InstructionRes : public Instruction {
    public:
        InstructionRes(ReqType d, RequestCallback<ResType> c = nullptr)
            : data(d), callback(c) {}

        std::shared_ptr<Waiter> Send(FactorioInstance& instance) {
            auto waiter = std::make_shared<Waiter>();
            waiter->Lock();

            if (!m_precomputed) Precompute();
            m_precomputeWaiter.Wait();

            if (instance.SendRequestDataRes(Name(), data,
                [waiter](FactorioInstance &i, const Response<ResType> &d) {
                    if (callback) callback(i, d);
                    waiter->Unlock();
            })) {
                return waiter;
            }

            return nullptr;
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

            Precompute();
            m_precomputeWaiter.Wait();

            if (instance.SendRequestDataResDL(Name(), data,
                [waiter](FactorioInstance &i, const ResponseDataless &d) {
                    if (callback) callback(i, d);
                    waiter->Unlock();
            })) {
                return waiter;
            }

            return nullptr;
        }

        ReqType data;
        RequestDatalessCallback callback;
    };

    struct RequestPathData {
        MapPosition start;
        MapPosition goal;

        SERIALIZABLE(RequestPathData, start, goal);
    };

    class RequestPath : public InstructionRes<RequestPathData, Path> {
    public:
        using InstructionRes::InstructionRes;
        constexpr std::string Name() { return "RequestPath"; }
    };

    class Walk : public InstructionDL<Path> {
    public:
        Walk(Path path, RequestDatalessCallback callback = nullptr) :
            InstructionDL(path, callback) { m_precomputed = true; }
        Walk(MapPosition start, MapPosition goal, RequestDatalessCallback callback = nullptr) :
            InstructionDL({}, callback) { m_precomputed = false; }

        constexpr std::string Name() { return "Walk"; }
        
        void Precompute(FactorioInstance &instance) {
            std::unique_lock lock(m_precomputeMutex);
            if (m_precomputeWaiter.IsLocked()) return;

            m_precomputed = true;
            m_precomputeWaiter.Lock();

            auto r = RequestPath(RequestPathData{.start = m_start, .goal = m_goal},
                [this] (FactorioInstance&, const Response<Path> &res) {
                    m_data = res.data;
                    m_precomputeWaiter.Unlock();
                }
            );
            r.Send(instance);
        }

        MapPosition m_start;
        MapPosition m_goal;
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