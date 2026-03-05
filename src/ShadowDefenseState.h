#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>

namespace GGL {
	/**
	 * ShadowDefenseState: A specialized 1v1 state setter designed to train 
	 * retreating defense ("Shadowing"). It positions the defender ahead of 
	 * the ball, facing their own goal, while both agents move at high speed 
	 * toward the defending net.
	 */
	class ShadowDefenseState : public RLGC::StateSetter {
	public:
		virtual void ResetArena(RLGC::Arena* arena) override {
			auto& cars = arena->GetCars();
			if (cars.size() < 2) {
				arena->ResetToRandomKickoff();
				return;
			}

			// Randomly pick which team is defending.
			RLGC::Team defendingTeam = (rand() % 2 == 0) ? RLGC::Team::BLUE : RLGC::Team::ORANGE;
			float sideMult = (defendingTeam == RLGC::Team::BLUE) ? -1.0f : 1.0f;

			// 1. Setup Ball - mid field moving toward defending goal.
			RLGC::BallState ballState = arena->ball->GetState();
			ballState.pos = RLGC::Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 3000.f, // Randomized X across midfield.
				((rand() / (float)RAND_MAX) * 2000.f - 1000.f) * -sideMult, // Positioned in midfield area.
				RLGC::CommonValues::BALL_RADIUS
			);
			ballState.vel = RLGC::Vec(
				((rand() / (float)RAND_MAX) - 0.5f) * 500.f,
				(1200.f + (rand() / (float)RAND_MAX) * 1000.f) * sideMult, // High velocity toward the goal.
				0.0f
			);
			arena->ball->SetState(ballState);

			// 2. Setup Cars
			for (RLGC::Car* car : cars) {
				RLGC::CarState carState = car->GetState();
				if (car->team == defendingTeam) {
					// Defender: "Shadowing" - Ahead of the ball, facing their own goal (retreating).
					// Placed 1200-2000 units ahead of the ball to force a retreating defensive line.
					carState.pos = RLGC::Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 400.f,
						ballState.pos.y + (1200.f + (rand() / (float)RAND_MAX) * 800.f) * sideMult,
						17.0f
					);
					// Facing THEIR goal (shadowing / retreating).
					carState.rotMat = RLGC::RotMat::LookAt(RLGC::Vec(0, sideMult, 0), RLGC::Vec(0, 0, 1));
					carState.vel = ballState.vel * 0.85f; // Retreating at a slightly lower speed than the ball.
					carState.angVel = RLGC::Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				} else {
					// Attacker: Dribbling behind the ball.
					carState.pos = RLGC::Vec(
						ballState.pos.x + ((rand() / (float)RAND_MAX) - 0.5f) * 200.f,
						ballState.pos.y - 400.f * sideMult, // Positioned directly behind the ball.
						17.0f
					);
					// Facing the goal to encourage an aggressive attack.
					carState.rotMat = RLGC::RotMat::LookAt(RLGC::Vec(0, sideMult, 0), RLGC::Vec(0, 0, 1));
					carState.vel = ballState.vel * 1.05f; // Moving slightly faster to maintain offensive pressure.
					carState.angVel = RLGC::Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				}
				car->SetState(carState);
			}
		}
	};
}
