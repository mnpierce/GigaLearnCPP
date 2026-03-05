#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>

namespace GGL {
	using namespace RLGC;

	class ShadowDefenseState : public StateSetter {
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

			// 1. Setup Ball - mid field moving toward defending goal
			BallState ballState = arena->ball->GetState();
			ballState.pos = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 3000.f, // x: -1500 to 1500
				((rand() / (float)RAND_MAX) * 2000.f - 1000.f) * -sideMult, // mid field
				CommonValues::BALL_RADIUS
			);
			ballState.vel = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 500.f,
				(1200.f + (rand() / (float)RAND_MAX) * 1000.f) * sideMult, // 1200-2200 speed toward goal
				0.0f
			);
			arena->ball->SetState(ballState);

			// 2. Setup Cars
			for (Car* car : cars) {
				CarState carState = car->GetState();
				if (car->team == defendingTeam) {
					// Defender: "Shadowing" - Ahead of the ball, facing their own goal (retreating)
					carState.pos = Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 400.f,
						ballState.pos.y + (1200.f + (rand() / (float)RAND_MAX) * 800.f) * sideMult, // 1200-2000 units ahead
						17.0f
					);
					// Facing THEIR goal (shadowing / retreating)
					carState.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
					carState.vel = ballState.vel * 0.85f; // Moving at similar speed as ball
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				} else {
					// Attacker: Dribbling behind the ball
					carState.pos = Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 200.f,
						ballState.pos.y - 400.f * sideMult, // Just behind the ball
						17.0f
					);
					// Facing goal
					carState.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
					carState.vel = ballState.vel * 1.05f; // Slightly faster to maintain pressure
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				}
				car->SetState(carState);
			}
		}
	};
}
