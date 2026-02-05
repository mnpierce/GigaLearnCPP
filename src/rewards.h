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
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        return player.isOnGround ? 0.0f : 1.0f;
    }
};

class TouchReward : public Reward {
public:
    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        return player.ballTouchedStep ? 1.0f : 0.0f;
    }
};

class CustomFaceBallReward : public Reward {
public:
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
            Vec velDiff = currentBallVel - prevBallVel;
            float acceleration = velDiff.Length() / CommonValues::BALL_MAX_SPEED;
            reward += acceleration * accelReward;

            if (currentBallVel.Length() >= minBallSpeed) {
                reward += touchReward;
            }
        }

        return reward;
    }
};

class KickoffReward : public Reward {
public:
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
    FirstTouchKickoffReward() : kickoffActive(true) {}

    virtual void Reset(const GameState& state) override {
        kickoffActive = true;
    }

    virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
        if (!kickoffActive) return 0.0f;

        float ballPosXY = std::sqrt(state.ball.pos.x * state.ball.pos.x + state.ball.pos.y * state.ball.pos.y);
        float ballSpeed = state.ball.vel.Length();
        
        if ((ballPosXY > 200.0f || ballSpeed > 500.0f) && !player.ballTouchedStep) {
            kickoffActive = false;
            return 0.0f;
        }
        
        if (player.ballTouchedStep) {
            float goalDirY = (player.team == Team::BLUE) ? 1.0f : -1.0f;
            float ballVelY = state.ball.vel.y * goalDirY;
            float ratio = ballVelY / 2000.0f;
            
            if (ratio > 0.5f) ratio = 0.5f;
            if (ratio < -0.5f) ratio = -0.5f;

            kickoffActive = false; 
            return 0.5f + ratio;
        }

        return 0.0f;
    }
};
