// VibH2O plugin - runtime module.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

VIBH2O_API DECLARE_LOG_CATEGORY_EXTERN(LogVibH2O, Log, All);

class FVibH2OModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
