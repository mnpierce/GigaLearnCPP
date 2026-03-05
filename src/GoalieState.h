#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>

namespace GGL {
	using namespace RLGC;

	class GoalieState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override {
			auto& cars = arena->GetCars();
			if (cars.size() < 2) {
				arena->ResetToRandomKickoff();
				return;
			}

			// Randomly pick which team is defending
			Team defendingTeam = (rand() % 2 == 0) ? Team::BLUE : Team::ORANGE;
			float sideMult = (defendingTeam == Team::BLUE) ? -1.0f : 1.0f;

			// 1. Setup Ball - moving toward the defender's goal
			BallState ballState = arena->ball->GetState();
			
			// Position the ball in front of defender's goal area
			ballState.pos = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 4000.f, // x: -2000 to 2000
				((rand() / (float)RAND_MAX) * 3000.f + 1000.f) * -sideMult, // y: +/- 1000 to +/- 4000
				CommonValues::BALL_RADIUS
			);

			// Velocity toward the goal
			ballState.vel = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 1000.f,
				(1500.f + (rand() / (float)RAND_MAX) * 2500.f) * sideMult, // 1500-4000 speed toward goal
				(rand() / (float)RAND_MAX) * 300.f
			);
			arena->ball->SetState(ballState);

			// 2. Setup Cars
			for (Car* car : cars) {
				CarState carState = car->GetState();
				if (car->team == defendingTeam) {
					// Defender: In or near goal
					carState.pos = Vec(
						((rand() / (float)RAND_MAX) - 0.5f) * 1400.f, // x: -700 to 700
						(CommonValues::BACK_WALL_Y - 100.f) * sideMult,
						17.0f
					);
					// Facing away from goal (toward the field)
					carState.rotMat = RotMat::LookAt(Vec(0, -sideMult, 0), Vec(0, 0, 1));
					carState.vel = Vec(0, 0, 0);
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				} else {
					// Attacker: Behind the ball or in a threatening position
					carState.pos = Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 400.f,
						ballState.pos.y - 800.f * sideMult,
						17.0f
					);
					// Facing goal
					carState.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
					carState.vel = ballState.vel * 0.9f;
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				}
				car->SetState(carState);
			}
		}
	};
}
