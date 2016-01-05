#pragma once

#include "Core.h"
#include "UnrealTournament.h"

#include "Runtime/Core/Public/Features/IModularFeature.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDebug, Log, All);

struct FUTWeaponLockerPlugin : IModularFeature, IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FUTWeaponLockerPlugin();

};

/** utility to detach and unregister a component and all its children */
extern void UnregisterComponentTree(USceneComponent* Comp);