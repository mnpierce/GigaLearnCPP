#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include <RLGymCPP/Gamestates/GameState.h>
#include <RLGymCPP/Gamestates/Player.h>
#include "rewards.h"

using namespace RLGC;

bool IsNear(float a, float b, float epsilon = 1e-5f) {
    return std::abs(a - b) < epsilon;
}

void TestSpeedTowardBallReward() {
    std::cout << "  Testing SpeedTowardBallReward..." << std::endl;
    SpeedTowardBallReward reward;
    GameState state;
    Player player;
    player.pos = Vec(0, 0, 0);
    player.vel = Vec(1000, 0, 0);
    state.ball.pos = Vec(2000, 0, 0); 

    float val = reward.GetReward(player, state, false);
    assert(val > 0);
    assert(IsNear(val, 1000.0f / 2300.0f));

    player.vel = Vec(-1000, 0, 0);
    val = reward.GetReward(player, state, false);
    assert(val == 0);
}

void TestInAirReward() {
    std::cout << "  Testing InAirReward..." << std::endl;
    InAirReward reward;
    GameState state;
    Player player;

    player.isOnGround = false;
    assert(reward.GetReward(player, state, false) == 1.0f);

    player.isOnGround = true;
    assert(reward.GetReward(player, state, false) == 0.0f);
}

void TestTouchReward() {
    std::cout << "  Testing TouchReward..." << std::endl;
    TouchReward reward;
    GameState state;
    Player player;

    player.ballTouchedStep = true;
    assert(reward.GetReward(player, state, false) == 1.0f);

    player.ballTouchedStep = false;
    assert(reward.GetReward(player, state, false) == 0.0f);
}

void TestCustomFaceBallReward() {
    std::cout << "  Testing CustomFaceBallReward..." << std::endl;
    CustomFaceBallReward reward;
    GameState state;
    Player player;
    
    player.pos = Vec(0, 0, 0);
    state.ball.pos = Vec(1000, 0, 0);
    
    // Facing exactly at ball
    player.rotMat.forward = Vec(1, 0, 0);
    assert(IsNear(reward.GetReward(player, state, false), 1.0f));

    // Facing away
    player.rotMat.forward = Vec(-1, 0, 0);
    assert(IsNear(reward.GetReward(player, state, false), -1.0f));

    // Facing perpendicular
    player.rotMat.forward = Vec(0, 1, 0);
    assert(IsNear(reward.GetReward(player, state, false), 0.0f));
}

void TestCustomVelocityBallToGoalReward() {
    std::cout << "  Testing CustomVelocityBallToGoalReward..." << std::endl;
    CustomVelocityBallToGoalReward reward;
    GameState state;
    Player player;
    
    player.team = Team::BLUE; // Blue scores on Orange goal (positive Y)
    state.ball.pos = Vec(0, 0, 0);
    state.ball.vel = Vec(0, 3000, 0); // Moving toward orange goal

    float val = reward.GetReward(player, state, false);
    assert(val > 0);
    assert(IsNear(val, 3000.0f / 6000.0f));

    state.ball.vel = Vec(0, -3000, 0); // Moving toward own goal
    assert(reward.GetReward(player, state, false) == 0);
}

void TestJumpTouchReward() {
    std::cout << "  Testing JumpTouchReward..." << std::endl;
    JumpTouchReward reward(120.0f, 1.0f, 0);
    GameState state;
    Player player;
    player.carId = 1;

    // Condition: Touched + In Air + Above Height
    player.ballTouchedStep = true;
    player.isOnGround = false;
    state.ball.pos = Vec(0, 0, 220.0f); // 100 units above min
    
    float val = reward.GetReward(player, state, false);
    assert(val > 0);
    assert(IsNear(val, 100.0f / 100.0f));

    // Condition: On ground
    player.isOnGround = true;
    assert(reward.GetReward(player, state, false) == 0);
}

void TestKickoffReward() {
    std::cout << "  Testing KickoffReward..." << std::endl;
    KickoffReward reward;
    GameState state;
    Player player;
    
    // Ball at center, low speed
    state.ball.pos = Vec(0, 0, 92.75f);
    state.ball.vel = Vec(0, 0, 0);
    
    player.pos = Vec(0, -2000, 0);
    player.vel = Vec(0, 1000, 0); // Toward ball
    
    float val = reward.GetReward(player, state, false);
    assert(val > 0);

    // Ball far away from center
    state.ball.pos = Vec(500, 0, 92.75f);
    assert(reward.GetReward(player, state, false) == 0);
}

int main() {
    std::cout << "Running Expanded Reward Tests..." << std::endl;
    try {
        TestSpeedTowardBallReward();
        TestInAirReward();
        TestTouchReward();
        TestCustomFaceBallReward();
        TestCustomVelocityBallToGoalReward();
        TestJumpTouchReward();
        TestKickoffReward();
        std::cout << "\nAll Reward Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test Failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
