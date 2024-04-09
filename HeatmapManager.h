#pragma once
#include <unordered_map>
#include <Engine/Source/Utility/EventSystem.h>
#include <Engine/Source/Graphics/Sprite/Sprite.h>
#include <Engine/Source/AI/HeatmapSystem/InfluenceComponent.h>
#include <Engine/Source/AI/HeatmapSystem/HeatmapCommonData.h>
#include <Engine/Source/AI/HeatmapSystem/Workmap.h>
#include "Heatmap.h"

#define DEBUG_ACTIVE

#ifdef DEBUG_ACTIVE
#define DEBUG_ONLY(...) do { __VA_ARGS__; } while (0);
#else
#define DEBUG_ONLY(...) /* Nothing */;
#endif

//#define THREAD_UPDATE

#ifdef THREAD_UPDATE
#define ON_THREAD(...) do { __VA_ARGS__; } while (0);
#define NO_THREAD(...) /* Nothing */;
#else
#define ON_THREAD(...) /* Nothing */;
#define NO_THREAD(...) do { __VA_ARGS__; } while (0);
#endif


namespace KE {
	class DebugRenderer;
	class RaycastHandler;
	class SpriteManager;
	class Navmesh;
}

namespace KE_EDITOR {
	class InfluenceView;
}

namespace AI
{
	// Data container for all the Debug Rendering //
	struct DebugInfo
	{
		float gridLinesAlpha = 0.0f;
		float heatSpriteAlpha = 1.0f;
		float rasterizeSpriteAlpha = 0.0f;
		Vector3f interestCell = {};
		Vector3f workmapOriginPos = {};
		Vector4f rasterizeBox[4] = {};
		std::vector<float> interestValues = {};
		std::vector<float> templateValues = {};
		HeatType spriteType = HeatType::Threat;
		Team spriteTeam = Team::Enemy;
		Team textTeam = Team::Enemy;
	};

	class HeatmapManager : ES::IObserver
	{
		KE_EDITOR_FRIEND;
		friend class KE_EDITOR::InfluenceView;
		friend class Workmap;
		friend class Heatmap;

	public:
		HeatmapManager();
		~HeatmapManager();
		bool Init(float aCellSize, Vector2f aMin, Vector2f aMax);
		void InitValidCells(KE::Navmesh& aNavmesh);
		void Reset();
		void Update();
		void LateUpdate();
		void Register(InfluenceComponent& aUser);
		void DeRegister(InfluenceComponent& aUser);

		Workmap* GetWorkmap(const Vector3f& aPosition, const int aRadius);
		float GetValueAtLocation(const Vector3f aPos, Team aTeam, HeatType aType);

#pragma region DEBUG
		inline DebugInfo& GetDebugInfo() { return myDebug; }	
		void InitDebug();
		void DebugDrawGrid();
		void DebugDrawHeatmap(Team aTeam = Team::Enemy, HeatType aType = HeatType::Threat);
		void DebugDrawValidCells();


		void OnReceiveEvent(ES::Event& aEvent) override;
		void OnInit() override;
		void OnDestroy() override;
#pragma endregion

	private:
		void RemoveUser(InfluenceComponent* aUser);
		void AddUser(InfluenceComponent* aUser);
		void RegistrationUpdate();
		void RepaintUserfluence();
		void CreateTemplates();
		void InitTemplate(const int aSize, FalloffFunction aFallofCurve, HeatTemplate& aTemplate);

		HeatTemplate& GetImprintTemplate(const InfluenceData& aImprintData);
		HeatTemplate& GetInterestTemplate(const int aRadius);
		Heatmap* GetHeatmap(Team aTeam, HeatType aType);
		inline Vector3f GetPosByIndex(const int aIndex) const;
		inline Vector2i GetCoordinate(const Vector3f& aPos) const;
		inline int GetIndexByPos(const Vector3f& aPos) const;

	private:
		Workmap myWorkMap;
		Vector2f myMin;
		Vector2f myMax;
		Vector2i myGridSize;
		Vector2i myWorldOrigin;
		float myCellSize = 0.0f;
		float myTimer = 0.0f;
		float myUpdateFrequency = 0.1f;
		const int myTemplateMaxSize = 10;
		
		std::vector<bool> myValidCells;
		std::unordered_map<Team, std::unordered_map<HeatType, Heatmap*>> myInfluenceMaps;
		std::unordered_map<HeatType, std::vector<HeatTemplate>> myImprintTemplates;
		std::vector<HeatTemplate> myInterestTemplates;

		std::vector<InfluenceComponent*> myUsers;
		std::vector<InfluenceComponent*> myUsersToAdd;
		std::vector<InfluenceComponent*> myUsersToRemove;

		bool myThreadWorking = false;
		std::thread myRepaintThread;

		// <[DEBUG]> //
		DebugInfo myDebug;
		KE::DebugRenderer* myDebugRenderer = nullptr;
		KE::RaycastHandler* myRaycastHandler = nullptr;
		KE::SpriteManager* mySpriteManager = nullptr;
		KE::SpriteBatch myHeatSpriteBatch;
		InfluenceComponent* mySelectedUser = nullptr;
	};

	inline Vector3f HeatmapManager::GetPosByIndex(const int aIndex) const
	{
		Vector3f output;

		Vector2i cord = { (aIndex % myGridSize.x), (aIndex / myGridSize.x) };

		output.x = myMin.x + (cord.x + 0.5f) * myCellSize;
		output.y = 0.0f;
		output.z = myMin.y + (cord.y + 0.5f) * myCellSize;

		return output;
	}
	inline Vector2i HeatmapManager::GetCoordinate(const Vector3f& aPos) const
	{
		Vector2i output =
		{
			static_cast<int>((aPos.x - myMin.x) / myCellSize),
			static_cast<int>((aPos.z - myMin.y) / myCellSize)
		};

		return output;
	}
	inline int HeatmapManager::GetIndexByPos(const Vector3f& aPos) const
	{
		Vector2i coordinate = GetCoordinate(aPos);

		return coordinate.y * myGridSize.x + coordinate.x;
	}

	namespace debug
	{
		void DrawLineToLocation(Vector3f aStart, Vector3f aEnd, Vector4f aColor = { 1,1,1,1 });
		void DrawCircleAtLocation(Vector3f aStart, float aSize, Vector4f aColor = { 1,1,1,1 });
		Vector4f BlendColors(const Vector4f& color1, const Vector4f& color2);
		Vector4f GetHeatColor(float aValue, HeatType aType, Team aTeam);
	}
}