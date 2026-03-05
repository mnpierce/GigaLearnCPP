#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>

namespace GGL {
	/**
	 * GoalieState: A specialized 1v1 state setter designed to train defensive saves.
	 * It positions one player (the Defender) deep in their own goal while spawning 
	 * the ball and an Attacker in front, moving toward the net at high speed.
	 */
	class GoalieState : public RLGC::StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override {
			auto& cars = arena->GetCars();
			if (cars.size() < 2) {
				arena->ResetToRandomKickoff();
				return;
			}

			// Randomly pick which team is defending to ensure the policy learns both sides.
			Team defendingTeam = (rand() % 2 == 0) ? Team::BLUE : Team::ORANGE;
			float sideMult = (defendingTeam == Team::BLUE) ? -1.0f : 1.0f;

			// 1. Setup Ball - moving toward the defender's goal
			BallState ballState = arena->ball->GetState();
			
			// Position the ball in front of defender's goal area.
			// X is randomized across the goal width (-2000 to 2000).
			// Y is randomized between 1000 and 4000 units away from the back wall.
			ballState.pos = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 4000.f, 
				((rand() / (float)RAND_MAX) * 3000.f + 1000.f) * -sideMult,
				RLGC::CommonValues::BALL_RADIUS
			);

			// Give the ball significant velocity toward the goal (1500-4000 speed).
			// This forces the defender to react quickly to a threatening shot.
			ballState.vel = Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 1000.f,
				(1500.f + (rand() / (float)RAND_MAX) * 2500.f) * sideMult,
				(rand() / (float)RAND_MAX) * 300.f
			);
			arena->ball->SetState(ballState);

			// 2. Setup Cars
			for (Car* car : cars) {
				CarState carState = car->GetState();
				if (car->team == defendingTeam) {
					// Defender: Positioned inside or just in front of the goal line.
					// X is randomized within the posts (-700 to 700).
					carState.pos = Vec(
						((rand() / (float)RAND_MAX) - 0.5f) * 1400.f,
						(RLGC::CommonValues::BACK_WALL_Y - 100.f) * sideMult,
						17.0f
					);
					// Facing away from their own goal (toward the field) to prepare for the save.
					carState.rotMat = RotMat::LookAt(Vec(0, -sideMult, 0), Vec(0, 0, 1));
					carState.vel = Vec(0, 0, 0);
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				} else {
					// Attacker: Spawning slightly behind the ball to simulate follow-up pressure.
					carState.pos = Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 400.f,
						ballState.pos.y - 800.f * sideMult,
						17.0f
					);
					// Facing the goal to encourage scoring logic.
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
