#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "GOAPDomainFactory.generated.h"

UCLASS()
class UGOAPDomainFactory final : public UFactory
{
    GENERATED_BODY()
public:
    UGOAPDomainFactory();
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent,
        FName Name, EObjectFlags Flags, UObject* Context,
        FFeedbackContext* Warn) override;
};
