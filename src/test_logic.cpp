#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include <RLGymCPP/Gamestates/GameState.h>
#include <RLGymCPP/Gamestates/Player.h>
#include "rewards.h"
#include "GoalieState.h"
#include "ShadowDefenseState.h"

using namespace RLGC;
using namespace GGL;

void TestRewards() {
    std::cout << "Running Reward Tests..." << std::endl;

    SpeedTowardBallReward reward;
    GameState state;
    Player player;
    player.carId = 1;
    player.pos = Vec(0, 0, 0);
    player.vel = Vec(1000, 0, 0);
    state.ball.pos = Vec(2000, 0, 0); // Ball is ahead of player

    float val = reward.GetReward(player, state, false);
    std::cout << "  SpeedTowardBall (Moving toward): " << val << std::endl;
    assert(val > 0);

    player.vel = Vec(-1000, 0, 0); // Moving away
    val = reward.GetReward(player, state, false);
    std::cout << "  SpeedTowardBall (Moving away): " << val << std::endl;
    assert(val <= 0);

    std::cout << "Reward Tests Passed!" << std::endl;
}

// Note: Testing StateSetters requires a valid Arena object which needs collision meshes.
// For a simple logic test, we can just verify the math in our state setters if we mock the Arena,
// but since GoalieState and ShadowDefenseState use arena->GetCars(), we'd need a real arena.
// For now, we'll stick to reward unit tests.

int main() {
    try {
        TestRewards();
        std::cout << "\nAll Tests Passed Successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test Failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
