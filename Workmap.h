#pragma once
#include "Heatmap.h"
#include <Engine/Source/Graphics/Sprite/Sprite.h>

namespace KE_EDITOR
{
	class InfluenceView;
}

namespace AI
{
	enum class HeatType;
	enum class Team;
	class HeatmapManager;
	class InfluenceComponent;

	struct WorkmapDebugData
	{
		std::vector<float> interestValues;
		std::vector<float> templateValues;
		KE::SpriteBatch myHeatSpriteBatch;
		Matrix4x4f spriteMatrix;
		Vector3f scale;
		int debugUserID = 0;
		Vector3f highestPoint;
		Vector4f templateBox[4] = {};
	};

	class Workmap : public Heatmap
	{
		friend class KE_EDITOR::InfluenceView;
		friend class HeatmapManager;

	public:
		Workmap(HeatmapManager* aManager);
		~Workmap() {};

		void Add(Team aTeam, HeatType aType, const float aInterest = 1.0f);
		void Subtract(Team aTeam, HeatType aType, const float aScalar = 1.0f);
		void Multiply(Team aTeam, HeatType aType, const float aScalar = 1.0f);
		void Normalize();
		void Invert();
		void DebugDraw(int aID = INT_MIN);

		void ExcludeUserInfluence(const InfluenceComponent& aUser, HeatType aType);
		Vector3f GetHighestPoint();

	private:
		void RandomizeTraverseOrder();
		void Init(const Vector2i aGridSize, Vector2i aAnchor, const Vector2f aMin, const Vector2f aMax, const float aCellSize) override;
		Vector2i myWorldGridSize; // Need this to check if a local cell is valid.
		WorkmapDebugData myDebug;
		int myScanRadius = 0;
		Vector3f myUserPos;
		std::array<Vector2i, 4> myTraverseOrder = { Vector2i(0,  1), Vector2i(-1,  0), Vector2i(0, -1), Vector2i(1,  0) };
	};
}