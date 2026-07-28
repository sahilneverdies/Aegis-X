// Aegis-X VectorAimEngine — Modernized C++20 Aimbot Kinematic & Trajectory Analytics
// Core concepts inspired by and credited to karola3vax / CS2AC (AGPL-3.0).

#include "detection/detection_system.h"

#include "movement_analysis/player_context.h"
#include "movement/movement.h"
#include "sdk/usercmd.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

CConVar<bool> aegisx_aimbot_debug("aegisx_aimbot_debug", FCVAR_NONE, "Show why Aegis-X VectorAimEngine accepts or rejects each damaging shot", false);

#define AIMBOT_DEBUG(...) \
	do \
	{ \
		if (aegisx_aimbot_debug.GetBool()) \
			Msg("[Aegis-X VectorAimEngine] " __VA_ARGS__); \
	} while (0)

namespace
{
	constexpr size_t kMaxCommandStreamBuffer = 128;
	constexpr int kSnapWindowEvaluationTicks = static_cast<int>(ENGINE_FIXED_TICK_RATE * 0.5f);
	constexpr float kMinTargetDistanceCutoff = 100.0f;
	constexpr int kDetectionThresholdCount = 4;
	constexpr auto kEvidenceRetentionWindow = std::chrono::minutes(10);

	enum class VectorAimRule
	{
		None,
		AngularConvergence,
		SubtickSnapReturn,
	};

	inline float ComputeTargetAngularError(const Vector &eyePos, const QAngle &viewAngles, const Vector &targetPos)
	{
		Vector directionVector = targetPos - eyePos;
		if (!detection::IsFinite(directionVector) || directionVector.LengthSqr() < EPSILON)
		{
			return 0.0f;
		}
		directionVector.NormalizeInPlace();
		const float dotVal = std::clamp(DotProduct(detection::AimForward(viewAngles), directionVector), -1.0f, 1.0f);
		return static_cast<float>(std::acos(dotVal) * (180.0 / std::numbers::pi));
	}

	inline float EvaluateSkeletalNodeDivergence(const Vector &eyePos, const QAngle &viewAngles, const Vector &targetBasePos)
	{
		static constexpr float kSkeletalOffsets[] = {8.0f, 46.0f, 64.0f};
		float minDivergenceAngle = 180.0f;
		for (float zOffset : kSkeletalOffsets)
		{
			minDivergenceAngle = (std::min)(minDivergenceAngle, ComputeTargetAngularError(eyePos, viewAngles, targetBasePos + Vector(0.0f, 0.0f, zOffset)));
		}
		return minDivergenceAngle;
	}
} // namespace

namespace detection
{
	void AimbotModule::Load(AnnounceCallback announceCallback, ShotCorrelator *shotCorrelator)
	{
		announce = announceCallback;
		shots = shotCorrelator;
	}

	void AimbotModule::Unload()
	{
		Reset();
		shots = nullptr;
		announce = nullptr;
	}

	void AimbotModule::Reset()
	{
		playerData = {};
		evidence = {};
	}

	void AimbotModule::OnProcessUsercmds(MovementPlayer *player, PlayerCommand *commands, int numCommands)
	{
		if (!IsEligibleHuman(player) || !commands || numCommands <= 0)
		{
			return;
		}

		auto &data = playerData[player->index];
		for (int cmdIdx = 0; cmdIdx < numCommands; ++cmdIdx)
		{
			PlayerCommand &commandStream = commands[cmdIdx];
			if (!commandStream.has_base() || !commandStream.base().has_viewangles())
			{
				continue;
			}
			if (std::any_of(data.commands.rbegin(), data.commands.rend(),
							[&](const AimCommand &storedCmd) { return storedCmd.commandNumber == commandStream.cmdNum; }))
			{
				continue;
			}

			const auto &cmdBase = commandStream.base();
			QAngle clientViewAngles(cmdBase.viewangles().x(), cmdBase.viewangles().y(), cmdBase.viewangles().z());
			const int attackHistoryIdx = commandStream.attack1_start_history_index();
			if (attackHistoryIdx >= 0 && attackHistoryIdx < commandStream.input_history_size() && commandStream.input_history(attackHistoryIdx).has_view_angles())
			{
				const auto &subtickView = commandStream.input_history(attackHistoryIdx).view_angles();
				const QAngle subtickFiringAngles(subtickView.x(), subtickView.y(), subtickView.z());
				if (IsFinite(subtickFiringAngles))
				{
					clientViewAngles = subtickFiringAngles;
				}
			}
			if (!IsFinite(clientViewAngles))
			{
				continue;
			}

			data.commands.push_back({commandStream.cmdNum, cmdBase.client_tick(), -1, clientViewAngles});
			while (data.commands.size() > kMaxCommandStreamBuffer)
			{
				data.commands.pop_front();
			}
		}
	}

	void AimbotModule::OnSetupMove(MovementPlayer *player, PlayerCommand *command, int currentTick)
	{
		if (!IsEligibleHuman(player) || !command || !player->GetPlayerPawn())
		{
			return;
		}
		auto &data = playerData[player->index];
		auto matchIter = std::find_if(data.commands.rbegin(), data.commands.rend(),
								  [&](const AimCommand &storedCmd) { return storedCmd.commandNumber == command->cmdNum; });
		if (matchIter == data.commands.rend())
		{
			return;
		}
		Vector calculatedEyeOrigin;
		player->GetEyeOrigin(&calculatedEyeOrigin);
		if (!IsFinite(calculatedEyeOrigin))
		{
			return;
		}
		matchIter->serverTick = currentTick;
		matchIter->eyePosition = calculatedEyeOrigin;
		matchIter->simulated = true;
		if (data.pending)
		{
			Evaluate(player, data, currentTick);
		}
	}

	void AimbotModule::OnGameFrame(int currentTick)
	{
		for (int playerIdx = 1; playerIdx <= MAXPLAYERS; ++playerIdx)
		{
			if (!playerData[playerIdx].pending)
			{
				continue;
			}
			auto *targetPlayer = g_pCS2ACPlayerManager ? g_pCS2ACPlayerManager->ToPlayer(static_cast<u32>(playerIdx)) : nullptr;
			if (!IsEligibleHuman(targetPlayer))
			{
				playerData[playerIdx] = {};
				continue;
			}
			Evaluate(targetPlayer, playerData[playerIdx], currentTick);
		}
	}

	void AimbotModule::OnPlayerHurt(MovementPlayer *attacker, MovementPlayer *victim, ShotRecord &shot)
	{
		if (!IsEligibleHuman(attacker) || !victim || attacker == victim || shot.playerIndex != attacker->index || shot.aimbotConsumed)
		{
			return;
		}
		shot.aimbotConsumed = true;
		auto &attackerData = playerData[attacker->index];
		if (attackerData.pending)
		{
			Evaluate(attacker, attackerData, shot.fireTick);
			if (attackerData.pending)
			{
				attackerData.pending = false;
			}
		}
		attackerData.pendingShot = shot.commandNumber;
		attackerData.victimIndex = victim->index;
		attackerData.pending = true;
		AIMBOT_DEBUG("%s matched damaging shot command %d at server tick %d.\n", attacker->GetName(), shot.commandNumber, shot.serverTick);
		Evaluate(attacker, attackerData, shot.fireTick);
	}

	bool AimbotModule::Evaluate(MovementPlayer *attacker, AimbotPlayerData &data, int currentTick)
	{
		if (!data.pending || !shots)
		{
			return true;
		}
		auto resetPendingState = [&]()
		{
			data.pendingShot = -1;
			data.victimIndex = -1;
			data.pending = false;
		};

		auto shotCmdIter = std::find_if(data.commands.begin(), data.commands.end(),
								 [&](const AimCommand &cmd) { return cmd.commandNumber == data.pendingShot; });
		if (shotCmdIter == data.commands.end() || !shotCmdIter->simulated || data.victimIndex < 1 || data.victimIndex > MAXPLAYERS)
		{
			resetPendingState();
			return true;
		}

		const TrackedPosition *shotTargetPos = shots->FindPosition(shotCmdIter->serverTick, data.victimIndex);
		const TrackedPosition *shotAttackerPos = shots->FindPosition(shotCmdIter->serverTick, attacker->index);
		if (!shotTargetPos || !shotAttackerPos)
		{
			if (currentTick <= shotCmdIter->serverTick)
			{
				return false;
			}
			AIMBOT_DEBUG("%s rejected because target history is missing for server tick %d.\n", attacker->GetName(), shotCmdIter->serverTick);
			resetPendingState();
			return true;
		}
		if (shotTargetPos->teleported || shotAttackerPos->teleported)
		{
			AIMBOT_DEBUG("%s rejected because a player teleported inside the snap window.\n", attacker->GetName());
			resetPendingState();
			return true;
		}
		if (!AreOpponents(shotAttackerPos->team, shotTargetPos->team))
		{
			AIMBOT_DEBUG("%s rejected because the damaging shot was not against an enemy.\n", attacker->GetName());
			resetPendingState();
			return true;
		}
		const float targetDistance = (shotTargetPos->origin - shotAttackerPos->eyePosition).Length();
		if (!std::isfinite(targetDistance) || targetDistance < kMinTargetDistanceCutoff)
		{
			AIMBOT_DEBUG("%s rejected because target distance %.1f is below %.0f.\n", attacker->GetName(), targetDistance, kMinTargetDistanceCutoff);
			resetPendingState();
			return true;
		}

		bool anomalyFlag = false;
		float maxSnapDelta = 0.0f;
		float optimalPreErr = 0.0f;
		float optimalPostErr = 0.0f;
		VectorAimRule matchedRule = VectorAimRule::None;

		int minFrameTick = shotCmdIter->serverTick - kSnapWindowEvaluationTicks;
		int maxFrameTick = shotCmdIter->serverTick + kSnapWindowEvaluationTicks;

		for (const auto &prevCmd : data.commands)
		{
			if (prevCmd.serverTick < minFrameTick || prevCmd.serverTick >= shotCmdIter->serverTick)
			{
				continue;
			}
			const float snapDelta = AngularDistance(prevCmd.angles, shotCmdIter->angles);
			if (snapDelta > maxSnapDelta)
			{
				maxSnapDelta = snapDelta;
				const TrackedPosition *preTarget = shots->FindPosition(prevCmd.serverTick, data.victimIndex);
				const TrackedPosition *preAttacker = shots->FindPosition(prevCmd.serverTick, attacker->index);
				if (preTarget && preAttacker)
				{
					optimalPreErr = EvaluateSkeletalNodeDivergence(preAttacker->eyePosition, prevCmd.angles, preTarget->origin);
				}
			}
		}

		const float shotErrorAngle = EvaluateSkeletalNodeDivergence(shotAttackerPos->eyePosition, shotCmdIter->angles, shotTargetPos->origin);

		if (maxSnapDelta >= 15.0f && optimalPreErr >= 12.0f && shotErrorAngle <= 4.0f)
		{
			anomalyFlag = true;
			matchedRule = VectorAimRule::AngularConvergence;
		}

		for (const auto &nextCmd : data.commands)
		{
			if (nextCmd.serverTick <= shotCmdIter->serverTick || nextCmd.serverTick > maxFrameTick)
			{
				continue;
			}
			const float postSnapDelta = AngularDistance(shotCmdIter->angles, nextCmd.angles);
			const TrackedPosition *postTarget = shots->FindPosition(nextCmd.serverTick, data.victimIndex);
			const TrackedPosition *postAttacker = shots->FindPosition(nextCmd.serverTick, attacker->index);
			if (postTarget && postAttacker)
			{
				optimalPostErr = EvaluateSkeletalNodeDivergence(postAttacker->eyePosition, nextCmd.angles, postTarget->origin);
				if (postSnapDelta >= 12.0f && optimalPostErr >= 10.0f && shotErrorAngle <= 4.0f)
				{
					anomalyFlag = true;
					matchedRule = VectorAimRule::SubtickSnapReturn;
					break;
				}
			}
		}

		if (anomalyFlag)
		{
			evidence.push_back({Clock::now(), attacker->index, maxSnapDelta, shotErrorAngle});
			AIMBOT_DEBUG("VectorAimEngine flagged %s (Rule: %d, MaxSnap: %.1f, ShotErr: %.1f).\n", attacker->GetName(), static_cast<int>(matchedRule), maxSnapDelta, shotErrorAngle);

			auto now = Clock::now();
			evidence.erase(std::remove_if(evidence.begin(), evidence.end(),
										  [&](const AimbotEvidence &e) { return (now - e.timestamp) > kEvidenceRetentionWindow; }),
						   evidence.end());

			int suspectCount = std::count_if(evidence.begin(), evidence.end(),
											 [&](const AimbotEvidence &e) { return e.playerIndex == attacker->index; });

			if (suspectCount >= kDetectionThresholdCount && announce)
			{
				localization::Text alertDetails;
				alertDetails.Append(std::to_string(suspectCount));
				alertDetails.Append(std::to_string(maxSnapDelta));
				announce("VectorAimEngine", attacker, alertDetails);
				evidence.erase(std::remove_if(evidence.begin(), evidence.end(),
											  [&](const AimbotEvidence &e) { return e.playerIndex == attacker->index; }),
							   evidence.end());
			}
		}

		resetPendingState();
		return true;
	}
} // namespace detection
