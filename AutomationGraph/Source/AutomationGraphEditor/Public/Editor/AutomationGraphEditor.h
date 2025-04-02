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
#include "CoreMinimal.h"

class UAutomationGraph;

class FAutomationGraphEditorCommands : public TCommands<FAutomationGraphEditorCommands>
{
public:
	FAutomationGraphEditorCommands(): TCommands<FAutomationGraphEditorCommands>(
		TEXT("AutomationGraphEditor"),
		NSLOCTEXT("Contexts", "AutomationGraphEditor", "Automation Graph Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName()
		)
	{
	}

	TSharedPtr<FUICommandInfo> ExecuteGraph;
	TSharedPtr<FUICommandInfo> CancelExecution;

// boilerplate
public:
	virtual void RegisterCommands() override;
};

class FAutomationGraphEditor : public FAssetEditorToolkit, public FNotifyHook, public FGCObject
{
public:
	FAutomationGraphEditor();
	virtual ~FAutomationGraphEditor();
	
	void Initialize(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UAutomationGraph* NewGraph);
	
	//~IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	//~End IToolkit interface
	
	//~FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FString GetDocumentationLink() const override;
	virtual void OnClose() override;
	/// virtual void SaveAsset_Execute() override;
	//~End FAssetEditorToolkit interface

	//~FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAutomationGraphEditor");
	}
	//~End FGCObject interface

// Representative Object (the object this editor is editing)
public:
	TObjectPtr<UAutomationGraph> TargetGraph;

// boilerplate
public:
	void BuildCustomCommands();
	void BuildGraphEditorCommands();
	void CreateInternalWidgets();
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);
	FGraphPanelSelectionSet GetSelectedNodes() const;
	
// Editor Commands
protected:
	// TODO(): check if target graph is running- if it is, do not allow these actions
	bool CanSelectAllNodes();
	void SelectAllNodes();

	bool CanDeleteNodes();
	void DeleteSelectedNodes();
	void DeleteSelectedDuplicatableNodes();
	
	bool CanCutNodes();
	void CutSelectedNodes();

	bool CanCopyNodes();
	void CopySelectedNodes();

	bool CanPasteNodes();
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);

	bool CanDuplicateNodes();
	void DuplicateNodes();

	bool CanRenameNodes() const;
	void OnRenameNode();

	TSharedPtr<FUICommandList> GraphEditorCommands;

// Main Viewport
protected:
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	
	TSharedPtr<SGraphEditor> SlateGraphEditor;

// properties panel
protected:
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);

	TSharedPtr<class IDetailsView> NodeProperties;
	
protected:
	void ExtendToolbar();

	bool CanExecuteGraph() const;
	void ExecuteGraph();

	bool CanCancelExecution() const;
	void CancelExecution();
};
