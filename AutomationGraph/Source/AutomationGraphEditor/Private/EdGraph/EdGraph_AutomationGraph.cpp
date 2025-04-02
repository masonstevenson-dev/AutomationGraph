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

#include "EdGraph/EdGraph_AutomationGraph.h"

#include "AutomationGraphEditorLoggingDefs.h"
#include "GraphEditorActions.h"
#include "AutomationNodes/ClearLandscapeLayers.h"
#include "EdGraph/EdNode_AutomationGraphEdge.h"
#include "EdGraph/EdNode_AutomationGraphNode.h"
#include "Foundation/AutomationGraphNode.h"
#include "Framework/Commands/GenericCommands.h"
#include "Macros/AutomationGraphLoggingMacros.h"
#include "Subsystems/AutomationGraphSubsystem.h"

#define LOCTEXT_NAMESPACE "EdGraph_AutomationGraph"

int32 UEdGraphSchema_AutomationGraph::CurrentCacheRefreshID = 0;

void FAssetSchemaAction_AutoGraph_NewNode::InitializeNode(UAutomationGraphNode* NewAutomationNode)
{
	if (auto* LayerClearNode = Cast<UAGN_ClearLandscapeLayers>(NewAutomationNode))
	{
		// Assume the user wants to write to an edit layer named "Procedural"
		LayerClearNode->EditLayers.Add("Procedural");
	}
}

EGraphType UEdGraphSchema_AutomationGraph::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return GT_StateMachine;
}

void UEdGraphSchema_AutomationGraph::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	auto* AutomationGraphSubsystem = GEditor->GetEditorSubsystem<UAutomationGraphSubsystem>();
	if (!AutomationGraphSubsystem)
	{
		AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("AutomationGraphSubsystem is invalid"));
		return;
	}
	
	auto* AutomationGraph = CastChecked<UAutomationGraph>(ContextMenuBuilder.CurrentGraph->GetOuter());
	TArray<FAutomationGraphNodeInfo> SupportedNodes = AutomationGraphSubsystem->GetSupportedNodes(AutomationGraph);

	if (SupportedNodes.Num() == 0)
	{
		AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("expected at least one context supported node type."));
	}

	for (const FAutomationGraphNodeInfo& NodeInfo : SupportedNodes)
	{
		// You can only drag from output pins, not from input.
		if (ContextMenuBuilder.FromPin && ContextMenuBuilder.FromPin->Direction != EGPD_Output)
		{
			continue;
		}
		if (NodeInfo.NodeType->HasAnyClassFlags(CLASS_Abstract))
		{
			AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("found abstract class in list of supported node types"));
			continue;
		}

		// This is the DisplayName as specified by the class metadata tag. 
		const FText DisplayName = FText::FromString(NodeInfo.NodeType->GetDescription());

		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Name"), DisplayName);
			const FText AddToolTip = FText::Format(LOCTEXT("NewAutomationGraphNodeTooltip", "Adds {Name} node here"), Arguments);
		
			TSharedPtr<FAssetSchemaAction_AutoGraph_NewNode> NewNodeAction(new FAssetSchemaAction_AutoGraph_NewNode(
				NodeInfo.NewNodeMenuCategory,
				DisplayName,
				AddToolTip, 0)
			);
			NewNodeAction->NodeClass = NodeInfo.NodeType;
			ContextMenuBuilder.AddAction(NewNodeAction);
		}
	}
}

void UEdGraphSchema_AutomationGraph::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	// TODO(): This does not work correctly, because the user is likely to click on a "pin" that not linked to anything.
	//         since this node is "pinless", we need to check all pins, and if any are connected, we need to build the pin action submenu.
	/*
	if (Context->Pin)
	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchema_AutomationGraphActions", LOCTEXT("PinActionsMenuHeader", "Automation Graph Pin Actions"));
		// Only display the 'Break Links' option if there is a link to break!
		if (Context->Pin->LinkedTo.Num() > 0)
		{
			Section.AddMenuEntry(FGraphEditorCommands::Get().BreakPinLinks);

			// add sub menu for break link to
			if (Context->Pin->LinkedTo.Num() > 1)
			{
				Section.AddSubMenu(
					"BreakLinkTo",
					LOCTEXT("BreakLinkTo", "Break Link To..."),
					LOCTEXT("BreakSpecificLinks", "Break a specific link..."),
					FNewToolMenuDelegate::CreateUObject((UEdGraphSchema_AutomationGraph* const)this, &UEdGraphSchema_AutomationGraph::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(Context->Pin)));
			}
			else
			{
				((UEdGraphSchema_AutomationGraph* const)this)->GetBreakLinkToSubMenuActions(Menu, const_cast<UEdGraphPin*>(Context->Pin));
			}
		}
	}*/
	
	if (Context->Pin || Context->Node)
	{
		FToolMenuSection& Section = Menu->AddSection("AG_ContextMenuActions", LOCTEXT("AG_ContextMenuActionHeader", "Node Actions"));
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Cut);
		Section.AddMenuEntry(FGenericCommands::Get().Copy);
		Section.AddMenuEntry(FGenericCommands::Get().Duplicate);

		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
	}
}

const FPinConnectionResponse UEdGraphSchema_AutomationGraph::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	auto* FromEdNode = Cast<UEdNode_AutomationGraphNode>(PinA->GetOwningNode());
	auto* ToEdNode = Cast<UEdNode_AutomationGraphNode>(PinB->GetOwningNode());
	if (FromEdNode == nullptr || ToEdNode == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinError_InvalidEdNode", "Not a valid UEdNode_AutomationGraphNode"));
	}
	if (FromEdNode == ToEdNode)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Can't connect a node to itself"));
	}

	TObjectPtr<UAutomationGraphNode> FromNode = FromEdNode->AutomationNode;
	TObjectPtr<UAutomationGraphNode> ToNode = ToEdNode->AutomationNode;
	if (FromEdNode == nullptr || ToEdNode == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinError_InvalidNode", "Not a valid AutomationGraphNode"));
	}

	if (FromNode->ChildNodes.Contains(ToNode) || ToNode->ChildNodes.Contains(FromNode))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinError_AlreadyConnected", "Can't connect nodes that are already connected"));
	}

	// Traverse FromNode and make sure that ToNode isn't one of its ancestors.
	TArray<TObjectPtr<UAutomationGraphNode>> NodeStack;
	TSet<TObjectPtr<UAutomationGraphNode>> Visited;
	
	NodeStack.Append(FromNode->ParentNodes);
	while (!NodeStack.IsEmpty())
	{
		TObjectPtr<UAutomationGraphNode> AncestorNode = NodeStack.Pop();

		if (!Visited.Contains(AncestorNode))
		{
			Visited.Add(AncestorNode);
			NodeStack.Append(AncestorNode->ParentNodes);
		}
	}

	if (Visited.Contains(ToNode))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinError_Cycle", "Can't create a graph cycle"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, LOCTEXT("PinConnect", "Connect nodes with edge"));
}

bool UEdGraphSchema_AutomationGraph::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	auto* FromEdNode = CastChecked<UEdNode_AutomationGraphNode>(PinA->GetOwningNode());
	auto* ToEdNode = CastChecked<UEdNode_AutomationGraphNode>(PinB->GetOwningNode());

	// We always connect output(A)-->input(B) regardless of which pin the user actually dragged off of.
	bool bModified = UEdGraphSchema::TryCreateConnection(FromEdNode->GetOutputPin(), ToEdNode->GetInputPin());

	if (bModified)
	{
		CastChecked<UEdGraph_AutomationGraph>(PinA->GetOwningNode()->GetGraph())->RebuildAutomationGraph();
	}

	return bModified;
}

bool UEdGraphSchema_AutomationGraph::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	auto* FromEdNode = Cast<UEdNode_AutomationGraphNode>(PinA->GetOwningNode());
	auto* ToEdNode = Cast<UEdNode_AutomationGraphNode>(PinB->GetOwningNode());

	// Are nodes and pins all valid?
	if (!FromEdNode || !FromEdNode->GetOutputPin() || !ToEdNode || !ToEdNode->GetInputPin())
	{
		return false;	
	}
	
	UEdGraph* Graph = FromEdNode->GetGraph();
	FVector2D InitPos((FromEdNode->NodePosX + ToEdNode->NodePosX) / 2, (FromEdNode->NodePosY + ToEdNode->NodePosY) / 2);

	FAssetSchemaAction_AutoGraph_NewEdge Action;
	auto* EdgeNode = Cast<UEdNode_AutomationGraphEdge>(Action.PerformAction(Graph, nullptr, InitPos, false));
	EdgeNode->CreateConnections(FromEdNode, ToEdNode);

	return true;
}

FLinearColor UEdGraphSchema_AutomationGraph::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FColor::White;
}

bool UEdGraphSchema_AutomationGraph::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return InVisualizationCacheID != CurrentCacheRefreshID;
}

int32 UEdGraphSchema_AutomationGraph::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UEdGraphSchema_AutomationGraph::ForceVisualizationCacheClear() const
{
	CurrentCacheRefreshID++;	
}

void UEdGraphSchema_AutomationGraph::GetBreakLinkToSubMenuActions(UToolMenu* SubMenu, UEdGraphPin* SelectedGraphPin)
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	FToolMenuSection& Section = SubMenu->FindOrAddSection("AutomationGraphSchemaPinActions");

	// Since this graph uses "pinless" nodes, we need to loop through all pins in order to get the full list of
	// connections that can be broken
	for (UEdGraphPin* GraphPin : SelectedGraphPin->GetOwningNode()->Pins)
	{
		// Add all the links we could break from
		for (TArray<class UEdGraphPin*>::TConstIterator Links(GraphPin->LinkedTo); Links; ++Links)
		{
			UEdGraphPin* Pin = *Links;
			FString TitleString = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
			FText Title = FText::FromString(TitleString);
			if (Pin->PinName != TEXT(""))
			{
				TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->PinName.ToString());

				// Add name of connection if possible
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeTitle"), Title);
				Args.Add(TEXT("PinName"), Pin->GetDisplayName());
				Title = FText::Format(LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args);
			}

			uint32& Count = LinkTitleCount.FindOrAdd(TitleString);

			FText Description;
			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), Title);
			Args.Add(TEXT("NumberOfNodes"), Count);

			if (Count == 0)
			{
				Description = FText::Format(LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args);
			}
			else
			{
				Description = FText::Format(LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args);
			}
			++Count;

			Section.AddMenuEntry(NAME_None, Description, Description, FSlateIcon(), FUIAction(
				FExecuteAction::CreateUObject(this, &UEdGraphSchema_AutomationGraph::BreakSinglePinLink, const_cast<UEdGraphPin*>(GraphPin), *Links)));
		}
	}
}

UAutomationGraph* UEdGraph_AutomationGraph::GetAutomationGraph()
{
	return CastChecked<UAutomationGraph>(GetOuter());
}

void UEdGraph_AutomationGraph::RebuildAutomationGraph()
{
	UAutomationGraph* AutomationGraph = GetAutomationGraph();
	
	AutomationGraph->RootNodes.Empty();
	
	for (TObjectPtr<UEdGraphNode> EdGraphNode : Nodes)
	{
		if (!EdGraphNode)
		{
			AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("found null EdGraphNode while rebuilding automation graph"));
			continue;
		}
		if (Cast<UEdNode_AutomationGraphEdge>(EdGraphNode))
		{
			continue;
		}
		auto* AutomationGraphNode = Cast<UEdNode_AutomationGraphNode>(EdGraphNode);
		if (!AutomationGraphNode)
		{
			AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("Unexpected EdGraphNode while rebuilding automation graph: %s"), *EdGraphNode->GetClass()->GetName());
			continue;
		}

		TObjectPtr<UAutomationGraphNode> AutomationNode = AutomationGraphNode->AutomationNode;
		if (!AutomationNode)
		{
			AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("Expected AutomationNode to be valid"));
			continue;
		}

		AutomationNode->ParentNodes.Empty();
		AutomationNode->ChildNodes.Empty();

		// We pre-load the nodes into a set so we can check for duplicates.
		TSet<TObjectPtr<UAutomationGraphNode>> ParentNodes;
		TSet<TObjectPtr<UAutomationGraphNode>> ChildNodes;
		
		for (UEdGraphPin* Pin : AutomationGraphNode->Pins)
		{
			if (!Pin)
			{
				AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("Expected Pin to be valid"));
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				auto* GraphEdge = CastChecked<UEdNode_AutomationGraphEdge>(LinkedPin->GetOwningNode());
				if (!GraphEdge)
				{
					AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("Expected Graph edge to be valid"));
					continue;
				}

				UEdNode_AutomationGraphNode* LinkedAutomationGraphNode = nullptr;
				if (Pin->Direction == EGPD_Input)
				{
					LinkedAutomationGraphNode = GraphEdge->GetStartNode();
				}
				else if (Pin->Direction == EGPD_Output)
				{
					LinkedAutomationGraphNode = GraphEdge->GetEndNode();
				}
				if (!LinkedAutomationGraphNode)
				{
					AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("Expected linked graph node to be valid"));
					continue;
				}
				if (LinkedAutomationGraphNode == AutomationGraphNode)
				{
					AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("Expected linked graph node to be a different node"));
					continue;
				}
				
				if (Pin->Direction == EGPD_Input)
				{
					if (ParentNodes.Contains(LinkedAutomationGraphNode->AutomationNode))
					{
						AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("Node has multiple connections to the same parent"));
						continue;
					}
					
					ParentNodes.Add(LinkedAutomationGraphNode->AutomationNode);
				}
				else if (Pin->Direction == EGPD_Output)
				{
					if (ChildNodes.Contains(LinkedAutomationGraphNode->AutomationNode))
					{
						AG_LOG_OBJECT(this, LogAutoGraphEditor, Warning, TEXT("Node has multiple connections to the same child"));
						continue;
					}
					
					ChildNodes.Add(LinkedAutomationGraphNode->AutomationNode);
				}
			}
		}

		for (TObjectPtr<UAutomationGraphNode> ParentNode : ParentNodes)
		{
			AutomationNode->ParentNodes.Add(ParentNode);
		}
		for (TObjectPtr<UAutomationGraphNode> ChildNode : ChildNodes)
		{
			AutomationNode->ChildNodes.Add(ChildNode);
		}
		
		if (AutomationNode->ParentNodes.IsEmpty())
		{
			AutomationGraph->RootNodes.Add(AutomationNode);
		}
	}
}

#undef LOCTEXT_NAMESPACE