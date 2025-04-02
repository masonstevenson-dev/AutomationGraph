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

#include "AutomationGraphExecutor.generated.h"

struct FGraphExecutionTask;
class UAutomationGraphNode;
class UAutomationGraph;

UCLASS()
class AUTOMATIONGRAPHRUNTIME_API UAutomationGraphExecutor : public UObject
{
	GENERATED_BODY()

public:
	virtual void StartExecution(FGraphExecutionTask ExecutionTask);

	// Returns true if the executor is still active. False if it unstarted, finished, failed, etc.
	virtual bool Execute(float DeltaSeconds);
	virtual void Cancel(UAutomationGraph* Graph);

protected:
	virtual void PreInitializeNodes(UWorld* World) {}
	virtual bool InitializeNode(UAutomationGraphNode* Node, UWorld* World);
	virtual void PostInitializeNodes() {}
	virtual void Reset();
	
	TWeakObjectPtr<UAutomationGraph> TargetGraph;
	TSet<TWeakObjectPtr<UAutomationGraphNode>> ActiveNodes;

	float TickRateSec = 0.0f;
	float ExecutionTimer = 0.0f;
};
