#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>
#include <cmath>

namespace GGL {
	/**
	 * AerialState: A specialized 1v1 state setter designed to train aerial mechanics.
	 * It launches the ball into the air at various heights and trajectories while
	 * placing the car on the ground beneath/near it. Four scenario modes:
	 *   - AERIAL_SAVE:    Ball flying toward the player's own goal (defensive aerial)
	 *   - POPUP:          Ball popping up nearby after a bounce (follow-up aerial)
	 *   - BACKBOARD_READ: Ball near the backboard above the goal (double-tap / read)
	 *   - HIGH_FLOAT:     Ball floating high at midfield (contested 50/50 aerial)
	 */
	class AerialState : public RLGC::StateSetter {
	private:
		enum AerialMode { AERIAL_SAVE, POPUP, BACKBOARD_READ, HIGH_FLOAT };

		// Helper: random float in [lo, hi]
		static float RandRange(float lo, float hi) {
			return lo + (rand() / (float)RAND_MAX) * (hi - lo);
		}

		// Helper: random sign (-1 or +1)
		static float RandSign() {
			return (rand() % 2 == 0) ? -1.0f : 1.0f;
		}

	public:
		virtual void ResetArena(Arena* arena) override {
			auto& cars = arena->GetCars();
			if (cars.size() < 2) {
				arena->ResetToRandomKickoff();
				return;
			}

			// Randomly pick which team is "attacking" (the one who needs to aerial).
			// In AERIAL_SAVE mode the attacker is actually defending, but we keep
			// the naming consistent: attackingTeam = the team we want to train aerials for.
			Team attackingTeam = (rand() % 2 == 0) ? Team::BLUE : Team::ORANGE;
			// sideMult: +1 means attacking toward ORANGE goal (+Y), -1 toward BLUE goal (-Y).
			float sideMult = (attackingTeam == Team::BLUE) ? 1.0f : -1.0f;

			// Roll a random aerial mode (equal 25% weights).
			AerialMode mode = static_cast<AerialMode>(rand() % 4);

			BallState ballState = arena->ball->GetState();

			// Attacker car position (set per-mode below)
			float attackerX = 0, attackerY = 0;
			// Opponent position
			float opponentX = 0, opponentY = 0;
			Vec opponentFacing = Vec(0, -sideMult, 0);
			Vec opponentVel = Vec(0, 0, 0);

			switch (mode) {
			case AERIAL_SAVE: {
				// Ball flying toward the attacker's OWN goal — must aerial to save.
				// Ball is in front of the goal, high up, moving toward it.
				float ballX = RandRange(-1500.f, 1500.f);
				float ballY = RandRange(2500.f, 4000.f) * -sideMult; // In attacker's defensive half
				float ballZ = RandRange(600.f, 1200.f);

				ballState.pos = Vec(ballX, ballY, ballZ);
				// Ball moving toward attacker's goal
				ballState.vel = Vec(
					RandRange(-500.f, 500.f),
					RandRange(1000.f, 2500.f) * -sideMult, // Toward their own goal
					RandRange(0.f, 400.f) // Rising or flat, never already falling
				);
				ballState.angVel = Vec(RandRange(-2.f, 2.f), RandRange(-2.f, 2.f), RandRange(-1.f, 1.f));

				// Attacker on the ground near their own goal, needs to go up
				attackerX = ballX + RandRange(-300.f, 300.f);
				attackerY = ballY + RandRange(100.f, 500.f) * -sideMult; // Close to ball, between ball and goal

				// Opponent (the "shooter") behind the ball
				opponentX = ballX + RandRange(-300.f, 300.f);
				opponentY = ballY - RandRange(500.f, 1500.f) * -sideMult;
				opponentFacing = Vec(0, -sideMult, 0); // Facing the goal they shot at
				opponentVel = Vec(0, RandRange(500.f, 1200.f) * -sideMult, 0);
				break;
			}
			case POPUP: {
				// Ball popping up nearby — a bounce or redirect scenario.
				// Ball is relatively close to the attacker, moderate height, going up.
				float ballX = RandRange(-2000.f, 2000.f);
				float ballY = RandRange(-500.f, 1500.f) * sideMult;
				float ballZ = RandRange(500.f, 900.f);

				ballState.pos = Vec(ballX, ballY, ballZ);
				// Ball going up and slightly toward opponent's goal
				ballState.vel = Vec(
					RandRange(-400.f, 400.f),
					RandRange(200.f, 800.f) * sideMult,
					RandRange(500.f, 1000.f) // Strong upward component
				);
				ballState.angVel = Vec(RandRange(-3.f, 3.f), RandRange(-3.f, 3.f), RandRange(-1.f, 1.f));

				// Attacker near the ball on the ground
				attackerX = ballX + RandRange(-400.f, 400.f);
				attackerY = ballY - RandRange(300.f, 800.f) * sideMult;

				// Opponent further back, rotating or recovering
				opponentX = RandRange(-1500.f, 1500.f);
				opponentY = RandRange(2000.f, 4000.f) * sideMult; // In their own half
				opponentFacing = Vec(0, -sideMult, 0);
				opponentVel = Vec(0, RandRange(-500.f, 0.f) * sideMult, 0);
				break;
			}
			case BACKBOARD_READ: {
				// Ball near/on the backboard above the opponent's goal.
				// Trains double-tap reads, backboard clears/shots.
				float ballX = RandRange(-1200.f, 1200.f);
				float ballY = RandRange(4200.f, 4900.f) * sideMult; // Near opponent's backboard
				float ballZ = RandRange(700.f, 1500.f); // Above goal height

				ballState.pos = Vec(ballX, ballY, ballZ);
				// Ball bouncing off backboard — coming back toward mid with some height
				ballState.vel = Vec(
					RandRange(-600.f, 600.f),
					RandRange(-800.f, -200.f) * sideMult, // Bouncing back from wall
					RandRange(0.f, 400.f) // Rising or flat off the backboard
				);
				ballState.angVel = Vec(RandRange(-3.f, 3.f), RandRange(-3.f, 3.f), RandRange(-2.f, 2.f));

				// Attacker approaching from midfield
				attackerX = ballX + RandRange(-500.f, 500.f);
				attackerY = RandRange(2000.f, 3500.f) * sideMult; // Closer to backboard

				// Opponent (defender) near their own goal
				opponentX = RandRange(-800.f, 800.f);
				opponentY = RandRange(4000.f, 4800.f) * sideMult;
				opponentFacing = Vec(0, -sideMult, 0);
				opponentVel = Vec(0, 0, 0);
				break;
			}
			case HIGH_FLOAT: {
				// Ball floating very high at midfield — contested aerial.
				float ballX = RandRange(-1500.f, 1500.f);
				float ballY = RandRange(-500.f, 500.f); // Near midfield
				float ballZ = RandRange(800.f, 1600.f); // Very high

				ballState.pos = Vec(ballX, ballY, ballZ);
				// Ball moving slowly or floating — both players must contest
				ballState.vel = Vec(
					RandRange(-300.f, 300.f),
					RandRange(-300.f, 300.f),
					RandRange(0.f, 200.f) // Floating or gently rising
				);
				ballState.angVel = Vec(RandRange(-1.f, 1.f), RandRange(-1.f, 1.f), RandRange(-1.f, 1.f));

				// Attacker on one side of the ball
				attackerX = ballX + RandRange(-400.f, 400.f);
				attackerY = ballY - RandRange(300.f, 1000.f) * sideMult;

				// Opponent on the other side — both contesting
				opponentX = ballX + RandRange(-500.f, 500.f);
				opponentY = ballY + RandRange(500.f, 1500.f) * sideMult;
				opponentFacing = Vec(0, -sideMult, 0);
				opponentVel = Vec(0, RandRange(-300.f, 0.f) * sideMult, 0);
				break;
			}
			}

			arena->ball->SetState(ballState);

			// ------------------------------------------------------------------
			// Setup Cars
			// ------------------------------------------------------------------
			for (Car* car : cars) {
				CarState carState = car->GetState();

				if (car->team == attackingTeam) {
					// --- ATTACKER (aerial player) ---
					carState.pos = Vec(attackerX, attackerY, 17.0f);
					// Face toward the ball
					Vec toBall = ballState.pos - carState.pos;
					toBall.z = 0; // Keep rotation level
					float len = toBall.Length();
					if (len > 0.1f) {
						toBall = toBall * (1.0f / len);
						carState.rotMat = RotMat::LookAt(toBall, Vec(0, 0, 1));
					} else {
						carState.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
					}
					// Small forward velocity to simulate approach
					carState.vel = Vec(0, RandRange(200.f, 600.f) * sideMult, 0);
					carState.angVel = Vec(0, 0, 0);
					// Good amount of boost for aerials (50-100)
					carState.boost = RandRange(50.f, 100.f);
				} else {
					// --- OPPONENT ---
					carState.pos = Vec(opponentX, opponentY, 17.0f);
					carState.rotMat = RotMat::LookAt(opponentFacing, Vec(0, 0, 1));
					carState.vel = opponentVel;
					carState.angVel = Vec(0, 0, 0);
					carState.boost = (float)(rand() % 101);
				}
				car->SetState(carState);
			}
		}
	};
}
