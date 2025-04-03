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
#include "AutomationGraphTypes.h"

#include "AutomationGraphNode.generated.h"

UCLASS(Abstract)
class AUTOMATIONGRAPHRUNTIME_API UAutomationGraphNode : public UObject
{
	GENERATED_BODY()

public:
	virtual bool Initialize(UWorld* World);
	virtual void Uninitialize();

	// Called whenever a node is finished or has reached a bad state. Also called on Uninitialize. Implementers should
	// use this fn to release any resources this node may have claimed, but should not use this fn to change the node
	// state.
	virtual void Cleanup() {}

	virtual TArray<EAutomationGraphNodeTrigger> GetTriggers() { return TArray({EAutomationGraphNodeTrigger::OnPlay}); }
	
	bool CanStartActivation();
	bool CanActivate();
	EAutomationGraphNodeState Activate(float DeltaSeconds);
	void Cancel();

	EAutomationGraphNodeState GetState() { return NodeState; }
	virtual FLinearColor GetStateColor();

	bool GetElapsedTime(float& OutElapsedTime);

	virtual FText GetNodeCategory() { return FAutomationGraphNodeCategory::Default; }
	
	// Text to push out to the UI.
	virtual FString GetMessageText();

	UPROPERTY()
	TArray<TObjectPtr<UAutomationGraphNode>> ParentNodes;

	UPROPERTY()
	TArray<TObjectPtr<UAutomationGraphNode>> ChildNodes;

	// This is the text that will appear in the edit field when you first create a node. For the text that appears in
	// the node creation menu, use the DisplayName meta tag.
	//
	// IMPORTANT: This must be a UPROPERTY or else it will not be saved when you exit the editor.
	UPROPERTY()
	FText Title;

protected:
	virtual EAutomationGraphNodeState ActivateInternal(float DeltaSeconds);
	virtual void CancelInternal() {}
	EAutomationGraphNodeState SetState(EAutomationGraphNodeState NodeState);

	float NodeTimeoutSec = 300.0f; // 5m

private:
	EAutomationGraphNodeState NodeState = EAutomationGraphNodeState::Uninitialized;
	float TimeElapsedSec = 0.0f;
};

// Used to distinguish "official" nodes defined by this plugin.
UCLASS(Abstract)
class AUTOMATIONGRAPHRUNTIME_API UCoreAutomationGraphNode : public UAutomationGraphNode { GENERATED_BODY() };

// Used to distinguish "unofficial" nodes defined by users.
UCLASS(Abstract)
class AUTOMATIONGRAPHRUNTIME_API UAutomationGraphUserNode : public UAutomationGraphNode { GENERATED_BODY() };
