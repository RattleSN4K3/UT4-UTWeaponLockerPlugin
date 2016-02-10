#include "UTWeaponLockerPlugin.h"

#include "UTWeaponLocker.h"
#include "UTCharacter.h"
#include "UTMutator_StartWithLockerWeapons.h"

AUTMutator_StartWithLockerWeapons::AUTMutator_StartWithLockerWeapons(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("Mutator_StartWithLockerWeapons", "Display Name", "Start with Locker Weapons");
}

void AUTMutator_StartWithLockerWeapons::ModifyPlayer_Implementation(APawn* Other, bool bIsNewSpawn)
{
	Super::ModifyPlayer_Implementation(Other, bIsNewSpawn);

	AUTCharacter* UTChar = Cast<AUTCharacter>(Other);
	if (UTChar && UTChar->Controller && bIsNewSpawn)
	{
		if (UTChar->bCanPickupItems)
		{
			AUTWeaponLocker* BestLocker = NULL;
			AUTTeamInfo* Team = Cast<AUTPlayerState>(UTChar->PlayerState) ? Cast<AUTPlayerState>(UTChar->PlayerState)->Team : NULL;
			if (Team)
			{
				if (AUTTeamPlayerStart* TeamStart = Cast<AUTTeamPlayerStart>(UTChar->Controller->StartSpot.Get()))
				{
					BestLocker = GetBestLockerFor(TeamStart);
				}
			}
			if (BestLocker == NULL && OnGetBestLockerFor.IsBound())
			{
				BestLocker = OnGetBestLockerFor.Execute(UTChar);
			}
			if (BestLocker != NULL)
			{
				BestLocker->GiveLockerWeapons(UTChar, false);
			}
		}
	}
}

AUTWeaponLocker* AUTMutator_StartWithLockerWeapons::GetBestLockerFor(AUTTeamPlayerStart* TeamStart)
{
	AUTWeaponLocker* BestLocker = NULL;
	if (TeamStart)
	{
		auto& CurrentLockerMap = BestLockersMap.FindOrAdd(TeamStart);
		BestLocker = CurrentLockerMap.BestLocker;
		if (BestLocker)
		{
			// check if current set Locker has been disabled or sleeps, force re-check if so
			if (!BestLocker->IsActive())
			{
				BestLocker = NULL;
			}
			else
			{
				// check if all previously inactive lockers may have been enabled/reset, force re-check if so
				for (int32 i = CurrentLockerMap.InactiveLockers.Num() - 1; i >= 0; i--)
				{
					if (auto Locker = CurrentLockerMap.InactiveLockers[i])
					{
						if (BestLocker && Locker->IsActive())
						{
							BestLocker = NULL;
						}
					}
					else
					{
						CurrentLockerMap.InactiveLockers.RemoveAt(i);
					}
				}
			}
		}

		if (BestLocker == NULL)
		{
			// find nearest weapon locker
			float Dist = 0.f;
			float BestDist = 0.f;
			CurrentLockerMap.InactiveLockers.Empty();
			for (TActorIterator<AUTWeaponLocker> It(GetWorld()); It; ++It)
			{
				if (AUTWeaponLocker* Locker = *It)
				{		
					Dist = FVector::DistSquared(TeamStart->GetActorLocation(), Locker->GetActorLocation());
					if (BestLocker == NULL || BestDist > Dist)
					{
						// store inactive closer lockers to check for activity again
						if (BestLocker && !Locker->IsActive())
						{
							CurrentLockerMap.InactiveLockers.Add(Locker);
						}
						else
						{
							BestDist = Dist;
							BestLocker = Locker;
						}
					}
				}
			}

			// store locker even if empty
			CurrentLockerMap.BestLocker = BestLocker;
		}
	}

	return BestLocker;
}
