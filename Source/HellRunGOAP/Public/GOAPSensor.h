#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GOAPSensor.generated.h"

class UGOAPBrainComponent;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class HELLRUNGOAP_API UGOAPSensor : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GOAP|Sensor",
        meta=(ClampMin="0.0", Units="s"))
    float UpdateInterval=0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GOAP|Sensor",
        meta=(ClampMin="0.0",ClampMax="1.0"))
    float PhaseSpread=1.0f;

    UFUNCTION(BlueprintNativeEvent, Category="GOAP|Sensor")
    void Sample(UGOAPBrainComponent* Brain);
    virtual void Sample_Implementation(UGOAPBrainComponent* Brain) {}

    double NextUpdateTime=0.0;
};
