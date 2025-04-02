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

#include "AutomationGraphEditorConstants.h"
#include "Editor/AutomationGraphEditor.h"
#include "Foundation/AutomationGraph.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "AutomationGraphEditor"

void FAutomationGraphEditorCommands::RegisterCommands()
{
	UI_COMMAND(ExecuteGraph, "Run", "Run", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CancelExecution, "Stop", "Stop", EUserInterfaceActionType::Button, FInputChord());
}

void FAutomationGraphEditor::BuildCustomCommands()
{
	ToolkitCommands->MapAction(
		FAutomationGraphEditorCommands::Get().ExecuteGraph,
		FExecuteAction::CreateSP(this, &FAutomationGraphEditor::ExecuteGraph),
		FCanExecuteAction::CreateSP(this, &FAutomationGraphEditor::CanExecuteGraph)
	);
	ToolkitCommands->MapAction(
		FAutomationGraphEditorCommands::Get().CancelExecution,
		FExecuteAction::CreateSP(this, &FAutomationGraphEditor::CancelExecution),
		FCanExecuteAction::CreateSP(this, &FAutomationGraphEditor::CanCancelExecution)
	);
}

void FAutomationGraphEditor::BuildGraphEditorCommands()
{
	if (GraphEditorCommands.IsValid())
	{
		// Already built, nothing to do...
		return;
	}
	
	GraphEditorCommands = MakeShareable(new FUICommandList);

	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::SelectAllNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanSelectAllNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanDeleteNodes)
	);
	
	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanCopyNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanCutNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::PasteNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanPasteNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &FAutomationGraphEditor::DuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &FAutomationGraphEditor::CanDuplicateNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &FAutomationGraphEditor::OnRenameNode),
		FCanExecuteAction::CreateSP(this, &FAutomationGraphEditor::CanRenameNodes)
	);
}

void FAutomationGraphEditor::CreateInternalWidgets()
{
	BuildGraphEditorCommands();

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_AutomationGraph", "Automation Graph");
	
	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FAutomationGraphEditor::OnSelectedNodesChanged);

	SlateGraphEditor = SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(TargetGraph->EditorGraph)
		.GraphEvents(InEvents)
		.AutoExpandActionMenu(true)
		.ShowGraphStateOverlay(false);

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;
	auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	NodeProperties = PropertyModule.CreateDetailView(Args);
	NodeProperties->SetObject(TargetGraph);
}

void FAutomationGraphEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<UObject*> Selection;

	for (UObject* SelectionEntry : NewSelection)
	{
		Selection.Add(SelectionEntry);
	}

	if (Selection.Num() == 0) 
	{
		NodeProperties->SetObject(TargetGraph);

	}
	else
	{
		NodeProperties->SetObjects(Selection);
	}
}

FGraphPanelSelectionSet FAutomationGraphEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	if (SlateGraphEditor.IsValid())
	{
		CurrentSelection = SlateGraphEditor->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FAutomationGraphEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AutomationGraphEditor", "Automation Graph Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// Spawn the graph canvas
	InTabManager->RegisterTabSpawner( AutomationGraphEditorConstants::GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FAutomationGraphEditor::SpawnTab_GraphCanvas) )
		.SetDisplayName( LOCTEXT("GraphCanvasTab", "Viewport") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	// Spawn the details panel
	InTabManager->RegisterTabSpawner( AutomationGraphEditorConstants::PropertiesTabId, FOnSpawnTab::CreateSP(this, &FAutomationGraphEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT("DetailsTab", "Details") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FAutomationGraphEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(AutomationGraphEditorConstants::GraphCanvasTabId);
	InTabManager->UnregisterTabSpawner(AutomationGraphEditorConstants::PropertiesTabId);
}

FName FAutomationGraphEditor::GetToolkitFName() const
{
	return FName("FAutomationGraphEditor");	
}

FText FAutomationGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AutomationGraphEditorAppLabel", "Automation Graph Editor");	
}

FText FAutomationGraphEditor::GetToolkitName() const
{
	// UE seems to check dirty state for us these days. Leaving this in for posterity.
	// const bool bDirtyState = TargetGraph->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("TargetGraphName"), FText::FromString(TargetGraph->GetName()));
	// Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("AutomationGraphEditorToolkitName", "{TargetGraphName}"), Args);
}

FText FAutomationGraphEditor::GetToolkitToolTipText() const
{
	return FAssetEditorToolkit::GetToolTipTextForObject(TargetGraph);	
}

FLinearColor FAutomationGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FAutomationGraphEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("AutomationGraphEditor");
}

FString FAutomationGraphEditor::GetDocumentationLink() const
{
	return TEXT("");
}

#undef LOCTEXT_NAMESPACE