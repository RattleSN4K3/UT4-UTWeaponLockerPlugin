#pragma once

#include "UTWeaponLocker.h"

#include "UTMutator.h"
#include "UTTeamPlayerStart.h"
#include "UTMutator_StartWithLockerWeapons.generated.h"

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(AUTWeaponLocker*, FGetBestLockerForDelegate, AUTCharacter*, SpawnedPlayer);

UCLASS(Config = WeaponLocker)
class AUTMutator_StartWithLockerWeapons : public AUTMutator
{
	GENERATED_UCLASS_BODY()

	virtual void ModifyPlayer_Implementation(APawn* Other, bool bIsNewSpawn) override;

	//UPROPERTY(BlueprintAssignable)
	FGetBestLockerForDelegate OnGetBestLockerFor;

	// TODO: Proper Blueprint support. It is not possible to create an delegate dynamically
	UFUNCTION(BlueprintCallable, Category = Blueprint)
	bool SetGetBestLockerForDelegate(const FGetBestLockerForDelegate& NewDelegate)
	{
		if (!OnGetBestLockerFor.IsBound())
		{
			OnGetBestLockerFor = NewDelegate;
			return true;
		}
		
		return false;
	}


	TMap<AUTTeamPlayerStart*, AUTWeaponLocker*> BestLockersMap;
	AUTWeaponLocker* GetBestLockerFor(AUTTeamPlayerStart* Team);
};