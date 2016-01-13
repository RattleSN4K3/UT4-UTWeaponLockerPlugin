#pragma once

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
	TSubclassOf<class UUTWeaponLockerState> StateClass;

	class UUTWeaponLockerState* State;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	FName StateName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	uint32 bAuto : 1;

	UPROPERTY()
	uint32 bUserChanged : 1;

	FORCEINLINE FStateInfo()
		: StateName(FName(TEXT("")))
		, bAuto(0)
	{}
	
	FORCEINLINE FStateInfo(TSubclassOf<UUTWeaponLockerState> InStateClass)
		: FStateInfo(InStateClass, false)
	{}

	FORCEINLINE FStateInfo(TSubclassOf<class UUTWeaponLockerState> InStateClass, FName InStateName)
		: FStateInfo(InStateClass, InStateName, false)
	{}

	FORCEINLINE FStateInfo(TSubclassOf<class UUTWeaponLockerState> InStateClass, bool InAuto)
		: StateClass(InStateClass)
		, bAuto(InAuto)
		, bUserChanged(0)
	{
		TSubclassOf<UScriptState> DefaultStateClass = InStateClass;
		if (DefaultStateClass && StateName.IsNone())
		{
			StateName = DefaultStateClass.GetDefaultObject()->DefaultStateName;
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

	UPROPERTY(BlueprintReadonly, Category = Pickup)
	UMeshComponent* PickupMesh;

	UPROPERTY(BlueprintReadonly, Category = Pickup)
	FVector DesiredScale3D;

	FWeaponEntry()
	{}
	FWeaponEntry(TSubclassOf<AUTWeapon> InWeaponClass)
		: WeaponClass(InWeaponClass)
	{}
};

/** when received on the client, replaces Weapons array classes with this array instead */
USTRUCT(BlueprintType)
struct FReplacementWeaponEntry
{
	GENERATED_USTRUCT_BODY()

	/** indicates whether this entry in the Weapons array was actually replaced
	* (so we can distinguish between WeaponClass == None because it wasn't touched vs. because it was removed
	*/
	uint32 bReplaced : 1;
	/** the class of weapon to replace with */
	TSubclassOf<AUTWeapon> WeaponClass;
};

UCLASS(Blueprintable, meta = (ChildCanTick))
class AUTWeaponLocker : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	friend class UUTWeaponLockerState;
	friend class UUTWeaponLockerStateDisabled;
	friend class UUTWeaponLockerStatePickup;

	// TODO: Move to UTWeapon as boolean flag
	TArray<TSubclassOf<AUTWeapon>> WarnIfInLocker;

	/** base mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base")
	UStaticMeshComponent* BaseMesh;

	UStaticMeshComponent* GetBaseMesh() const { return BaseMesh; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Locker)
	TArray<FWeaponEntry> Weapons;

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ReplacementWeapons)
	FReplacementWeaponEntry ReplacementWeapons[6];

	UFUNCTION()
	virtual void OnRep_ReplacementWeapons();

	/** offsets from locker location where we can place weapon meshes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FVector> LockerPositions;

	/** how high the weapons floats (additional Z axis translation applied to pickup mesh) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker|Display", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	float LockerFloatHeight;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = Locker)
	float LockerRespawnTime;

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
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;
	//End AActor Interface

	//Begin AUTPickup Interface
	virtual void Reset_Implementation() override;
	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
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

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void GiveLockerWeapons(AActor* Other, bool bHideWeapons);

	UFUNCTION(BlueprintCallable, Category = Locker, meta = (BlueprintProtected))
	virtual void GiveLockerWeaponsInternal(AActor* Other, bool bHideWeapons);

	/** Announce pickup to recipient */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void AnnouncePickup(AUTCharacter* P, TSubclassOf<AUTInventory> NewInventoryType, AUTInventory* NewInventory = nullptr);

	UPROPERTY()
	float MaxDesireability;

	/** clientside flag - whether the locker should be displayed as active and having weapons available */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	uint32 bIsActive : 1;

	/** clientside flag - whether or not a local player is near this locker */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	uint32 bPlayerNearby : 1;

	/** whether weapons are currently scaling up */
	UPROPERTY(BlueprintReadWrite, Category = "Locker|Client", meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	uint32 bScalingUp : 1;

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

	/** list of characters that have picked up weapons from this locker recently */
	UPROPERTY(BlueprintReadOnly, Category = Locker, meta = (AllowPrivateAccess = "true"))
	TArray<FWeaponPickupCustomer> Customers;

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual bool AddCustomer(APawn* P);
	UFUNCTION(BlueprintPure, Category = Locker)
	virtual bool HasCustomer(APawn* P);

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void SetPlayerNearby(APlayerController* PC, bool bNewPlayerNearby, bool bPlayEffects);

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ShowActive();
	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ShowHidden();

	/** replaces an entry in the Weapons array (generally used by mutators) */
	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ReplaceWeapon(int32 Index, TSubclassOf<AUTWeapon> NewWeaponClass);

	/** In sleeping state */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_IsSleeping, Category = Locker)
	uint32 bIsSleeping : 1;

	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnRep_IsSleeping();

	/** In disabled state */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_IsDisabled, Category = Locker)
	uint32 bIsDisabled : 1;

	UFUNCTION()
	virtual void OnRep_IsDisabled();

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void DisablePickup()
	{
		bIsDisabled = true;
		GotoState(DisabledState);
	}

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void EnablePickup()
	{
		bIsDisabled = false;
		SetInitialStateGlobal();
	}

	virtual void RegisterLocalPlayer(AController* C);
	virtual void NotifyLocalPlayerDead(APlayerController* PC);

	UFUNCTION()
	virtual void OnPawnDied(AController* Killer, const UDamageType* DamageType);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = State, meta = (BlueprintProtected, AllowPrivateAccess = "true"))
	TArray<FStateInfo> States;

	UFUNCTION(BlueprintPure, Category = State)
	TArray<FStateInfo> GetStates()
	{
		return States;
	}

	inline UUTWeaponLockerState* GetCurrentState()
	{
		return CurrentState;
	}

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetInitialState();

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void GotoState(class UUTWeaponLockerState* NewState);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
	UUTWeaponLockerState* InitialState;

	UUTWeaponLockerState* AutoState;

	UPROPERTY(Instanced, NoClear, EditDefaultsOnly, BlueprintReadOnly, Category = "State")
	UUTWeaponLockerState* GlobalState;

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

#if WITH_EDITOR
public:
	virtual void CheckForErrors() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	virtual bool HasStateErrors(TArray<FString>& StateErrors);
protected:
#endif // WITH_EDITOR

};

UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew, CustomConstructor, Within = UTWeaponLocker)
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

	UFUNCTION(BlueprintNativeEvent)
	void BeginState(const UUTWeaponLockerState* PrevState);

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
	void GiveLockerWeapons(AActor* Other, bool bHideWeapons);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup)
	float BotDesireability(APawn* Asker, float TotalDistance);

	UFUNCTION(BlueprintNativeEvent)
	void NotifyLocalPlayerDead(APlayerController* PC);

	UFUNCTION(BlueprintPure, Category = State)
	static class UUTWeaponLockerState* GetState(const FStateInfo& StateInfo)
	{
		return StateInfo.State;
	}
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
inline void UUTWeaponLockerState::GiveLockerWeapons_Implementation(AActor* Other, bool bHideWeapons){ return; }
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

	virtual void GiveLockerWeapons_Implementation(AActor* Other, bool bHideWeapons) override;
	virtual void ShowActive_Implementation() override;
	virtual void StartSleeping_Implementation() override;

	virtual void NotifyLocalPlayerDead_Implementation(APlayerController* PC) override;
};