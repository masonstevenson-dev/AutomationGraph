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

#include "Boilerplate/ConnectionDrawingPolicy_AutomationGraph.h"
#include "EdGraph/EdGraph_AutomationGraph.h"
#include "EdGraph/EdNode_AutomationGraphEdge.h"
#include "EdGraph/EdNode_AutomationGraphNode.h"

#define LOCTEXT_NAMESPACE "EdGraph_AutomationGraph"

UEdGraphNode* FAssetSchemaAction_AutoGraph_NewNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	const FScopedTransaction Transaction(LOCTEXT("FAssetSchemaAction_AutoGraph_NewNode", "Automation Graph: New Node"));
	ParentGraph->Modify();
	if (FromPin != nullptr)
	{
		FromPin->Modify();	
	}
	
	// First construct the underlying GraphNode
	auto* ParentEdGraph = CastChecked<UEdGraph_AutomationGraph>(ParentGraph);
	UAutomationGraph* ParentAG = ParentEdGraph->GetAutomationGraph();
	auto* NewAutomationNode = NewObject<UAutomationGraphNode>(ParentAG, NodeClass, NAME_None, RF_Transactional);

	InitializeNode(NewAutomationNode);

	// Then construct the editor node
	FGraphNodeCreator<UEdNode_AutomationGraphNode> NodeCreator(*ParentGraph);
	UEdNode_AutomationGraphNode* NewGraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode); // node must be UserInvoked in order to allow for renaming on create
	NewGraphNode->AutomationNode = NewAutomationNode;

	// This calls Node->CreateNewGuid(), Node->PostPlacedNewNode(), and Node->AllocateDefaultPins()
	NodeCreator.Finalize();
	NewGraphNode->AutowireNewNode(FromPin);
	
	NewGraphNode->NodePosX = Location.X;
	NewGraphNode->NodePosY = Location.Y;

	ParentEdGraph->RebuildAutomationGraph();
	ParentAG->PostEditChange();
	ParentAG->MarkPackageDirty();
	
	return NewGraphNode;
}

UEdGraphNode* FAssetSchemaAction_AutoGraph_NewEdge::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	const FScopedTransaction Transaction(LOCTEXT("AssetSchemaAction_AutoGraph_NewEdge", "Automation Graph: New Edge"));
	ParentGraph->Modify();
	if (FromPin != nullptr)
	{
		FromPin->Modify();	
	}

	auto* ParentEdGraph = CastChecked<UEdGraph_AutomationGraph>(ParentGraph);
	UAutomationGraph* ParentAG = ParentEdGraph->GetAutomationGraph();
	
	FGraphNodeCreator<UEdNode_AutomationGraphEdge> NodeCreator(*ParentGraph);
	UEdNode_AutomationGraphEdge* NewEdgeNode = NodeCreator.CreateNode();

	// This calls Node->CreateNewGuid(), Node->PostPlacedNewNode(), and Node->AllocateDefaultPins()
	NodeCreator.Finalize();
	NewEdgeNode->AutowireNewNode(FromPin);

	NewEdgeNode->NodePosX = Location.X;
	NewEdgeNode->NodePosY = Location.Y;

	ParentEdGraph->RebuildAutomationGraph();
	ParentAG->PostEditChange();
	ParentAG->MarkPackageDirty();

	return NewEdgeNode;
}

FConnectionDrawingPolicy* UEdGraphSchema_AutomationGraph::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	return new FConnectionDrawingPolicy_AutomationGraph(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

void UEdGraphSchema_AutomationGraph::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links"));

	Super::BreakNodeLinks(TargetNode);
	CastChecked<UEdGraph_AutomationGraph>(TargetNode.GetGraph())->RebuildAutomationGraph();
}

void UEdGraphSchema_AutomationGraph::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links"));

	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	if (bSendsNodeNotifcation)
	{
		CastChecked<UEdGraph_AutomationGraph>(TargetPin.GetOwningNode()->GetGraph())->RebuildAutomationGraph();
	}
}

void UEdGraphSchema_AutomationGraph::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link"));

	Super::BreakSinglePinLink(SourcePin, TargetPin);
	CastChecked<UEdGraph_AutomationGraph>(SourcePin->GetOwningNode()->GetGraph())->RebuildAutomationGraph();
}

UEdGraphPin* UEdGraphSchema_AutomationGraph::DropPinOnNode(UEdGraphNode* InTargetNode, const FName& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const
{
	auto* EdNode = Cast<UEdNode_AutomationGraphNode>(InTargetNode);
	switch (InSourcePinDirection)
	{
	case EGPD_Input:
		return EdNode->GetOutputPin();
	case EGPD_Output:
		return EdNode->GetInputPin();
	default:
		return nullptr;
	}
}

bool UEdGraphSchema_AutomationGraph::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	return Cast<UEdNode_AutomationGraphNode>(InTargetNode) != nullptr;
}

#undef LOCTEXT_NAMESPACE