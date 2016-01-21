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
		AUTWeaponLocker* BestLocker = NULL;
		if (UTChar->bCanPickupItems)
		{
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
	AUTWeaponLocker* Locker = NULL;
	if (TeamStart)
	{
		if (BestLockersMap.Contains(TeamStart))
		{
			Locker = BestLockersMap[TeamStart];
		}
		else
		{
			// find nearest weapon locker
			float Dist = 0.f;
			float BestDist = 0.f;
			for (TActorIterator<AUTWeaponLocker> It(GetWorld()); It; ++It)
			{
				if (AUTWeaponLocker* LockerIt = *It)
				{
					Dist = FVector::DistSquared(TeamStart->GetActorLocation(), LockerIt->GetActorLocation());
					if (Locker == NULL || BestDist > Dist)
					{
						BestDist = Dist;
						Locker = LockerIt;
					}
				}
			}

			// store locker even if empty
			BestLockersMap.Add(TeamStart, Locker);
		}
	}

	return Locker;
}
