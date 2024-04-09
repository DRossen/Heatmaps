#pragma once
#include <Engine/Source/AI/HeatmapSystem/InterestCurves.h>

namespace AI
{
	enum class Team
	{
		Player,
		Enemy,
		Count
	};

	enum class HeatType
	{
		Threat,
		Location,
		Attractor,
		COUNT
	};

	enum class FalloffType
	{
		Linear,
		EaseInQuint,
		InverseLinear,
		COUNT
	};

	struct InfluenceData
	{
		int radius = 1;
		float maxValue = 1.0f;
		HeatType type = HeatType::COUNT;
		FalloffType fallofType = FalloffType::Linear;
	};

	struct HeatTemplate
	{
		int dimensions = NULL;
		Vector2i centerCell;
		std::vector<float> values;
	};

	static inline FalloffFunction GetFalloffFunction(const AI::HeatType aType)
	{
		switch (aType)
		{
		case AI::HeatType::Threat:
			return FalloffCurve::Linear;

		case AI::HeatType::Location:
			return FalloffCurve::Linear;

		case AI::HeatType::Attractor:
			return FalloffCurve::Attractor;

		default:
			return FalloffCurve::Linear;
		}
	}
}