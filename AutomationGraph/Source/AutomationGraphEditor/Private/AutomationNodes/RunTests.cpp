// Copyright © Mason Stevenson
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted (subject to the limitations in the disclaimer
// below) provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
// THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
// NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "AutomationNodes/RunTests.h"

#include "AutomationControllerSettings.h"
#include "AutomationGraphEditorLoggingDefs.h"
#include "AutomationGroupFilter.h"
#include "Macros/AutomationGraphLoggingMacros.h"
#include "Subsystems/AutomationGraphSubsystem.h"

bool UAGN_RunTests::Initialize(UWorld* World)
{
	if (!Super::Initialize(World))
	{
		return false;
	}
	
	TestState = ETestState::Idle;
	FindWorkersTimerSec = 0.0f;
	FindWorkersAttemtps = 0;
	AutomationController = nullptr;

	SessionID = FApp::GetSessionId();
	
	return true;
}

void UAGN_RunTests::Cleanup()
{
	if (AutomationController.IsValid())
	{
		if (auto* AGSubsystem = GEditor->GetEditorSubsystem<UAutomationGraphSubsystem>())
		{
			AutomationController->OnTestsRefreshed().RemoveAll(this);
			AGSubsystem->ReleaseAutomationController(this);
			AutomationController = nullptr;
		}
	}
}

void UAGN_RunTests::TestsReady()
{
	if (AutomationController->GetNumDeviceClusters() == 0 || TestState != ETestState::WaitForTestsReady)
	{
		return;
	}

	TArray<FString> FilteredTestNames;
	TSharedPtr <AutomationFilterCollection> AutomationFilters = MakeShareable(new AutomationFilterCollection());
	AutomationController->SetFilter(AutomationFilters);
	AutomationController->SetVisibleTestsEnabled(true);
	AutomationController->GetEnabledTestNames(FilteredTestNames);

	GenerateTestNames(AutomationFilters, FilteredTestNames);

	if (FilteredTestNames.Num())
	{
		AutomationController->StopTests();
		AutomationController->SetEnabledTests(FilteredTestNames);
		AutomationController->RunTests();
		
		TestState = ETestState::RunningTests;
	}
	else
	{
		TestState = ETestState::Complete;
	}
}

EAutomationGraphNodeState UAGN_RunTests::ActivateInternal(float DeltaSeconds)
{
	// Standard activation, ensures the node is active past this block.
	{
		EAutomationGraphNodeState CurrentState = GetState();
		if (CurrentState == EAutomationGraphNodeState::Standby)
		{
			return SetState(EAutomationGraphNodeState::Active);
		}
		if (CurrentState != EAutomationGraphNodeState::Active)
		{
			return CurrentState;
		}
	}

	EAutomationGraphNodeState UpdatedNodeState = EAutomationGraphNodeState::Active;

	if (TestState == ETestState::Idle)
	{
		if (Tests.IsEmpty())
		{
			TestState = ETestState::Complete;
			UpdatedNodeState = EAutomationGraphNodeState::Finished;
		}
		else
		{
			TestState = ETestState::WaitForController;	
		}
	}
	else if (TestState == ETestState::WaitForController)
	{
		if (auto* AGSubsystem = GEditor->GetEditorSubsystem<UAutomationGraphSubsystem>())
		{
			AutomationController = AGSubsystem->LockAutomationController(this);
		}

		if (AutomationController.IsValid())
		{
			TestState = ETestState::WaitForTestsReady;
		}
	}
	else if (TestState == ETestState::WaitForTestsReady)
	{
		// We have to check this every time because if the user has the test automation window open and then they
		// close it, this causes all delegates to be removed from the AutomationController.
		if (!AutomationController->OnTestsRefreshed().IsBoundToObject(this))
		{
			AutomationController->OnTestsRefreshed().AddUObject(this, &ThisClass::TestsReady);
		}

		FindWorkersTimerSec += DeltaSeconds;
		
		if (FindWorkersAttemtps == 0 || FindWorkersTimerSec >= FindWorkersTimeoutSec)
		{
			FindWorkersAttemtps++;
			if (FindWorkersAttemtps > MaxFindWorkersAttempts)
			{
				return SetState(EAutomationGraphNodeState::Expired);
			}
			
			AutomationController->RequestAvailableWorkers(SessionID);
			FindWorkersTimerSec = 0;
		}
	}
	else if (TestState == ETestState::RunningTests)
	{
		if (AutomationController->GetTestState() != EAutomationControllerModuleState::Running)
		{
			TestState = ETestState::Complete;
			UpdatedNodeState = EAutomationGraphNodeState::Finished;
		}
	}
	else if (TestState == ETestState::Complete)
	{
		UpdatedNodeState = EAutomationGraphNodeState::Finished;
	}
	else
	{
		UpdatedNodeState = EAutomationGraphNodeState::Error;
	}

	if (UpdatedNodeState != GetState())
	{
		SetState(UpdatedNodeState);
	}
	
	return UpdatedNodeState;
}

void UAGN_RunTests::GenerateTestNames(TSharedPtr<AutomationFilterCollection> InFilters, TArray<FString>& OutFilteredTestNames)
{
	// This fn is basically 1:1 with FAutomationExecCmd::GenerateTestNamesFromCommandLine(). See AutomationCommandline.cpp
	// for more details
	
	OutFilteredTestNames.Empty();

	// get our settings CDO where things are stored
	UAutomationControllerSettings* Settings = UAutomationControllerSettings::StaticClass()->GetDefaultObject<UAutomationControllerSettings>();

	// iterate through the arguments to build a filter list by doing the following -
	// 1) If argument is a filter (StartsWith:system) then make sure we only filter-in tests that start with that filter
	// 2) If argument is a group then expand that group into multiple filters based on ini entries
	// 3) Otherwise just substring match (default behavior in 4.22 and earlier).
	FAutomationGroupFilter* FilterAny = new FAutomationGroupFilter();
	TArray<FAutomatedTestFilter> FiltersList;
	for (int32 ArgumentIndex = 0; ArgumentIndex < Tests.Num(); ++ArgumentIndex)
	{
		const FString GroupPrefix = TEXT("Group:");
		const FString FilterPrefix = TEXT("StartsWith:");

		FString ArgumentName = Tests[ArgumentIndex].TrimStartAndEnd();

		// if the argument is a filter (e.g. Filter:System) then create a filter that matches from the start
		if (ArgumentName.StartsWith(FilterPrefix))
		{
			FString FilterName = ArgumentName.RightChop(FilterPrefix.Len()).TrimStart();

			if (FilterName.EndsWith(TEXT(".")) == false)
			{
				FilterName += TEXT(".");
			}

			FiltersList.Add(FAutomatedTestFilter(FilterName, true, false));
		}
		else if (ArgumentName.StartsWith(GroupPrefix))
		{
			// if the argument is a group (e.g. Group:Rendering) then seach our groups for one that matches
			FString GroupName = ArgumentName.RightChop(GroupPrefix.Len()).TrimStart();

			bool FoundGroup = false;

			for (int32 i = 0; i < Settings->Groups.Num(); ++i)
			{
				FAutomatedTestGroup* GroupEntry = &(Settings->Groups[i]);
				if (GroupEntry && GroupEntry->Name == GroupName)
				{
					FoundGroup = true;
					// if found add all this groups filters to our current list
					if (GroupEntry->Filters.Num() > 0)
					{
						FiltersList.Append(GroupEntry->Filters);
					}
					else
					{
						AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("Group %s contains no filters"), *GroupName);
					}
				}
			}

			if (!FoundGroup)
			{
				AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("No matching group named %s"), *GroupName);
			}
		}			
		else
		{
			bool bMatchFromStart = false;
			bool bMatchFromEnd = false;

			if (ArgumentName.StartsWith("^"))
			{
				bMatchFromStart = true;
				ArgumentName.RightChopInline(1);
			}
			if (ArgumentName.EndsWith("$"))
			{
				bMatchFromEnd = true;
				ArgumentName.LeftChopInline(1);
			}

			FiltersList.Add(FAutomatedTestFilter(ArgumentName, bMatchFromStart, bMatchFromEnd));
		}
	}

	if (!FiltersList.IsEmpty())
	{
		FilterAny->SetFilters(FiltersList);
		InFilters->Add(MakeShareable(FilterAny));

		// SetFilter applies all filters from the AutomationFilters array
		AutomationController->SetFilter(InFilters);
		// Fill OutFilteredTestNames array with filtered test names
		AutomationController->GetFilteredTestNames(OutFilteredTestNames);
	}
}
