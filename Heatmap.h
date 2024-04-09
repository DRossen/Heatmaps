#pragma once
#include <mutex>

namespace KE
{
	class DebugRenderer;
}

namespace AI
{
	struct InfluenceData;
	struct HeatTemplate;
	class HeatmapManager;

	class Heatmap
	{
		friend class HeatmapManager;
		friend class Workmap;
		KE_EDITOR_FRIEND;

	public:
		Heatmap(HeatmapManager* aManager);
		Heatmap() {};
		virtual ~Heatmap() {}
		virtual void Init(const Vector2i aGridSize, Vector2i aAnchor, const Vector2f aMin, const Vector2f aMax, const float aCellSize);
		virtual void DebugRender(KE::DebugRenderer* aDbg);
		 
		virtual void FloodFillInfluence(const Vector2i& aOriginCoord, const InfluenceData& aData, float aAmount = 1.0f);
		virtual void Clear() { std::fill(myValues.begin(), myValues.end(), 0.0f); }

		//void LEGCAY_SpreadInfluence(const Vector2i& aOriginCoord, const HeatTemplate& aTemplate, float aAmount = 1.0f);
		
		inline Vector3f GetPosByIndex(const int aIndex) const;
		inline Vector2i GetCoordinate(const Vector3f& aPos) const;
		inline int GetIndexByPos(const Vector3f& aPos) const;
		
	protected:
		void LockMutex() { this->myMutex.lock(); }
		void UnlockMutex() { this->myMutex.unlock(); }
		std::mutex myMutex;

		Vector2i myWorldOrigin;
		Vector2i myGridSize;
		Vector2i myBoundsMin;
		Vector2i myBoundsMax;
		Vector2f myMin;
		Vector2f myMax;
		std::vector<float> myValues;
		float myCellSize = 1.0f;
		const std::vector<bool>* myValidCells = nullptr;
		HeatmapManager* myManager = nullptr;
	};

	inline Vector3f Heatmap::GetPosByIndex(const int aIndex) const
	{
		Vector3f output = { 0,0,0 };

		Vector2i cord = { (aIndex % myGridSize.x), (aIndex / myGridSize.x) };

		output.x = myMin.x + (cord.x + 0.5f) * myCellSize;
		output.y = 0.0f;
		output.z = myMin.y + (cord.y + 0.5f) * myCellSize;

		return output;
	}
	inline Vector2i Heatmap::GetCoordinate(const Vector3f& aPos) const
	{
		Vector2i output =
		{
			static_cast<int>((aPos.x - myMin.x) / myCellSize),
			static_cast<int>((aPos.z - myMin.y) / myCellSize)
		};

		return output;
	}
	inline int Heatmap::GetIndexByPos(const Vector3f& aPos) const
	{
		Vector2i coordinate = GetCoordinate(aPos);

		return coordinate.y * myGridSize.x + coordinate.x;
	}
}