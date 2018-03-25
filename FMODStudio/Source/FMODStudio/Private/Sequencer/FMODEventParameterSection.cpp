// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODEventParameterSection.h"

FFMODEventParameterNameAndCurve::FFMODEventParameterNameAndCurve(FName InParameterName)
{
    ParameterName = InParameterName;
}

UFMODEventParameterSection::UFMODEventParameterSection(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UFMODEventParameterSection::AddParameterKey(FName InParameterName, float InTime, float InValue)
{
    FRichCurve* Curve = nullptr;
    for (FFMODEventParameterNameAndCurve& ParameterNameAndCurve : EventParameterNamesAndCurves)
    {
        if (ParameterNameAndCurve.ParameterName == InParameterName)
        {
            Curve = &ParameterNameAndCurve.Curve;
            break;
        }
    }
    if (Curve == nullptr)
    {
        int32 NewIndex = EventParameterNamesAndCurves.Add(FFMODEventParameterNameAndCurve(InParameterName));
        EventParameterNamesAndCurves[NewIndex].Index = EventParameterNamesAndCurves.Num() - 1;
        Curve = &EventParameterNamesAndCurves[NewIndex].Curve;
    }
    Curve->AddKey(InTime, InValue);
}

bool UFMODEventParameterSection::RemoveParameter(FName InParameterName)
{
    for (int32 i = 0; i < EventParameterNamesAndCurves.Num(); i++)
    {
        if (EventParameterNamesAndCurves[i].ParameterName == InParameterName)
        {
            EventParameterNamesAndCurves.RemoveAt(i);
            UpdateParameterIndicesFromRemoval(i);
            return true;
        }
    }
    return false;
}

const TArray<FFMODEventParameterNameAndCurve>& UFMODEventParameterSection::GetParameterNamesAndCurves() const
{
    return EventParameterNamesAndCurves;
}

TArray<FFMODEventParameterNameAndCurve>& UFMODEventParameterSection::GetParameterNamesAndCurves()
{
    return EventParameterNamesAndCurves;
}

void UFMODEventParameterSection::GetParameterNames(TSet<FName>& ParameterNames) const
{
    for (const FFMODEventParameterNameAndCurve& ParameterNameAndCurve : EventParameterNamesAndCurves)
    {
        ParameterNames.Add(ParameterNameAndCurve.ParameterName);
    }
}

void UFMODEventParameterSection::Eval(float Position, TArray<FFMODEventParameterNameAndValue>& OutValues) const
{
    for (const FFMODEventParameterNameAndCurve& ParameterNameAndCurve : EventParameterNamesAndCurves)
    {
        OutValues.Add(FFMODEventParameterNameAndValue(ParameterNameAndCurve.ParameterName, ParameterNameAndCurve.Curve.Eval(Position)));
    }
}

void UFMODEventParameterSection::UpdateParameterIndicesFromRemoval(int32 RemovedIndex)
{
    for (FFMODEventParameterNameAndCurve& ParameterAndCurve : EventParameterNamesAndCurves)
    {
        if (ParameterAndCurve.Index > RemovedIndex)
        {
            ParameterAndCurve.Index--;
        }
    }
}

void UFMODEventParameterSection::GatherCurves(TArray<const FRichCurve*>& OutCurves) const
{
    for (const FFMODEventParameterNameAndCurve& ParameterNameAndCurve : EventParameterNamesAndCurves)
    {
        OutCurves.Add(&ParameterNameAndCurve.Curve);
    }

}

void UFMODEventParameterSection::GatherCurves(TArray<FRichCurve*>& OutCurves)
{
    for (FFMODEventParameterNameAndCurve& ParameterNameAndCurve : EventParameterNamesAndCurves)
    {
        OutCurves.Add(&ParameterNameAndCurve.Curve);
    }
}

void UFMODEventParameterSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
    Super::DilateSection(DilationFactor, Origin, KeyHandles);

    TArray<FRichCurve*> AllCurves;
    GatherCurves(AllCurves);

    for (auto Curve : AllCurves)
    {
        Curve->ScaleCurve(Origin, DilationFactor, KeyHandles);
    }
}

void UFMODEventParameterSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
    if (!TimeRange.Overlaps(GetRange()))
    {
        return;
    }

    TArray<const FRichCurve*> AllCurves;
    GatherCurves(AllCurves);

    for (auto Curve : AllCurves)
    {
        for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
        {
            float Time = Curve->GetKeyTime(It.Key());
            if (TimeRange.Contains(Time))
            {
                OutKeyHandles.Add(It.Key());
            }
        }
    }
}


void UFMODEventParameterSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
    Super::MoveSection(DeltaPosition, KeyHandles);

    TArray<FRichCurve*> AllCurves;
    GatherCurves(AllCurves);

    for (auto Curve : AllCurves)
    {
        Curve->ShiftCurve(DeltaPosition, KeyHandles);
    }
}


TOptional<float> UFMODEventParameterSection::GetKeyTime(FKeyHandle KeyHandle) const
{
    TArray<const FRichCurve*> AllCurves;
    GatherCurves(AllCurves);

    for (auto Curve : AllCurves)
    {
        if (Curve->IsKeyHandleValid(KeyHandle))
        {
            return TOptional<float>(Curve->GetKeyTime(KeyHandle));
        }
    }
    return TOptional<float>();
}


void UFMODEventParameterSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
    TArray<FRichCurve*> AllCurves;
    GatherCurves(AllCurves);

    for (auto Curve : AllCurves)
    {
        if (Curve->IsKeyHandleValid(KeyHandle))
        {
            Curve->SetKeyTime(KeyHandle, Time);
            break;
        }
    }
}
