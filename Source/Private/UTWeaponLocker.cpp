#include "UTWeaponLockerPlugin.h"
#include "UnrealNetwork.h"
#include "UTSquadAI.h"
#include "UTWeaponLocker.h"

#include "MessageLog.h"
#include "UObjectToken.h"

#include "UTWorldSettings.h"
#include "UTPickupMessage.h"

#include "UTWeap_BioRifle.h"
#include "UTWeap_ShockRifle.h"
#include "UTWeap_RocketLauncher.h"
#include "UTWeap_LinkGun.h"
#include "UTWeap_FlakCannon.h"

#define LOCTEXT_NAMESPACE "UTWeaponLocker"

static FString NewLine = FString(TEXT("\n"));
static FString NewParagraph = FString(TEXT("\n\n"));

FCollisionResponseParams WorldResponseParams = []()
{
	FCollisionResponseParams Result(ECR_Ignore);
	Result.CollisionResponse.WorldStatic = ECR_Block;
	Result.CollisionResponse.WorldDynamic = ECR_Block;
	return Result;
}();

AUTWeaponLocker::AUTWeaponLocker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		//.DoNotCreateDefaultSubobject(TEXT("TimerEffect")) // TimerEffect is not optional
		.DoNotCreateDefaultSubobject(TEXT("BaseEffect"))
	)
{
	//// Structure to hold one-time initialization
	//struct FConstructorStatics
	//{
	//	ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMesh;

	//	FConstructorStatics()
	//		: BaseMesh(TEXT("Class'/UTWeaponLockerPlugin/Mesh/S_GP_Ons_Weapon_Locker.S_GP_Ons_Weapon_Locker'"))
	//	{
	//	}
	//};
	//static FConstructorStatics ConstructorStatics;

	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false);

	Collision->InitCapsuleSize(78.0f, 110.0f);
	Collision->Mobility = EComponentMobility::Movable;
	Collision->AttachParent = RootComponent;
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTWeaponLocker::OnOverlapBegin);
	Collision->RelativeLocation.Z = 110.f;

	BaseMesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("BaseMeshComp"));
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//BaseMesh->SetStaticMesh(ConstructorStatics.BaseMesh.Object);
	BaseMesh->AttachParent = RootComponent;

	if (TimerEffect != NULL)
	{
		TimerEffect->Mobility = EComponentMobility::Movable;
		TimerEffect->AttachParent = RootComponent;
		TimerEffect->SetActive(false, false);
	}

	LockerRespawnTime = 30.f;
	LockerString = LOCTEXT("LockerString", "Weapon Locker");

	LockerPositions.Add(FVector(18.0, -30.0, 0.f));
	LockerPositions.Add(FVector(-15.0, 25.0, 0.f));
	LockerPositions.Add(FVector(34.0, -2.0, 0.f));
	LockerPositions.Add(FVector(-30.0, 0.0, 0.f));
	LockerPositions.Add(FVector(16.0, 22.0, 0.f));
	LockerPositions.Add(FVector(-19.0, -32.0, 0.f));

	LockerFloatHeight = 55.f;

	WeaponLockerOffset = FVector(0.f, 0.f, 15.f);
	WeaponLockerRotation = FRotator(0.f, 0.f, 270.f);
	WeaponLockerScale3D = FVector(0.75, 0.65f, 0.75f);

	ProximityDistanceSquared = 1500000.0f;
	ScaleRate = 2.f;

	bClearCustomersOnReset = true;

	GlobalState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponLockerState>(this, TEXT("StateGlobal"));
	
	FStateInfo PickupStateInfo(UUTWeaponLockerStatePickup::StaticClass(), true);
	States.Add(PickupStateInfo);

	FStateInfo DisabledStateInfo(UUTWeaponLockerStateDisabled::StaticClass());
	States.Add(DisabledStateInfo);

	FStateInfo SleepingStateInfo(UUTWeaponLockerStateSleeping::StaticClass());
	States.Add(SleepingStateInfo);

	// Structure to hold one-time initialization
	struct FConstructorStaticsWarn
	{
		ConstructorHelpers::FObjectFinder<UClass> Redeemer;
		ConstructorHelpers::FObjectFinder<UClass> Sniper;
		//ConstructorHelpers::FObjectFinder<UClass> Avril;

		FConstructorStaticsWarn()
			: Redeemer(TEXT("Class'/Game/RestrictedAssets/Weapons/Redeemer/BP_Redeemer.BP_Redeemer_C'"))
			, Sniper(TEXT("Class'/Game/RestrictedAssets/Weapons/Sniper/BP_Sniper.BP_Sniper_C'"))
			//, Avril(TEXT("Class'/Game/RestrictedAssets/Weapons/Avril/BP_Avril.BP_Avril_C'"))
		{
		}
	};
	static FConstructorStaticsWarn ConstructorStaticsWarn;
	WarnIfInLocker.AddUnique(ConstructorStaticsWarn.Redeemer.Object);
	WarnIfInLocker.AddUnique(ConstructorStaticsWarn.Sniper.Object);
	//WarnIfInLocker.AddUnique(ConstructorStaticsWarn.Avril.Object);
	WarnIfInLocker.Remove(NULL);


	// Structure to hold one-time initialization
	struct FConstructorStaticsAmmo
	{
		ConstructorHelpers::FObjectFinder<UClass> Stinger;

		FConstructorStaticsAmmo()
			: Stinger(TEXT("Class'/Game/RestrictedAssets/Weapons/Minigun/BP_Minigun.BP_Minigun_C'"))
		{
		}
	};
	static FConstructorStaticsAmmo ConstructorStaticsAmmo;

	// negative to multiple ammo by its absolute value
	WeaponLockerAmmo.Add(AUTWeap_BioRifle::StaticClass(), -2.f);
	WeaponLockerAmmo.Add(AUTWeap_ShockRifle::StaticClass(), -1.5f);
	WeaponLockerAmmo.Add(AUTWeap_RocketLauncher::StaticClass(), -2.f);
	WeaponLockerAmmo.Add(AUTWeap_LinkGun::StaticClass(), -2.f);
	WeaponLockerAmmo.Add(AUTWeap_FlakCannon::StaticClass(), -2.f);
	WeaponLockerAmmo.Add(ConstructorStaticsAmmo.Stinger.Object, -1.5f);

	// runtime vars initialisation
	MaxDesireability = 0.f;

	bIsActive = false;
	bIsDisabled = false;
	bIsSleeping = false;

	bScalingUp = false;
	CurrentWeaponScaleX = 0.f;

	bPlayerNearby = false;
	bForceNearbyPlayers = false;
	NextProximityCheckTime = 0.f;
}

void AUTWeaponLocker::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		for (auto& StateInfo : States)
		{
			if (StateInfo.StateClass)
			{
				if (StateInfo.State == NULL)
				{
					StateInfo.State = NewObject<UUTWeaponLockerState>(this, StateInfo.StateClass);
				}

				if (StateInfo.bAuto)
				{
					AutoState = StateInfo.State;
				}

				if (StateInfo.StateName == FName(TEXT("Pickup")))
					PickupState = StateInfo.State;
				else if (StateInfo.StateName == FName(TEXT("Disabled")))
					DisabledState = StateInfo.State;
				else if (StateInfo.StateName == FName(TEXT("Sleeping")))
					SleepingState = StateInfo.State;
			}
		}
	}

	if (InitialState == NULL && AutoState == NULL)
	{
		InitialState = GlobalState;
	}

#if WITH_EDITOR
	CreateEditorPickupMeshes();
#endif
}

void AUTWeaponLocker::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ReceivePreBeginPlay();

	WeaponsCopy = Weapons;
}

void AUTWeaponLocker::BeginPlay()
{
	if (bIsDisabled)
	{
		return;
	}

	Super::BeginPlay();

	InitializeWeapons();
	SetInitialState();
}

void AUTWeaponLocker::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTWeaponLocker, LockerRespawnTime);
	DOREPLIFETIME(AUTWeaponLocker, bIsSleeping);
	DOREPLIFETIME(AUTWeaponLocker, bIsDisabled);
	DOREPLIFETIME_CONDITION(AUTWeaponLocker, Weapons, COND_None);
	DOREPLIFETIME_CONDITION(AUTWeaponLocker, ReplacementWeapons, COND_InitialOnly);
}

void AUTWeaponLocker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState->Tick(DeltaTime);
	}
}

void AUTWeaponLocker::PostActorCreated()
{
	Super::PostActorCreated();

	// no need load the editor mesh when there is no editor, meshes are created dynamically in Tick
#if WITH_EDITOR
	if (!IsRunningCommandlet())
	{
		CreateEditorPickupMeshes();
	}
#endif
}

void AUTWeaponLocker::InitializeWeapons_Implementation()
{
	// clear out null entries
	for (int32 i = 0; i < Weapons.Num() && i < LockerPositions.Num(); i++)
	{
		if (Weapons[i].WeaponClass == NULL)
		{
			Weapons.RemoveAt(i, 1);
			i--;
		}
	}

	if (LockerWeapons.Num() > 0)
	{
		DestroyWeapons();
		LockerWeapons.Empty();
	}

	// initialize weapons
	MaxDesireability = 0.f;
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		// add desirability
		MaxDesireability += Weapons[i].WeaponClass.GetDefaultObject()->BaseAISelectRating;

		// create local weapon info object
		LockerWeapons.Add(FWeaponInfo(Weapons[i].WeaponClass));
	}

	// force locker to re-create weapon meshes
	bPlayerNearby = false;
}

void AUTWeaponLocker::DestroyWeapons_Implementation()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::DestroyWeapons"), *GetName());
	for (int32 i = 0; i < LockerWeapons.Num(); i++)
	{
		if (LockerWeapons[i].PickupMesh != NULL)
		{
			UnregisterComponentTree(LockerWeapons[i].PickupMesh);
			LockerWeapons[i].PickupMesh = NULL;
		}
	}
}

void AUTWeaponLocker::CreatePickupMeshForSlot(UMeshComponent*& PickupMesh, int32 SlotIndex, TSubclassOf<AUTInventory> PickupInventoryType)
{
	if (LockerPositions.IsValidIndex(SlotIndex))
	{
		const FRotator RotationOffset = WeaponLockerRotation;
		AUTPickupInventory::CreatePickupMesh(this, PickupMesh, PickupInventoryType, 0.f, RotationOffset, false);
		if (PickupMesh != NULL)
		{
			FVector LocationOffset(LockerPositions[SlotIndex] + WeaponLockerOffset);
			LocationOffset += FVector(0.f, 0.f, LockerFloatHeight);
			PickupMesh->SetRelativeLocation(LocationOffset);
			if (!WeaponLockerScale3D.IsZero())
			{
				PickupMesh->SetRelativeScale3D(PickupMesh->RelativeScale3D * WeaponLockerScale3D);
			}
		}
	}
}

void AUTWeaponLocker::OnRep_Weapons_Implementation()
{
	if (Weapons != WeaponsCopy)
	{
		WeaponsCopy = Weapons;
		InitializeWeapons();
	}
}

void AUTWeaponLocker::OnRep_ReplacementWeapons()
{
	for (int32 i = 0; i < ARRAY_COUNT(ReplacementWeapons); i++)
	{
		if (ReplacementWeapons[i].bReplaced)
		{
			if (i >= Weapons.Num())
			{
				Weapons.SetNum(i + 1);
			}
			Weapons[i].WeaponClass = ReplacementWeapons[i].WeaponClass;

			if (LockerWeapons.IsValidIndex(i) && LockerWeapons[i].PickupMesh != NULL)
			{
				UnregisterComponentTree(LockerWeapons[i].PickupMesh);
				LockerWeapons[i].PickupMesh = NULL;
			}
		}
	}

	InitializeWeapons();
}

void AUTWeaponLocker::OnRep_IsDisabled_Implementation()
{
	SetInitialStateGlobal();
}

void AUTWeaponLocker::Reset_Implementation()
{
	if (!State.bActive)
	{
		WakeUp();
	}

	if (!bIsDisabled && IsInState(SleepingState))
	{
		GotoState(PickupState);
	}

	if (bClearCustomersOnReset)
	{
		// clear customers, so players are able to pick up weapons again on reset
		Customers.Empty();
	}
}

bool AUTWeaponLocker::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	// TODO: vehicle consideration
	return bDefaultAllowPickup && Cast<AUTCharacter>(Other) != NULL &&
		!((AUTCharacter*)Other)->IsRagdoll() &&
		((AUTCharacter*)Other)->bCanPickupItems &&
		!HasCustomer(Other);
}

void AUTWeaponLocker::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	APawn* P = Cast<APawn>(OtherActor);
	if (P == NULL || P->bTearOff || !AllowPickupBy(P, true))
	{
		return;
	}
	else if (P->Controller == NULL)
	{
		// re-check later in case this Pawn is in the middle of spawning, exiting a vehicle, etc
		// and will have a Controller shortly
		GetWorldTimerManager().SetTimer(RecheckValidTouchHandle, this, &AUTWeaponLocker::RecheckValidTouch, 0.2f, false);
		return;
	}
	else
	{
		// make sure not touching through wall
		FVector TraceEnd = Collision ? Collision->GetComponentLocation() : GetActorLocation();
		if (GetWorld()->LineTraceTestByChannel(P->GetActorLocation(), TraceEnd, ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
		{
			GetWorldTimerManager().SetTimer(RecheckValidTouchHandle, this, &AUTWeaponLocker::RecheckValidTouch, 0.5f, false);
			return;
		}
	}

	GetWorldTimerManager().ClearTimer(RecheckValidTouchHandle);
	ProcessTouch(P);
}

void AUTWeaponLocker::ProcessTouch_Implementation(APawn* TouchedBy)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::ProcessTouch - TouchedBy: %s"), *GetName(), *GetNameSafe(TouchedBy));
	if (CurrentState && CurrentState->OverrideProcessTouch(TouchedBy))
	{
		UE_LOG(LogDebug, Verbose, TEXT("%s::ProcessTouch - Overriden"), *GetName());
		return;
	}

	Super::ProcessTouch_Implementation(TouchedBy);
}

void AUTWeaponLocker::CheckTouching()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::CheckTouching"), *GetName());

	bForceNearbyPlayers = true;
	GetWorldTimerManager().ClearTimer(CheckTouchingHandle);

	TArray<AActor*> Touching;
	GetOverlappingActors(Touching, APawn::StaticClass());
	FHitResult UnusedHitResult;
	for (AActor* TouchingActor : Touching)
	{
		APawn* P = Cast<APawn>(TouchingActor);
		if (P != NULL && P->GetMovementComponent() != NULL)
		{
			FHitResult UnusedHitResult;
			OnOverlapBegin(P, Cast<UPrimitiveComponent>(P->GetMovementComponent()->UpdatedComponent), INDEX_NONE, false, UnusedHitResult);
		}
	}

	bForceNearbyPlayers = false;

	// see if we should reset the timer
	float NextCheckInterval = 0.0f;
	for (const auto& PrevCustomer : Customers)
	{
		NextCheckInterval = FMath::Max<float>(NextCheckInterval, PrevCustomer.NextPickupTime - GetWorld()->TimeSeconds);
	}
	if (NextCheckInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(CheckTouchingHandle, this, &AUTWeaponLocker::CheckTouching, NextCheckInterval, false);
	}
}

void AUTWeaponLocker::RecheckValidTouch()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::RecheckValidTouch"), *GetName());
	CheckTouching();
}

void AUTWeaponLocker::GiveTo_Implementation(APawn* Target)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::GiveTo - Target: %s"), *GetName(), *GetNameSafe(Target));
	HandlePickUpWeapons(Target, true);
}

void AUTWeaponLocker::AnnouncePickup(AUTCharacter* P, TSubclassOf<AUTInventory> NewInventoryType, AUTInventory* NewInventory/* = nullptr*/)
{
	if (auto PC = Cast<APlayerController>(P->GetController()))
	{
		PC->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, NewInventoryType);
	}
}

void AUTWeaponLocker::HandlePickUpWeapons(AActor* Other, bool bHideWeapons)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::HandlePickUpWeapons - Other: %s - bHideWeapons: %i"), *GetName(), *GetNameSafe(Other), (int)bHideWeapons);
	if (CurrentState)
	{
		CurrentState->HandlePickUpWeapons(Other, bHideWeapons);
	}
}

void AUTWeaponLocker::GiveLockerWeapons(AActor* Other, bool bHideWeapons)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::GiveLockerWeapons - Other: %s - bHideWeapons: %i"), *GetName(), *GetNameSafe(Other), (int)bHideWeapons);
	AUTCharacter* Recipient = Cast<AUTCharacter>(Other);
	if (Recipient == NULL)
		return;

	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENetRole"), true);
	UE_LOG(LogDebug, Verbose, TEXT("%s::GiveLockerWeapons - Role: %s"), *GetName(), EnumPtr ? *EnumPtr->GetDisplayNameText(Role.GetValue()).ToString() : *FString(TEXT("")));

	APawn* DriverPawn = Recipient->DrivenVehicle ? Recipient->DrivenVehicle : Recipient;
	if (DriverPawn && DriverPawn->IsLocallyControlled())
	{
		if (bHideWeapons && bIsActive)
		{
			UE_LOG(LogDebug, Verbose, TEXT("%s::GiveLockerWeapons - Register player %s and start timer ShowActive in %i"), *GetName(), *GetNameSafe(DriverPawn->GetController()), LockerRespawnTime);

			// register local player by bind to the Died event in order
			// to track when the local player dies... to reset the locker
			RegisterLocalPlayer(DriverPawn->GetController());

			ShowHidden();
			GetWorldTimerManager().SetTimer(HideWeaponsHandle, this, &AUTWeaponLocker::ShowActive, LockerRespawnTime, false);
		}
		else
		{
			UE_LOG(LogDebug, Verbose, TEXT("%s::GiveLockerWeapons - Clear timer ShowActive"), *GetName());
			GetWorldTimerManager().ClearTimer(HideWeaponsHandle);
		}
	}

	if (Role < ROLE_Authority)
		return;
	if (bHideWeapons && !AddCustomer(Recipient))
		return;

	bool bWeaponAdded = false;
	AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		bool bAllowPickup = true;
		TSubclassOf<AUTWeapon> LocalInventoryType = Weapons[i].WeaponClass;
		if (LocalInventoryType && (UTGameMode == NULL || !UTGameMode->OverridePickupQuery(Recipient, LocalInventoryType, this, bAllowPickup) || bAllowPickup))
		{
			AUTWeapon* Copy = Recipient->FindInventoryType<AUTWeapon>(LocalInventoryType, true);
			if (Copy == NULL || !Copy->StackPickup(NULL))
			{
				FActorSpawnParameters Params;
				Params.bNoCollisionFail = true;
				Params.Instigator = Recipient;
				Copy = GetWorld()->SpawnActor<AUTWeapon>(LocalInventoryType, GetActorLocation(), GetActorRotation(), Params);
				if (Copy)
				{
					Recipient->AddInventory(Copy, true);
					bWeaponAdded = true;
				}
			}

			if (Copy)
			{
				int32 LockerAmmo = GetLockerAmmo(LocalInventoryType);
				if (LockerAmmo - Copy->Ammo > 0)
				{
					Copy->AddAmmo(LockerAmmo - Copy->Ammo);
				}				

				AnnouncePickup(Recipient, LocalInventoryType);

				if (Copy->PickupSound)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), Copy->PickupSound, this, SRT_All, false, FVector::ZeroVector, NULL, Recipient, false);
				}
			}
		}
	}

	if (bWeaponAdded)
	{
		Recipient->DeactivateSpawnProtection();
	}
}

void AUTWeaponLocker::RegisterLocalPlayer(AController* C)
{
	if (C == NULL)
		return;

	if (AUTCharacter* UTChar = Cast<AUTCharacter>(C->GetPawn()))
	{
		UTChar->OnDied.AddUniqueDynamic(this, &AUTWeaponLocker::OnPawnDied);
	}
}

void AUTWeaponLocker::NotifyLocalPlayerDead(APlayerController* PC)
{
	if (CurrentState)
	{
		CurrentState->NotifyLocalPlayerDead(PC);
	}
}

void AUTWeaponLocker::OnPawnDied(AController* Killer, const UDamageType* DamageType)
{
	AUTPlayerController* LocalPC(nullptr);
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC && PC->IsLocalPlayerController())
		{
			auto UTChar = Cast<AUTCharacter>(PC->GetPawn());
			if (UTChar == NULL || UTChar->Health <= 0)
			{
				LocalPC = PC;
				break;
			}
		}
	}

	if (LocalPC)
	{
		NotifyLocalPlayerDead(LocalPC);
	}
}

void UUTWeaponLockerStatePickup::NotifyLocalPlayerDead_Implementation(APlayerController* PC)
{
	GetOuterAUTWeaponLocker()->ShowActive();
}

void UUTWeaponLockerStatePickup::HandlePickUpWeapons_Implementation(AActor* Other, bool bHideWeapons)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::HandlePickUpWeapons (Pickup) - Other: %s - bHideWeapons: %i"), *GetName(), *GetNameSafe(Other), (int)bHideWeapons);
	GetOuterAUTWeaponLocker()->GiveLockerWeapons(Other, bHideWeapons);
}

void AUTWeaponLocker::SetPlayerNearby(APlayerController* PC, bool bNewPlayerNearby, bool bPlayEffects, bool bForce/* = false*/)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::SetPlayerNearby - PC: %s - bNewPlayerNearby: %i - bPlayEffects: %i - bForce: %i"), *GetName(), *GetNameSafe(PC), (int)bNewPlayerNearby, (int)bPlayEffects, (int)bForce);
	if (bNewPlayerNearby != bPlayerNearby || bForce)
	{
		bPlayerNearby = bNewPlayerNearby;
		if (GetNetMode() == NM_DedicatedServer)
		{
			return;
		}

		if (bPlayerNearby)
		{
			UE_LOG(LogDebug, Verbose, TEXT("%s::SetPlayerNearby - Show weapons"), *GetName());
			bScalingUp = true;
			CurrentWeaponScaleX = 0.1f;
			for (int32 i = 0; i < LockerWeapons.Num(); i++)
			{
				if (LockerWeapons[i].PickupMesh == NULL)
				{
					CreatePickupMeshForSlot(LockerWeapons[i].PickupMesh, i, LockerWeapons[i].WeaponClass);
					if (LockerWeapons[i].PickupMesh != NULL)
					{
						FVector NewScale = LockerWeapons[i].PickupMesh->GetComponentScale();
						if (ScaleRate > 0.f)
						{
							LockerWeapons[i].DesiredScale3D = NewScale;
							NewScale.X *= 0.1;
							NewScale.Z *= 0.1;

							LockerWeapons[i].PickupMesh->SetWorldScale3D(NewScale);
						}
					}
				}
				if (LockerWeapons[i].PickupMesh != NULL)
				{
					LockerWeapons[i].PickupMesh->SetHiddenInGame(false);

					if (bPlayEffects)
					{
						UGameplayStatics::SpawnEmitterAtLocation(this, WeaponSpawnEffectTemplate, LockerWeapons[i].PickupMesh->GetComponentLocation());
					}
				}
			}
			if (ProximityEffect)
			{
				ProximityEffect->SetActive(true);
			}
			GetWorld()->GetTimerManager().ClearTimer(DestroyWeaponsHandle);
		}
		else
		{
			UE_LOG(LogDebug, Verbose, TEXT("%s::SetPlayerNearby - Hide weapons"), *GetName());
			AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
			bPlayEffects = bPlayEffects && (WS == NULL || WS->EffectIsRelevant(this, GetActorLocation(), true, true, 10000.f, 0.f));
			bScalingUp = false;
			for (int32 i = 0; i < LockerWeapons.Num(); i++)
			{
				if (LockerWeapons[i].PickupMesh != NULL)
				{
					LockerWeapons[i].PickupMesh->SetHiddenInGame(true);
					if (bPlayEffects)
					{
						UGameplayStatics::SpawnEmitterAtLocation(this, WeaponSpawnEffectTemplate, LockerWeapons[i].PickupMesh->GetComponentLocation());
					}
				}
			}
			if (ProximityEffect)
			{
				ProximityEffect->DeactivateSystem();
			}
			GetWorldTimerManager().SetTimer(DestroyWeaponsHandle, this, &AUTWeaponLocker::DestroyWeapons, 5.f, false);
		}
	}
}

bool AUTWeaponLocker::AddCustomer(APawn* P)
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(P);
	if (UTChar == NULL)
		return false;

	if (Customers.Num() > 0)
	{
		for (int32 i = 0; i < Customers.Num(); i++)
		{
			if (Customers[i].NextPickupTime < GetWorld()->TimeSeconds)
			{
				if (Customers[i].P == P)
				{
					Customers[i].NextPickupTime = GetWorld()->TimeSeconds + LockerRespawnTime;
					return true;
				}
				Customers.RemoveAt(i, 1);
				i--;
			}
			else if (Customers[i].P == P)
			{
				return false;
			}
		}
	}

	FWeaponPickupCustomer PT(P, GetWorld()->TimeSeconds + LockerRespawnTime);
	Customers.Add(PT);
	return true;
}

bool AUTWeaponLocker::HasCustomer(APawn* TestPawn)
{
	for (int32 i = Customers.Num() - 1; i >= 0; i--)
	{
		if (Customers[i].P == NULL || Customers[i].P->bTearOff || Customers[i].P->bPendingKillPending)
		{
			Customers.RemoveAt(i);
		}
		else if (Customers[i].P == TestPawn)
		{
			return (GetWorld()->TimeSeconds < Customers[i].NextPickupTime);
		}
	}
	return false;
}

void AUTWeaponLocker::AddWeapon(TSubclassOf<AUTWeapon> NewWeaponClass)
{
	if (Role < ROLE_Authority)
		return;

	FWeaponEntry NewWeaponEntry(NewWeaponClass);
	Weapons.Add(NewWeaponEntry);

	InitializeWeapons();
}

void AUTWeaponLocker::ReplaceWeapon(int32 Index, TSubclassOf<AUTWeapon> NewWeaponClass)
{
	if (Role < ROLE_Authority)
		return;

	if (Index >= 0)
	{
		if (Index >= Weapons.Num())
		{
			Weapons.SetNum(Index + 1);
		}
		Weapons[Index].WeaponClass = NewWeaponClass;
		if (Index < ARRAY_COUNT(ReplacementWeapons))
		{
			ReplacementWeapons[Index].bReplaced = true;
			ReplacementWeapons[Index].WeaponClass = NewWeaponClass;
		}

		// update weapons locally
		if (GetNetMode() != NM_DedicatedServer)
		{
			OnRep_ReplacementWeapons();
		}
	}
}

void AUTWeaponLocker::StartSleeping_Implementation()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::StartSleeping"), *GetName());
	// override original sleeping mechanism

	if (CurrentState)
	{
		CurrentState->StartSleeping();
	}
}

void AUTWeaponLocker::ShowActive()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::ShowActive"), *GetName());
	if (CurrentState)
	{
		CurrentState->ShowActive();
	}
}

// Note: This is only in LockerPickup state
void UUTWeaponLockerStatePickup::ShowActive_Implementation()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::ShowActive (Pickup)"), *GetName());
	GetOuterAUTWeaponLocker()->bIsActive = true;
	GetOuterAUTWeaponLocker()->NextProximityCheckTime = 0.f;
	if (GetOuterAUTWeaponLocker()->AmbientEffect)
	{
		GetOuterAUTWeaponLocker()->AmbientEffect->SetTemplate(GetOuterAUTWeaponLocker()->ActiveEffectTemplate);
	}

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		UE_LOG(LogDebug, Verbose, TEXT("%s::ShowActive (Pickup) - Call CheckTouching on client"), *GetName());
		GetOuterAUTWeaponLocker()->CheckTouching();
	}
}

void AUTWeaponLocker::ShowHidden()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::ShowHidden"), *GetName());
	if (CurrentState)
	{
		CurrentState->ShowHidden();
	}
	else
	{
		ShowHiddenGlobal();
	}
}

void AUTWeaponLocker::ShowHiddenGlobal_Implementation()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::ShowHiddenGlobal"), *GetName());
	bIsActive = false;
	SetPlayerNearby(nullptr, false, false, bForceNearbyPlayers);
	if (AmbientEffect)
	{
		AmbientEffect->SetTemplate(InactiveEffectTemplate);
	}
}

void AUTWeaponLocker::SetInitialState()
{
	if (CurrentState)
	{
		CurrentState->SetInitialState();
	}
	else
	{
		SetInitialStateGlobal();
	}
}

void AUTWeaponLocker::SetInitialStateGlobal_Implementation()
{
	if (bIsDisabled)
	{
		GotoState(DisabledState);
	}
	else
	{
		GotoState(InitialState != NULL ? InitialState : AutoState);
	}
}

void AUTWeaponLocker::GotoState(UUTWeaponLockerState* NewState)
{
	if (NewState == NULL || !NewState->IsIn(this))
	{
		UE_LOG(LogDebug, Warning, TEXT("Attempt to send %s to invalid state %s"), *GetName(), *GetNameSafe(NewState));
		NewState = GlobalState;
	}

	if (NewState)
	{
		if (CurrentState != NewState)
		{
			UUTWeaponLockerState* PrevState = CurrentState;
			if (CurrentState != NULL)
			{
				CurrentState->EndState(NewState); // NOTE: may trigger another GotoState() call
			}
			if (CurrentState == PrevState)
			{
				CurrentState = NewState;
				CurrentState->BeginState(PrevState); // NOTE: may trigger another GotoState() call
				StateChanged();
			}
		}
	}
}

void AUTWeaponLocker::StateChanged_Implementation()
{
}

float AUTWeaponLocker::BotDesireability_Implementation(APawn* Asker, float TotalDistance)
{
	if (CurrentState)
	{
		return CurrentState->BotDesireability(Asker, TotalDistance);
	}

	return BotDesireabilityGlobal(Asker, TotalDistance);
}

float AUTWeaponLocker::BotDesireabilityGlobal_Implementation(APawn* Asker, float TotalDistance)
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(Asker);
	if (bHidden || Asker == NULL || UTChar == NULL)
		return 0.f;

	AUTBot* Bot = Cast<AUTBot>(Asker->Controller);
	if (Bot == NULL || HasCustomer(Asker))
		return 0.f;

	float desire = 0.f;
	
	// see if bot already has a weapon of this type
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		if (Weapons[i].WeaponClass != NULL)
		{
			AUTWeapon* AlreadyHas = UTChar->FindInventoryType<AUTWeapon>(Weapons[i].WeaponClass, true);
			if (AlreadyHas == NULL)
			{
				desire += Weapons[i].WeaponClass.GetDefaultObject()->BaseAISelectRating;
			}
			else if (AlreadyHas->Ammo < AlreadyHas->GetClass()->GetDefaultObject<AUTWeapon>()->Ammo) // NeedAmmo()
			{
				desire += 0.15;
			}
		}
	}

	if (Bot->HuntingTarget && UTChar->GetWeapon() && (desire * 0.833 < UTChar->GetWeapon()->BaseAISelectRating - 0.1f))
	{
		return 0.f;
	}

	// incentivize bot to get this weapon if it doesn't have a good weapon already
	if ((UTChar->GetWeapon() == NULL) || (UTChar->GetWeapon()->BaseAISelectRating < 0.5f))
	{
		return 2.f * desire;
	}

	return desire;
}

float AUTWeaponLocker::DetourWeight_Implementation(APawn* Asker, float PathDistance)
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(Asker);
	if (bHidden || Asker == NULL || UTChar == NULL)
		return 0.f;

	AUTBot* Bot = Cast<AUTBot>(Asker->Controller);
	if (Bot == NULL || HasCustomer(Asker))
		return 0.f;

	float desire = 0.f;

	// see if bot already has a weapon of this type
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		AUTWeapon* AlreadyHas = UTChar->FindInventoryType<AUTWeapon>(Weapons[i].WeaponClass, true);
		if (AlreadyHas == NULL)
		{
			desire += Weapons[i].WeaponClass.GetDefaultObject()->BaseAISelectRating;
		}
		else if (AlreadyHas->Ammo < AlreadyHas->GetClass()->GetDefaultObject<AUTWeapon>()->Ammo) // NeedAmmo()
		{
			desire += 0.15;
		}
	}

	float PathWeight = PathDistance;
	if (Bot->GetSquad() && Bot->GetSquad()->HasHighPriorityObjective(Bot)
		&& ((UTChar->GetWeapon() && UTChar->GetWeapon()->BaseAISelectRating > 0.5) || (PathWeight > 400)))
	{
		return 0.2f / PathWeight;
	}

	return PathWeight != 0.f ? desire / PathWeight : 0.f;
}

#if WITH_EDITOR

void AUTWeaponLocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	CreateEditorPickupMeshes();
}

void AUTWeaponLocker::CreateEditorPickupMeshes()
{
	if (GetWorld() != NULL && GetWorld()->WorldType == EWorldType::Editor)
	{
		for (auto& EditorMesh : EditorMeshes)
		{
			if (EditorMesh)
			{
				UnregisterComponentTree(EditorMesh);
				EditorMesh->DestroyComponent();
				EditorMesh = NULL;
			}
		}

		EditorMeshes.Empty();

		for (int32 i = 0; i < Weapons.Num() && i < LockerPositions.Num(); i++)
		{
			EditorMeshes.AddZeroed(1);
			if (Weapons[i].WeaponClass != NULL)
			{
				CreatePickupMeshForSlot(EditorMeshes[i], i, Weapons[i].WeaponClass);
				if (EditorMeshes[i] != NULL)
				{
					EditorMeshes[i]->SetHiddenInGame(true);
				}
			}
		}
	}
}

void AUTWeaponLocker::CheckForErrors()
{
	Super::CheckForErrors();

	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));

		FText Message = FText();
		if (Weapons[i].WeaponClass == NULL)
		{	
			Message = LOCTEXT("MapCheck_Message_WeaponClassInLockerEmpty", "Empty weapon class in weapon locker");
		}
		else
		{
			Arguments.Add(TEXT("WeaponClass"), FText::FromString(Weapons[i].WeaponClass->GetName()));

			if (RespawnTime < Weapons[i].WeaponClass.GetDefaultObject()->RespawnTime)
			{
				static const FNumberFormattingOptions RespawnTimeFormat = FNumberFormattingOptions()
					.SetMinimumFractionalDigits(0)
					.SetMaximumFractionalDigits(1);

				Arguments.Add(TEXT("LockerRespawnTime"), FText::AsNumber(RespawnTime, &RespawnTimeFormat));
				Arguments.Add(TEXT("WeaponRespawnTime"), FText::AsNumber(Weapons[i].WeaponClass.GetDefaultObject()->RespawnTime, &RespawnTimeFormat));
				
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_WeaponClassRespawnTimeFaster", "{WeaponClass} is in weapon lockers which will respawn items each {LockerRespawnTime}s (instead of {WeaponRespawnTime}s)."), Arguments)));
			}

			TSubclassOf<AUTWeapon> WeaponSuper = Weapons[i].WeaponClass;
			while (WeaponSuper != NULL)
			{
				if (WarnIfInLocker.Find(WeaponSuper) != INDEX_NONE)
				{
					Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
					Arguments.Add(TEXT("WarnClass"), FText::FromString(WeaponSuper->GetName()));
					Message = LOCTEXT("MapCheck_Message_WeaponClassInLockerWarning", "{WeaponClass} should not be in weapon lockers.");
					WeaponSuper = NULL;
				}
				else
				{
					WeaponSuper = WeaponSuper->GetSuperClass();
				}
			}
		}

		if (!Message.IsEmpty())
		{
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(Message, Arguments)));
		}
	}

	TArray<FString> StateErrors;
	if (HasStateErrors(StateErrors))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));

		for (auto StateError : StateErrors)
		{
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(FText::FromString(StateError), Arguments)));
		}
	}
}

void AUTWeaponLocker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName != MemberName && MemberName == GET_MEMBER_NAME_CHECKED(AUTWeaponLocker, States))
	{
		// count auto states
		TArray<FString> AutoStates;
		TArray<FString> NoNamesStates;
		TArray<FString> DupNamesStates;
		TArray<FName> StateNames;
		for (int32 i = 0; i < States.Num(); i++)
		{
			FString StateInfo = FString::Printf(TEXT("[%i] Name: %s  State: %s"), i, *States[i].StateName.ToString(), States[i].State ? *States[i].State->GetName() : TEXT("None"));
			if (States[i].bAuto)
			{
				AutoStates.Add(StateInfo);
			}

			if (States[i].StateName.IsNone())
			{
				NoNamesStates.Add(StateInfo);
			}
			else
			{
				if (StateNames.Find(States[i].StateName) > INDEX_NONE)
				{
					DupNamesStates.Add(StateInfo);
				}
				StateNames.Add(States[i].StateName);
			}
		}

		FString OptionalNewLine = NewLine;
		TArray<FString> MessageStrs;
		if (AutoStates.Num() > 1)
		{
			MessageStrs.Add(FString::Printf(TEXT("Multiple States have 'Auto' flag set: %s%s"), *OptionalNewLine, *FString::Join(AutoStates, *NewLine)));
		}
		if (NoNamesStates.Num() > 0)
		{
			MessageStrs.Add(FString::Printf(TEXT("Some States have no name: %s%s"), *OptionalNewLine, *FString::Join(NoNamesStates, *NewLine)));
		}
		if (DupNamesStates.Num() > 0)
		{
			MessageStrs.Add(FString::Printf(TEXT("Duplicate state names found: %s%s"), *OptionalNewLine, *FString::Join(DupNamesStates, *NewLine)));
		}

		if (MessageStrs.Num() > 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Join(MessageStrs, *NewParagraph)));
		}
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(AUTWeaponLocker, Weapons))
	{
		CreateEditorPickupMeshes();
	}
}

void AUTWeaponLocker::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();
	const FName PropertyName = PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue()->GetFName();

	if (PropertyChangedEvent.Property && PropertyName != MemberPropertyName && MemberPropertyName == GET_MEMBER_NAME_CHECKED(AUTWeaponLocker, States))
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
		{
			const FName TailPropName = PropertyChangedEvent.PropertyChain.GetTail()->GetValue()->GetFName();

			int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberPropertyName.ToString());
			if (States.IsValidIndex(NewArrayIndex))
			{
				if (TailPropName == GET_MEMBER_NAME_CHECKED(FStateInfo, StateClass))
				{
					TSubclassOf<UScriptState> DefaultStateClass = States[NewArrayIndex].StateClass;
					if (DefaultStateClass && (States[NewArrayIndex].StateName.IsNone() || !States[NewArrayIndex].bUserChanged))
					{
						States[NewArrayIndex].StateName = DefaultStateClass.GetDefaultObject()->DefaultStateName;
					}
				}
				else if (TailPropName == GET_MEMBER_NAME_CHECKED(FStateInfo, StateName))
				{
					States[NewArrayIndex].bUserChanged = !States[NewArrayIndex].StateName.IsNone();
				}
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

bool AUTWeaponLocker::HasStateErrors(TArray<FString>& StateErrors)
{
	TArray<FString> AutoStates;
	TArray<FString> NoNamesStates;
	TArray<FString> DupNamesStates;
	TArray<FString> InvalidStates;
	TArray<FName> StateNames;
	for (int32 i = 0; i < States.Num(); i++)
	{
		FString StateInfo = FString::Printf(TEXT("[%i] Name: %s  State: %s"), i, *States[i].StateName.ToString(), States[i].State ? *States[i].State->GetName() : TEXT("None"));
		if (States[i].bAuto)
		{
			AutoStates.Add(StateInfo);
		}

		if (States[i].StateName.IsNone())
		{
			NoNamesStates.Add(StateInfo);
		}
		else
		{
			if (StateNames.Find(States[i].StateName) > INDEX_NONE)
			{
				DupNamesStates.Add(StateInfo);
			}
			StateNames.Add(States[i].StateName);
		}

		if (States[i].State == NULL)
		{
			InvalidStates.Add(StateInfo);
		}
	}

	if (AutoStates.Num() > 1)
	{
		StateErrors.Add(FString(TEXT("Multiple states have 'Auto' flag set.")));
	}
	if (NoNamesStates.Num() > 0)
	{
		StateErrors.Add(FString(TEXT("Some states have no name.")));
	}
	if (DupNamesStates.Num() > 0)
	{
		StateErrors.Add(FString(TEXT("Duplicate state names found.")));
	}
	if (InvalidStates.Num() > 0)
	{
		StateErrors.Add(FString(TEXT("Some states have no class set.")));
	}

	return StateErrors.Num() > 0;
}

#endif // WITH_EDITOR

void UUTWeaponLockerStateDisabled::SetInitialState_Implementation()
{
	// override
	return;
}

void UUTWeaponLockerStateDisabled::BeginState_Implementation(const UUTWeaponLockerState* PrevState)
{
	Super::BeginState_Implementation(PrevState);

	GetOuterAUTWeaponLocker()->SetActorHiddenInGame(true);
	GetOuterAUTWeaponLocker()->SetActorEnableCollision(false);

	GetOuterAUTWeaponLocker()->ShowHidden();
}

float UUTWeaponLockerStateDisabled::BotDesireability_Implementation(APawn* Asker, float TotalDistance)
{
	return 0.f;
}

bool UUTWeaponLockerStateDisabled::OverrideProcessTouch_Implementation(APawn* TouchedBy)
{
	return true;
}

void UUTWeaponLockerStateSleeping::StartSleeping_Implementation()
{
}

void UUTWeaponLockerStateSleeping::BeginState_Implementation(const UUTWeaponLockerState* PrevState)
{
	Super::BeginState_Implementation(PrevState);

	GetOuterAUTWeaponLocker()->SetActorEnableCollision(false);
	GetOuterAUTWeaponLocker()->ShowHidden();

	GetOuterAUTWeaponLocker()->bIsSleeping = true;
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		GetOuterAUTWeaponLocker()->OnRep_IsSleeping();
	}
}

void UUTWeaponLockerStateSleeping::EndState_Implementation(const UUTWeaponLockerState* NextState)
{
	Super::EndState_Implementation(NextState);

	GetOuterAUTWeaponLocker()->bIsSleeping = false;
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		GetOuterAUTWeaponLocker()->OnRep_IsSleeping();
	}
}

float UUTWeaponLockerStateSleeping::BotDesireability_Implementation(APawn* Asker, float TotalDistance)
{
	return 0.f;
}

bool UUTWeaponLockerStateSleeping::OverrideProcessTouch_Implementation(APawn* TouchedBy)
{
	return true;
}

void UUTWeaponLockerStatePickup::BeginState_Implementation(const UUTWeaponLockerState* PrevState)
{
	Super::BeginState_Implementation(PrevState);

	// allow to re-enable weapon locker
	GetOuterAUTWeaponLocker()->SetActorHiddenInGame(false);
	GetOuterAUTWeaponLocker()->SetActorEnableCollision(true);

	GetOuterAUTWeaponLocker()->ShowActive();
}

bool UUTWeaponLockerStatePickup::OverrideProcessTouch_Implementation(APawn* TouchedBy)
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::OverrideProcessTouch (Pickup) - TouchedBy: %s"), *GetName(), *GetNameSafe(TouchedBy));

	// handle client effects (hiding weapons in locker).
	// ProcessTouch is aborting on the cient machine and won't trigger GiveLockerWeapons
	// HandlePickUpWeapons is aborting itself and only hiding the weapons
	if (GetOuterAUTWeaponLocker()->Role < ROLE_Authority && GetOuterAUTWeaponLocker()->bIsActive && TouchedBy && TouchedBy->Controller && TouchedBy->Controller->IsLocalController())
	{
		if (GetOuterAUTWeaponLocker()->AllowPickupBy(TouchedBy, true))
		{
			UE_LOG(LogDebug, Verbose, TEXT("%s::OverrideProcessTouch (Pickup) - Handle client touch"), *GetName());
			GetOuterAUTWeaponLocker()->HandlePickUpWeapons(TouchedBy, true);
			GetOuterAUTWeaponLocker()->StartSleeping();
		}
	}

	return false;
}

void UUTWeaponLockerStatePickup::StartSleeping_Implementation()
{
	UE_LOG(LogDebug, Verbose, TEXT("%s::StartSleeping (Pickup)"), *GetName());
	if (auto WL = GetOuterAUTWeaponLocker())
	{
		if (WL->LockerRespawnTime <= 0.f)
		{
			WL->GotoState(WL->SleepingState);
		}
		else if (WL->Role == ROLE_Authority)
		{
			const bool bTimerActive = WL->GetWorldTimerManager().IsTimerActive(*WL->GetCheckTouchingHandle());
			UE_LOG(LogDebug, Verbose, TEXT("%s::StartSleeping - CheckTouching Timer active: %s"), *GetName(), bTimerActive ? TEXT("true") : TEXT("false"));
			if (!bTimerActive)
			{
				WL->GetWorldTimerManager().SetTimer(*WL->GetCheckTouchingHandle(), WL, &AUTWeaponLocker::CheckTouching, WL->LockerRespawnTime, false);
			}
		}
	}
}

void UUTWeaponLockerStatePickup::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	auto WL = GetOuterAUTWeaponLocker();
	if (WL == NULL)
		return;

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		if (GetWorld()->TimeSeconds > WL->NextProximityCheckTime)
		{
			APlayerController* NearbyPC = nullptr;
			bool bNewPlayerNearby = false;

			WL->NextProximityCheckTime = GetWorld()->TimeSeconds + 0.2f + 0.1f * FMath::FRand();
			if (WL->bIsActive)
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->GetPawn())
					{
						if ((WL->GetActorLocation() - It->PlayerController->GetPawn()->GetActorLocation()).SizeSquared() < WL->ProximityDistanceSquared)
						{
							UE_LOG(LogDebug, Verbose, TEXT("%s::Tick (Pickup) - %s is near this locker"), *GetName(), *GetNameSafe(It->PlayerController));
							bNewPlayerNearby = true;
							NearbyPC = It->PlayerController;
							break;
						}
					}
				}
			}

			if (bNewPlayerNearby != WL->bPlayerNearby)
			{
				WL->SetPlayerNearby(NearbyPC, bNewPlayerNearby, true);
			}
		}

		if (WL->bScalingUp && WL->ScaleRate > 0.f)
		{
			WL->CurrentWeaponScaleX += DeltaTime * WL->ScaleRate;
			if (WL->CurrentWeaponScaleX >= 1.f)
			{
				WL->CurrentWeaponScaleX = 1.f;
				WL->bScalingUp = false;
			}

			for (int32 i = 0; i < WL->LockerWeapons.Num(); i++)
			{
				if (WL->LockerWeapons[i].PickupMesh != NULL)
				{
					FVector NewScale = WL->LockerWeapons[i].DesiredScale3D * WL->CurrentWeaponScaleX;
					NewScale.Y = WL->LockerWeapons[i].DesiredScale3D.Y;
					WL->LockerWeapons[i].PickupMesh->SetWorldScale3D(NewScale);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE