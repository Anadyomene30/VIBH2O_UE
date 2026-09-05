#include "VibH2OSettings.h"

UVibH2OSettings::UVibH2OSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("VibH2O");
}

const UVibH2OSettings* UVibH2OSettings::Get()
{
	const UVibH2OSettings* Settings = GetDefault<UVibH2OSettings>();
	check(Settings);
	return Settings;
}

FName UVibH2OSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
