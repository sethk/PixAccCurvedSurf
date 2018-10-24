#pragma once

struct AnimationCurve
{
	float min, max;
	float speed, phase;

	AnimationCurve(float min = -1, float max = 1, float speed = 1, float phase = 0)
		: min(min), max(max), speed(speed), phase(phase)
	{}

	float
	Sample(float t)
	{
		float halfRange = (max - min) * 0.5;
		return fma(sinf(fma(t, speed, phase)), halfRange, min + halfRange);
	}

	void
	RenderUI(const std::string &label)
	{
		ImGui::DragFloat((label + " min").c_str(), &min, 0.1);
		ImGui::DragFloat((label + " max").c_str(), &max, 0.1);
		ImGui::DragFloat((label + " speed").c_str(), &speed, 0.1);
		ImGui::DragFloat((label + " phase").c_str(), &phase, 0.1);
	}
};
