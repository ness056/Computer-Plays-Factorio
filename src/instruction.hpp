#pragma once

#include <string>
#include <cstdint>
#include <mutex>
#include <condition_variable>

#include "factorioAPI.hpp"
#include "serializer.hpp"
#include "utils.hpp"
    
/**
 * Instructions are a special type requests that are waitable and mainly used to control
 *  the player's character. For instance, if you send a Walk instruction and call the wait function
 *  the thread will be blocked until the player has reached the destination.
 */

namespace ComputerPlaysFactorio {

    class Instruction {
    public:
        virtual constexpr std::string GetName() = 0;
        virtual bool Send(FactorioInstance&) = 0;
        bool Wait();
        virtual void Cancel() {
            m_waiting = false;
            Notify(true);
        }

    protected:
        Instruction() = default;

        bool Lock();
        void Notify(bool cancel = false);

        bool m_canceled = false;
        bool m_waiting = false;
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cond;
    };

    using CallbackDL = std::function<void(const ResponseDataless&)>;

    template<class T>
    class InstructionDL : public Instruction {
    public:
        bool Send(FactorioInstance &instance) override {
            if (!Lock()) return false;
    
            return instance.SendRequestDataResDL<T>(GetName(), *(T*)this, [this](FactorioInstance&, const ResponseDataless &res) {
                if (m_callback) m_callback(res);
                Notify();
            });
        }

    protected:
        InstructionDL(CallbackDL callback) : m_callback(callback) {};

        const CallbackDL m_callback;
    };

    template<class R>
    using CallbackR = std::function<void(const Response<R>&)>;

    template<class T, class R>
    class InstructionR : public Instruction {
    public:
        bool Send(FactorioInstance &instance) override {
            if (!Lock()) return false;
    
            return instance.SendRequestDataRes<T, R>(GetName(), *(T*)this, [this](FactorioInstance&, const Response<R> &res) {
                if (m_callback) m_callback(res);
                Notify();
            });
        }

    protected:
        InstructionR(CallbackR<R> callback) : m_callback(callback) {}

        const CallbackR<R> m_callback;
    };

    struct PathfinderWaypoint {
        MapPosition pos;
        bool needsDestroyToReach;

        SERIALIZABLE_CUSTOM_NAMES(PathfinderWaypoint, "position", pos, "needs_destroy_to_reach", needsDestroyToReach);
    };

    using Path = std::vector<PathfinderWaypoint>;

    class InstructionRequestPath : public InstructionR<InstructionRequestPath, Path> {
    public:
        InstructionRequestPath(MapPosition start_, MapPosition goal_, CallbackR<Path> callback) :
            InstructionR(callback), start(start_), goal(goal_) {}
        inline constexpr std::string GetName() override { return "RequestPath"; }

        MapPosition start;
        MapPosition goal;

        SERIALIZABLE(InstructionRequestPath, start, goal);
    };

    class InstructionWalk : public InstructionDL<InstructionWalk> {
    public:
        InstructionWalk(FactorioInstance &instance, MapPosition start_, MapPosition goal_, CallbackDL callback)
            : InstructionDL(callback), m_requestPath(start_, goal_, [this](const Response<Path> &res) {
                if (res.success == false) {
                    if (m_callback) m_callback(res);
                    m_canceled = true;
                }
                else m_path = res.data;
            }
        ) {
            SendRequestPath(instance);
        }
        void Cancel() override {
            m_requestPath.Cancel();
            Instruction::Cancel();
        }

        inline constexpr std::string GetName() override { return "Walk"; }

        bool Send(FactorioInstance &instance) override {
            if (!m_requestPath.Wait() || !Lock()) return false;
    
            return instance.SendRequestDataResDL("Walk", m_path, [this](FactorioInstance&, const ResponseDataless &res) {
                if (m_callback) m_callback(res);
                Notify();
            });
        }
        inline bool SendRequestPath(FactorioInstance &i) { return m_requestPath.Send(i); }

    private:
        InstructionRequestPath m_requestPath;
        Path m_path;

        SERIALIZABLE(InstructionWalk) // Not needed but compiler isn't happy without it
    };

    // struct ResponseCraft {
        
    // };

    // class InstructionCraft : public InstructionDL<InstructionCraft> {
    // };

    // struct ResponseCancel {
        
    // };

    // class InstructionCancel : public InstructionDL<InstructionCancel> {
    // };

    // struct ResponseTech {
        
    // };

    // class InstructionTech : public InstructionDL<InstructionTech> {
    // };

    // struct ResponsePickup {
        
    // };

    // class InstructionPickup : public InstructionDL<InstructionPickup> {
    // };

    // struct ResponseDrop {
        
    // };

    // class InstructionDrop : public InstructionDL<InstructionDrop> {
    // };

    // struct ResponseMine {
        
    // };

    // class InstructionMine : public InstructionDL<InstructionMine> {
    // };

    // struct ResponseShoot {
        
    // };

    // class InstructionShoot : public InstructionDL<InstructionShoot> {
    // };

    // struct ResponseUse {
        
    // };

    // class InstructionUse : public InstructionDL<InstructionUse> {
    // };

    // struct ResponseEquip {
        
    // };

    // class InstructionEquip : public InstructionDL<InstructionEquip> {
    // };

    // struct ResponseTake {
        
    // };

    // class InstructionTake : public InstructionDL<InstructionTake> {
    // };

    // struct ResponsePut {
        
    // };

    // class InstructionPut : public InstructionDL<InstructionPut> {
    // };

    // struct ResponseBuild {
        
    // };

    // class InstructionBuild : public InstructionDL<InstructionBuild> {
    // };

    // struct ResponseRecipe {
        
    // };

    // class InstructionRecipe : public InstructionDL<InstructionRecipe> {
    // };

    // struct ResponseLimit {
        
    // };

    // class InstructionLimit : public InstructionDL<InstructionLimit> {
    // };

    // struct ResponseFilter {
        
    // };

    // class InstructionFilter : public InstructionDL<InstructionFilter> {
    // };

    // struct ResponsePriority {
        
    // };

    // class InstructionPriority : public InstructionDL<InstructionPriority> {
    // };

    // struct ResponseLaunch {
        
    // };

    // class InstructionLaunch : public InstructionDL<InstructionLaunch> {
    // };

    // struct ResponseRotate {
        
    // };

    // class InstructionRotate : public InstructionDL<InstructionRotate> {
    // };

}