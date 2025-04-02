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

#include "EditorSubsystem.h"
#include "Foundation/AutomationGraphTypes.h"
#include "IAutomationControllerManager.h"
#include "Tickable.h"
#include "AutomationGraphSubsystem.generated.h"

class UAutomationGraphExecutor;
class UAutomationGraph;
class UAutomationGraphNode;

USTRUCT()
struct FAutomationGraphNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSubclassOf<UAutomationGraphNode> NodeType;

	UPROPERTY()
	FText NewNodeMenuCategory;
};

UCLASS()
class AUTOMATIONGRAPHEDITOR_API UAutomationGraphSubsystem : public UEditorSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem interface
	
	//~ Begin FTickableGameObject interface
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual bool IsTickableInEditor() const { return true; }
	TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAutomationGraphSubsystem, STATGROUP_Tickables); }

	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End FTickableGameObject interface

	//~ UObject interface
	virtual UWorld* GetWorld() const override;
	//~ End UObject interface

	void EnqueueAutomationGraph(UAutomationGraph* NewGraph, EAutomationGraphNodeTrigger EnqueueReason);
	void CancelGraphExecution(UAutomationGraph* Graph);
	void ClearTaskQueue() { TaskQueue.Empty(); }
	TArray<FAutomationGraphNodeInfo> GetSupportedNodes(UAutomationGraph* Graph);

	// Only one graph node should run tests at a time.
	IAutomationControllerManagerPtr LockAutomationController(UAutomationGraphNode* NewOwner);
	bool ReleaseAutomationController(UAutomationGraphNode* Owner);

protected:
	void StartExecution(FGraphExecutionTask& ExecutionTask);
	void EnqueueStartupGraphs();
	
	TArray<FGraphExecutionTask> TaskQueue;

	UPROPERTY()
	TMap<TSubclassOf<UAutomationGraphExecutor>, TObjectPtr<UAutomationGraphExecutor>> Executors;

	UPROPERTY()
	TObjectPtr<UAutomationGraphExecutor> CurrentExecutor;

	UPROPERTY()
	TArray<FAutomationGraphNodeInfo>  AllNodeInfo;

	IAutomationControllerManagerPtr AutomationController;
	TWeakObjectPtr<UAutomationGraphNode> AutomationControllerOwner;
};
