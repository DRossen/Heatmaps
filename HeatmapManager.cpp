#include "stdafx.h"
#include "HeatmapManager.h"

#include <Engine/Source/Graphics/Texture/TextureLoader.h>

#include <Engine/Source/Graphics/Sprite/SpriteManager.h>
#include <Engine/Source/Graphics/DebugRenderer.h>
#include <Engine/Source/Input/UserInput.h>
#include <Engine/Source/Input/InputEvents.h>
#include <Engine/Source/Navigation/RaycastHandler.h>
#include "Engine/Source/AI/HeatmapSystem/InterestCurves.h"
#include <Engine/Source/Navigation/Navmesh.h>


namespace AI
{
	HeatmapManager::HeatmapManager() : myWorkMap(this) {}
	HeatmapManager::~HeatmapManager()
	{
		for (auto& key : myInfluenceMaps)
		{
			for (auto& map : key.second)
			{
				delete map.second;
			}
		}
	}

	bool HeatmapManager::Init(float aCellSize, Vector2f aMin, Vector2f aMax)
	{
		KE_GLOBAL::blackboard.Register<HeatmapManager>("heatmapManager", this);
		
		myCellSize = aCellSize;
		myMin = { aMin.x, aMin.y };
		myMax = { aMax.x, aMax.y };
		myTimer = myUpdateFrequency;
		myGridSize = { (int)(abs(aMax.x - aMin.x) / myCellSize), (int)(abs(aMax.y - aMin.y) / myCellSize) };
		myGridSize.x = myGridSize.x % 2 != 0 ? myGridSize.x : myGridSize.x + 1;
		myGridSize.y = myGridSize.y % 2 != 0 ? myGridSize.y : myGridSize.y + 1;
		myWorldOrigin = { myGridSize.x / 2, myGridSize.y / 2 };
		myValidCells.resize(myGridSize.x * myGridSize.y);

		CreateTemplates();

		// Create a container for both teams.
		for (int i = 0; i < static_cast<int>(Team::COUNT); i++)
		{
			Team team = static_cast<Team>(i);
			myInfluenceMaps[team] = std::unordered_map<HeatType, Heatmap*>();
		}

		// Create every type of influence map for both teams.
		// Would only create necessary maps based on registration in a real scenario.
		for (int i = 0; i < static_cast<int>(HeatType::COUNT); i++)
		{
			HeatType type = static_cast<HeatType>(i);

			myInfluenceMaps[Team::Player].insert({ type, new Heatmap(this) });
			myInfluenceMaps[Team::Enemy].insert({ type, new Heatmap(this) });
			myInfluenceMaps[Team::Player][type]->Init(myGridSize, myWorldOrigin, myMin, myMax, myCellSize);
			myInfluenceMaps[Team::Enemy][type]->Init(myGridSize, myWorldOrigin, myMin, myMax, myCellSize);
		}

		InitDebug();

		return true;
	}
	void HeatmapManager::CreateTemplates()
	{
		for (int i = 0; i < static_cast<int>(HeatType::COUNT); i++)
		{
			HeatType type = static_cast<HeatType>(i);
			myImprintTemplates[type].resize(myTemplateMaxSize);
			auto& imprintTemplates = myImprintTemplates[type];

			for (int r = 0; r < myTemplateMaxSize; r++)
			{
				int relativeSize = r * static_cast<int>(1.0f / myCellSize);
				FalloffFunction curve = GetFalloffFunction(type);
				HeatTemplate& heatTemplate = imprintTemplates[r];

				InitTemplate(relativeSize, curve, heatTemplate);
			}
		}

		// All interest templates use the same curve.
		myInterestTemplates.resize(myTemplateMaxSize);
		for (int r = 0; r < myTemplateMaxSize; r++)
		{
			int relativeSize = r * static_cast<int>(1.0f / myCellSize);

			InitTemplate(relativeSize, FalloffCurve::Read, myInterestTemplates[r]);
		}
	}
	void HeatmapManager::InitTemplate(const int aSize, FalloffFunction aFallofCurve, HeatTemplate& aTemplate)
	{
		int dimension = (aSize * 2);
		dimension = dimension % 2 != 0 ? dimension : dimension + 1;

		aTemplate.dimensions = dimension;
		aTemplate.values.resize(dimension * dimension);
		aTemplate.centerCell = { dimension / 2, dimension / 2 };

		int index = 0;

		for (int row = 0; row < dimension; row++)
		{
			for (int col = 0; col < dimension; col++)
			{
				int coordDistance = (Vector2i(col, row) - aTemplate.centerCell).LengthSqr();
				float distance = sqrt(static_cast<float>(coordDistance)) * myCellSize;
				float interest = aFallofCurve(distance, static_cast<float>(aSize));

				aTemplate.values[index] = interest;
				index++;
			}
		}
	}
	void HeatmapManager::InitValidCells(KE::Navmesh& aNavmesh)
	{
		for (int i = 0; i < myGridSize.y; i++)
		{
			int row = i * myGridSize.x;

			for (int j = 0; j < myGridSize.x; j++)
			{
				int index = row + j;
				Vector3f position = GetPosByIndex(index);

				myValidCells[index] = aNavmesh.IsPointInside(position);
			}
		}
	}

	HeatTemplate& HeatmapManager::GetImprintTemplate(const InfluenceData& aImprintData)
	{
		// if the radius is bigger than what we have, return the largest template.
		int typeMaxSize = static_cast<int>(myImprintTemplates[aImprintData.type].size() - 1);
		int maxSize = std::min(typeMaxSize, aImprintData.radius);

		return myImprintTemplates[aImprintData.type][maxSize];
	}
	HeatTemplate& HeatmapManager::GetInterestTemplate(const int aRadius)
	{
		// if the radius is bigger than what we have, return the largest template.
		int maxSize = std::min(static_cast<int>(myInterestTemplates.size() - 1), aRadius);

		return myInterestTemplates[maxSize];
	}
	Heatmap* HeatmapManager::GetHeatmap(Team aTeam, HeatType aType)
	{
		if (myInfluenceMaps.find(aTeam) != myInfluenceMaps.end())
		{
			auto& container = myInfluenceMaps.at(aTeam);

			if (container.find(aType) != container.end())
			{
				return container.at(aType);
			}
		}

		return nullptr;
	}
	Workmap* HeatmapManager::GetWorkmap(const Vector3f& aPosition, const int aRadius)
	{
		int size = aRadius * 2;
		size += ((size + 1) % 2);

		Vector2i worldCoordOrigin = GetCoordinate(aPosition);
		Vector2f coordPos = { (worldCoordOrigin.x + 1) * myCellSize, (worldCoordOrigin.y + 1) * myCellSize };
		float halfSize = ((size / 2.0f) * myCellSize) + myCellSize * 0.5f;

		Vector2f min = myMin + coordPos - Vector2f(halfSize, halfSize);
		Vector2f max = myMin + coordPos + Vector2f(halfSize - 1, halfSize - 1);
		myWorkMap.Init({ size, size }, worldCoordOrigin, min, max, myCellSize);
		myWorkMap.myWorldGridSize = myGridSize;
		myWorkMap.myScanRadius = aRadius;
		myWorkMap.myUserPos = aPosition;

		return &myWorkMap;
	}
	float HeatmapManager::GetValueAtLocation(const Vector3f aPos, Team aTeam, HeatType aType)
	{
		Heatmap* map = GetHeatmap(aTeam, aType);
		int index = map->GetIndexByPos(aPos);

		if (index < 0 || index >= map->myValues.size())
			return 0.0f;

		return map->myValues[index];
	}

	void HeatmapManager::Register(InfluenceComponent& aUser)
	{
		myUsersToAdd.push_back(&aUser);
	}
	void HeatmapManager::DeRegister(InfluenceComponent& aUser)
	{
		myUsersToRemove.push_back(&aUser);
	}
	void HeatmapManager::RemoveUser(InfluenceComponent* aUser)
	{
		// Locate the user to remove //
		InfluenceComponent* userToRemove = nullptr;
		int index = 0;
		for (int i = 0; i < myUsers.size(); ++i)
		{
			if (myUsers[i]->GetID() == aUser->GetID())
			{
				userToRemove = myUsers[i];
				index = i;
				break;
			}
		}

		if (!userToRemove) return;

		// Remove user influence //
		auto& container = myInfluenceMaps[userToRemove->myTeam];
		for (auto& imprint : userToRemove->myImprints)
		{
			HeatTemplate& heatTemplate = GetImprintTemplate(imprint);
			Heatmap& map = *container[imprint.type];

			if (imprint.type == HeatType::Location)
			{
				map.FloodFillInfluence(userToRemove->location, imprint, -0.5f);
				map.FloodFillInfluence(userToRemove->location, imprint, -0.5f);
			}
			else
			{
				map.FloodFillInfluence(userToRemove->location, imprint, -imprint.maxValue);
			}
		}

		// Remove user //
		std::swap(myUsers[index], myUsers.back());
		myUsers.pop_back();
	}
	void HeatmapManager::AddUser(InfluenceComponent* aUser)
	{
		myUsers.push_back(aUser);
		aUser->location = GetCoordinate(aUser->myPosition);
		aUser->futureLocation = aUser->location;

		// [Paint Influence upon registration] //
		auto& container = myInfluenceMaps[aUser->myTeam];
		for (auto& imprint : aUser->myImprints)
		{
			container[imprint.type]->FloodFillInfluence(aUser->location, imprint, imprint.maxValue);
		}
	}

	void HeatmapManager::Update()
	{
	#ifdef DEBUG_ACTIVE
	
		myWorkMap.myDebug.myHeatSpriteBatch.myInstances.clear();
		for (int i = 0; i < myHeatSpriteBatch.myInstances.size(); ++i)
		{
			myHeatSpriteBatch.myInstances[i].myAttributes.myColor = { 0, 0, 0, 0 };
		}
	#endif

		RegistrationUpdate();

		myTimer += KE_GLOBAL::deltaTime;
		if (myTimer > myUpdateFrequency && !myThreadWorking)
		{
			ON_THREAD(
				myThreadWorking = true;
				myRepaintThread = std::thread(&AI::HeatmapManager::RepaintInfluence, this);
				myRepaintThread.detach();
			);

			NO_THREAD(RepaintInfluence())
		}
	}
	void HeatmapManager::RegistrationUpdate()
	{
		if (myThreadWorking) return;

		for (auto userToRemove : myUsersToRemove)
		{
			RemoveUser(userToRemove);
		}

		for (auto& user : myUsersToAdd)
		{
			AddUser(user);
		}

		myUsersToAdd.clear();
		myUsersToRemove.clear();
	}
	void HeatmapManager::RepaintInfluence()
	{
		myThreadWorking = true;

		for (auto& user : myUsers)
		{
			Vector2i currentLocation = GetCoordinate(user->myPosition);

			// We only update if the user has moved.
			if (user->location == currentLocation) continue;

			auto& container = myInfluenceMaps[user->myTeam];

			// Dynamic users can apply influence on their future location as well.
			if (user->isDynamic)
			{
				// Limit future location within the grid bounds.
				Vector2i futureLocation = GetCoordinate(user->myPosition + *user->myVelocity);
				futureLocation.x = std::clamp(futureLocation.x, 0, myGridSize.x - 1);
				futureLocation.y = std::clamp(futureLocation.y, 0, myGridSize.y - 1);

				for (auto& imprint : user->myImprints)
				{
					// [TODO] -> Imprints should tell if the influence should be applied to future location or not.
					HeatTemplate& heatTemplate = GetImprintTemplate(imprint);
					Heatmap& map = *container[imprint.type];

					ON_THREAD(map.LockMutex());

					if (imprint.type == HeatType::Location)
					{
						map.FloodFillInfluence(user->location, imprint, -0.5f);
						map.FloodFillInfluence(user->futureLocation, imprint, -0.5f);
						map.FloodFillInfluence(currentLocation, imprint, 0.5f);
						map.FloodFillInfluence(futureLocation, imprint, 0.5f);
					}
					else
					{
						map.FloodFillInfluence(user->location, imprint, -imprint.maxValue);
						map.FloodFillInfluence(currentLocation, imprint, imprint.maxValue);
					}
					ON_THREAD(map.UnlockMutex());
				}

				user->location = currentLocation;
				user->futureLocation = futureLocation;
			}
			else
			{
				for (auto& imprint : user->myImprints)
				{
					HeatTemplate& heatTemplate = GetImprintTemplate(imprint);

					container[imprint.type]->FloodFillInfluence(user->location, imprint, -imprint.maxValue);
					container[imprint.type]->FloodFillInfluence(currentLocation, imprint, imprint.maxValue);
				}

				user->location = currentLocation;
			}
		}

		myThreadWorking = false;
		myTimer = 0.0f;
	}

	void HeatmapManager::Reset()
	{
		myUsers.clear();
		myUsersToAdd.clear();
		myImprintTemplates.clear();
		myInterestTemplates.clear();
		myInfluenceMaps.clear();
	}

#pragma region Debug
	void HeatmapManager::LateUpdate()
	{
#ifdef DEBUG_ACTIVE
		mySpriteManager->QueueSpriteBatch(&myWorkMap.myDebug.myHeatSpriteBatch);
		mySpriteManager->QueueSpriteBatch(&myHeatSpriteBatch);
#endif
	}
	void HeatmapManager::DebugDrawGrid()
	{
#ifdef DEBUG_ACTIVE
		Vector4f color = { 0,0,0, myDebug.gridLinesAlpha };
		Vector4f from = { 0,0.11f,0,1 };
		Vector4f to = { 0,0.11f,0,1 };

		for (int i = 0; i < myGridSize.x + 1; i++)
		{
			from.x = myMin.x + (i * myCellSize);
			to.x = myMin.x + (i * myCellSize);

			from.z = myMin.y + 0;
			to.z = myMin.y + (myGridSize.y * myCellSize);

			myDebugRenderer->RenderLine(from, to, &color);
		}

		for (int i = 0; i < myGridSize.y + 1; i++)
		{
			from.x = myMin.x;
			to.x = myMin.x + myGridSize.x * myCellSize;

			from.z = myMin.y + (i * myCellSize);
			to.z = myMin.y + (i * myCellSize);

			myDebugRenderer->RenderLine(from, to, &color);
		}
#endif
	}
	void HeatmapManager::DebugDrawHeatmap(Team aTeam, HeatType aType)
	{
	#ifdef DEBUG_ACTIVE

		auto& container = myInfluenceMaps[aTeam];

		if (container.find(aType) == container.end()) { return; }

		auto& map = container.at(aType);

		Vector2i cord = {};
		Vector2f cellCenter = {};

		for (int i = 0; i < map->myValues.size(); ++i)
		{
			if (!myValidCells[i]) {

				myHeatSpriteBatch.myInstances[i].myAttributes.myColor = { 0,0,0,0 };
				continue;
			}

			cord = { i % myGridSize.x, i / myGridSize.x };
			cellCenter = {
				myMin.x + ((cord.x + 0.5f) * myCellSize) - 10.f,
				myMin.y + ((cord.y + 0.5f) * myCellSize) - 10.f
			};

			Vector4f debugColor = debug::GetHeatColor(map->myValues[i], aType, aTeam);

			myHeatSpriteBatch.myInstances[i].myAttributes.myColor = { debugColor.x, debugColor.y, debugColor.z, debugColor.w };
		}

		mySpriteManager->QueueSpriteBatch(&myHeatSpriteBatch);

	#endif
	}
	void HeatmapManager::DebugDrawValidCells()
	{
#ifdef DEBUG_ACTIVE
		Vector3f cellCenterPos;
		Vector4f color = { 1,0,0,1 };

		bool walkable = false;
		for (int i = 0; i < myGridSize.y; i++)
		{
			int row = i * myGridSize.x;

			for (int j = 0; j < myGridSize.x; j++)
			{
				int index = row + j;
				if (myValidCells[index]) continue;

				cellCenterPos = GetPosByIndex(index);

				for (int c = 0; c < 2; c++)
				{
					Vector3f from = cellCenterPos;
					Vector3f to = cellCenterPos;
					to.y += 0.2f;
					from.y += 0.2f;


					if (c == 0)
					{
						from.x -= myCellSize / 2.0f;
						from.z -= myCellSize / 2.0f;
						to.x += myCellSize / 2.0f;
						to.z += myCellSize / 2.0f;
					}
					else
					{
						from.x -= myCellSize / 2.0f;
						from.z += myCellSize / 2.0f;
						to.x += myCellSize / 2.0f;
						to.z -= myCellSize / 2.0f;
					}

					myDebugRenderer->RenderLine(from, to, &color);
				}

				//myDebugRenderer->RenderSphere(cellCenterPos, 0.3f, color);
			}
		}
#endif
	}
	void HeatmapManager::InitDebug()
	{
		int cellCount = myGridSize.x * myGridSize.y;

		myDebug.interestValues.resize(cellCount);
		myDebug.templateValues.resize(cellCount);
		myRaycastHandler = KE_GLOBAL::blackboard.Get<KE::RaycastHandler>("raycastHandler");
		myDebugRenderer = KE_GLOBAL::blackboard.Get<KE::DebugRenderer>("debugRenderer");
		mySpriteManager = KE_GLOBAL::blackboard.Get<KE::SpriteManager>("spriteManager");


		KE::TextureLoader* textureLoader = KE_GLOBAL::blackboard.Get<KE::TextureLoader>("textureLoader");
		myHeatSpriteBatch.myData.myTexture = textureLoader->GetTextureFromPath("Data/EngineAssets/KEDefault_c.dds");
		myHeatSpriteBatch.myInstances.reserve(8192);
		myHeatSpriteBatch.myData.myMode = KE::SpriteBatchMode::Default;
		myHeatSpriteBatch.myInstances.resize(cellCount);

		Matrix4x4f spriteMatrix;
		spriteMatrix(2, 2) = 0.0f;
		spriteMatrix(2, 3) = 1.0f;
		spriteMatrix(3, 3) = 0.0;
		spriteMatrix(3, 2) = 1.0f;
		Vector3f scale = { myCellSize / 2.0f, myCellSize / 2.0f, myCellSize / 2.0f };

		for (int i = 0; i < cellCount; i++)
		{
			Vector3f position = GetPosByIndex(i);
			position.y += 0.1f;

			myHeatSpriteBatch.myInstances[i].myAttributes.myTransform.SetMatrix(spriteMatrix);
			myHeatSpriteBatch.myInstances[i].myAttributes.myTransform.SetPosition(position);
			myHeatSpriteBatch.myInstances[i].myAttributes.myTransform.SetScale(scale);
			myHeatSpriteBatch.myInstances[i].myAttributes.myColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		}
	}

	namespace debug
	{
		void DrawLineToLocation(Vector3f aStart, Vector3f aEnd, Vector4f aColor)
		{
			KE::DebugRenderer& dbg = *KE_GLOBAL::blackboard.Get<KE::DebugRenderer>("debugRenderer");
			Vector4f color = aColor;
			aStart.y += 0.5f;
			aEnd.y += 0.5f;
			dbg.RenderLine(aStart, aEnd, &color);
		}

		void DrawCircleAtLocation(Vector3f aStart, float aSize, Vector4f aColor)
		{
			KE::DebugRenderer& dbg = *KE_GLOBAL::blackboard.Get<KE::DebugRenderer>("debugRenderer");
			Vector4f color = aColor;
			aStart.y += 0.5f;
			dbg.RenderCircle(aStart, aSize, color);

		}

		Vector4f BlendColors(const Vector4f& color1, const Vector4f& color2) {
			Vector4f blendedColor;

			// Add the original green and red components
			blendedColor.x = color1.x + color2.x; // Green
			blendedColor.y = color1.y + color2.y; // Red

			// Calculate the overlap between green and red components to determine the blue component
			float overlap = std::min(color1.x, color2.y) + std::min(color1.y, color2.x);
			// Adjust the blue component based on the ratio of green to red
			blendedColor.z = color1.z + color2.z + (overlap * 1); // Blue

			blendedColor.w = std::min(1.0f, std::max(0.0f, color1.w + color2.w)); // Alpha component

			// Clamp the values to ensure they are within [0, 1]
			blendedColor.x = std::min(1.0f, std::max(0.0f, blendedColor.x));
			blendedColor.y = std::min(1.0f, std::max(0.0f, blendedColor.y));
			blendedColor.z = std::min(1.0f, std::max(0.0f, blendedColor.z));

			return blendedColor;
		}

		Vector4f GetHeatColor(float aValue, HeatType aType, Team aTeam)
		{
			Vector4f output = { 0,0,0,0 };

			if (aValue <= 0) return output;

			//float* targetColor = nullptr;
			//
			//targetColor = aTeam == Team::Enemy ? &output.x : &output.y;
			//
			//*targetColor = std::max(0.0f, std::min(1.0f, aValue))

			if (aTeam == Team::Enemy)
			{
				float addition = std::max(0.0f, aValue - 1.0f);

				output.x = std::max(0.0f, std::min(1.0f, aValue));
				output.y = std::max(0.0f, std::max(0.2f, ((output.x + addition) / 2.f)));
				output.z = std::max(0.0f, std::max(0.2f, ((output.x + addition) / 4.f)));
			}
			else
			{
				float addition = std::max(0.0f, aValue - 1.0f);
				//float addition = 1.0f - (aValue * 0.5f);
				output.y = std::max(0.0f, std::min(1.0f, aValue * 0.8f));
				output.x = std::max(0.0f, ((output.y + addition) / 2.0f));
				output.z = std::max(0.0f, ((output.y + addition) / 4.0f));
			}

			float alpha = aTeam == Team::Player ? output.y : output.x;


			output.w = alpha * 2.0f;

			return output;
		}
	}
#pragma endregion

}