#pragma once

#include "factorio-API.hpp"

namespace ComputerPlaysFactorio {

    class FactorioPrototypes {
    public:
        Result Fetch();

        inline const json &Get() const {
            CheckValid();
            return m_prototypes;
        }
        inline const json &Get(const std::string &type, const std::string &name) const {
            CheckValid();
            return m_prototypes[type][name];
        }

        // Gets an entity prototype from its name without needing its prototype type.
        // If you know the type beforehand, the Get method will be way faster.
        const json &GetEntity(const std::string &name) const;

        bool HasFlag(const json &prototype, const std::string &flag) const;
        inline bool HasFlag(const std::string &type, const std::string &name, const std::string &flag) const {
            return HasFlag(m_prototypes[type][name], flag);
        }
        inline bool HasFlag(const Entity &e, const std::string &flag) const {
            return HasFlag(e.GetPrototype(), flag);
        };

        bool HasCollisionMask(const json &prototype, const std::string &mask) const;
        inline bool HasCollisionMask(const std::string &type, const std::string &name, const std::string &mask) const {
            return HasCollisionMask(m_prototypes[type][name], mask);
        }
        inline bool HasCollisionMask(const Entity &e, const std::string &mask) const {
            return HasCollisionMask(e.GetPrototype(), mask);
        };

    private:
        void CheckValid() const;

        bool valid = false;
        json m_prototypes;

        static inline constexpr auto s_entity_types = std::to_array<std::string_view>({
            "accumulator",
            "agricultural-tower",
            "ammo-turret",
            "arithmetic-combinator",
            "arrow",
            "artillery-flare",
            "artillery-projectile",
            "artillery-turret",
            "artillery-wagon",
            "assembling-machine",
            "asteroid",
            "asteroid-collector",
            "beacon",
            "beam",
            "boiler",
            "burner-generator",
            "capture-robot",
            "car",
            "cargo-bay",
            "cargo-landing-pad",
            "cargo-pod",
            "cargo-wagon",
            "character",
            "character-corpse",
            "cliff",
            "combat-robot",
            "constant-combinator",
            "construction-robot",
            "container",
            "corpse",
            "curved-rail-a",
            "curved-rail-b",
            "decider-combinator",
            "deconstructible-tile-proxy",
            "display-panel",
            "electric-energy-interface",
            "electric-pole",
            "electric-turret",
            "elevated-curved-rail-a",
            "elevated-curved-rail-b",
            "elevated-half-diagonal-rail",
            "elevated-straight-rail",
            "entity-ghost",
            "explosion",
            "fire",
            "fish",
            "fluid-turret",
            "fluid-wagon",
            "furnace",
            "fusion-generator",
            "fusion-reactor",
            "gate",
            "generator",
            "half-diagonal-rail",
            "heat-interface",
            "heat-pipe",
            "highlight-box",
            "infinity-cargo-wagon",
            "infinity-container",
            "infinity-pipe",
            "inserter",
            "item-entity",
            "item-request-proxy",
            "lab",
            "lamp",
            "land-mine",
            "lane-splitter",
            "legacy-curved-rail",
            "legacy-straight-rail",
            "lightning",
            "lightning-attractor",
            "linked-belt",
            "linked-container",
            "loader",
            "loader-1x1",
            "locomotive",
            "logistic-container",
            "logistic-robot",
            "market",
            "mining-drill",
            "offshore-pump",
            "particle-source",
            "pipe",
            "pipe-to-ground",
            "plant",
            "player-port",
            "power-switch",
            "programmable-speaker",
            "projectile",
            "proxy-container",
            "pump",
            "radar",
            "rail-chain-signal",
            "rail-ramp",
            "rail-remnants",
            "rail-signal",
            "rail-support",
            "reactor",
            "resource",
            "roboport",
            "rocket-silo",
            "rocket-silo-rocket",
            "rocket-silo-rocket-shadow",
            "segment",
            "segmented-unit",
            "selector-combinator",
            "simple-entity",
            "simple-entity-with-force",
            "simple-entity-with-owner",
            "smoke-with-trigger",
            "solar-panel",
            "space-platform-hub",
            "speech-bubble",
            "spider-leg",
            "spider-unit",
            "spider-vehicle",
            "splitter",
            "sticker",
            "storage-tank",
            "straight-rail",
            "stream",
            "temporary-container",
            "thruster",
            "tile-ghost",
            "train-stop",
            "transport-belt",
            "tree",
            "turret",
            "underground-belt",
            "unit",
            "unit-spawner",
            "valve",
            "wall",
        });
    };

    extern FactorioPrototypes g_prototypes;
}