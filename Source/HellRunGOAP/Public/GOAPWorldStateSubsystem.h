#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GOAPTypes.h"
#include "GOAPWorldStateSubsystem.generated.h"

class UGOAPBrainComponent;

UCLASS()
class HELLRUNGOAP_API UGOAPWorldStateSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterBrain(UGOAPBrainComponent* Brain);
    void UnregisterBrain(UGOAPBrainComponent* Brain);
    void GetBrains(TArray<UGOAPBrainComponent*>& OutBrains) const;

    bool SetSharedFact(EGOAPFactScope Scope, FName SquadKey,
        const FGuid& FactId, const FGOAPValue& Value, FName Source,
        float Confidence, float Lifetime);
    bool ClearSharedFact(EGOAPFactScope Scope, FName SquadKey,
        const FGuid& FactId);
    bool GetSharedFact(EGOAPFactScope Scope, FName SquadKey,
        const FGuid& FactId, FGOAPWorldFactRecord& OutRecord) const;
    int32 GetRevision() const { return Revision; }

    /** Per-world visualization override. Zero leaves only the legacy CVar. */
    void SetDebugDrawLevel(int32 InLevel)
    { DebugDrawLevel = FMath::Clamp(InLevel, 0, 2); }
    int32 GetDebugDrawLevel() const { return DebugDrawLevel; }
    int32 GetDebugDrawLevel(FName DebugGroup) const
    {
        const bool* bEnabled = DebugGroupOverrides.Find(DebugGroup);
        return bEnabled && !*bEnabled ? 0 : DebugDrawLevel;
    }
    void SetDebugGroupEnabled(FName DebugGroup, bool bEnabled)
    { DebugGroupOverrides.Add(DebugGroup, bEnabled); }

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return false; }

    virtual void Deinitialize() override;

private:
    void NotifyChanged(EGOAPFactScope Scope,FName SquadKey,
        const FGuid& FactId,EGOAPRuntimeEventType EventType,
        const FGOAPWorldFactRecord* OldRecord,
        const FGOAPWorldFactRecord* NewRecord,const FString& Reason);
    TMap<FGuid, FGOAPWorldFactRecord> WorldFacts;
    TMap<FName, TMap<FGuid, FGOAPWorldFactRecord>> SquadFacts;
    TSet<TWeakObjectPtr<UGOAPBrainComponent>> Brains;
    int32 Revision = 0;
    int32 DebugDrawLevel = 0;
    TMap<FName, bool> DebugGroupOverrides;
};
