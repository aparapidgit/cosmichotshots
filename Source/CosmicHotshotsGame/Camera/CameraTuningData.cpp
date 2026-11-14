#include "Camera/CameraTuningData.h"

UCameraTuningData::UCameraTuningData()
{
	// Close = nearer and lower, Wide = farther and higher.
	FRichCurve* Distance = DistanceByBaseFov.GetRichCurve();
	Distance->AddKey(80.f, 600.f);
	Distance->AddKey(95.f, 750.f);
	Distance->AddKey(110.f, 900.f);

	FRichCurve* Height = HeightByBaseFov.GetRichCurve();
	Height->AddKey(80.f, 170.f);
	Height->AddKey(95.f, 220.f);
	Height->AddKey(110.f, 270.f);

	// A wide base FOV already exaggerates speed, so it gets a weaker speed effect.
	FRichCurve* SpeedEffect = SpeedEffectByBaseFov.GetRichCurve();
	SpeedEffect->AddKey(80.f, 1.2f);
	SpeedEffect->AddKey(95.f, 1.0f);
	SpeedEffect->AddKey(110.f, 0.75f);
}

float UCameraTuningData::GetPresetFov(ECameraFovPreset Preset) const
{
	switch (Preset)
	{
	case ECameraFovPreset::Close:
		return FovClose;
	case ECameraFovPreset::Wide:
		return FovWide;
	default:
		return FovNormal;
	}
}
