#include "UTWeaponLockerPlugin.h"
#include "UnrealTournament.h"
//#include "ModuleInterface.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

IMPLEMENT_MODULE(FUTWeaponLockerPlugin, UTWeaponLockerPlugin)

//DEFINE_LOG_CATEGORY_STATIC(LogDebug, Log, All);
DEFINE_LOG_CATEGORY(LogDebug);

FUTWeaponLockerPlugin::FUTWeaponLockerPlugin()
{

}

void FUTWeaponLockerPlugin::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(TEXT("UTWeaponLockerPlugin"), this);
}

void FUTWeaponLockerPlugin::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(TEXT("UTWeaponLockerPlugin"), this);
}

void UnregisterComponentTree(USceneComponent* Comp)
{
	if (Comp != NULL)
	{
		TArray<USceneComponent*> Children;
		Comp->GetChildrenComponents(true, Children);
		Comp->DetachFromParent();
		Comp->UnregisterComponent();
		for (USceneComponent* Child : Children)
		{
			Child->UnregisterComponent();
		}
	}
}
