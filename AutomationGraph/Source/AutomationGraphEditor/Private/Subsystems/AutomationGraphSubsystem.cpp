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

#include "Subsystems/AutomationGraphSubsystem.h"

#include "AutomationGraphEditorLoggingDefs.h"
#include "IAutomationControllerModule.h"
#include "Editor/UnrealEdEngine.h"
#include "Foundation/AutomationGraph.h"
#include "Foundation/AutomationGraphNode.h"
#include "Foundation/AutomationGraphExecutor.h"
#include "UnrealEdGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AutomationNodes/RunTests.h"
#include "Macros/AutomationGraphLoggingMacros.h"
#include "Subsystems/UnrealEditorSubsystem.h"

UAGN_RunTests::UAGN_RunTests(const FObjectInitializer& Initializer): Super(Initializer)
{
	Title = FText::FromString("Run Tests");
}

void UAutomationGraphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		UClass* Class = *ClassIterator;
		if (!ClassIterator->IsChildOf(UAutomationGraphNode::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		auto* CDO = Class->GetDefaultObject<UAutomationGraphNode>();
		AllNodeInfo.Add(FAutomationGraphNodeInfo(Class, CDO->GetNodeCategory()));
	}

	auto* AutomationControllerModule = &FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
	AutomationController = AutomationControllerModule->GetAutomationController();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &ThisClass::EnqueueStartupGraphs);
}

void UAutomationGraphSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UAutomationGraphSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if !WITH_EDITOR
	return false;
#endif
	
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	
	return Super::ShouldCreateSubsystem(Outer);
}

ETickableTickType UAutomationGraphSubsystem::GetTickableTickType() const
{
	// Prevent the CDO from ticking
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UAutomationGraphSubsystem::IsAllowedToTick() const
{
#if WITH_EDITOR
	return !GUnrealEd->IsPlayingSessionInEditor();
#else
	return false;
#endif
}

void UAutomationGraphSubsystem::Tick(float DeltaSeconds)
{
	// check if we are mid-execution
	if (CurrentExecutor && CurrentExecutor->Execute(DeltaSeconds))
	{
		return;
	}
	CurrentExecutor = nullptr;

	while(TaskQueue.Num() > 0)
	{
		if (TaskQueue[0].TargetGraph.IsValid())
		{
			StartExecution(TaskQueue[0]);
		}
		
		TaskQueue.RemoveAt(0);
	}
}

UWorld* UAutomationGraphSubsystem::GetWorld() const
{
	if (UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
	{
		if (!GUnrealEd->IsPlayingSessionInEditor())
		{
			return UnrealEditorSubsystem->GetEditorWorld();
		}
	}

	return nullptr;
}

void UAutomationGraphSubsystem::EnqueueAutomationGraph(UAutomationGraph* NewGraph, EAutomationGraphNodeTrigger EnqueueReason)
{
	if (!NewGraph)
	{
		return;
	}
	
	for (FGraphExecutionTask GraphTask : TaskQueue)
	{
		if (GraphTask.TargetGraph.Get() == NewGraph)
		{
			return;
		}
	}
	TaskQueue.Add(FGraphExecutionTask(NewGraph, nullptr, EnqueueReason));	
}

void UAutomationGraphSubsystem::CancelGraphExecution(UAutomationGraph* Graph)
{
	if (!CurrentExecutor)
	{
		return;
	}

	CurrentExecutor->Cancel(Graph);
}

TArray<FAutomationGraphNodeInfo> UAutomationGraphSubsystem::GetSupportedNodes(UAutomationGraph* Graph)
{
	TArray<FAutomationGraphNodeInfo> FilteredNodeInfo;
	if (!Graph)
	{
		return FilteredNodeInfo;
	}

	for (const FAutomationGraphNodeInfo& NodeInfo : AllNodeInfo)
	{
		if (Graph->IsNodeSupported(NodeInfo.NodeType))
		{
			FilteredNodeInfo.Add(NodeInfo);
		}
	}

	return FilteredNodeInfo;
}

IAutomationControllerManagerPtr UAutomationGraphSubsystem::LockAutomationController(UAutomationGraphNode* NewOwner)
{
	if (!NewOwner || !AutomationController.IsValid())
	{
		return nullptr;
	}
	
	UAutomationGraphNode* CurrentOwner = AutomationControllerOwner.Get();
	if (CurrentOwner && NewOwner != CurrentOwner)
	{
		return nullptr;
	}

	AutomationControllerOwner = NewOwner;
	return AutomationController;
}

bool UAutomationGraphSubsystem::ReleaseAutomationController(UAutomationGraphNode* Owner)
{
	UAutomationGraphNode* CurrentOwner = AutomationControllerOwner.Get();
	if (CurrentOwner && Owner != CurrentOwner)
	{
		return false;
	}

	AutomationControllerOwner = nullptr;
	return true;
}

void UAutomationGraphSubsystem::StartExecution(FGraphExecutionTask& ExecutionTask)
{
	UAutomationGraph* Graph = ExecutionTask.TargetGraph.Get();
	
	TSubclassOf<UAutomationGraphExecutor> ExecutorType = Graph->GetExecutorType();
	TObjectPtr<UAutomationGraphExecutor>* FindResult = Executors.Find(ExecutorType);
	
	if (FindResult)
	{
		CurrentExecutor = *FindResult;
	}
	else
	{
		CurrentExecutor = NewObject<UAutomationGraphExecutor>(this, ExecutorType);
		Executors.Add(ExecutorType, CurrentExecutor);
	}

	ExecutionTask.TargetWorld = GetWorld();
	CurrentExecutor->StartExecution(ExecutionTask);
}

void UAutomationGraphSubsystem::EnqueueStartupGraphs()
{
	TArray<FAssetData> AssetData;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByClass(UAutomationGraph::StaticClass()->GetClassPathName(), AssetData);

	if (AssetData.Num() > 0)
	{
		UE_LOG(LogAutomationGraphSubsystem, Log, TEXT("Found %d graph assets. Checking for startup triggers"), AssetData.Num());
	}

	TArray<UAutomationGraph*> GraphsToEnqueue;
	
	for (const FAssetData& Asset : AssetData)
	{
		auto* GraphAsset = Cast<UAutomationGraph>(Asset.GetAsset());
		
		if (!GraphAsset)
		{
			UE_LOG(LogAutomationGraphSubsystem, Error, TEXT("Invalid graph asset"));
		}

		bool bContinueSearch = true;
		for (int NodeIndex = 0; NodeIndex < GraphAsset->RootNodes.Num() && bContinueSearch; ++NodeIndex)
		{
			if (GraphAsset->RootNodes[NodeIndex]->GetTriggers().Contains(EAutomationGraphNodeTrigger::OnStartup))
			{
				GraphsToEnqueue.Add(GraphAsset);
				bContinueSearch = false;
			}
		}
	}

	if (GraphsToEnqueue.Num() == 1)
	{
		UE_LOG(LogAutomationGraphSubsystem, Log, TEXT("Enqueuing 1 startup graph."));
	}
	else
	{
		UE_LOG(LogAutomationGraphSubsystem, Log, TEXT("Enqueuing %d startup graphs."), GraphsToEnqueue.Num());
	}
	
	for (UAutomationGraph* Graph : GraphsToEnqueue)
	{
		EnqueueAutomationGraph(Graph, EAutomationGraphNodeTrigger::OnStartup);
	}
}
