#pragma once
#include <Engine/Source/ComponentSystem/Components/Component.h>
#include <Engine/Source/AI/HeatmapSystem/InterestCurves.h>
#include <Engine/Source/AI/HeatmapSystem/HeatmapCommonData.h>



namespace AI
{
	class HeatmapManager;

	struct InfluenceComponentData
	{
		Team team = Team::COUNT;
		std::vector<InfluenceData> influenceTypes;
		HeatmapManager* heatmapManager = nullptr;
	};
	
	class InfluenceComponent : public KE::Component
	{
		friend class HeatmapManager;
		friend class LevelImporter;

	public:
		InfluenceComponent(KE::GameObject& aGo);
		~InfluenceComponent();
		void Awake() override;
		void SetData(void* aDataObject = nullptr) override;
		void AssignVelocityPtr(const Vector3f* aVelocity);

		const InfluenceData* GetTemplate(HeatType aType) const;
		inline Team GetTeam() const { return myTeam; }
		const int GetID() const;


		Vector2i location;
		Vector2i futureLocation;
		const Vector3f& myPosition;
		const Vector3f* myVelocity = nullptr;
	private:
		std::vector<InfluenceData> myImprints;
		Team myTeam = Team::COUNT;
		HeatmapManager* myHeatmapManager = nullptr;
		bool isDynamic = false;
	};
}