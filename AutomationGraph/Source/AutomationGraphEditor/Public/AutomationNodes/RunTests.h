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

#pragma once
#include "AutomationGraphRuntimeConstants.h"
#include "IAutomationControllerManager.h"
#include "Foundation/AutomationGraphNode.h"

#include "RunTests.generated.h"

UCLASS(meta=( DisplayName="Run Tests" ))
class AUTOMATIONGRAPHEDITOR_API UAGN_RunTests : public UCoreAutomationGraphNode
{
	GENERATED_BODY()

	enum class ETestState : uint8
	{
		Idle,
		WaitForController,
		WaitForTestsReady,
		RunningTests,
		Complete
	};

public:
	UAGN_RunTests(const FObjectInitializer& Initializer);
	
	//~UAutomationGraphNode interface.
	virtual FText GetNodeCategory() override { return FAutomationGraphNodeCategory::TestAutomation; }
	virtual bool Initialize(UWorld* World) override;
	virtual void Cleanup() override;
	//~End UAutomationGraphNode interface.

	virtual void TestsReady();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Tests;

protected:
	//~UAutomationGraphNode interface.
	virtual EAutomationGraphNodeState ActivateInternal(float DeltaSeconds) override;
	//~End UAutomationGraphNode interface.

	void GenerateTestNames(TSharedPtr <AutomationFilterCollection> InFilters, TArray<FString>& OutFilteredTestNames);
	
	float FindWorkersTimeoutSec = 10.0f;
	int32 MaxFindWorkersAttempts = 6;

	ETestState TestState = ETestState::Idle;
	float FindWorkersTimerSec = 0.0f;
	int32 FindWorkersAttemtps = 0;

	FGuid SessionID;
	IAutomationControllerManagerPtr AutomationController;
};