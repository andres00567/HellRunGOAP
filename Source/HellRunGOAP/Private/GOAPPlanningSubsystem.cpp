#include "GOAPPlanningSubsystem.h"
#include "GOAPBrainComponent.h"
#include "HAL/PlatformTime.h"

bool UGOAPPlanningSubsystem::Enqueue(UGOAPBrainComponent* Brain)
{
    if(!IsValid(Brain)||Queued.Contains(Brain)) return false;
    Queued.Add(Brain);
    Queue.Add(Brain);
    return true;
}

void UGOAPPlanningSubsystem::Tick(float DeltaTime)
{
    const double Started=FPlatformTime::Seconds();
    int32 Processed=0;
    while(!Queue.IsEmpty()&&Processed<MaximumPlansPerFrame)
    {
        const TWeakObjectPtr<UGOAPBrainComponent> Entry=Queue[0];
        Queue.RemoveAt(0,1,EAllowShrinking::No);
        Queued.Remove(Entry);
        if(Entry.IsValid()) Entry->ExecuteQueuedReplan();
        ++Processed;
        if((FPlatformTime::Seconds()-Started)*1000.0>=PlanningTimeBudgetMs) break;
    }
}

TStatId UGOAPPlanningSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGOAPPlanningSubsystem,STATGROUP_Tickables);
}

void UGOAPPlanningSubsystem::Deinitialize()
{
    Queue.Reset(); Queued.Reset();
    Super::Deinitialize();
}
