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

#include "Editor/AutomationGraphEditor.h"

#include "AutomationGraphEditorConstants.h"
#include "AutomationGraphEditorLoggingDefs.h"
#include "EdGraphUtilities.h"
#include "GraphEditorActions.h"
#include "EdGraph/EdGraph_AutomationGraph.h"
#include "EdGraph/EdNode_AutomationGraphEdge.h"
#include "EdGraph/EdNode_AutomationGraphNode.h"
#include "Foundation/AutomationGraph.h"
#include "Framework/Commands/GenericCommands.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Macros/AutomationGraphLoggingMacros.h"
#include "Subsystems/AutomationGraphSubsystem.h"

#define LOCTEXT_NAMESPACE "AutomationGraphEditor"

FAutomationGraphEditor::FAutomationGraphEditor()
{
	TargetGraph = nullptr;
}

FAutomationGraphEditor::~FAutomationGraphEditor()
{
	
}

void FAutomationGraphEditor::Initialize(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UAutomationGraph* NewGraph
)
{
	TargetGraph = NewGraph;

	FAutomationGraphEditorCommands::Register();

	// Create the corresponding UEdGraph
	if (TargetGraph->EditorGraph == nullptr && !TargetGraph->HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		TargetGraph->EditorGraph = CastChecked<UEdGraph_AutomationGraph>(
		FBlueprintEditorUtils::CreateNewGraph(
				TargetGraph,
				NAME_None,
				UEdGraph_AutomationGraph::StaticClass(),
				UEdGraphSchema_AutomationGraph::StaticClass()
			)
		);
		TargetGraph->EditorGraph->bAllowDeletion = false;

		// Give the schema a chance to fill out any required nodes.
		const UEdGraphSchema* Schema = TargetGraph->EditorGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*TargetGraph->EditorGraph);
	}

	FGenericCommands::Register();
	FGraphEditorCommands::Register();

	BuildCustomCommands();
	CreateInternalWidgets();

	// IMPORTANT: Unreal silently caches this layout to an ini file. This means that if you make ANY change to this
	//            layout, you must increment the LayoutName suffix (Layout_v[NUM++]).
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_AutomationGraphEditor_Layout_v11")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split(FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.75f)
				->SetHideTabWell(true)
				->AddTab(AutomationGraphEditorConstants::GraphCanvasTabId, ETabState::OpenedTab)
			)
			->Split(
			FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->SetHideTabWell(true)
				->AddTab(AutomationGraphEditorConstants::PropertiesTabId, ETabState::OpenedTab)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		TEXT("AutomationGraphEditorApp"),
		StandaloneDefaultLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		NewGraph);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FAutomationGraphEditor::OnClose()
{
	// TODO(): the user should probably get a popup if they try to close the editor while a graph is running
	CancelExecution();
	if (TargetGraph)
	{
		TargetGraph->UninitializeNodes();
	}
	
	FAssetEditorToolkit::OnClose();
}

void FAutomationGraphEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TargetGraph);	
}

bool FAutomationGraphEditor::CanSelectAllNodes()
{
	return true;
}
void FAutomationGraphEditor::SelectAllNodes()
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	SlateGraphEditor->SelectAllNodes();
}

bool FAutomationGraphEditor::CanDeleteNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node != nullptr && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}
void FAutomationGraphEditor::DeleteSelectedNodes()
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	SlateGraphEditor->GetCurrentGraph()->Modify();
	const FGraphPanelSelectionSet SelectedNodes = SlateGraphEditor->GetSelectedNodes();
	SlateGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = CastChecked<UEdGraphNode>(*NodeIt);

		if (Node->CanUserDeleteNode())
		{
			if (auto* GraphNode = Cast<UEdNode_AutomationGraphNode>(Node))
			{
				GraphNode->AutomationNode.Get()->Uninitialize();
				FBlueprintEditorUtils::RemoveNode(NULL, GraphNode, true);
				CastChecked<UEdGraph_AutomationGraph>(TargetGraph->EditorGraph)->RebuildAutomationGraph();
				TargetGraph->MarkPackageDirty();
			}
			else if (auto* EdgeNode = Cast<UEdNode_AutomationGraphEdge>(Node))
			{
				FBlueprintEditorUtils::RemoveNode(NULL, EdgeNode, true);
				CastChecked<UEdGraph_AutomationGraph>(TargetGraph->EditorGraph)->RebuildAutomationGraph();
				TargetGraph->MarkPackageDirty();
			}
			else
			{
				AG_LOG(LogAutoGraphEditor, Warning, TEXT("warning: FAutomationGraphEditor::DeleteSelectedNodes(): unknown node type"));
				FBlueprintEditorUtils::RemoveNode(NULL, Node, true);
			}
		}
	}
}

void FAutomationGraphEditor::DeleteSelectedDuplicatableNodes()
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet OldSelectedNodes = SlateGraphEditor->GetSelectedNodes();
	SlateGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			SlateGraphEditor->SetNodeSelection(Node, true);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	SlateGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			SlateGraphEditor->SetNodeSelection(Node, true);
		}
	}
}

bool FAutomationGraphEditor::CanCutNodes()
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FAutomationGraphEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FAutomationGraphEditor::CanCopyNodes()
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FAutomationGraphEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node == nullptr)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		if (auto* EdNode_Edge = Cast<UEdNode_AutomationGraphEdge>(*SelectedIter))
		{
			UEdNode_AutomationGraphNode* StartNode = EdNode_Edge->GetStartNode();
			UEdNode_AutomationGraphNode* EndNode = EdNode_Edge->GetEndNode();

			// Only copy an edge if both nodes it is connected to are also selected.
			if (!SelectedNodes.Contains(StartNode) || !SelectedNodes.Contains(EndNode))
			{
				SelectedIter.RemoveCurrent();
				continue;
			}
		}

		Node->PrepareForCopying();
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	// TODO(): Determine if the node's owner needs to change. See FSoundCueEditor::CopySelectedNodes() -> PostCopyNode
}

bool FAutomationGraphEditor::CanPasteNodes()
{
	if (!SlateGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(SlateGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FAutomationGraphEditor::PasteNodes()
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	PasteNodesHere(SlateGraphEditor->GetPasteLocation());
}

void FAutomationGraphEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	
	// Undo/Redo support
	UEdGraph* EdGraph = SlateGraphEditor->GetCurrentGraph();
	EdGraph->Modify();
	TargetGraph->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	SlateGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f, 0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		AvgNodePosition.X += Node->NodePosX;
		AvgNodePosition.Y += Node->NodePosY;
	}

	float InvNumNodes = 1.0f / float(PastedNodes.Num());
	AvgNodePosition.X *= InvNumNodes;
	AvgNodePosition.Y *= InvNumNodes;

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		SlateGraphEditor->SetNodeSelection(Node, true);

		Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X;
		Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y;

		Node->SnapToGrid(16);

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	CastChecked<UEdGraph_AutomationGraph>(TargetGraph->EditorGraph)->RebuildAutomationGraph();

	// Update UI
	SlateGraphEditor->NotifyGraphChanged();
	TargetGraph->PostEditChange();
	TargetGraph->MarkPackageDirty();
}

bool FAutomationGraphEditor::CanDuplicateNodes()
{
	return CanCopyNodes();
}

void FAutomationGraphEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FAutomationGraphEditor::CanRenameNodes() const
{
	return GetSelectedNodes().Num() == 1;
}

void FAutomationGraphEditor::OnRenameNode()
{
	if (!SlateGraphEditor.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt);
		if (SelectedNode != NULL && SelectedNode->bCanRenameNode)
		{
			SlateGraphEditor->IsNodeTitleVisible(SelectedNode, true);
			break;
		}
	}
}

TSharedRef<SDockTab> FAutomationGraphEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == AutomationGraphEditorConstants::GraphCanvasTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("AutomationGraphEditor_GraphCanvasTitle", "Viewport"));

	if (SlateGraphEditor.IsValid())
	{
		SpawnedTab->SetContent(SlateGraphEditor.ToSharedRef());
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FAutomationGraphEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == AutomationGraphEditorConstants::PropertiesTabId );

	return SNew(SDockTab)
		.Label(LOCTEXT("AutomationGraphDetailsTitle", "Details"))
		[
			NodeProperties.ToSharedRef()
		];
}

void FAutomationGraphEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolBar(FToolBarBuilder& ToolBarBuilder, TWeakPtr<FAutomationGraphEditor> Editor)
		{
			ToolBarBuilder.BeginSection("ManagerSelectionToolbar");
			{
				ToolBarBuilder.AddToolBarButton(
					FAutomationGraphEditorCommands::Get().ExecuteGraph,
					NAME_None,
					LOCTEXT("Playbutton_Label", "Run"),
					LOCTEXT("Playbutton_Tooltip", "Runs this graph"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play")
				);
				ToolBarBuilder.AddToolBarButton(
					FAutomationGraphEditorCommands::Get().CancelExecution,
					NAME_None,
					LOCTEXT("Stopbutton_Label", "Cancel"),
					LOCTEXT("Stupbutton_Tooltip", "Cancels this graph, if it is running"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Stop")
				);
			}
			ToolBarBuilder.EndSection();
		}
	};
	
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolBar, SharedThis(this).ToWeakPtr())
	);

	AddToolbarExtender(ToolbarExtender);
}

bool FAutomationGraphEditor::CanExecuteGraph() const
{
	return true;
}

void FAutomationGraphEditor::ExecuteGraph()
{
	if (!TargetGraph)
	{
		AG_LOG(LogAutoGraphEditor, Error, TEXT("TargetGraph is invalid"));
		return;
	}
	
	auto* AutomationGraphSubsystem = GEditor->GetEditorSubsystem<UAutomationGraphSubsystem>();
	if (!AutomationGraphSubsystem)
	{
		AG_LOG(LogAutoGraphEditor, Error, TEXT("AutomationGraphSubsystem is invalid"));
		return;
	}

	AutomationGraphSubsystem->EnqueueAutomationGraph(TargetGraph, EAutomationGraphNodeTrigger::OnPlay);
}

bool FAutomationGraphEditor::CanCancelExecution() const
{
	return true;
}

void FAutomationGraphEditor::CancelExecution()
{
	if (!TargetGraph)
	{
		AG_LOG(LogAutoGraphEditor, Error, TEXT("TargetGraph is invalid"));
		return;
	}
	
	if (auto* AutomationGraphSubsystem = GEditor->GetEditorSubsystem<UAutomationGraphSubsystem>())
	{
		AutomationGraphSubsystem->CancelGraphExecution(TargetGraph);
	}
}

#undef LOCTEXT_NAMESPACE