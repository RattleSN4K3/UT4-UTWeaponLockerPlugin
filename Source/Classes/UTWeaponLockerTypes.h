#pragma once

#include "UTWeaponLockerTypes.generated.h"

UENUM(BlueprintType)
namespace ELockerCustomerAction
{
	enum Type
	{
		None UMETA(DisplayName = "Keep"),
		Update UMETA(DisplayName = "Update times"),
		Clear UMETA(DisplayName = "Clear customers"),
		// should always be last, used to size arrays
		MAX UMETA(Hidden)
	};
}

UENUM(BlueprintType)
namespace ELockerPickupStatus
{
	enum Type
	{
		Available UMETA(DisplayName = "Available"),
		Taken UMETA(DisplayName = "Taken"),
		// should always be last, used to size arrays
		MAX UMETA(Hidden)
	};
}
