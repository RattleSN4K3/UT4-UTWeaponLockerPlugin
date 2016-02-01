#pragma once

#include "UTWeaponLockerTypes.h"
#include "UTWeaponLocker.generated.h"

class UUTWeaponLockerState;

UCLASS(Abstract, CustomConstructor)
class UScriptState : public UObject
{
	GENERATED_UCLASS_BODY()

	UScriptState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = State)
	FName DefaultStateName;
};

USTRUCT(BlueprintType)
struct FStateInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(NoClear, EditDefaultsOnly, BlueprintReadWrite, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TSubclassOf<UUTWeaponLockerState> StateClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = State)
	UUTWeaponLockerState* State;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	FName StateName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	bool bAuto;

	UPROPERTY()
	bool bUserChanged;

	FORCEINLINE FStateInfo()
		: StateClass(NULL)
		, State(NULL)
		, StateName(NAME_None)
		, bAuto(false)
		, bUserChanged(false)
	{}
	
	FORCEINLINE FStateInfo(TSubclassOf<UUTWeaponLockerState> InStateClass)
		: FStateInfo(InStateClass, false)
	{}

	FORCEINLINE FStateInfo(TSubclassOf<UUTWeaponLockerState> InStateClass, FName InStateName)
		: FStateInfo(InStateClass, InStateName, false)
	{}

	FORCEINLINE FStateInfo(TSubclassOf<UUTWeaponLockerState> InStateClass, bool InAuto)
		: StateClass(InStateClass)
		, State(NULL)
		, bAuto(InAuto)
		, bUserChanged(false)
	{
		TSubclassOf<UScriptState> DefaultStateClass = InStateClass;
		if (DefaultStateClass && StateName.IsNone())
		{
			StateName = DefaultStateClass.GetDefaultObject()->DefaultStateName;
		}
		else
		{
			StateName = NAME_None;
		}
	}

	FORCEINLINE FStateInfo(TSubclassOf<UUTWeaponLockerState> InStateClass, FName InStateName, bool InAuto)
		: FStateInfo(InStateClass, InAuto)
	{
		StateName = InStateName;
		bUserChanged = true;
	}
};

USTRUCT(BlueprintType)
struct FWeaponEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	TSubclassOf<AUTWeapon> WeaponClass;

	FWeaponEntry() : WeaponClass(NULL) {}
	FWeaponEntry(TSubclassOf<AUTWeapon> InWeaponClass) : WeaponClass(InWeaponClass) {}

	bool operator ==(const FWeaponEntry& Other) const
	{
		return WeaponClass == Other.WeaponClass;
	}

	bool operator !=(const FWeaponEntry& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct FWeaponInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadonly, Category = Pickup)
	TSubclassOf<AUTWeapon> WeaponClass;
	
	UPROPERTY(BlueprintReadonly, Category = Pickup)
	UMeshComponent* PickupMesh;

	UPROPERTY(BlueprintReadonly, Category = Pickup)
	FVector DesiredScale3D;

	FWeaponInfo()
		: FWeaponInfo(NULL)
	{}
	FWeaponInfo(TSubclassOf<AUTWeapon> InWeaponClass)
		: WeaponClass(InWeaponClass)
		, PickupMesh(NULL)
		, DesiredScale3D(FVector::ZeroVector)
	{}

	bool operator ==(const FWeaponInfo& Other) const
	{
		return WeaponClass == Other.WeaponClass;
	}

	bool operator !=(const FWeaponInfo& Other) const
	{
		return !(*this == Other);
	}
};

/** when received on the client, replaces Weapons array classes with this array instead */
USTRUCT(BlueprintType)
struct FReplacementWeaponEntry
{
	GENERATED_USTRUCT_BODY()

	/** indicates whether this entry in the Weapons array was actually replaced
	* (so we can distinguish between WeaponClass == None because it wasn't touched vs. because it was removed
	*/
	bool bReplaced;
	/** the class of weapon to replace with */
	TSubclassOf<AUTWeapon> WeaponClass;

	FReplacementWeaponEntry()
		: WeaponClass(NULL), bReplaced(false)
	{}

	bool operator ==(const FReplacementWeaponEntry& Other) const
	{
		return WeaponClass == Other.WeaponClass;
	}

	bool operator !=(const FReplacementWeaponEntry& Other) const
	{
		return !(*this == Other);
	}
};

UCLASS(Blueprintable, meta = (ChildCanTick))
class AUTWeaponLocker : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLockerPickupStatusChangedDelegate, AUTWeaponLocker*, Locker, APawn*, P, ELockerPickupStatus::Type, Status);

	friend class UUTWeaponLockerState;
	friend class UUTWeaponLockerStateDisabled;
	friend class UUTWeaponLockerStatePickup;

	/** Broadcast when the pickup status changes [Server only] */
	UPROPERTY(BlueprintAssignable, Category = Events)
	FLockerPickupStatusChangedDelegate OnPickupStatusChange;

	// TODO: Move to UTWeapon as boolean flag
	TArray<TSubclassOf<AUTWeapon>> WarnIfInLocker;

	// TODO: Move to UTWeapon
	TMap<TSubclassOf<AUTWeapon>, float> WeaponLockerAmmo;

	int32 GetLockerAmmo(TSubclassOf<AUTWeapon> WeaponClass)
	{
		if (auto CDO = WeaponClass.GetDefaultObject())
		{
			int32 Ammo = WeaponClass.GetDefaultObject()->Ammo;
			float* AmmoMultiplier = WeaponLockerAmmo.Find(WeaponClass);
			if (AmmoMultiplier == NULL)
			{
				for (auto& Elem : WeaponLockerAmmo)
				{
					if (CDO->IsA(Elem.Key))
					{
						AmmoMultiplier = &Elem.Value;
						break;
					}
				}
			}

			if (AmmoMultiplier)
			{
				// Multiply weapon ammo count if value is negative, otherwise use the stored value
				float AmmoFactor = *AmmoMultiplier;
				Ammo = FMath::TruncToInt(AmmoFactor >= 0.f ? AmmoFactor : FMath::Abs((float)CDO->Ammo * AmmoFactor));
			}

			return Ammo;
		}

		return 0;
	}

protected:

	/** base mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, ReplicatedUsing = OnRep_Weapons, Category = Locker, meta = (BlueprintProtected, AllowPrivateAccess = "true", ExposeOnSpawn = true))
	TArray<FWeaponEntry> Weapons;
	/** local copy of weapon entries to determine whether to initialize the set of weapons by comparing the actual weapon array against this array */
	UPROPERTY()
	TArray<FWeaponEntry> WeaponsCopy;

	UPROPERTY(BlueprintReadOnly, Category = Locker, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FWeaponInfo> LockerWeapons;

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ReplacementWeapons)
	FReplacementWeaponEntry ReplacementWeapons[6];

	UPROPERTY()
	bool bReplacementWeaponsDirty;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<UMeshComponent*> EditorMeshes;
#endif

public:

	UFUNCTION(BlueprintPure, Category = Locker)
	TArray<FWeaponEntry> GetWeapons() const { return Weapons; }

	UStaticMeshComponent* GetBaseMesh() const { return BaseMesh; }

	void CreatePickupMeshForSlot(UMeshComponent*& PickupMesh, int32 SlotIndex, TSubclassOf<AUTInventory> PickupInventoryType);

	UFUNCTION(BlueprintNativeEvent, Category = Locker)
	void OnRep_Weapons();

	UFUNCTION()
	virtual void OnRep_ReplacementWeapons();

	/** offsets from locker location where we can place weapon meshes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FVector> LockerPositions;

	/** offsets from locker rotation how to rotate weapon meshes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FRotator> LockerRotations;

	/** how high the weapons floats (additional Z axis translation applied to pickup mesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	float LockerFloatHeight;

	// TODO: Move to UTWeapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	FVector WeaponLockerOffset;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	FRotator WeaponLockerRotation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	FVector WeaponLockerScale3D;

	/** component for the active/inactive effect, depending on state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Locker)
	UParticleSystemComponent* AmbientEffect;
	/** effect that's visible when active and the player gets nearby */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Locker)
	UParticleSystemComponent* ProximityEffect;
	/** effect for when the weapon locker cannot be used by the player right now */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	UParticleSystem* InactiveEffectTemplate;
	/** effect for when the weapon locker is usable by the player right now */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	UParticleSystem* ActiveEffectTemplate;
	/** effect played over weapons being scaled in when the player is nearby */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	UParticleSystem* WeaponSpawnEffectTemplate;

	/** respawn time for the Locker; if it's <= 0 then the pickup doesn't respawn until the round resets */
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_LockerRespawnTimeChanged, Category = Locker)
	float LockerRespawnTime;
public:

	/** returns respawn time for the Locker */
	virtual float GetLockerRespawnTime() const
	{
		return LockerRespawnTime;
	}

	/** event when the Locker respawn time value has been replicated 
	Note: Also called for local players in a Listen Server environment */
	UFUNCTION(BlueprintNativeEvent)
	void OnRep_LockerRespawnTimeChanged(float OldLockerRespawnTime);

	/** Locker message to display on player HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Locker)
	FText LockerString;

	/** returns name of item for HUD displays, etc */
	virtual FText GetDisplayName() const
	{
		return LockerString;
	}

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "PreBeginPlay"))
	virtual void ReceivePreBeginPlay();

	//Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void Reset() override;
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

	virtual void PostActorCreated() override;
	//End AActor Interface

	//Begin AUTPickup Interface
	virtual void Reset_Implementation() override;
	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult) override;
	virtual void ProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void StartSleeping_Implementation() override;

	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float PathDistance) override;
	//End AUTPickup Interface

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI, meta = (BlueprintProtected))
	float BotDesireabilityGlobal(APawn* Asker, float PathDistance);

	/** initialize properties/display for the weapons that are listed in the array */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Locker)
	void InitializeWeapons();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Locker)
	void DestroyWeapons();

	/** return whether Other can pick up this item (checks for stacking limits, etc)
	* the default implementation checks for GameMode/Mutator overrides and returns bDefaultAllowPickup if no overrides are found
	*/
	UFUNCTION(BlueprintCallable, Category = Pickup, meta = (DisplayName = "Allow Pickup By", bDefaultAllowPickup = "true", AdvancedDisplay = "bDefaultAllowPickup"))
	bool Blueprint_AllowPickupBy(APawn* Other, bool bDefaultAllowPickup)
	{
		return AllowPickupBy(Other, bDefaultAllowPickup);
	}

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void HandlePickUpWeapons(AActor* Other, bool bHideWeapons);

	UFUNCTION(BlueprintCallable, Category = Locker, meta = (BlueprintProtected))
	virtual void GiveLockerWeapons(AActor* Other, bool bHideWeapons);

	/** Announce pickup to recipient */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void AnnouncePickup(AUTCharacter* P, TSubclassOf<AUTInventory> NewInventoryType, AUTInventory* NewInventory = nullptr);

	UPROPERTY()
	float MaxDesireability;

	/** clientside flag - whether the locker should be displayed as active and having weapons available */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	bool bIsActive;

	/** clientside flag - whether or not a local player is near this locker */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	bool bPlayerNearby;

	/** whether weapons are currently scaling up */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	bool bScalingUp;

	/** current scaling up weapon scale */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	float CurrentWeaponScaleX;

	/** the rate to scale the weapons's Scale3D.X when spawning them in (set to 0.0 to disable the scaling) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locker|Display")
	float ScaleRate;

	/** how close a player needs to be to be considered nearby */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locker|Display")
	float ProximityDistanceSquared;

	/** Next proximity check time */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Display")
	float NextProximityCheckTime;

	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	bool bForceNearbyPlayers;

	/** list of characters that have picked up weapons from this locker recently */
	UPROPERTY(BlueprintReadOnly, Category = Locker, meta = (AllowPrivateAccess = "true"))
	TArray<FWeaponPickupCustomer> Customers;

	/** whether to clear customers on reset (when the level is reset or the Locker is reset manually).
	 * Clearing customers would allow the same players taking the weapon again on reset
	 * (otherwise only if players die, they are able to take the weapons or wait) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Locker)
	bool bClearCustomersOnReset;

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual bool AddCustomer(APawn* P);
	UFUNCTION(BlueprintPure, Category = Locker)
	virtual bool HasCustomer(APawn* P);

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void SetPlayerNearby(APlayerController* PC, bool bNewPlayerNearby, bool bPlayEffects, bool bForce = false);

	/** client sided event called when the Locker weapons are created or destroyed */
	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnPlayerNearByChanged(APlayerController* PC, bool bEffectsPlayed);

	/** set the respawn time for the Locker; if it's <= 0 then the pickup will go into sleep mode after a valid pickup.
	* @param	NewLockerRespawnTime - new respawn time
	* @param	bAutoSleep - whether to call StartSleep
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Locker, meta = (bAutoSleep = "true", AdvancedDisplay = "bAutoSleep,CustomersAction"))
	virtual void SetLockerRespawnTime(float NewLockerRespawnTime, bool bAutoSleep = true, TEnumAsByte<ELockerCustomerAction::Type> CustomersAction = ELockerCustomerAction::Update);

	/** event called when the Locker respawn time has changed programmatically, server-sided only */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly)
	virtual void OnLockerRespawnTimeSet(float OldTime);

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ShowActive();
	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ShowHidden();

	/** adds an entry to the Weapons array (generally used by mutators) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Locker)
	virtual void AddWeapon(TSubclassOf<AUTWeapon> NewWeaponClass);

	/** replaces an entry in the Weapons array (generally used by mutators) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Locker)
	virtual void ReplaceWeapon(int32 Index, TSubclassOf<AUTWeapon> NewWeaponClass);

	/** In sleeping state */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_IsSleeping, Category = Locker)
	bool bIsSleeping;

	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnRep_IsSleeping();

	/** In disabled state */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_IsDisabled, Category = Locker)
	bool bIsDisabled;

	UPROPERTY(BlueprintReadOnly, Category = Locker, meta = (BlueprintProtected))
	bool bIsEnabling;

	UFUNCTION(BlueprintNativeEvent)
	void OnRep_IsDisabled();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Locker)
	virtual void DisablePickup()
	{
		bIsDisabled = true;
		GotoState(DisabledState);
	}

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Locker, meta = (bReset = "true", AdvancedDisplay = "bReset"))
	virtual void EnablePickup(bool bReset = true)
	{
		bIsEnabling = true;
		bIsDisabled = false;
		SetInitialStateGlobal();

		if (bReset)
		{
			AUTWeaponLocker::Reset();
		}
		bIsEnabling = false;
	}

	/** checks for anyone touching the pickup and checks if they should get the item
	* this is necessary because this type of pickup doesn't toggle collision when weapon stay is on
	*/
	virtual void CheckTouching();
	/** Pickup was touched through a wall.  Check to see if touching pawn is no longer obstructed */
	virtual void RecheckValidTouch();

	virtual FTimerHandle* GetCheckTouchingHandle()
	{
		return &CheckTouchingHandle;
	}

	virtual void RegisterLocalPlayer(AController* C);
	virtual void NotifyLocalPlayerDead(APlayerController* PC);

	UFUNCTION()
	virtual void OnPawnDied(AController* Killer, const UDamageType* DamageType);

	UPROPERTY(BlueprintReadOnly, Category = "State")
	UUTWeaponLockerState* GlobalState;

	/** Pre-defined states */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FStateInfo> States;

	/** Returns all available states */
	UFUNCTION(BlueprintPure, Category = State)
	TArray<FStateInfo> GetStates()
	{
		return States;
	}

	/** Returns the current active state*/
	inline UUTWeaponLockerState* GetCurrentState()
	{
		return CurrentState;
	}

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetInitialState();

	/**
	* Transitions to the desired state.
	*
	* @param	NewState - new state to transition to
	*/
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void GotoState(class UUTWeaponLockerState* NewState);

	/**
	* Transitions to the desired state.
	*
	* @param	NewState - new state to transition to
	*/
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void GotoStateByName(FName NewStateName)
	{
		if (NewStateName.IsNone())
		{
			GotoState(GlobalState);
		}
		else if (FStateInfo* NewState = States.FindByPredicate([&](const FStateInfo& StateInfo){ return StateInfo.StateName == NewStateName; }))
		{
			GotoState(NewState->State);
		}
		else
		{
			UE_LOG(LogDebug, Warning, TEXT("Attempt to send %s to invalid state with name of %s (unable to find state)"), *GetName(), *NewStateName.ToString());
		}
	}

	/**
	* Checks the current state and determines whether or not this object
	* is actively in the specified state.  Note: This does work with
	* inherited states.
	*
	* @param	TestState - state to check for
	*
	* @return	True if currently in TestState
	*/
	UFUNCTION(BlueprintPure, Category = Pickup)
	virtual bool IsInState(class UUTWeaponLockerState* TestState)
	{
		return TestState == CurrentState;
	}

	/**
	* Checks the current state and determines whether or not this object
	* is actively in the specified state.  Note: This does work with
	* inherited states.
	*
	* @param	TestStateName - state to check for
	*
	* @return	True if currently in the state with the name of TestStateName
	*/
	UFUNCTION(BlueprintPure, Category = Pickup)
	virtual bool IsInStateByName(FName TestStateName)
	{
		if (TestStateName.IsNone())
		{
			return IsInState(GlobalState);
		}
		else if (FStateInfo* TestState = States.FindByPredicate([&](const FStateInfo& StateInfo){ return StateInfo.StateName == TestStateName; }))
		{
			return IsInState(TestState->State);
		}

		return false;
	}

	/**
	* Returns the current state name, useful for determining current
	* state similar to IsInState.  Note:  This *doesn't* work with
	* inherited states, in that it will only compare at the lowest
	* state level.
	*
	* @return	Name of the current state
	*/
	UFUNCTION(BlueprintPure, Category = Pickup)
	virtual FName GetStateName()
	{
		if (CurrentState)
		{
			if (FStateInfo* CurrentStateInfo = States.FindByPredicate([&](const FStateInfo& StateInfo){ return StateInfo.State == CurrentState; }))
			{
				return CurrentStateInfo->StateName;
			}
		}
		
		return NAME_None;
	}

	/** notification of state change (CurrentState is new state)
	* if a state change triggers another state change (i.e. within BeginState()/EndState())
	* this function will only be called once, when CurrentState is the final state
	*/
	UFUNCTION(BlueprintNativeEvent)
	void StateChanged();

protected:

	UPROPERTY(BlueprintReadOnly, Category = "State")
	UUTWeaponLockerState* CurrentState;

	UPROPERTY(BlueprintReadWrite, Category = "State", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	UUTWeaponLockerState* InitialState;

	UUTWeaponLockerState* AutoState;

	UPROPERTY(BlueprintReadWrite, Category = "State", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	UUTWeaponLockerState* PickupState;

	UFUNCTION(BlueprintPure, Category = "State")
	UUTWeaponLockerState* GetPickupState()
	{
		return PickupState;
	}

	UPROPERTY(BlueprintReadWrite, Category = "State", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	UUTWeaponLockerState* DisabledState;

	UFUNCTION(BlueprintPure, Category = "State")
	UUTWeaponLockerState* GetDisabledState()
	{
		return DisabledState;
	}

	UPROPERTY(BlueprintReadWrite, Category = "State", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	UUTWeaponLockerState* SleepingState;

	UFUNCTION(BlueprintPure, Category = "State")
	UUTWeaponLockerState* GetSleepingState()
	{
		return SleepingState;
	}
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	void SetInitialStateGlobal();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	void ShowHiddenGlobal();

	FTimerHandle HideWeaponsHandle;
	FTimerHandle DestroyWeaponsHandle;
	FTimerHandle RecheckValidTouchHandle;
	FTimerHandle CheckTouchingHandle;

#if WITH_EDITOR
public:
	virtual void CheckForErrors() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	virtual bool HasStateErrors(TArray<FString>& StateErrors);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	/** cleanup transient pickup meshes in editor previewing */
	virtual void CleanupEditorPickupMeshes();
	/** create transient pickup mesh for editor previewing */
	virtual void CreateEditorPickupMeshes();

#endif // WITH_EDITOR

	UFUNCTION(BlueprintImplementableEvent, Category = Locker, meta = (CallInEditor = "true"))
	virtual void OnEditorPickupMeshesCreated();

	UFUNCTION(BlueprintImplementableEvent, Category = Locker, meta = (CallInEditor = "true"))
	virtual void OnEditorPickupMeshesCleanUp();

};

UCLASS(Blueprintable, EditInlineNew, CustomConstructor, Within = UTWeaponLocker)
class UUTWeaponLockerState : public UScriptState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UFUNCTION(BlueprintPure, Category = Locker, meta = (DisplayName = "Get Outer UTWeaponLocker"))
	AUTWeaponLocker* Blueprint_GetOuterAUTWeaponLocker() const
	{
		return GetOuterAUTWeaponLocker();
	}

	virtual UWorld* GetWorld() const override
	{
		return GetOuterAUTWeaponLocker()->GetWorld();
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = State, meta = (BlueprintProtected))
	void SetInitialState();

	/**
	* Called immediately when entering a state, while within the
	* GotoState() call that caused the state change (before any
	* state code is executed).
	*/
	UFUNCTION(BlueprintNativeEvent)
	void BeginState(const UUTWeaponLockerState* PrevState);

	/**
	* Called immediately before going out of the current state, while
	* within the GotoState() call that caused the state change, and
	* before BeginState() is called within the new state.
	*/
	UFUNCTION(BlueprintNativeEvent)
	void EndState(const UUTWeaponLockerState* NextState);

	UFUNCTION(BlueprintNativeEvent)
	void Tick(float DeltaTime);

	UFUNCTION(BlueprintNativeEvent)
	bool OverrideProcessTouch(APawn* TouchedBy);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	void StartSleeping();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	void ShowActive();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	void ShowHidden();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup)
	void HandlePickUpWeapons(AActor* Other, bool bHideWeapons);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup)
	float BotDesireability(APawn* Asker, float TotalDistance);

	UFUNCTION(BlueprintNativeEvent)
	void NotifyLocalPlayerDead(APlayerController* PC);
};

inline void UUTWeaponLockerState::SetInitialState_Implementation()
{
	GetOuterAUTWeaponLocker()->SetInitialStateGlobal();
}
inline void UUTWeaponLockerState::BeginState_Implementation(const UUTWeaponLockerState* PrevState){ return; }
inline void UUTWeaponLockerState::EndState_Implementation(const UUTWeaponLockerState* NextState){ return; }
inline void UUTWeaponLockerState::Tick_Implementation(float DeltaTime){ return; }
inline bool UUTWeaponLockerState::OverrideProcessTouch_Implementation(APawn* TouchedBy)
{
	return false;
}
inline void UUTWeaponLockerState::StartSleeping_Implementation(){ return; }
inline void UUTWeaponLockerState::ShowActive_Implementation(){ return; }
inline void UUTWeaponLockerState::ShowHidden_Implementation(){ GetOuterAUTWeaponLocker()->ShowHiddenGlobal(); }
inline void UUTWeaponLockerState::HandlePickUpWeapons_Implementation(AActor* Other, bool bHideWeapons){ return; }
inline float UUTWeaponLockerState::BotDesireability_Implementation(APawn* Asker, float TotalDistance)
{
	return GetOuterAUTWeaponLocker()->BotDesireabilityGlobal(Asker, TotalDistance);
}
inline void UUTWeaponLockerState::NotifyLocalPlayerDead_Implementation(APlayerController* PC){ return; }

UCLASS(CustomConstructor)
class UUTWeaponLockerStateDisabled : public UUTWeaponLockerState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerStateDisabled(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		DefaultStateName = FName(TEXT("Disabled"));
	}

	virtual void SetInitialState_Implementation() override;
	virtual void BeginState_Implementation(const UUTWeaponLockerState* PrevState) override;
	virtual bool OverrideProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override;
};

UCLASS(CustomConstructor)
class UUTWeaponLockerStateSleeping : public UUTWeaponLockerState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerStateSleeping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		DefaultStateName = FName(TEXT("Sleeping"));
	}

	virtual void BeginState_Implementation(const UUTWeaponLockerState* PrevState) override;
	virtual void EndState_Implementation(const UUTWeaponLockerState* NextState) override;
	virtual void StartSleeping_Implementation() override;
	virtual bool OverrideProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override;
};

UCLASS(CustomConstructor)
class UUTWeaponLockerStatePickup : public UUTWeaponLockerState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerStatePickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		DefaultStateName = FName(TEXT("Pickup"));
	}

	virtual void BeginState_Implementation(const UUTWeaponLockerState* PrevState) override;
	virtual bool OverrideProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual void Tick_Implementation(float DeltaTime) override;

	virtual void HandlePickUpWeapons_Implementation(AActor* Other, bool bHideWeapons) override;
	virtual void ShowActive_Implementation() override;
	virtual void StartSleeping_Implementation() override;

	virtual void NotifyLocalPlayerDead_Implementation(APlayerController* PC) override;
};