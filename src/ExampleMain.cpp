#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/OBSBuilders/DefaultObs.h>
#include <RLGymCPP/OBSBuilders/AdvancedObs.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

#include "rewards.h"
#include "GoalieState.h"
#include "ShadowDefenseState.h"
#include <iostream>
#include <string>

using namespace GGL; // GigaLearn
using namespace RLGC; // RLGymCPP

// --- CONFIGURATION ---
enum TrainingStage {
	EARLY,  // 0-100M steps: Learn to touch ball
	MID,    // 100M-1B steps: Learn to score
	LATE,   // 1B-5B steps: Advanced mechanics & kickoffs
	MASTER  // 5B+ steps: Boost management, defense, and strategy
};

// ** CHANGE THIS TO SWITCH STAGES **
TrainingStage CURRENT_STAGE;

// Create the RLGymCPP environment for each of our games
EnvCreateResult EnvCreateFunc(int index) {
	std::vector<WeightedReward> rewards;

	if (CURRENT_STAGE == TrainingStage::EARLY) {
		rewards.push_back(WeightedReward(new InAirReward(), 0.15f));
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 6.0f));
		rewards.push_back(WeightedReward(new CustomFaceBallReward(), 1.0f));
		rewards.push_back(WeightedReward(new TouchReward(), 50.0f));
		// Small goal orientation even in early to prevent circular driving
		rewards.push_back(WeightedReward(new ZeroSumReward(new CustomVelocityBallToGoalReward(), 0.0f), 2.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new GoalReward(0.0f), 0.0f), 100.0f));
	} else if (CURRENT_STAGE == TrainingStage::MID) {
		rewards.push_back(WeightedReward(new InAirReward(), 0.15f));
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 5.0f));
		rewards.push_back(WeightedReward(new CustomFaceBallReward(), 0.5f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new CustomVelocityBallToGoalReward(), 0.0f), 10.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new GoalReward(0.0f), 0.0f), 1000.0f));
		rewards.push_back(WeightedReward(new AdvancedTouchReward(0.05f, 1.0f, 300.0f), 75.0f));
	} else if (CURRENT_STAGE == TrainingStage::LATE) {
		rewards.push_back(WeightedReward(new SpeedTowardBallReward(), 5.0f));
		rewards.push_back(WeightedReward(new JumpTouchReward(), 200.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new CustomVelocityBallToGoalReward(), 0.0f), 20.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new GoalReward(0.0f), 0.0f), 1500.0f));
		rewards.push_back(WeightedReward(new AdvancedTouchReward(0.05f, 1.0f, 300.0f), 75.0f));
		rewards.push_back(WeightedReward(new KickoffReward(), 20.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new FirstTouchKickoffReward(), 0.0f), 500.0f));
		// WavedashReward removed due to exploiting normal flips
	} else { // MASTER
		// Core scoring and defense (Zero-sum scales down slightly to balance with resource rewards)
		rewards.push_back(WeightedReward(new ZeroSumReward(new GoalReward(0.0f), 0.0f), 1200.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new CustomVelocityBallToGoalReward(), 0.0f), 20.0f));
		
		// Refined mechanics & Kickoffs
		rewards.push_back(WeightedReward(new AdvancedTouchReward(0.05f, 1.0f, 300.0f), 75.0f)); 
		rewards.push_back(WeightedReward(new JumpTouchReward(), 150.0f)); 
		rewards.push_back(WeightedReward(new KickoffReward(), 20.0f));
		rewards.push_back(WeightedReward(new ZeroSumReward(new FirstTouchKickoffReward(), 0.0f), 400.0f));
		// WavedashReward removed due to exploiting normal flips
		
		// Resource Management & Strategy
		rewards.push_back(WeightedReward(new PickupBoostReward(), 10.0f)); // Path over pads
		rewards.push_back(WeightedReward(new DemoReward(), 50.0f)); // Encourage physical disruption
	}

	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(10), // 10 seconds
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

	if (CURRENT_STAGE == TrainingStage::EARLY) {
		result.stateSetter = new RandomState(true, true, false); 
	} else if (CURRENT_STAGE == TrainingStage::MID) {
		result.stateSetter = new CombinedState({
			{new RandomState(true, true, false), 0.4f},
			{new KickoffState(), 0.4f},
			{new GoalieState(), 0.1f},
			{new ShadowDefenseState(), 0.1f}
		});
	} else if (CURRENT_STAGE == TrainingStage::LATE) {
		result.stateSetter = new CombinedState({
			{new RandomState(true, true, false), 0.15f},
			{new KickoffState(), 0.65f},
			{new GoalieState(), 0.1f},
			{new ShadowDefenseState(), 0.1f}
		});
	} else { // MASTER
		// In high level 1v1, kickoffs are critical, but so is awkward field positioning.
		result.stateSetter = new CombinedState({
			{new RandomState(true, true, false), 0.2f}, // Slightly higher random to test defense
			{new KickoffState(), 0.5f},
			{new GoalieState(), 0.15f},
			{new ShadowDefenseState(), 0.15f}
		});
	}

	result.terminalConditions = terminalConditions;
	result.rewards = rewards;

	result.arena = arena;

	return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
	// To prevent expensive metrics from eating at performance, we will only run them on 1/4th of steps
	bool doExpensiveMetrics = (rand() % 4) == 0;

	// Persistence for mechanics metrics
	// Indexing: [game_idx][player_idx]
	static std::vector<std::vector<uint64_t>> lastTouchTicks;
	static std::vector<std::vector<Vec>> lastBallVels;

	if (lastTouchTicks.size() != states.size() || (!states.empty() && lastTouchTicks[0].size() != states[0].players.size())) {
		lastTouchTicks.assign(states.size(), std::vector<uint64_t>(states[0].players.size(), 0));
		lastBallVels.assign(states.size(), std::vector<Vec>(states[0].players.size(), Vec()));
	}

	// Add our metrics
	for (int g = 0; g < states.size(); g++) {
		auto& state = states[g];

		for (int p = 0; p < state.players.size(); p++) {
			auto& player = state.players[p];


			if (doExpensiveMetrics) {
				report.AddAvg("Player/In Air Ratio", !player.isOnGround);
				report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
				report.AddAvg("Player/Demoed Ratio", player.isDemoed);
				
				// --- Baseline Mechanics Trackers ---
				report.AddAvg("Mechanics/Jumping Ratio", player.isJumping);
				report.AddAvg("Mechanics/Flipping Ratio", player.isFlipping);
				report.AddAvg("Mechanics/Supersonic Ratio", player.isSupersonic);

				report.AddAvg("Player/Speed", player.vel.Length());
				Vec dirToBall = (state.ball.pos - player.pos).Normalized();
				report.AddAvg("Player/Speed Towards Ball", RS_MAX(0, player.vel.Dot(dirToBall)));

				report.AddAvg("Player/Boost", player.boost);

				if (player.ballTouchedStep)
					report.AddAvg("Player/Touch Height", state.ball.pos.z);
			}

			// --- New Transition Logic Mechanics ---
			if (player.ballTouchedStep) {
				// 1. Ball Speed Change (Impact)
				Vec curBallVel = state.ball.vel;
				Vec prevBallVel = lastBallVels[g][p];
				float speedChange = (curBallVel - prevBallVel).Length();
				report.AddAvg("Mechanics/Ball Speed Change on Touch", speedChange);

				// 2. Time Between Touches (Temporal)
				uint64_t curTick = state.lastTickCount;
				uint64_t prevTick = lastTouchTicks[g][p];
				if (prevTick > 0) {
					uint64_t diff = curTick - prevTick;
					report.AddAvg("Mechanics/Time Between Touches", (float)diff);
				}

				lastTouchTicks[g][p] = curTick;
			}

			lastBallVels[g][p] = state.ball.vel;
		}

		if (state.goalScored) {
			report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
			report.Add("Game/Total Goals", 1.0f);
		}
	}

	// Reward Ratio Logic (Alternative approach: Use the data already in the report)
	// The Learner automatically adds "Rewards/TouchReward" etc. to the report.
	// We can use those values to compute the ratio for WandB.
	float touchRewardVal = report.GetAvg("Rewards/TouchReward");
	float advTouchRewardVal = report.GetAvg("Rewards/AdvancedTouchReward");
	
	// Total reward is reported as "Policy Reward" by PPOLearner
	float totalReward = report.GetAvg("Policy Reward");
	
	if (totalReward > 0) {
		float touchRatio = (touchRewardVal + advTouchRewardVal) / totalReward;
		report.AddAvg("Reward/Touch Ratio", touchRatio);
	}
}

int main(int argc, char* argv[]) {
	// Simple argument parsing
	bool render = false;
	bool metrics = true;
	bool stageSet = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--render") {
			render = true;
			metrics = false;
		} else if (arg == "--stage=early") {
			CURRENT_STAGE = TrainingStage::EARLY;
			stageSet = true;
		} else if (arg == "--stage=mid") {
			CURRENT_STAGE = TrainingStage::MID;
			stageSet = true;
		} else if (arg == "--stage=late") {
			CURRENT_STAGE = TrainingStage::LATE;
			stageSet = true;
		} else if (arg == "--stage=master") {
			CURRENT_STAGE = TrainingStage::MASTER;
			stageSet = true;
		}
	}

	if (!stageSet) {
		std::cerr << "Error: Mandatory argument --stage=[early|mid|late|master] is missing or invalid." << std::endl;
		return EXIT_FAILURE;
	}

	// Initialize RocketSim with collision meshes
	// Change this path to point to your meshes!
	RocketSim::Init("./collision_meshes");

	// Make configuration for the learner
	LearnerConfig cfg = {};

	cfg.deviceType = LearnerDeviceType::GPU_CUDA;

	cfg.tickSkip = 8;
	cfg.actionDelay = cfg.tickSkip - 1; // Normal value in other RLGym frameworks

	// --- Common Settings (Defaults) ---
	cfg.numGames = 512;
	cfg.ppo.epochs = 3; // keep at 2 or 3 (community suggestion)

	cfg.sendMetrics = metrics;
	cfg.metricsProjectName = "gigalearncpp";
	cfg.metricsGroupName = "MattBot-v3";

	cfg.renderMode = render; // Fixed infinite loop in source, now we can render and train!
	cfg.renderTimeScale = 1.0f;
	std::cout << "Debug: renderTimeScale set to " << cfg.renderTimeScale << std::endl;
	std::cout << "Debug: renderMode set to " << (cfg.renderMode ? "true" : "false") << std::endl;
	std::cout << "Debug: sendMetrics set to " << (cfg.sendMetrics ? "true" : "false") << std::endl;

	cfg.ppo.sharedHead.layerSizes = {};
	cfg.ppo.policy.layerSizes = { 512, 512, 512, 512 };
	cfg.ppo.critic.layerSizes = { 512, 512, 512, 512 };

	auto optim = ModelOptimType::ADAM;
	cfg.ppo.policy.optimType = optim;
	cfg.ppo.critic.optimType = optim;

	auto activation = ModelActivationType::RELU;
	cfg.ppo.policy.activationType = activation;
	cfg.ppo.critic.activationType = activation;

	bool addLayerNorm = true;
	cfg.ppo.policy.addLayerNorm = addLayerNorm;
	cfg.ppo.critic.addLayerNorm = addLayerNorm;

	// --- Skill Tracker (ELO) Settings ---
	cfg.skillTracker.enabled = true;
	cfg.skillTracker.numArenas = 16;      // How many games to run at once for ELO testing
	cfg.skillTracker.updateInterval = 300; // Run ELO matches every 10 iterations
	cfg.skillTracker.simTime = 60.0f;     // How long each ELO match lasts (seconds)
	cfg.savePolicyVersions = true;        // Required to save snapshots for ELO testing
	

	// --- Stage-Specific Settings ---
	// Note: Mini-batch size should be between 1/2 and 1/8 of batch size
	if (CURRENT_STAGE == EARLY) {
		cfg.checkpointFolder = "checkpoints/early";
		cfg.metricsRunName = "early";
		cfg.tsPerVersion = 10'000'000;        // Save a new version every 25M steps
		cfg.tsPerSave = 50'000'000;
		// cfg.timestepLimit = 50'000'000; // Handled by auto_train.py
		cfg.ppo.tsPerItr = 50000;
		cfg.ppo.batchSize = 50000;
		cfg.ppo.miniBatchSize = 25000;
		cfg.ppo.policyLR = 2e-4;
		cfg.ppo.criticLR = 2e-4;
		cfg.ppo.gaeGamma = 0.99f;
		cfg.ppo.entropyScale = 0.035f;
	} else if (CURRENT_STAGE == MID) {
		cfg.checkpointFolder = "checkpoints/mid";
		cfg.metricsRunName = "mid";
		cfg.tsPerVersion = 100'000'000;
		cfg.tsPerSave = 250'000'000;
		// cfg.timestepLimit = 2'000'000'000; // Handled by auto_train.py
		cfg.ppo.tsPerItr = 100000;
		cfg.ppo.batchSize = 100000;
		cfg.ppo.miniBatchSize = 25000;
		cfg.ppo.policyLR = 1.5e-4;
		cfg.ppo.criticLR = 1.5e-4;
		cfg.ppo.gaeGamma = 0.993f;
		cfg.ppo.entropyScale = 0.0225f;
	} else if (CURRENT_STAGE == LATE) {
		cfg.checkpointFolder = "checkpoints/late";
		cfg.metricsRunName = "late";
		cfg.tsPerVersion = 100'000'000;
		cfg.tsPerSave = 500'000'000;
		// cfg.timestepLimit = 5'000'000'000;
		cfg.ppo.tsPerItr = 200'000;
		cfg.ppo.batchSize = 200'000;
		cfg.ppo.miniBatchSize = 50'000;
		cfg.ppo.policyLR = 1.2e-4;
		cfg.ppo.criticLR = 1.2e-4;
		cfg.ppo.gaeGamma = 0.995f;
		cfg.ppo.entropyScale = 0.01f;
	} else { // MASTER
		cfg.checkpointFolder = "checkpoints/master";
		cfg.metricsRunName = "master";
		cfg.tsPerVersion = 100'000'000;
		cfg.tsPerSave = 500'000'000;
		cfg.ppo.tsPerItr = 300'000;
		cfg.ppo.batchSize = 300'000;
		cfg.ppo.miniBatchSize = 60'000;
		cfg.ppo.policyLR = 1.0e-4;
		cfg.ppo.criticLR = 1.0e-4;
		cfg.ppo.gaeGamma = 0.997f;
		cfg.ppo.entropyScale = 0.005f;
	}

	// Make the learner with the environment creation function and the config we just made
	Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);

	// Start learning!
	learner->Start();

	return EXIT_SUCCESS;
}
