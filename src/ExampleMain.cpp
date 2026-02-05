#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/OBSBuilders/DefaultObs.h>
#include <RLGymCPP/OBSBuilders/AdvancedObs.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

#include "rewards.h"
#include <iostream>

using namespace GGL; // GigaLearn
using namespace RLGC; // RLGymCPP

// --- CONFIGURATION ---
enum TrainingStage {
	EARLY, // 0-100M steps: Learn to touch ball
	MID,   // 100M-1B steps: Learn to score
	LATE   // 1B+ steps: Advanced mechanics & kickoffs
};

// ** CHANGE THIS TO SWITCH STAGES **
const TrainingStage CURRENT_STAGE = TrainingStage::MID;

// Create the RLGymCPP environment for each of our games
EnvCreateResult EnvCreateFunc(int index) {
	std::vector<WeightedReward> rewards;

	if (CURRENT_STAGE == TrainingStage::EARLY) {
		rewards.push_back(WeightedReward(new InAirReward(), 0.15f));
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 5.0f));
		rewards.push_back(WeightedReward(new CustomFaceBallReward(), 0.5f));
		rewards.push_back(WeightedReward(new TouchReward(), 50.0f));
	} else if (CURRENT_STAGE == TrainingStage::MID) {
		rewards.push_back(WeightedReward(new InAirReward(), 0.15f));
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 5.0f));
		rewards.push_back(WeightedReward(new CustomFaceBallReward(), 0.5f));
		rewards.push_back(WeightedReward(new CustomVelocityBallToGoalReward(), 10.0f));
		rewards.push_back(WeightedReward(new GoalReward(), 1000.0f));
		rewards.push_back(WeightedReward(new AdvancedTouchReward(0.05f, 1.0f, 300.0f), 75.0f));
	} else {
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 5.0f));
		rewards.push_back(WeightedReward(new JumpTouchReward(), 200.0f));
		rewards.push_back(WeightedReward(new CustomFaceBallReward(), 0.5f));
		rewards.push_back(WeightedReward(new CustomVelocityBallToGoalReward(), 20.0f));
		rewards.push_back(WeightedReward(new GoalReward(), 1500.0f));
		rewards.push_back(WeightedReward(new AdvancedTouchReward(0.05f, 1.0f, 300.0f), 75.0f));
		rewards.push_back(WeightedReward(new KickoffReward(), 10.0f));
		rewards.push_back(WeightedReward(new FirstTouchKickoffReward(), 500.0f));
	}

	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(30), // 30 seconds
		new GoalScoreCondition()
	};

	// Make the arena
	int playersPerTeam = 1;
	auto arena = Arena::Create(GameMode::SOCCAR);
	for (int i = 0; i < playersPerTeam; i++) {
		arena->AddCar(Team::BLUE);
		arena->AddCar(Team::ORANGE);
	}

	EnvCreateResult result = {};
	result.actionParser = new DefaultAction();
	result.obsBuilder = new AdvancedObs();
	result.stateSetter = new KickoffState();
	result.terminalConditions = terminalConditions;
	result.rewards = rewards;

	result.arena = arena;

	return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
	// To prevent expensive metrics from eating at performance, we will only run them on 1/4th of steps
	// This doesn't really matter unless you have expensive metrics (which this example doesn't)
	bool doExpensiveMetrics = (rand() % 4) == 0;

	// Add our metrics
	for (auto& state : states) {
		if (doExpensiveMetrics) {
			for (auto& player : state.players) {
				report.AddAvg("Player/In Air Ratio", !player.isOnGround);
				report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
				report.AddAvg("Player/Demoed Ratio", player.isDemoed);

				report.AddAvg("Player/Speed", player.vel.Length());
				Vec dirToBall = (state.ball.pos - player.pos).Normalized();
				report.AddAvg("Player/Speed Towards Ball", RS_MAX(0, player.vel.Dot(dirToBall)));

				report.AddAvg("Player/Boost", player.boost);

				if (player.ballTouchedStep)
					report.AddAvg("Player/Touch Height", state.ball.pos.z);
			}
		}

		if (state.goalScored)
			report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
	}
}

int main(int argc, char* argv[]) {
	// Initialize RocketSim with collision meshes
	// Change this path to point to your meshes!
	RocketSim::Init("./collision_meshes");

	// Make configuration for the learner
	LearnerConfig cfg = {};

	cfg.deviceType = LearnerDeviceType::GPU_CUDA;

	cfg.tickSkip = 8;
	cfg.actionDelay = cfg.tickSkip - 1; // Normal value in other RLGym frameworks

	// --- Common Settings (Defaults) ---
	cfg.numGames = 256;
	cfg.ppo.epochs = 2;
	cfg.ppo.entropyScale = 0.01f;

	cfg.sendMetrics = true;
	cfg.metricsProjectName = "gigalearncpp";
	cfg.metricsGroupName = "MattBot-v1";

	cfg.renderMode = false; // Fixed infinite loop in source, now we can render and train!
	cfg.renderTimeScale = 1.0f;
	std::cout << "Debug: renderTimeScale set to " << cfg.renderTimeScale << std::endl;

	cfg.ppo.sharedHead.layerSizes = {};
	cfg.ppo.policy.layerSizes = { 512, 512, 512 };
	cfg.ppo.critic.layerSizes = { 512, 512, 512 };

	// --- Skill Tracker (ELO) Settings ---
	cfg.skillTracker.enabled = true;
	cfg.skillTracker.numArenas = 16;      // How many games to run at once for ELO testing
	cfg.skillTracker.updateInterval = 300; // Run ELO matches every 10 iterations
	cfg.skillTracker.simTime = 60.0f;     // How long each ELO match lasts (seconds)
	cfg.savePolicyVersions = true;        // Required to save snapshots for ELO testing
	

	// --- Stage-Specific Settings ---
	if (CURRENT_STAGE == EARLY) {
		cfg.checkpointFolder = "checkpoints/early";
		cfg.metricsRunName = "early";
		cfg.tsPerVersion = 25'000'000;        // Save a new version every 25M steps
		cfg.tsPerSave = 50'000'000;
		cfg.timestepLimit = 100'000'000;
		cfg.ppo.tsPerItr = 50000;
		cfg.ppo.batchSize = 50000;
		cfg.ppo.miniBatchSize = 25000;
		cfg.ppo.policyLR = 2e-4;
		cfg.ppo.criticLR = 2e-4;
	} else if (CURRENT_STAGE == MID) {
		cfg.checkpointFolder = "checkpoints/mid";
		cfg.metricsRunName = "mid";
		cfg.tsPerVersion = 100'000'000;
		cfg.tsPerSave = 250'000'000;
		cfg.timestepLimit = 2'000'000'000;
		cfg.ppo.tsPerItr = 100000;
		cfg.ppo.batchSize = 100000;
		cfg.ppo.miniBatchSize = 25000;
		cfg.ppo.policyLR = 1.5e-4;
		cfg.ppo.criticLR = 1.5e-4;
		cfg.ppo.gaeGamma = 0.993f;
	} else { // LATE
		cfg.checkpointFolder = "checkpoints/late";
		cfg.metricsRunName = "late";
		cfg.tsPerVersion = 500'000'000;
		cfg.tsPerSave = 1'000'000'000;
		cfg.timestepLimit = 5'000'000'000;
		cfg.ppo.tsPerItr = 200000;
		cfg.ppo.batchSize = 200000;
		cfg.ppo.miniBatchSize = 50000;
		cfg.ppo.policyLR = 1e-4;
		cfg.ppo.criticLR = 1e-4;
		cfg.ppo.gaeGamma = 0.995f;
	}

	// Make the learner with the environment creation function and the config we just made
	Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);

	// Start learning!
	learner->Start();

	return EXIT_SUCCESS;
}
