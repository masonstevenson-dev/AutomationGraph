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

#include "Foundation/AutomationGraph.h"

#include "AutomationNodes/ClearLandscapeLayers.h"
#include "Foundation/AutomationGraphExecutor.h"

#define LOCTEXT_NAMESPACE "AutomationGraph"

TSubclassOf<UAutomationGraphExecutor> UAutomationGraph::GetExecutorType()
{
	return UAutomationGraphExecutor::StaticClass();	
}

bool UAutomationGraph::IsNodeSupported(TSubclassOf<UAutomationGraphNode> NodeType)
{
	// By default, only "official" nodes are supported. Users can create their own custom graph extension that supports
	// a wider set of node types.
	return NodeType->IsChildOf(UCoreAutomationGraphNode::StaticClass());
}

void UAutomationGraph::UninitializeNodes()
{
	TArray<TObjectPtr<UAutomationGraphNode>> NodeStack;
	TSet<TObjectPtr<UAutomationGraphNode>> Visited;

	NodeStack.Append(RootNodes);

	while (!NodeStack.IsEmpty())
	{
		TObjectPtr<UAutomationGraphNode> AutomationNode = NodeStack.Pop();

		if (Visited.Contains(AutomationNode))
		{
			continue;
		}

		Visited.Add(AutomationNode);
		AutomationNode->Uninitialize();
		NodeStack.Append(AutomationNode->ChildNodes);
	}
}

void UAutomationGraph::CancelNodes()
{
	TArray<TObjectPtr<UAutomationGraphNode>> NodeStack;
	TSet<TObjectPtr<UAutomationGraphNode>> Visited;

	NodeStack.Append(RootNodes);

	while (!NodeStack.IsEmpty())
	{
		TObjectPtr<UAutomationGraphNode> AutomationNode = NodeStack.Pop();

		if (Visited.Contains(AutomationNode))
		{
			continue;
		}

		Visited.Add(AutomationNode);
		AutomationNode->Cancel();
		NodeStack.Append(AutomationNode->ChildNodes);
	}
}

#undef LOCTEXT_NAMESPACE