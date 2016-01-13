#include "UTWeaponLockerPlugin.h"
#include "UnrealNetwork.h"
#include "UTSquadAI.h"
#include "UTWeaponLocker.h"

#include "MessageLog.h"
#include "UObjectToken.h"

#include "UTWorldSettings.h"
#include "UTPickupMessage.h"

#define LOCTEXT_NAMESPACE "UTWeaponLocker"

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
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTPickup::OnOverlapBegin);
	Collision->RelativeLocation.Z = 110.f;

	BaseMesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("BaseMeshComp"));
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//BaseMesh->SetStaticMesh(ConstructorStatics.BaseMesh.Object);
	BaseMesh->AttachParent = RootComponent;

	if (TimerEffect != NULL)
	{
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
}

void AUTWeaponLocker::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ReceivePreBeginPlay();
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

	// initialize weapons
	MaxDesireability = 0.f;
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		MaxDesireability += Weapons[i].WeaponClass.GetDefaultObject()->BaseAISelectRating;
	}
}

void AUTWeaponLocker::DestroyWeapons_Implementation()
{
	for (int32 i = 0; i < Weapons.Num(); i++)
	{
		if (Weapons[i].PickupMesh != NULL)
		{
			UnregisterComponentTree(Weapons[i].PickupMesh);
			Weapons[i].PickupMesh = NULL;
		}
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
			if (Weapons[i].PickupMesh != NULL)
			{
				UnregisterComponentTree(Weapons[i].PickupMesh);
				Weapons[i].PickupMesh = NULL;
			}
		}
	}

	InitializeWeapons();
}

void AUTWeaponLocker::OnRep_IsDisabled()
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

	// TODO: Clear customers
}

bool AUTWeaponLocker::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	// TODO: vehicle consideration
	return bDefaultAllowPickup && Cast<AUTCharacter>(Other) != NULL &&
		!((AUTCharacter*)Other)->IsRagdoll() &&
		((AUTCharacter*)Other)->bCanPickupItems &&
		!HasCustomer(Other);
}

void AUTWeaponLocker::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (CurrentState && CurrentState->OverrideProcessTouch(TouchedBy))
	{
		return;
	}

	Super::ProcessTouch_Implementation(TouchedBy);
}

void AUTWeaponLocker::GiveTo_Implementation(APawn* Target)
{
	GiveLockerWeapons(Target, true);
}

void AUTWeaponLocker::AnnouncePickup(AUTCharacter* P, TSubclassOf<AUTInventory> NewInventoryType, AUTInventory* NewInventory/* = nullptr*/)
{
	if (auto PC = Cast<APlayerController>(P->GetController()))
	{
		PC->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, NewInventoryType);
	}
}

void AUTWeaponLocker::GiveLockerWeapons(AActor* Other, bool bHideWeapons)
{
	if (CurrentState)
	{
		CurrentState->GiveLockerWeapons(Other, bHideWeapons);
	}
}

void AUTWeaponLocker::GiveLockerWeaponsInternal(AActor* Other, bool bHideWeapons)
{
	AUTCharacter* Recipient = Cast<AUTCharacter>(Other);
	if (Recipient == NULL)
		return;

	APawn* DriverPawn = Recipient->DrivenVehicle ? Recipient->DrivenVehicle : Recipient;
	if (DriverPawn && DriverPawn->IsLocallyControlled())
	{
		if (bHideWeapons && bIsActive)
		{
			// register local player by bind to the Died event in order
			// to track when the local player dies... to reset the locker
			RegisterLocalPlayer(DriverPawn->GetController());

			ShowHidden();
			GetWorldTimerManager().SetTimer(HideWeaponsHandle, this, &AUTWeaponLocker::ShowActive, LockerRespawnTime, false);
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
				if (Copy && Recipient->AddInventory(Copy, true))
				{
					AnnouncePickup(Recipient, LocalInventoryType);
				}
			}

			if (Copy && Copy->PickupSound)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), Copy->PickupSound, this, SRT_IfSourceNotReplicated, false, FVector::ZeroVector, NULL, Recipient, false);
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

// Note: This is only in LockerPickup state
void UUTWeaponLockerStatePickup::GiveLockerWeapons_Implementation(AActor* Other, bool bHideWeapons)
{
	GetOuterAUTWeaponLocker()->GiveLockerWeaponsInternal(Other, bHideWeapons);
}

void AUTWeaponLocker::SetPlayerNearby(APlayerController* PC, bool bNewPlayerNearby, bool bPlayEffects)
{
	if (bNewPlayerNearby != bPlayerNearby)
	{
		bPlayerNearby = bNewPlayerNearby;
		if (GetNetMode() == NM_DedicatedServer)
		{
			return;
		}

		if (bPlayerNearby)
		{
			bScalingUp = true;
			CurrentWeaponScaleX = 0.1f;
			for (int32 i = 0; i < Weapons.Num(); i++)
			{
				if (Weapons[i].PickupMesh == NULL)
				{
					const FRotator RotationOffset = WeaponLockerRotation;
					AUTPickupInventory::CreatePickupMesh(this, Weapons[i].PickupMesh, Weapons[i].WeaponClass, LockerFloatHeight, RotationOffset, false);
					if (Weapons[i].PickupMesh != NULL)
					{
						Weapons[i].PickupMesh->SetRelativeLocation(LockerPositions[i] + WeaponLockerOffset);
						if (!WeaponLockerScale3D.IsZero())
						{
							Weapons[i].PickupMesh->SetRelativeScale3D(Weapons[i].PickupMesh->RelativeScale3D * WeaponLockerScale3D);
						}

						FVector NewScale = Weapons[i].PickupMesh->GetComponentScale();
						if (ScaleRate > 0.f)
						{
							Weapons[i].DesiredScale3D = NewScale;
							NewScale.X *= 0.1;
							NewScale.Z *= 0.1;

							Weapons[i].PickupMesh->SetWorldScale3D(NewScale);
						}
					}
				}
				if (Weapons[i].PickupMesh != NULL)
				{
					Weapons[i].PickupMesh->SetHiddenInGame(false);

					if (bPlayEffects)
					{
						UGameplayStatics::SpawnEmitterAtLocation(this, WeaponSpawnEffectTemplate, Weapons[i].PickupMesh->GetComponentLocation());
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
			AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
			bPlayEffects = bPlayEffects && (WS == NULL || WS->EffectIsRelevant(this, GetActorLocation(), true, true, 10000.f, 0.f));
			bScalingUp = false;
			for (int32 i = 0; i < Weapons.Num(); i++)
			{
				if (Weapons[i].PickupMesh != NULL)
				{
					Weapons[i].PickupMesh->SetHiddenInGame(true);
					if (bPlayEffects)
					{
						UGameplayStatics::SpawnEmitterAtLocation(this, WeaponSpawnEffectTemplate, Weapons[i].PickupMesh->GetComponentLocation());
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

void AUTWeaponLocker::ReplaceWeapon(int32 Index, TSubclassOf<AUTWeapon> NewWeaponClass)
{
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
	}
}

void AUTWeaponLocker::StartSleeping_Implementation()
{
	// override original sleeping mechanism

	if (CurrentState)
	{
		CurrentState->StartSleeping();
	}
}

void AUTWeaponLocker::ShowActive()
{
	if (CurrentState)
	{
		CurrentState->ShowActive();
	}
}

// Note: This is only in LockerPickup state
void UUTWeaponLockerStatePickup::ShowActive_Implementation()
{
	GetOuterAUTWeaponLocker()->bIsActive = true;
	GetOuterAUTWeaponLocker()->NextProximityCheckTime = 0.f;
	if (GetOuterAUTWeaponLocker()->AmbientEffect)
	{
		GetOuterAUTWeaponLocker()->AmbientEffect->SetTemplate(GetOuterAUTWeaponLocker()->ActiveEffectTemplate);
	}
}

void AUTWeaponLocker::ShowHidden()
{
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
	bIsActive = false;
	SetPlayerNearby(nullptr, false, false);
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
		UE_LOG(LogDebug, Warning, TEXT("Attempt to send %s to invalid state %s"), *GetName(), *GetFullNameSafe(NewState));
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

		FString(*JoinArray)(const TArray<FString>& InStates, FString Delimiter);
		JoinArray = [](const TArray<FString>& InStates, FString Delimiter)
		{
			FString Str;
			if (InStates.Num() > 0)
			{
				Str = InStates[0];
				for (int32 i = 1; i < InStates.Num(); i++)
				{
					Str += Delimiter;
					Str += InStates[i];
				}
			}
			return Str;
		};

		TArray<FString> MessageStrs;
		if (AutoStates.Num() > 1)
		{
			MessageStrs.Add(FString::Printf(TEXT("Multiple States have 'Auto' flag set:\n%s"), *JoinArray(AutoStates, FString(TEXT("\n")))));
		}
		if (NoNamesStates.Num() > 0)
		{
			MessageStrs.Add(FString::Printf(TEXT("Some States have no name:\n%s"), *JoinArray(NoNamesStates, FString(TEXT("\n")))));
		}
		if (DupNamesStates.Num() > 0)
		{
			MessageStrs.Add(FString::Printf(TEXT("Duplicate state names found:\n%s"), *JoinArray(DupNamesStates, FString(TEXT("\n")))));
		}

		if (MessageStrs.Num() > 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(JoinArray(MessageStrs, FString(TEXT("\n\n")))));
		}
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
					States[NewArrayIndex].bUserChanged = true;
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
	// handle client effects (hiding weapons in locker).
	// ProcessTouch is aborting on the cient machine and won't trigger GiveLockerWeapons
	// GiveLockerWeapons is aborting itself and only hiding the weapons
	if (GetOuterAUTWeaponLocker()->Role < ROLE_Authority && GetOuterAUTWeaponLocker()->bIsActive && TouchedBy && TouchedBy->Controller && TouchedBy->Controller->IsLocalController())
	{
		if (GetOuterAUTWeaponLocker()->AllowPickupBy(TouchedBy, true))
		{
			GetOuterAUTWeaponLocker()->GiveLockerWeapons(TouchedBy, true);
		}
	}

	return false;
}

void UUTWeaponLockerStatePickup::StartSleeping_Implementation()
{
	if (GetOuterAUTWeaponLocker()->LockerRespawnTime <= 0.f)
	{
		GetOuterAUTWeaponLocker()->GotoState(GetOuterAUTWeaponLocker()->SleepingState);
	}
}

void UUTWeaponLockerStatePickup::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	auto WL = GetOuterAUTWeaponLocker();
	if (WL == NULL)
		return;

	if (WL->bIsActive && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		if (GetWorld()->TimeSeconds > WL->NextProximityCheckTime)
		{
			APlayerController* NearbyPC = nullptr;
			bool bNewPlayerNearby = false;

			WL->NextProximityCheckTime = GetWorld()->TimeSeconds + 0.2f + 0.1f * FMath::FRand();
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				if (It->PlayerController != NULL && It->PlayerController->GetPawn())
				{
					if ((WL->GetActorLocation() - It->PlayerController->GetPawn()->GetActorLocation()).SizeSquared() < WL->ProximityDistanceSquared)
					{
						bNewPlayerNearby = true;
						NearbyPC = It->PlayerController;
						break;
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

			for (int32 i = 0; i < WL->Weapons.Num(); i++)
			{
				if (WL->Weapons[i].PickupMesh != NULL)
				{
					FVector NewScale = WL->Weapons[i].DesiredScale3D * WL->CurrentWeaponScaleX;
					NewScale.Y = WL->Weapons[i].DesiredScale3D.Y;
					WL->Weapons[i].PickupMesh->SetWorldScale3D(NewScale);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE