#pragma once

#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/Gamestates/GameState.h>
#include <RLGymCPP/Gamestates/Player.h>
#include <RLGymCPP/EnvSet/EnvSet.h>
#include <RLGymCPP/CommonValues.h>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>

using namespace RLGC;

// --- Ported Reward Functions ---

class SpeedTowardBallReward : public Reward {
public:
    virtual std::string GetName() override { return "SpeedTowardBallReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        Vec posDiff = state.ball.pos - player.pos;
        float distToBall = posDiff.Length();

        if (distToBall > 0) {
            Vec dirToBall = posDiff / distToBall;
            float speedTowardBall = player.vel.Dot(dirToBall);
            return std::max(speedTowardBall / CommonValues::CAR_MAX_SPEED, 0.0f);
        }
        return 0.0f;
    }
};

class JumpTouchReward : public Reward {
    float minHeight;
    float exp;
    int cooldown;
    std::map<int, int> timers;

public:
    virtual std::string GetName() override { return "JumpTouchReward"; }
    JumpTouchReward(float minHeight = 120.f, float exp = 0.5f, int cooldown = 30)
        : minHeight(minHeight), exp(exp), cooldown(cooldown) {}

    virtual void Reset(const GameState& state) override {
        timers.clear();
    }

    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        int id = player.carId;
        if (timers[id] > 0) {
            timers[id]--;
            return 0.0f;
        }

        if (player.ballTouchedStep && !player.isOnGround && state.ball.pos.z >= minHeight) {
            float heightDiff = state.ball.pos.z - minHeight;
            float reward = std::pow(heightDiff, exp) / 100.0f;
            timers[id] = cooldown;
            return reward;
        }

        return 0.0f;
    }
};

class InAirReward : public Reward {
public:
    virtual std::string GetName() override { return "InAirReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        return player.isOnGround ? 0.0f : 1.0f;
    }
};

class TouchReward : public Reward {
public:
    virtual std::string GetName() override { return "TouchReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        return player.ballTouchedStep ? 1.0f : 0.0f;
    }
};

class CustomFaceBallReward : public Reward {
public:
    virtual std::string GetName() override { return "CustomFaceBallReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        Vec posDiff = state.ball.pos - player.pos;
        float dist = posDiff.Length();
        
        if (dist < 1e-9) return 0.0f;
        
        Vec dirToBall = posDiff / dist;
        Vec carForward = player.rotMat.forward; 
        
        return carForward.Dot(dirToBall);
    }
};

class CustomVelocityBallToGoalReward : public Reward {
public:
    virtual std::string GetName() override { return "CustomVelocityBallToGoalReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        float targetY = (player.team == Team::BLUE) ? CommonValues::BACK_NET_Y : -CommonValues::BACK_NET_Y;
        Vec targetPos = Vec(0, targetY, 0); 
        
        Vec posDiff = targetPos - state.ball.pos;
        float dist = posDiff.Length();

        if (dist > 0) {
            Vec ballDirToGoal = posDiff / dist;
            float dot = ballDirToGoal.Dot(state.ball.vel / CommonValues::BALL_MAX_SPEED);
            return std::max(dot, 0.0f);
        }
        return 0.0f;
    }
};

class AdvancedTouchReward : public Reward {
    float touchReward;
    float accelReward;
    float minBallSpeed;

    Vec prevBallVel;
    Vec currentBallVel;
    uint64_t lastTick; 

public:
    virtual std::string GetName() override { return "AdvancedTouchReward"; }
    AdvancedTouchReward(float touchReward = 1.0f, float accelReward = 0.0f, float minBallSpeed = 0.0f)
        : touchReward(touchReward), accelReward(accelReward), minBallSpeed(minBallSpeed), lastTick(0) {}

    virtual void Reset(const GameState& state) override {
        prevBallVel = state.ball.vel;
        currentBallVel = state.ball.vel;
        lastTick = state.lastTickCount;
    }

    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (state.lastTickCount != lastTick) {
            prevBallVel = currentBallVel;
            currentBallVel = state.ball.vel;
            lastTick = state.lastTickCount;
        }

        float reward = 0.0f;
        if (player.ballTouchedStep) {
            // Only reward the touch IF the ball is moving fast enough
            if (currentBallVel.Length() >= minBallSpeed) {
                Vec velDiff = currentBallVel - prevBallVel;
                float acceleration = velDiff.Length() / CommonValues::BALL_MAX_SPEED;
                
                reward += acceleration * accelReward;
                reward += touchReward;
            }
        }

        return reward;
    }
};

class KickoffReward : public Reward {
public:
    virtual std::string GetName() override { return "KickoffReward"; }
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        float ballPosXY = std::sqrt(state.ball.pos.x * state.ball.pos.x + state.ball.pos.y * state.ball.pos.y);
        float ballSpeed = state.ball.vel.Length();

        if (ballPosXY < 100.0f && ballSpeed < 10.0f) {
            if (player.isDemoed) return 0.0f;

            Vec posDiff = state.ball.pos - player.pos;
            float distToBall = posDiff.Length();

            if (distToBall > 0) {
                Vec dirToBall = posDiff / distToBall;
                float speedTowardBall = player.vel.Dot(dirToBall);
                return std::max(speedTowardBall / CommonValues::CAR_MAX_SPEED, 0.0f);
            }
        }
        return 0.0f;
    }
};

class FirstTouchKickoffReward : public Reward {
    bool kickoffActive;

public:
    virtual std::string GetName() override { return "FirstTouchKickoffReward"; }
    FirstTouchKickoffReward() : kickoffActive(true) {}

    virtual void Reset(const GameState& state) override {
        kickoffActive = true;
    }

    virtual std::vector<float> GetAllRewards(const GameState& state, bool isFinal) override {
        std::vector<float> rewards(state.players.size(), 0.0f);
        if (!kickoffActive) return rewards;

        bool touched = false;
        for (size_t i = 0; i < state.players.size(); ++i) {
            const Player& player = state.players[i];
            if (player.ballTouchedStep) {
                touched = true;
                float goalDirY = (player.team == Team::BLUE) ? 1.0f : -1.0f;
                float ballVelY = state.ball.vel.y * goalDirY;
                float ratio = ballVelY / 2000.0f;
                
                if (ratio > 0.5f) ratio = 0.5f;
                if (ratio < -0.5f) ratio = -0.5f;

                rewards[i] = 0.5f + ratio;
            }
        }

        float ballPosXY = std::sqrt(state.ball.pos.x * state.ball.pos.x + state.ball.pos.y * state.ball.pos.y);
        float ballSpeed = state.ball.vel.Length();

        if (touched || ballPosXY > 200.0f || ballSpeed > 500.0f) {
            kickoffActive = false;
        }

        return rewards;
    }

    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        return 0.0f; // Logic handled in GetAllRewards
    }
};

class AsymmetricBoostReward : public Reward {
    float gainScale;
    float lossScale;

public:
    virtual std::string GetName() override { return "AsymmetricBoostReward"; }

    AsymmetricBoostReward(float gainScale = 1.5f, float lossScale = 0.8f)
        : gainScale(gainScale), lossScale(lossScale) {}

    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!player.prev)
            return 0.0f;

        float delta = player.boost - player.prev->boost;

        if (delta > 0.0f) {
            // Gain: concave sqrt curve — picking up boost when empty is worth more
            return gainScale * sqrtf(delta / 100.0f);
        } else if (delta < 0.0f) {
            // Loss: linear penalty, scaled by height — zero penalty at/above goal height
            float heightRatio = player.pos.z / CommonValues::GOAL_HEIGHT;
            heightRatio = std::max(0.0f, std::min(1.0f, heightRatio));
            // delta is negative, so this produces a negative reward (penalty)
            return lossScale * (delta / 100.0f) * (1.0f - heightRatio);
        }

        return 0.0f;
    }
};
