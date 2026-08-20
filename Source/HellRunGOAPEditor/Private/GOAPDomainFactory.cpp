#include "GOAPDomainFactory.h"
#include "GOAPDomain.h"

UGOAPDomainFactory::UGOAPDomainFactory()
{
    SupportedClass=UGOAPDomain::StaticClass();
    bCreateNew=true;
    bEditAfterNew=true;
}

UObject* UGOAPDomainFactory::FactoryCreateNew(UClass* Class,
    UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
    FFeedbackContext* Warn)
{
    return NewObject<UGOAPDomain>(InParent,Class,Name,Flags|RF_Transactional);
}
