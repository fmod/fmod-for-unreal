// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2017.

#pragma once

class FFMODStudioStyle : public FEditorStyle
{
public:
	static void Initialize();

	static void Shutdown();

private:
	static TSharedRef<class FSlateStyleSet> Create();

private:
	static TSharedPtr<class FSlateStyleSet> StyleInstance;

private:
	FFMODStudioStyle() {}
};
