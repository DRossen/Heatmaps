#pragma once
#include <functional>

using FalloffFunction = std::function<float(float, float)>;

class FalloffCurve
{
public:
	static inline float EaseInCirc(float aDistance, float aRadius)
	{
		float value = 1.0f - (aDistance / aRadius);

		if (value == 1.0f) return 1.0f;
		
		value = std::clamp(value, 0.0f, 1.0f);

		return sqrt(1.0f - pow(value, 2.0f));
	}

	static inline float EaseInQuint(float aDistance, float aRadius)
	{
		float t = (aDistance / aRadius);
		
		if (t >= 1.0f) return 0.0f;

		return 1.0f - (t * t * t * t * t);
	}

	static inline float Read(float aDistance, float aRadius)
	{
		// Drag down distance with a fifth of the radius,
		// in order to preserve high values on nearby cells.
		float scalar = aRadius * 0.2f;

		float t = (std::max(0.0f, aDistance - scalar) / aRadius);
		//float t = (aDistance / aRadius * 1.15f);

		float value = 1.0f - t;

		return std::max(0.0f, value);
	}

	static inline float InverseLinear(float aDistance, float aRadius)
	{
		return std::max(0.0f, aDistance / aRadius);
	}

	static inline float Linear(float aDistance, float aRadius)
	{
		return std::min(1.0f, std::max(0.0f, 1.0f - (aDistance * 0.9f / aRadius)));
	}

	static inline float Attractor(float aDistance, float aRadius)
	{
		// Calculate the halfway distance
		float halfway = aRadius / 2.0f;

		// Calculate the value using a normalized Gaussian function
		float sigma = halfway / 2.0f; // Standard deviation, adjust as needed
		float exponent = -0.5f * powf((aDistance - halfway) / sigma, 2);
		float value = exp(exponent);

		// Normalize the value to range from 0 to 1
		value = (value > 0) ? value : 0;
		value = (value < 1) ? value : 1;

		return value;
	}
};

