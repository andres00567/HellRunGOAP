#include "GOAPTypes.h"

namespace
{
    bool CompareNumbers(const double Left, const double Right,
        const EGOAPComparison Comparison)
    {
        switch (Comparison)
        {
        case EGOAPComparison::Equal: return FMath::IsNearlyEqual(Left, Right);
        case EGOAPComparison::NotEqual: return !FMath::IsNearlyEqual(Left, Right);
        case EGOAPComparison::Less: return Left < Right;
        case EGOAPComparison::LessOrEqual: return Left <= Right;
        case EGOAPComparison::Greater: return Left > Right;
        case EGOAPComparison::GreaterOrEqual: return Left >= Right;
        default: return false;
        }
    }
}

FGOAPValue FGOAPValue::MakeBool(const bool Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Bool; R.BoolValue=Value; return R; }
FGOAPValue FGOAPValue::MakeInteger(const int32 Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Integer; R.IntegerValue=Value; return R; }
FGOAPValue FGOAPValue::MakeFloat(const float Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Float; R.FloatValue=Value; return R; }
FGOAPValue FGOAPValue::MakeName(const FName Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Name; R.NameValue=Value; return R; }
FGOAPValue FGOAPValue::MakeVector(const FVector& Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Vector; R.VectorValue=Value; return R; }
FGOAPValue FGOAPValue::MakeObject(UObject* Value)
{ FGOAPValue R; R.Type=EGOAPValueType::Object; R.ObjectValue=Value; return R; }

bool FGOAPValue::IsSet() const
{
    if (Type == EGOAPValueType::None) return false;
    if (Type == EGOAPValueType::Object) return IsValid(ObjectValue);
    if (Type == EGOAPValueType::Name) return !NameValue.IsNone();
    return true;
}

bool FGOAPValue::Equals(const FGOAPValue& Other) const
{
    if (Type != Other.Type) return false;
    switch (Type)
    {
    case EGOAPValueType::None: return true;
    case EGOAPValueType::Bool: return BoolValue == Other.BoolValue;
    case EGOAPValueType::Integer: return IntegerValue == Other.IntegerValue;
    case EGOAPValueType::Float: return FMath::IsNearlyEqual(FloatValue, Other.FloatValue);
    case EGOAPValueType::Name: return NameValue == Other.NameValue;
    case EGOAPValueType::Vector: return VectorValue.Equals(Other.VectorValue, 0.1);
    case EGOAPValueType::Object: return ObjectValue == Other.ObjectValue;
    default: return false;
    }
}

double FGOAPValue::AsNumber(const double Fallback) const
{
    switch (Type)
    {
    case EGOAPValueType::Bool: return BoolValue ? 1.0 : 0.0;
    case EGOAPValueType::Integer: return IntegerValue;
    case EGOAPValueType::Float: return FloatValue;
    default: return Fallback;
    }
}

FString FGOAPValue::ToString() const
{
    switch (Type)
    {
    case EGOAPValueType::Bool: return BoolValue ? TEXT("true") : TEXT("false");
    case EGOAPValueType::Integer: return FString::FromInt(IntegerValue);
    case EGOAPValueType::Float: return FString::SanitizeFloat(FloatValue);
    case EGOAPValueType::Name: return NameValue.ToString();
    case EGOAPValueType::Vector: return VectorValue.ToCompactString();
    case EGOAPValueType::Object: return GetNameSafe(ObjectValue);
    default: return TEXT("unset");
    }
}

uint32 GetTypeHash(const FGOAPValue& Value)
{
    uint32 Hash = GetTypeHash(static_cast<uint8>(Value.Type));
    switch (Value.Type)
    {
    case EGOAPValueType::Bool: return HashCombine(Hash, GetTypeHash(Value.BoolValue));
    case EGOAPValueType::Integer: return HashCombine(Hash, GetTypeHash(Value.IntegerValue));
    case EGOAPValueType::Float: return HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Value.FloatValue * 1000.0f)));
    case EGOAPValueType::Name: return HashCombine(Hash, GetTypeHash(Value.NameValue));
    case EGOAPValueType::Vector:
        return HashCombine(Hash, GetTypeHash(FIntVector(
            FMath::RoundToInt(Value.VectorValue.X), FMath::RoundToInt(Value.VectorValue.Y),
            FMath::RoundToInt(Value.VectorValue.Z))));
    case EGOAPValueType::Object: return HashCombine(Hash, GetTypeHash(Value.ObjectValue));
    default: return Hash;
    }
}

void FGOAPCompiledDomain::Reset()
{
    Facts.Reset(); Actions.Reset(); Goals.Reset();
    FactIndices.Reset(); ActionIndices.Reset(); GoalIndices.Reset();
}

bool FGOAPPlanningState::operator==(const FGOAPPlanningState& Other) const
{
    if (Values.Num() != Other.Values.Num()) return false;
    for (int32 Index=0; Index<Values.Num(); ++Index)
        if (!Values[Index].Equals(Other.Values[Index])) return false;
    return true;
}

uint32 GetTypeHash(const FGOAPPlanningState& State)
{
    uint32 Hash = 0;
    for (const FGOAPValue& Value : State.Values)
        Hash = HashCombineFast(Hash, GetTypeHash(Value));
    return Hash;
}

bool GOAPEvaluateCondition(const FGOAPCondition& Condition,
    const FGOAPCompiledDomain& Domain, const FGOAPPlanningState& State)
{
    const int32* Index = Domain.FactIndices.Find(Condition.FactId);
    if (!Index || !State.Values.IsValidIndex(*Index)) return false;
    const FGOAPValue& Actual = State.Values[*Index];
    if (Condition.Comparison == EGOAPComparison::IsSet) return Actual.IsSet();
    if (Condition.Comparison == EGOAPComparison::IsNotSet) return !Actual.IsSet();
    if ((Actual.Type == EGOAPValueType::Bool
        || Actual.Type == EGOAPValueType::Integer
        || Actual.Type == EGOAPValueType::Float)
        && (Condition.Value.Type == EGOAPValueType::Bool
            || Condition.Value.Type == EGOAPValueType::Integer
            || Condition.Value.Type == EGOAPValueType::Float))
    {
        return CompareNumbers(Actual.AsNumber(), Condition.Value.AsNumber(),
            Condition.Comparison);
    }
    const bool bEqual = Actual.Equals(Condition.Value);
    return Condition.Comparison == EGOAPComparison::Equal ? bEqual
        : Condition.Comparison == EGOAPComparison::NotEqual && !bEqual;
}

void GOAPApplyEffect(const FGOAPEffect& Effect,
    const FGOAPCompiledDomain& Domain, FGOAPPlanningState& State)
{
    const int32* Index = Domain.FactIndices.Find(Effect.FactId);
    if (!Index || !State.Values.IsValidIndex(*Index)) return;
    FGOAPValue& Current = State.Values[*Index];
    if (Effect.Operation == EGOAPEffectOperation::Clear)
    {
        Current = {};
        return;
    }
    if (Effect.Operation == EGOAPEffectOperation::Set)
    {
        Current = Effect.Value;
        return;
    }
    if (Current.Type == EGOAPValueType::Integer
        && Effect.Value.Type == EGOAPValueType::Integer)
    {
        Current.IntegerValue += Effect.Operation == EGOAPEffectOperation::Add
            ? Effect.Value.IntegerValue : -Effect.Value.IntegerValue;
    }
    else if (Current.Type == EGOAPValueType::Float
        && (Effect.Value.Type == EGOAPValueType::Float
            || Effect.Value.Type == EGOAPValueType::Integer))
    {
        const float Delta = static_cast<float>(Effect.Value.AsNumber());
        Current.FloatValue += Effect.Operation == EGOAPEffectOperation::Add
            ? Delta : -Delta;
    }
}
