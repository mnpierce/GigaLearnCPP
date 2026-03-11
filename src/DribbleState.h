#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <random>
#include <cmath>

namespace GGL {
	/**
	 * DribbleState: A specialized 1v1 state setter designed to train dribble plays and flicks.
	 * It spawns the attacker carrying the ball on their roof, moving toward the opponent's goal,
	 * with the defender placed in one of four randomized defensive postures:
	 *   - HEAD_ON:  Facing the attacker straight on (trains flicks over a committed challenge)
	 *   - SHADOW:   Ahead of attacker, retreating toward own goal (trains dribble vs shadow defense)
	 *   - ANGLED:   Approaching from a diagonal angle (trains directional flicks / cuts)
	 *   - FAR_BACK: Deep in own half, far from attacker (trains long carries and timing)
	 */
	class DribbleState : public RLGC::StateSetter {
	private:
		enum DefenderMode { HEAD_ON, SHADOW, ANGLED, FAR_BACK };

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

			// Randomly pick which team is attacking (dribbling) to learn both sides.
			Team attackingTeam = (rand() % 2 == 0) ? Team::BLUE : Team::ORANGE;
			// sideMult: +1 means attacking toward ORANGE goal (+Y), -1 means toward BLUE goal (-Y).
			float sideMult = (attackingTeam == Team::BLUE) ? 1.0f : -1.0f;

			// Roll a random defender mode (equal 25% weights).
			DefenderMode defMode = static_cast<DefenderMode>(rand() % 4);

			// ------------------------------------------------------------------
			// 1. ATTACKER SETUP
			// ------------------------------------------------------------------
			// Position the attacker somewhere between mid-field and ~40% into opponent's half.
			// X is randomized for varied field positions.
			float attackerX = RandRange(-2000.f, 2000.f);
			float attackerY = RandRange(-500.f, 2000.f) * sideMult;
			float attackerZ = 17.0f; // On ground

			// Car velocity: controlled forward speed (500-1400 uu/s) toward opponent's goal.
			float carSpeed = RandRange(500.f, 1400.f);
			// Add a slight lateral velocity component for less-perfect carries.
			float carVelX = RandRange(-150.f, 150.f);
			float carVelY = carSpeed * sideMult;

			// ------------------------------------------------------------------
			// 2. BALL ON ROOF
			// ------------------------------------------------------------------
			// Rocket League Octane: ground clearance ~17, hitbox half-height ~17,
			// so roof is at ~34 units above ground. Ball radius ~92.75.
			// Ball center on roof: 17 (ground) + 34 (hitbox center+half) + 92.75 ≈ ~144 Z.
			// In practice the physics settle at around 135-145. We use ~140 and add a tiny
			// upward velocity to let physics settle naturally.
			float ballZ = 140.0f;

			// Slightly imperfect placement to train correction (±15 units X/Y offset from car center).
			float ballX = attackerX + RandRange(-15.f, 15.f);
			float ballY = attackerY + RandRange(-15.f, 15.f);

			// Ball velocity: match the car + small random deviation for a "live" carry.
			float ballVelX = carVelX + RandRange(-50.f, 50.f);
			float ballVelY = carVelY + RandRange(-50.f, 50.f);
			float ballVelZ = RandRange(0.f, 30.f); // Tiny upward to settle gently

			// Small angular velocity for imperfect spin (wobbly dribble).
			float ballAngX = RandRange(-1.0f, 1.0f);
			float ballAngY = RandRange(-1.0f, 1.0f);
			float ballAngZ = RandRange(-0.5f, 0.5f);

			BallState ballState = arena->ball->GetState();
			ballState.pos = Vec(ballX, ballY, ballZ);
			ballState.vel = Vec(ballVelX, ballVelY, ballVelZ);
			ballState.angVel = Vec(ballAngX, ballAngY, ballAngZ);
			arena->ball->SetState(ballState);

			// ------------------------------------------------------------------
			// 3. SETUP CARS
			// ------------------------------------------------------------------
			for (Car* car : cars) {
				CarState carState = car->GetState();

				if (car->team == attackingTeam) {
					// --- ATTACKER (dribbler) ---
					carState.pos = Vec(attackerX, attackerY, attackerZ);
					// Facing opponent's goal.
					carState.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
					carState.vel = Vec(carVelX, carVelY, 0);
					carState.angVel = Vec(0, 0, 0);
					// Enough boost to flick, but not always full.
					carState.boost = RandRange(30.f, 100.f);
				} else {
					// --- DEFENDER ---
					SetupDefender(carState, defMode, sideMult,
						Vec(attackerX, attackerY, attackerZ),
						Vec(carVelX, carVelY, 0));
				}
				car->SetState(carState);
			}
		}

	private:
		void SetupDefender(CarState& state, DefenderMode mode, float sideMult,
			const Vec& attackerPos, const Vec& attackerVel) {

			state.angVel = Vec(0, 0, 0);
			state.boost = (float)(rand() % 101);

			switch (mode) {
			case HEAD_ON: {
				// Defender directly ahead of attacker, facing them.
				// Distance: 1500–2500 units ahead (toward opponent's goal from attacker perspective).
				float dist = RandRange(1500.f, 2500.f);
				state.pos = Vec(
					attackerPos.x + RandRange(-300.f, 300.f),
					attackerPos.y + dist * sideMult,
					17.0f
				);
				// Facing TOWARD the attacker (opposite direction of sideMult).
				state.rotMat = RotMat::LookAt(Vec(0, -sideMult, 0), Vec(0, 0, 1));
				// Driving toward the attacker for a committed challenge.
				state.vel = Vec(0, RandRange(-800.f, -200.f) * sideMult, 0);
				break;
			}
			case SHADOW: {
				// Defender ahead of attacker, facing own goal (retreating / shadowing).
				float dist = RandRange(1000.f, 2000.f);
				state.pos = Vec(
					attackerPos.x + RandRange(-200.f, 200.f),
					attackerPos.y + dist * sideMult,
					17.0f
				);
				// Facing own goal (same direction as sideMult = retreating).
				state.rotMat = RotMat::LookAt(Vec(0, sideMult, 0), Vec(0, 0, 1));
				// Retreating at slightly less speed than attacker.
				state.vel = attackerVel * RandRange(0.6f, 0.9f);
				break;
			}
			case ANGLED: {
				// Defender approaching from a diagonal (±30-60° off center).
				float dist = RandRange(1200.f, 2200.f);
				float angleDeg = RandRange(30.f, 60.f) * RandSign();
				float angleRad = angleDeg * (3.14159265f / 180.0f);

				float offsetX = dist * sinf(angleRad);
				float offsetY = dist * cosf(angleRad) * sideMult;

				state.pos = Vec(
					attackerPos.x + offsetX,
					attackerPos.y + offsetY,
					17.0f
				);
				// Facing toward the attacker.
				Vec toAttacker = (attackerPos - state.pos);
				toAttacker.z = 0;
				float len = toAttacker.Length();
				if (len > 0.1f) {
					toAttacker = toAttacker * (1.0f / len); // Normalize
					state.rotMat = RotMat::LookAt(toAttacker, Vec(0, 0, 1));
				} else {
					state.rotMat = RotMat::LookAt(Vec(0, -sideMult, 0), Vec(0, 0, 1));
				}
				// Driving toward attacker at moderate speed.
				state.vel = toAttacker * RandRange(400.f, 1000.f);
				break;
			}
			case FAR_BACK: {
				// Defender deep in own half, far from attacker. Trains long carries and timing.
				float defenderY = RandRange(3000.f, 4500.f) * sideMult;
				state.pos = Vec(
					RandRange(-1500.f, 1500.f),
					defenderY,
					17.0f
				);
				// Facing the attacker / field.
				state.rotMat = RotMat::LookAt(Vec(0, -sideMult, 0), Vec(0, 0, 1));
				// Mostly stationary or slowly retreating.
				state.vel = Vec(0, RandRange(-200.f, 200.f) * sideMult, 0);
				break;
			}
			}
		}
	};
}
