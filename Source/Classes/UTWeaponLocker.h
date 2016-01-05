#pragma once

#include "UTWeaponLocker.generated.h"

USTRUCT(BlueprintType)
struct FWeaponEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	TSubclassOf<AUTWeapon> WeaponClass;

	UPROPERTY(BlueprintReadonly, Category = Pickup)
	UMeshComponent* PickupMesh;

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

	/** Locker message to display on player HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Locker)
	FText LockerString;

	/** returns name of item for HUD displays, etc */
	virtual FText GetDisplayName() const
	{
		return LockerString;
	}

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Reset_Implementation() override;
	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void ProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void StartSleeping_Implementation() override;

	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float PathDistance) override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI, meta = (BlueprintProtected))
	float BotDesireabilityGlobal(APawn* Asker, float PathDistance);

	/** initialize properties/display for the weapons that are listed in the array */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Locker)
	void InitializeWeapons();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Locker)
	void DestroyWeapons();

	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void GiveLockerWeapons(AActor* Other, bool bHideWeapons);

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

	/** how close a player needs to be to be considered nearby */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locker|Display")
	float ProximityDistanceSquared;

	/** Next proximity check time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locker|Display")
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
	virtual void ShowActive(); // TODO: Add BlueprintNativeEvent
	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ShowHidden(); // TODO: Add BlueprintNativeEvent

	/** replaces an entry in the Weapons array (generally used by mutators) */
	UFUNCTION(BlueprintCallable, Category = Locker)
	virtual void ReplaceWeapon(int32 Index, TSubclassOf<AUTWeapon> NewWeaponClass);

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

	UFUNCTION(BlueprintPure, Category = Pickup, meta = (DisplayName = "GetCurrentState"))
	UUTWeaponLockerState* Blueprint_GetCurrentState()
	{
		return GetCurrentState();
	}

	inline UUTWeaponLockerState* GetCurrentState()
	{
		return CurrentState;
	}

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetInitialState(); // TODO: Add BlueprintNativeEvent

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void GotoState(class UUTWeaponLockerState* NewState);
	/** notification of state change (CurrentState is new state)
	* if a state change triggers another state change (i.e. within BeginState()/EndState())
	* this function will only be called once, when CurrentState is the final state
	*/
	virtual void StateChanged() // TODO: Add BlueprintNativeEvent
	{}

protected:

	UUTWeaponLockerState* CurrentState;

	UUTWeaponLockerState* InitialState;
	UUTWeaponLockerState* AutoState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponLockerState* GlobalState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponLockerState* DisabledState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponLockerState* PickupState;

	UFUNCTION(BlueprintCallable, Category = Pickup, meta = (BlueprintProtected))
	virtual void SetInitialStateGlobal(); // TODO: Add BlueprintNativeEvent

	FTimerHandle HideWeaponsHandle;
	FTimerHandle DestroyWeaponsHandle;

#if WITH_EDITOR
public:
	virtual void CheckForErrors() override;
protected:
#endif // WITH_EDITOR

};

UCLASS(DefaultToInstanced, EditInlineNew, CustomConstructor, Within = UTWeaponLocker)
class UUTWeaponLockerState : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual UWorld* GetWorld() const override
	{
		return GetOuterAUTWeaponLocker()->GetWorld();
	}

	virtual void SetInitialState()
	{}
	virtual void BeginState(const UUTWeaponLockerState* PrevState)
	{}
	virtual void EndState()
	{}
	virtual void Tick(float DeltaTime)
	{}

	virtual bool OverrideProcessTouch(APawn* TouchedBy)
	{
		return false;
	}

	virtual void ShowActive()
	{}

	virtual void GiveLockerWeapons(AActor* Other, bool bHideWeapons)
	{}

	virtual float BotDesireability(APawn* Asker, float TotalDistance)
	{
		return GetOuterAUTWeaponLocker()->BotDesireabilityGlobal(Asker, TotalDistance);
	}

	virtual void NotifyLocalPlayerDead(APlayerController* PC)
	{}
};

UCLASS(CustomConstructor)
class UUTWeaponLockerStateDisabled : public UUTWeaponLockerState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerStateDisabled(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BeginState(const UUTWeaponLockerState* PrevState) override;
	virtual bool OverrideProcessTouch(APawn* TouchedBy) override;
	virtual float BotDesireability(APawn* Asker, float TotalDistance) override;
};

UCLASS(CustomConstructor)
class UUTWeaponLockerStatePickup : public UUTWeaponLockerState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponLockerStatePickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BeginState(const UUTWeaponLockerState* PrevState) override;
	virtual bool OverrideProcessTouch(APawn* TouchedBy) override;
	virtual void Tick(float DeltaTime) override;

	virtual void GiveLockerWeapons(AActor* Other, bool bHideWeapons) override;
	virtual void ShowActive() override;

	virtual void NotifyLocalPlayerDead(APlayerController* PC) override;
};