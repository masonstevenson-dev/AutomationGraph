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

#include "Foundation/AutomationGraphExecutor.h"

#include "AutomationGraphRuntimeLoggingDefs.h"
#include "Foundation/AutomationGraph.h"
#include "Macros/AutomationGraphLoggingMacros.h"

void UAutomationGraphExecutor::StartExecution(FGraphExecutionTask ExecutionTask)
{
	if (!ExecutionTask.TargetGraph.IsValid())
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Tried to execute an invalid graph. Skipping execution."));
		return;
	}
	if (!ExecutionTask.TargetWorld.IsValid())
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Tried to execute a graph on an invalid world. Skipping execution."));
		return;
	}
	if (ExecutionTask.Trigger == EAutomationGraphNodeTrigger::Unknown)
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("AutomationGraph execution trigger is unknown. Skipping execution."));
		return;
	}

	Reset();
	TargetGraph = ExecutionTask.TargetGraph;
	PreInitializeNodes(ExecutionTask.TargetWorld.Get());

	struct CycleCheckNode
	{
		UAutomationGraphNode* Node;
		TSet<UAutomationGraphNode*> Ancestors;
	};

	TArray<CycleCheckNode> NodeStack;
	for (UAutomationGraphNode* Node : TargetGraph->RootNodes)
	{
		if (Node->GetTriggers().Contains(ExecutionTask.Trigger))
		{
			ActiveNodes.Add(Node);
			NodeStack.Add(CycleCheckNode(Node, TSet<UAutomationGraphNode*>()));
		}
	}
	
	TSet<UAutomationGraphNode*> Visited;
	while (!NodeStack.IsEmpty())
	{
		CycleCheckNode DFSNode = NodeStack.Pop();
		UAutomationGraphNode* GraphNode = DFSNode.Node;
		TSet<UAutomationGraphNode*> Ancestors = DFSNode.Ancestors; // Make a copy.

		if (Visited.Contains(GraphNode))
		{
			continue;
		}

		InitializeNode(GraphNode, ExecutionTask.TargetWorld.Get());
		Visited.Add(GraphNode);
		Ancestors.Add(GraphNode);
		
		for (UAutomationGraphNode* ChildNode : GraphNode->ChildNodes)
		{
			if (Ancestors.Contains(ChildNode))
			{
				AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Failed to start graph execution: A cycle exists in the build graph."));
				PostInitializeNodes();
				Reset();
				return;
			}
			NodeStack.Add(CycleCheckNode(ChildNode, Ancestors));
		}
	}

	PostInitializeNodes();
}

bool UAutomationGraphExecutor::Execute(float DeltaSeconds)
{
	if (ActiveNodes.IsEmpty())
	{
		return false;
	}

	ExecutionTimer -= DeltaSeconds;
	if (ExecutionTimer > 0.0f)
	{
		return true;
	}
	ExecutionTimer = TickRateSec;
	
	TSet<TWeakObjectPtr<UAutomationGraphNode>> ToAdd;
	TSet<TWeakObjectPtr<UAutomationGraphNode>> ToRemove;
	
	for(TWeakObjectPtr<UAutomationGraphNode> WeakNode : ActiveNodes)
	{
		UAutomationGraphNode* CurrentNode = WeakNode.Get();
		if (!CurrentNode)
		{
			AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Found an invalid AutomationGraphNode. Resetting executor."));
			Reset();
			return false;
		}

		EAutomationGraphNodeState NodeState = CurrentNode->GetState();
		
		switch (NodeState)
		{
		case EAutomationGraphNodeState::Standby:
		case EAutomationGraphNodeState::Active:
			if (CurrentNode->CanActivate())
			{
				CurrentNode->Activate(DeltaSeconds);
			}
			
			continue;
		case EAutomationGraphNodeState::Finished:
			for (UAutomationGraphNode* ChildNode : CurrentNode->ChildNodes)
			{
				if (ChildNode->CanStartActivation())
				{
					ChildNode->Activate(DeltaSeconds);
					ToAdd.Add(ChildNode);
				}
			}
			
			ToRemove.Add(CurrentNode);
			continue;
		case EAutomationGraphNodeState::Expired:
		case EAutomationGraphNodeState::Cancelled:
		case EAutomationGraphNodeState::Error:
			ToRemove.Add(CurrentNode);
			continue;
		default:
			AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("unexpected build state: %s."), *UEnum::GetValueAsString(NodeState));
			ToRemove.Add(CurrentNode);
			continue;
		}
	}

	ActiveNodes.Append(ToAdd);
	for(TWeakObjectPtr<UAutomationGraphNode> RemoveNode : ToRemove)
	{
		if (RemoveNode.IsValid())
		{
			RemoveNode->Cleanup();
		}
		ActiveNodes.Remove(RemoveNode);
	}
	
	bool bExecutionFinished = ActiveNodes.IsEmpty();
	return !bExecutionFinished;
}

void UAutomationGraphExecutor::Cancel(UAutomationGraph* Graph)
{
	UAutomationGraph* CurrentGraph = TargetGraph.Get();
	if (CurrentGraph && Graph == CurrentGraph)
	{
		CurrentGraph->CancelNodes();
		ActiveNodes.Empty();
	}
}

bool UAutomationGraphExecutor::InitializeNode(UAutomationGraphNode* Node, UWorld* World)
{
	return Node->Initialize(World);
}

void UAutomationGraphExecutor::Reset()
{
	UAutomationGraph* CurrentGraph = TargetGraph.Get();
	if (CurrentGraph)
	{
		CurrentGraph->CancelNodes();
	}
	TargetGraph = nullptr;
	ActiveNodes.Empty();
	ExecutionTimer = 0.0f;
}
