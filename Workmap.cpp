#include "stdafx.h"
#include "Workmap.h"
#include <Engine/Source/AI/HeatmapSystem/HeatmapCommonData.h>
#include <Engine/Source/AI/HeatmapSystem/HeatmapManager.h>
#include <Engine/Source/AI/HeatmapSystem/InfluenceComponent.h>

#include <Engine/Source/Graphics/DebugRenderer.h>
#include <Engine/Source/Graphics/Sprite/SpriteManager.h>
#include <Engine/Source/Graphics/Texture/TextureLoader.h>

namespace AI
{
	Workmap::Workmap(HeatmapManager* aManager) : Heatmap(aManager)
	{
		myCellSize = aManager->myCellSize;
		myMin = aManager->myMin;
		myMax = aManager->myMax;
		myWorldGridSize = aManager->myGridSize;

		KE::TextureLoader* textureLoader = KE_GLOBAL::blackboard.Get<KE::TextureLoader>("textureLoader");
		myDebug.myHeatSpriteBatch.myData.myTexture = textureLoader->GetTextureFromPath("Data/EngineAssets/KEDefault_c.dds");
		myDebug.spriteMatrix(2, 2) = 0.0f;
		myDebug.spriteMatrix(2, 3) = 1.0f;
		myDebug.spriteMatrix(3, 3) = 0.0;
		myDebug.spriteMatrix(3, 2) = 1.0f;

	}

	void Workmap::RandomizeTraverseOrder()
	{
		// Randomize the order of how BFS traverse.
		for (int i = static_cast<int>(myTraverseOrder.size() - 1); i > 0; --i) {
			int j = std::rand() % (i + 1);
			std::swap(myTraverseOrder[i], myTraverseOrder[j]);
		}
	}

	void Workmap::Init(const Vector2i aGridSize, Vector2i aAnchor, const Vector2f aMin, const Vector2f aMax, const float aCellSize)
	{
		myValues.clear();
		myValues.resize(aGridSize.x * aGridSize.y);

		myGridSize = aGridSize;
		myWorldOrigin = aAnchor;
		myMin = aMin;
		myMax = aMax;
		myCellSize = aCellSize;
		myDebug.scale = { myCellSize / 2.0f, myCellSize / 2.0f, myCellSize / 2.0f };

		int halfSize = myGridSize.x / 2;
		myBoundsMin.x = abs(std::min(0, myWorldOrigin.x - halfSize));
		myBoundsMin.y = abs(std::min(0, myWorldOrigin.y - halfSize));
		myBoundsMax.x = (myGridSize.x - 1) - abs(std::min(0, myManager->myGridSize.x - (myWorldOrigin.x + halfSize + 1)));
		myBoundsMax.y = (myGridSize.y - 1) - abs(std::min(0, myManager->myGridSize.y - (myWorldOrigin.y + halfSize + 1)));
	}

	void Workmap::DebugDraw(int aID)
	{
		if (aID != myDebug.debugUserID) return;

		KE::DebugRenderer* dbg = KE_GLOBAL::blackboard.Get<KE::DebugRenderer>("debugRenderer");

		// Draw a Square around the workmap
		Vector4f color = { 1.0f, 1.0f, 0.0f, 1.0f };
		Vector3f min = { myMin.x, 0.0f, myMin.y };
		Vector3f max = { myMax.x, 0.0f, myMax.y };
		Vector3f p0 = { min.x, 0.0f, min.z };
		Vector3f p1 = { max.x, 0.0f, min.z };
		Vector3f p2 = { max.x, 0.0f, max.z };
		Vector3f p3 = { min.x, 0.0f, max.z };
		debug::DrawLineToLocation(p0, p1, color);
		debug::DrawLineToLocation(p1, p2, color);
		debug::DrawLineToLocation(p2, p3, color);
		debug::DrawLineToLocation(p3, p0, color);

		int currentSize = (int)myDebug.myHeatSpriteBatch.myInstances.size();
		int addition = myGridSize.x * myGridSize.y;
		myDebug.myHeatSpriteBatch.myInstances.resize(currentSize + addition);

		float highestValue = 0.0f;
		for (int i = 0; i < myValues.size(); ++i)
		{
			if (highestValue < abs(myValues[i])) {
				highestValue = abs(myValues[i]);
			}
		}

		if (highestValue == 0) { return; }

		int j = 0;
		for (int i = currentSize; i < myDebug.myHeatSpriteBatch.myInstances.size(); i++)
		{
			Vector3f pos = GetPosByIndex(j);
			pos.y += 0.2f;

			KE::Sprite& sprite = myDebug.myHeatSpriteBatch.myInstances[i];
			sprite.myAttributes.myTransform.SetMatrix(myDebug.spriteMatrix);
			sprite.myAttributes.myTransform.SetPosition(pos);
			sprite.myAttributes.myTransform.SetScale(myDebug.scale);
			sprite.myAttributes.myColor = { 0.0f,0.0f,0.0f,0.0f };

			float value = myValues[j] / highestValue;

			if (myValues[j] < 0) {
				sprite.myAttributes.myColor.r = abs(value);
				sprite.myAttributes.myColor.a = abs(value);
			}
			else {
				sprite.myAttributes.myColor.g = value;
				sprite.myAttributes.myColor.a = value;
			}

			j++;
		}
	}

#pragma region Workmap Operations

	void Workmap::Add(Team aTeam, HeatType aType, const float aInterest)
	{
		if (Heatmap* map = myManager->GetHeatmap(aTeam, aType))
		{
			ON_THREAD(map->LockMutex())

				// Defines the bounds of the heat template in the world grid.
				Vector2i boundsMin = {
					abs(std::min(0, myWorldOrigin.x - myScanRadius)),
					abs(std::min(0, myWorldOrigin.y - myScanRadius)) };

			Vector2i boundsMax = {
				abs(std::max(0, (myWorldOrigin.x + myScanRadius) - map->myGridSize.x + 1)),
				abs(std::max(0, (myWorldOrigin.y + myScanRadius) - map->myGridSize.y + 1)) };

			Vector2i mapCoord = {
				myWorldOrigin.x - myScanRadius,
				myWorldOrigin.y - myScanRadius
			};

			for (int i = boundsMin.y; i < myGridSize.y - boundsMax.y; i++)
			{
				int heatRow = mapCoord.y + i;

				for (int j = boundsMin.x; j < myGridSize.x - boundsMax.x; j++)
				{
					int heatCol = mapCoord.x + j;

					int workmapIndex = i * myGridSize.x + j;
					int heatmapIndex = heatRow * map->myGridSize.x + heatCol;

					myValues[workmapIndex] += map->myValues[heatmapIndex] * aInterest;
				}
			}

			ON_THREAD(map->UnlockMutex();)
		}
	}
	void Workmap::Subtract(Team aTeam, HeatType aType, const float aScalar)
	{
		if (Heatmap* map = myManager->GetHeatmap(aTeam, aType))
		{
			for (int i = 0; i < map->myValues.size(); ++i)
			{
				myValues[i] -= map->myValues[i] * aScalar;
			}
		}
	}
	void Workmap::Multiply(Team aTeam, HeatType aType, const float aScalar)
	{
		if (Heatmap* map = myManager->GetHeatmap(aTeam, aType))
		{
			for (int i = 0; i < map->myValues.size(); ++i)
			{
				myValues[i] -= map->myValues[i] * aScalar;
			}
		}
	}
	void Workmap::Normalize()
	{
		float highestValue = 0.0f;
		for (int i = 0; i < myValues.size(); ++i)
		{
			if (highestValue < myValues[i]) {
				highestValue = myValues[i];
			}
		}

		if (highestValue == 0) { return; }

		for (int i = 0; i < myValues.size(); ++i)
		{
			myValues[i] /= highestValue;
		}
	}
	void Workmap::Invert()
	{
		for (int i = 0; i < myValues.size(); i++)
		{
			myValues[i] *= -1;
		}
	}

#pragma endregion

	void Workmap::ExcludeUserInfluence(const InfluenceComponent& aUser, HeatType aType)
	{
		const InfluenceData& data = *aUser.GetTemplate(aType);
		Vector2i localOrigin = { myGridSize.x / 2, myGridSize.y / 2 };

		if (aType == HeatType::Location)
		{
			Vector2i futureLocalOrigin = localOrigin + (aUser.futureLocation - aUser.location);
			Workmap::FloodFillInfluence(localOrigin, data, -0.5f);
			Workmap::FloodFillInfluence(futureLocalOrigin, data, -0.5f);
		}
		else
		{
			Workmap::FloodFillInfluence(localOrigin, data, -1.0f);
		}
	}

	Vector3f Workmap::GetHighestPoint()
	{
		DEBUG_ONLY(
			myDebug.interestValues.resize(myValues.size());
			myDebug.templateValues.resize(myValues.size());
			std::fill(myDebug.interestValues.begin(), myDebug.interestValues.end(), 0.0f);
			std::fill(myDebug.templateValues.begin(), myDebug.templateValues.end(), 0.0f);
		);

		HeatTemplate& heatTemplate = myManager->GetInterestTemplate(myScanRadius);
		std::queue<std::tuple<Vector2i, Vector2i>> bfsQueue;
		std::set<int> visited;

		RandomizeTraverseOrder();

		int halfSize = static_cast<int>(myGridSize.x / 2);

		int worldStartIndex = myWorldOrigin.y * myWorldGridSize.x + myWorldOrigin.x;
		int localStartIndex = halfSize * myGridSize.x + halfSize;

		int bestCell = INT_MIN;
		float highestValue = INT_MIN;

		if (myValues[localStartIndex] != 0)
		{
			highestValue = myValues[localStartIndex];
			bestCell = localStartIndex;
		}

		bfsQueue.push({ myWorldOrigin, {halfSize, halfSize} });
		visited.insert(worldStartIndex);

		while (!bfsQueue.empty())
		{
			Vector2i worldCoord, templateCoord;
			Vector2i nextWorldCoord, nextLocalCoord;

			std::tie(worldCoord, templateCoord) = bfsQueue.front();
			bfsQueue.pop();

			for (const auto& direction : myTraverseOrder)
			{
				nextWorldCoord = worldCoord + direction;
				nextLocalCoord = templateCoord + direction;

				if (nextLocalCoord.x < myBoundsMin.x || nextLocalCoord.x > myBoundsMax.x ||
					nextLocalCoord.y < myBoundsMin.y || nextLocalCoord.y > myBoundsMax.y) {
					continue;
				}

				int worldIndex = nextWorldCoord.y * myWorldGridSize.x + nextWorldCoord.x;
				int localIndex = nextLocalCoord.y * myGridSize.x + nextLocalCoord.x;

				if (!myValidCells->at(worldIndex)) continue;

				if (visited.find(localIndex) == visited.end())
				{
					bfsQueue.push({ nextWorldCoord, nextLocalCoord });
					visited.insert(localIndex);

					float value = myValues[localIndex] * heatTemplate.values[localIndex];
					if (value == 0) continue;

					if (value > highestValue)
					{
						highestValue = value;
						bestCell = localIndex;
					}

					DEBUG_ONLY(myDebug.interestValues[localIndex] = (myValues[localIndex] * heatTemplate.values[localIndex]);)
					DEBUG_ONLY(myDebug.templateValues[localIndex] = heatTemplate.values[localIndex];)
				}
			}
		}

		if (bestCell == INT_MIN) return myUserPos;

		return GetPosByIndex(bestCell);
	}

}