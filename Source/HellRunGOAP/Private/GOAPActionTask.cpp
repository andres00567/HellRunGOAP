#include "GOAPActionTask.h"
#include "GOAPBrainComponent.h"

void UGOAPActionTask::Initialize(UGOAPBrainComponent* InBrain,
    const FGuid& InActionId)
{ Brain=InBrain; ActionId=InActionId; Status=EGOAPTaskStatus::Inactive; CompletionReason.Reset(); }

EGOAPTaskStatus UGOAPActionTask::Activate()
{
    if (Status != EGOAPTaskStatus::Inactive) return Status;
    Status = StartTask();
    if (Status == EGOAPTaskStatus::Inactive)
    {
        Status = EGOAPTaskStatus::Failed;
        CompletionReason = TEXT("Action task returned Inactive from StartTask");
    }
    return Status;
}

void UGOAPActionTask::Abort(const FString& Reason)
{
    if (Status == EGOAPTaskStatus::Succeeded
        || Status == EGOAPTaskStatus::Failed
        || Status == EGOAPTaskStatus::Aborted) return;
    AbortTask(Reason);
    Status = EGOAPTaskStatus::Aborted;
    CompletionReason = Reason;
}

EGOAPTaskStatus UGOAPActionTask::StartTask_Implementation()
{ return EGOAPTaskStatus::Succeeded; }

void UGOAPActionTask::Succeed(const FString& Reason)
{ Status=EGOAPTaskStatus::Succeeded; CompletionReason=Reason; }

void UGOAPActionTask::Fail(const FString& Reason)
{ Status=EGOAPTaskStatus::Failed; CompletionReason=Reason; }

AActor* UGOAPActionTask::GetAgent() const
{ return Brain.IsValid()?Brain->GetOwner():nullptr; }
