// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2018.

#include "FMODParameterSection.h"
#include "Sequencer/FMODEventParameterSection.h"
#include "FloatCurveKeyArea.h"

#define LOCTEXT_NAMESPACE "FMODParameterSection"

DECLARE_DELEGATE(FLayoutParameterDelegate);

struct FIndexAndLayoutParameterDelegate
{
    int32 Index;
    FLayoutParameterDelegate LayoutParameterDelegate;

    bool operator<(FIndexAndLayoutParameterDelegate const& Other) const
    {
        return Index < Other.Index;
    }
};

void LayoutParameter(ISectionLayoutBuilder* LayoutBuilder, FFMODEventParameterNameAndCurve* ParameterNameAndCurve, UFMODEventParameterSection* ParameterSection)
{
    LayoutBuilder->AddKeyArea(
        ParameterNameAndCurve->ParameterName,
        FText::FromName(ParameterNameAndCurve->ParameterName),
        MakeShareable(new FFloatCurveKeyArea(&ParameterNameAndCurve->Curve, ParameterSection)));
}

void FFMODParameterSection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
    UFMODEventParameterSection* ParameterSection = Cast<UFMODEventParameterSection>(&SectionObject);
    TArray<FIndexAndLayoutParameterDelegate> IndexAndLayoutDelegates;

    // Collect parameters
    TArray<FFMODEventParameterNameAndCurve>& ParameterNamesAndCurves = ParameterSection->GetParameterNamesAndCurves();
    for (int32 i = 0; i < ParameterNamesAndCurves.Num(); i++)
    {
        FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
        IndexAndLayoutDelegate.Index = ParameterNamesAndCurves[i].Index;
        IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic(&LayoutParameter, &LayoutBuilder, &ParameterNamesAndCurves[i], ParameterSection);
        IndexAndLayoutDelegates.Add(IndexAndLayoutDelegate);
    }

    // Sort and perform layout
    IndexAndLayoutDelegates.Sort();
    for (FIndexAndLayoutParameterDelegate& IndexAndLayoutDelegate : IndexAndLayoutDelegates)
    {
        IndexAndLayoutDelegate.LayoutParameterDelegate.Execute();
    }
}

bool FFMODParameterSection::RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath)
{
    const FScopedTransaction Transaction(LOCTEXT("DeleteEventParameter", "Delete event parameter"));
    for (auto Name : KeyAreaNamePath)
    {
        UFMODEventParameterSection* ParameterSection = Cast<UFMODEventParameterSection>(&SectionObject);
        if (!ParameterSection->TryModify() || !ParameterSection->RemoveParameter(KeyAreaNamePath[0]))
        {
            return false;
        }
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
