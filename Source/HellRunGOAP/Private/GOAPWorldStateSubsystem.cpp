#include "GOAPWorldStateSubsystem.h"
#include "GOAPBrainComponent.h"

void UGOAPWorldStateSubsystem::RegisterBrain(UGOAPBrainComponent* Brain)
{ if (IsValid(Brain)) Brains.Add(Brain); }

void UGOAPWorldStateSubsystem::UnregisterBrain(UGOAPBrainComponent* Brain)
{ Brains.Remove(Brain); }

void UGOAPWorldStateSubsystem::GetBrains(TArray<UGOAPBrainComponent*>& OutBrains) const
{
    OutBrains.Reset();
    for (const TWeakObjectPtr<UGOAPBrainComponent>& Brain : Brains)
        if (Brain.IsValid()) OutBrains.Add(Brain.Get());
    OutBrains.Sort([](const UGOAPBrainComponent& A, const UGOAPBrainComponent& B)
    { return GetNameSafe(A.GetOwner()) < GetNameSafe(B.GetOwner()); });
}

bool UGOAPWorldStateSubsystem::SetSharedFact(const EGOAPFactScope Scope,
    const FName SquadKey, const FGuid& FactId, const FGOAPValue& Value,
    const FName Source, const float Confidence, const float Lifetime)
{
    if (Scope == EGOAPFactScope::Agent || !FactId.IsValid()) return false;
    TMap<FGuid,FGOAPWorldFactRecord>* Store = Scope == EGOAPFactScope::World
        ? &WorldFacts : &SquadFacts.FindOrAdd(SquadKey);
    FGOAPWorldFactRecord& Record=Store->FindOrAdd(FactId);
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    const double NewExpiry=Lifetime>0.0f?Now+Lifetime:-1.0;
    const bool bChanged=!Record.Value.Equals(Value)
        ||!FMath::IsNearlyEqual(Record.Confidence,Confidence)
        ||(Lifetime<=0.0f&&Record.ExpiresAt>=0.0);
    Record.Value=Value; Record.Source=Source;
    Record.Confidence=FMath::Clamp(Confidence,0.0f,1.0f); Record.UpdatedAt=Now;
    Record.ExpiresAt=NewExpiry;
    if(bChanged)
    {
        ++Revision;
        NotifyChanged(Scope,SquadKey,FactId,TEXT("Shared fact value changed"));
    }
    return true;
}

bool UGOAPWorldStateSubsystem::ClearSharedFact(const EGOAPFactScope Scope,
    const FName SquadKey, const FGuid& FactId)
{
    if (Scope==EGOAPFactScope::Agent) return false;
    bool bRemoved=false;
    if(Scope==EGOAPFactScope::World) bRemoved=WorldFacts.Remove(FactId)>0;
    else if(TMap<FGuid,FGOAPWorldFactRecord>* Store=SquadFacts.Find(SquadKey))
        bRemoved=Store->Remove(FactId)>0;
    if(bRemoved)
    {
        ++Revision;
        NotifyChanged(Scope,SquadKey,FactId,TEXT("Shared fact cleared"));
    }
    return bRemoved;
}

void UGOAPWorldStateSubsystem::NotifyChanged(const EGOAPFactScope Scope,
    const FName SquadKey,const FGuid& FactId,const FString& Reason)
{
    for(auto It=Brains.CreateIterator();It;++It)
    {
        if(!It->IsValid()){It.RemoveCurrent();continue;}
        It->Get()->NotifySharedFactChanged(Scope,SquadKey,FactId,Reason);
    }
}

void UGOAPWorldStateSubsystem::Tick(float DeltaTime)
{
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    TArray<FGuid,TInlineAllocator<8>> ExpiredWorld;
    for(const TPair<FGuid,FGOAPWorldFactRecord>& Pair:WorldFacts)
        if(Pair.Value.IsExpired(Now)) ExpiredWorld.Add(Pair.Key);
    for(const FGuid& Id:ExpiredWorld)
    {
        WorldFacts.Remove(Id); ++Revision;
        NotifyChanged(EGOAPFactScope::World,NAME_None,Id,TEXT("World fact expired"));
    }
    for(TPair<FName,TMap<FGuid,FGOAPWorldFactRecord>>& Squad:SquadFacts)
    {
        TArray<FGuid,TInlineAllocator<8>> ExpiredSquad;
        for(const TPair<FGuid,FGOAPWorldFactRecord>& Pair:Squad.Value)
            if(Pair.Value.IsExpired(Now)) ExpiredSquad.Add(Pair.Key);
        for(const FGuid& Id:ExpiredSquad)
        {
            Squad.Value.Remove(Id); ++Revision;
            NotifyChanged(EGOAPFactScope::Squad,Squad.Key,Id,TEXT("Squad fact expired"));
        }
    }
}

TStatId UGOAPWorldStateSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGOAPWorldStateSubsystem,STATGROUP_Tickables);
}

bool UGOAPWorldStateSubsystem::GetSharedFact(const EGOAPFactScope Scope,
    const FName SquadKey, const FGuid& FactId,
    FGOAPWorldFactRecord& OutRecord) const
{
    const FGOAPWorldFactRecord* Record=nullptr;
    if(Scope==EGOAPFactScope::World) Record=WorldFacts.Find(FactId);
    else if(Scope==EGOAPFactScope::Squad)
        if(const TMap<FGuid,FGOAPWorldFactRecord>* Store=SquadFacts.Find(SquadKey))
            Record=Store->Find(FactId);
    if(!Record) return false;
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0;
    if(Record->IsExpired(Now)) return false;
    OutRecord=*Record; return true;
}

void UGOAPWorldStateSubsystem::Deinitialize()
{
    WorldFacts.Reset(); SquadFacts.Reset(); Brains.Reset();
    Super::Deinitialize();
}
