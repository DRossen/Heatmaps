#include "stdafx.h"
#include "Heatmap.h"
#include <Engine/Source/AI/HeatmapSystem/HeatmapCommonData.h>
#include <Engine/Source/Graphics/DebugRenderer.h>
#include <Engine/Source/AI/HeatmapSystem/HeatmapManager.h>

namespace AI
{
	Heatmap::Heatmap(HeatmapManager* aManager) : 
		myManager(aManager), myValidCells(&aManager->myValidCells), myMutex() 
	{
	
	}

	void Heatmap::Init(const Vector2i aGridSize, Vector2i aAnchor, const Vector2f aMin, const Vector2f aMax, const float aCellSize)
	{
		myGridSize = aGridSize;
		myWorldOrigin = aAnchor;
		myMin = aMin;
		myMax = aMax;
		myCellSize = aCellSize;
		myValues.resize(aGridSize.x * aGridSize.y);

		int halfSize = myGridSize.x / 2;
		myBoundsMin.x = abs(std::min(0, myWorldOrigin.x - halfSize));
		myBoundsMin.y = abs(std::min(0, myWorldOrigin.y - halfSize));
		myBoundsMax.x = std::min(myManager->myGridSize.x, myWorldOrigin.x + halfSize);
		myBoundsMax.y = std::min(myManager->myGridSize.y, myWorldOrigin.y + halfSize);
	}

	void Heatmap::FloodFillInfluence(const Vector2i& aOriginCoord, const InfluenceData& aData, float aAmount)
	{
		HeatTemplate& heatTemplate = myManager->GetImprintTemplate(aData);
		FalloffFunction curveFunc = GetFalloffFunction(aData.type);

		int radius = heatTemplate.dimensions / 2;
		int maxIterations = radius + 1;

		std::set<int> visited;
		std::queue<std::tuple<Vector2i, Vector2i, int>> bfsQueue;
		std::array<Vector2i, 4> directions = { Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1), Vector2i(1, 0) };

		Vector2i boundsMin = { std::max(myBoundsMin.x, aOriginCoord.x - radius), std::max(myBoundsMin.y, aOriginCoord.y - radius) };
		Vector2i boundsMax = { std::min(myBoundsMax.x, aOriginCoord.x + radius), std::min(myBoundsMax.y, aOriginCoord.y + radius) };

		bfsQueue.push({ aOriginCoord, heatTemplate.centerCell, 1 });

		int worldStartIndex = aOriginCoord.y * myGridSize.x + aOriginCoord.x;
		int localStartIndex = heatTemplate.centerCell.y * heatTemplate.dimensions + heatTemplate.centerCell.x;

		if (worldStartIndex >= 0 && worldStartIndex < myValues.size())
		{
			myValues[worldStartIndex] += heatTemplate.values[localStartIndex] * aAmount;
			visited.insert(worldStartIndex);
		}

		float bfsFallof = 1.0f;
		int distance = 1;
		int iteration = 1;
		while (!bfsQueue.empty())
		{
			Vector2i nextLocalCoord, nextTemplateCoord;
			Vector2i localCoord, templateCoord;

			std::tie(localCoord, templateCoord, distance) = bfsQueue.front();
			bfsQueue.pop();

			bfsFallof = curveFunc(static_cast<float>(distance), static_cast<float>(maxIterations));

			for (const auto& direction : directions)
			{
				nextLocalCoord = localCoord + direction;
				nextTemplateCoord = templateCoord + direction;

				if (nextLocalCoord.x < boundsMin.x || nextLocalCoord.x > boundsMax.x ||
					nextLocalCoord.y < boundsMin.y || nextLocalCoord.y > boundsMax.y) {
					continue;
				}

				int heatmapIndex = nextLocalCoord.y * myGridSize.x + nextLocalCoord.x;
				int templateIndex = nextTemplateCoord.y * heatTemplate.dimensions + nextTemplateCoord.x;

				if (!myValidCells->at(heatmapIndex)) continue;

				if (visited.find(heatmapIndex) == visited.end())
				{
					myValues[heatmapIndex] += (heatTemplate.values[templateIndex] * bfsFallof) * aAmount;
					bfsQueue.push({ nextLocalCoord, nextTemplateCoord, distance + 1 });
					visited.insert(heatmapIndex);
				}
			}
		}
	}

	void Heatmap::DebugRender(KE::DebugRenderer* aDbg)
	{

	}
}