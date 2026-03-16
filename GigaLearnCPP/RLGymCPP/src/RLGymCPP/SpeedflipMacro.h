#pragma once
#include "Framework.h"
#include <cmath>

// Physics-based speedflip kickoff macro, matching the RLBot bot.py state machine.
// Instead of fixed tick windows, this uses angle and distance thresholds for
// robust alignment across all spawn positions and call rates.
//
// ALL states update on the same UPDATE_INTERVAL (8 tick) cadence, matching
// bot.py's ~15 Hz get_output() call rate. Controls are cached and held between
// updates. This ensures the same jump timing, dodge registration, and steering
// dynamics as the proven RLBot version.
//
// State flow: drive → align → first_jump → release_jump → dodge → cancel → done

struct SpeedflipMacro {
	// How far to drive straight before steering (UU). Matches bot.py INITIAL_DRIVE_DIST.
	static constexpr float INITIAL_DRIVE_DIST = 75.0f;

	// Maximum ticks before force-ending the macro as a safety net.
	static constexpr int MAX_TICKS = 300;

	// Update interval: only recalculate controls every N ticks (matches bot.py ~15 Hz).
	static constexpr int UPDATE_INTERVAL = 8;

	enum class State {
		Inactive,
		Drive,         // Drive straight to build speed (~75 UU)
		Align,         // Steer to pre-align, then drive straight to total distance
		FirstJump,     // Hold jump until airborne (checked every UPDATE_INTERVAL)
		ReleaseJump,   // Release jump for one full UPDATE_INTERVAL cycle
		Dodge,         // Diagonal dodge (held one full UPDATE_INTERVAL cycle)
		Cancel,        // Cancel flip rotation, steer toward ball
		Done
	};

	static const char* StateName(State s) {
		switch (s) {
			case State::Inactive:    return "Inactive";
			case State::Drive:       return "Drive";
			case State::Align:       return "Align";
			case State::FirstJump:   return "FirstJump";
			case State::ReleaseJump: return "ReleaseJump";
			case State::Dodge:       return "Dodge";
			case State::Cancel:      return "Cancel";
			case State::Done:        return "Done";
			default: return "?";
		}
	}


	State state         = State::Inactive;
	int   tickCounter   = 0;     // Safety counter (total ticks since start)
	int   updateCounter = 0;     // Ticks since last state machine update
	int   releaseCount  = 0;     // Ticks spent in ReleaseJump (need >=3 for RocketSim)
	int   dodgeCount    = 0;     // Ticks spent holding dodge controls
	float direction     = -1.0f; // +1 = flip right, -1 = flip left
	float yawStrength   = 0.8f;  // Scales yaw during cancel
	float alignSteer    = 0.2f;  // Reduced steer during alignment (RocketSim ~5x faster)

	// Cached controls — held between state machine updates
	CarControls cachedControls = {};

	// Captured at kickoff start
	Vec   initialForward  = {};   // Car's forward direction at spawn
	Vec   initialPosition = {};   // Car's position at spawn

	// Per-spawn parameters
	float totalDriveDistance = 290.0f;  // UU to travel before jumping
	float initialAngle      = 0.0f;    // Target alignment angle (radians)

	bool IsActive() const {
		return state != State::Inactive && state != State::Done;
	}

	void Reset() {
		state             = State::Inactive;
		tickCounter       = 0;
		updateCounter     = 0;
		releaseCount      = 0;
		dodgeCount        = 0;
		direction         = -1.0f;
		yawStrength       = 0.8f;
		alignSteer        = 0.2f;
		cachedControls    = {};
		initialForward    = {};
		initialPosition   = {};
		totalDriveDistance = 290.0f;
		initialAngle      = 0.0f;
	}

	// Compute flip direction and parameters from car kickoff state.
	void Start(const CarState& carState, const Vec& /*ballPos*/) {
		tickCounter   = 0;
		dodgeCount    = 0;
		releaseCount  = 0;
		updateCounter = UPDATE_INTERVAL; // Force immediate first update

		// Capture initial state — use car's ACTUAL forward direction (matches bot.py)
		Vec carPos = carState.pos;
		initialPosition = carPos;

		// Use rotation matrix forward, projected to 2D and normalized
		Vec fwd = carState.rotMat.forward;
		float fwdLen2D = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
		if (fwdLen2D > 0.001f) {
			initialForward = Vec(fwd.x / fwdLen2D, fwd.y / fwdLen2D, 0.0f);
		} else {
			initialForward = Vec(1.0f, 0.0f, 0.0f);
		}

		float absX = std::abs(carPos.x);

		if (absX < 25.0f) {
			// Center kickoff
			direction         = -1.0f;
			yawStrength       = 1.0f;
			alignSteer        = 0.2f;  // RocketSim steer ~5x faster than real game
			totalDriveDistance = 290.0f;
			initialAngle      = 3.14159265f / 22.0f;
		} else if (absX < 500.0f) {
			// Near kickoff — steer=0.7 balances between 0.6 (too little) and 0.8 (too much).
			// Coast from alignment to driveDist=350 carries angular momentum.
			direction         = -(carPos.x > 0.0f ? 1.0f : -1.0f);
			yawStrength       = 0.6f;
			alignSteer        = 0.68f;
			totalDriveDistance = 350.0f;
			initialAngle      = 3.14159265f / 16.0f;  // 11.25°
		} else {
			// Corner kickoff — steer=0.6 builds angle fast enough to hit π/16 at t≈57
			// (dist≈243), then coast to driveDist=300 carries ~4° via angular momentum.
			// Total rotation ≈ 18° at dodge, jump 80UU earlier than steer=0.4 approach.
			direction         = (carPos.x > 0.0f ? 1.0f : -1.0f);
			yawStrength       = 0.4f;
			alignSteer        = 0.6f;
			totalDriveDistance = 300.0f;
			initialAngle      = 3.14159265f / 16.0f;  // 11.25° — reached at dist≈243
		}

		// Correct for team facing direction
		if (carPos.y < 0.0f)
			direction = -direction;

		state = State::Drive;
		RG_LOG("[MACRO START] pos=(" << carPos.x << "," << carPos.y << ") absX=" << absX
			<< " dir=" << direction << " yawStr=" << yawStrength
			<< " driveDist=" << totalDriveDistance << " angle=" << initialAngle
			<< " fwd=(" << initialForward.x << "," << initialForward.y << ")");
	}

	// Get controls based on the current car physics state.
	// HYBRID TIMING: Drive/Align/Cancel use 8-tick cadence (matching bot.py ground behavior).
	// FirstJump/ReleaseJump/Dodge use per-tick resolution (keeps car low for proper speedflip).
	CarControls GetControls(const CarState& carState) {
		if (state == State::Inactive || state == State::Done)
			return {};

		tickCounter++;
		if (tickCounter > MAX_TICKS) {
			state = State::Done;
			return {};
		}

		// Per-tick states: these need tick-level precision for jump/dodge timing
		bool isPerTickState = (state == State::FirstJump ||
		                       state == State::ReleaseJump ||
		                       state == State::Dodge);

		// For non-per-tick states, only update every UPDATE_INTERVAL ticks
		if (!isPerTickState) {
			updateCounter++;
			if (updateCounter < UPDATE_INTERVAL) {
				return cachedControls;  // Hold previous controls
			}
			updateCounter = 0;
		}

		// --- Compute current physics ---
		Vec curForward = Vec(
			carState.rotMat.forward.x,
			carState.rotMat.forward.y,
			0.0f
		);
		float fwdLen = std::sqrt(curForward.x * curForward.x + curForward.y * curForward.y);
		if (fwdLen > 0.001f) {
			curForward.x /= fwdLen;
			curForward.y /= fwdLen;
		}

		Vec curPos = carState.pos;
		float dx = curPos.x - initialPosition.x;
		float dy = curPos.y - initialPosition.y;
		float distance = std::sqrt(dx * dx + dy * dy);

		bool isOnGround = carState.isOnGround;

		// --- Diagnostic: log at state machine updates ---
		float yawDeg = std::atan2(curForward.y, curForward.x) * (180.0f / 3.14159265f);
		float speed = carState.vel.Length();
		float angVelZ = carState.angVel.z;
		// Only log at update boundaries (not every per-tick for jump states)
		if (!isPerTickState || state == State::FirstJump && !isOnGround ||
		    state == State::Dodge && dodgeCount == 0) {
			RG_LOG("[MACRO t=" << tickCounter << " st=" << StateName(state)
				<< "] pos=(" << curPos.x << "," << curPos.y << "," << curPos.z << ")"
				<< " yaw=" << yawDeg << "deg dist=" << distance
				<< " spd=" << speed << " angZ=" << angVelZ
				<< " gnd=" << isOnGround);
		}

		CarControls ctrl = {};

		// --- State machine ---

		if (state == State::Drive) {
			if (distance < INITIAL_DRIVE_DIST) {
				ctrl.throttle = 1.0f;
				ctrl.boost    = true;
				cachedControls = ctrl;
				return ctrl;
			} else {
				state = State::Align;
				// Fall through to Align
			}
		}

		if (state == State::Align) {
			float dot = curForward.x * initialForward.x + curForward.y * initialForward.y;
			if (dot > 1.0f) dot = 1.0f;
			if (dot < -1.0f) dot = -1.0f;
			float angle = std::acos(dot);

			if (angle < initialAngle) {
				// Still aligning — steer to build angle offset
				ctrl.throttle = 1.0f;
				ctrl.steer    = alignSteer * direction;  // Reduced: RocketSim steers ~5x faster
				ctrl.boost    = true;
				cachedControls = ctrl;
				return ctrl;
			} else if (distance < totalDriveDistance) {
				// Angle is good but haven't driven far enough — drive straight
				ctrl.throttle = 1.0f;
				ctrl.boost    = true;
				cachedControls = ctrl;
				return ctrl;
			} else {
				state = State::FirstJump;
				// Fall through to FirstJump
			}
		}

		if (state == State::FirstJump) {
			// Per-tick: hold jump until airborne
			if (!isOnGround) {
				state = State::ReleaseJump;
				releaseCount = 0;
				ctrl.throttle = 1.0f;
				ctrl.boost    = true;
				// No jump = release
				return ctrl;
			} else {
				ctrl.throttle = 1.0f;
				ctrl.jump     = true;
				ctrl.boost    = true;
				return ctrl;
			}
		}

		if (state == State::ReleaseJump) {
			// Per-tick: hold jump released for 3 ticks before dodging
			releaseCount++;
			if (releaseCount < 3) {
				ctrl.throttle = 1.0f;
				ctrl.boost    = true;
				return ctrl;
			}
			state = State::Dodge;
			// Fall through to Dodge
		}

		if (state == State::Dodge) {
			// Per-tick: hold dodge controls for UPDATE_INTERVAL ticks
			dodgeCount++;
			if (dodgeCount == 1) {
				// First tick: send the actual dodge inputs
				ctrl.boost = true;
				ctrl.jump  = true;
				ctrl.pitch = -1.0f;
				ctrl.yaw   = -direction;
				cachedControls = ctrl;
				RG_LOG("[MACRO DODGE] dir=" << direction << " yaw=" << -direction
					<< " pos=(" << curPos.x << "," << curPos.y << "," << curPos.z << ") t=" << tickCounter);
				return ctrl;
			} else if (dodgeCount < UPDATE_INTERVAL) {
				// Subsequent ticks: keep holding the same dodge controls
				return cachedControls;
			} else {
				// Done holding — transition to cancel
				state = State::Cancel;
				updateCounter = UPDATE_INTERVAL; // Force immediate first cancel update
				// Fall through to Cancel
			}
		}

		if (state == State::Cancel) {
			// Cancel flip rotation and correct heading toward ball.
			if (isOnGround) {
				state = State::Done;
				cachedControls = {};
				RG_LOG("[MACRO DONE] pos=(" << curPos.x << "," << curPos.y << ") dist=" << distance << " t=" << tickCounter);
				return {};
			}
			float speed = carState.vel.Length();
			ctrl.throttle = 1.0f;
			ctrl.boost    = (speed < 2295.0f);
			ctrl.pitch    = 1.0f;
			ctrl.yaw      = -yawStrength * direction;  // Negated: RocketSim air yaw inverted
			cachedControls = ctrl;
			return ctrl;
		}

		// Fallback
		state = State::Done;
		return {};
	}
};
