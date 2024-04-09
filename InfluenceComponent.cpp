#include "stdafx.h"
#include "InfluenceComponent.h"
#include "InterestCurves.h"
#include <Engine/Source/ComponentSystem/GameObject.h>
#include <Engine/Source/AI/HeatmapSystem/HeatmapManager.h>
#include <Project/Source/Enemies/EnemyComponent.h>

namespace AI
{
	InfluenceComponent::InfluenceComponent(KE::GameObject& aGo) : KE::Component(aGo),
		myPosition(aGo.myTransform.GetPositionRef())
	{
	
	}

	InfluenceComponent::~InfluenceComponent() 
	{
	
	}

	void InfluenceComponent::Awake()
	{

	}

	void InfluenceComponent::SetData(void* aDataObject)
	{
		InfluenceComponentData& data = *(InfluenceComponentData*)(aDataObject);

		myTeam = data.team;
		myImprints = data.influenceTypes;
		data.heatmapManager->Register(*this);
	}

	void InfluenceComponent::AssignVelocityPtr(const Vector3f* aVelocity)
	{
		myVelocity = aVelocity;
		isDynamic = true;
	}

	const InfluenceData* InfluenceComponent::GetTemplate(HeatType aType) const
	{
		for (auto& data : myImprints)
		{
			if(data.type != aType) continue;

			return &data;
		}
		return nullptr;
	}

	const int InfluenceComponent::GetID() const
	{
		return myGameObject.myID;
	}
}