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

#include "EdGraph/EdNode_AutomationGraphNode.h"

#include "AutomationGraphEditorConstants.h"
#include "AutomationGraphEditorLoggingDefs.h"
#include "Boilerplate/AutomationGraphDragConnection.h"
#include "GraphEditorSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "Macros/AutomationGraphLoggingMacros.h"
#include "Styles/AutomationGraphEditorStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "EdNode_AutomationGraphNode"

class SAutomationNodeGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SAutomationNodeGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		this->SetCursor(EMouseCursor::Default);

		bShowLabel = true;

		GraphPinObj = InPin;
		check(GraphPinObj != nullptr);

		const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
		check(Schema);

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(this, &SAutomationNodeGraphPin::GetPinBorder)
			.BorderBackgroundColor(this, &SAutomationNodeGraphPin::GetPinColor)
			.OnMouseButtonDown(this, &SAutomationNodeGraphPin::OnPinMouseDown)
			.Cursor(this, &SAutomationNodeGraphPin::GetPinCursor)
			.Padding(FMargin(5.0f))
		);
	}

protected:
	/*
	virtual FSlateColor GetPinColor() const override
	{
		return FLinearColor(0.02f, 0.02f, 0.02f);
	}*/

	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override
	{
		return SNew(STextBlock);
	}

	const FSlateBrush* GetPinBorder() const
	{
		return ( IsHovered() )
		? FAppStyle::GetBrush( TEXT("Graph.StateNode.Pin.BackgroundHovered") )
		: FAppStyle::GetBrush( TEXT("Graph.StateNode.Pin.Background") );
	}

	virtual TSharedRef<FDragDropOperation> SpawnPinDragEvent(const TSharedRef<class SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins) override
	{
		FAutomationGraphDragConnection::FDraggedPinTable PinHandles;
		PinHandles.Reserve(InStartingPins.Num());
		// since the graph can be refreshed and pins can be reconstructed/replaced 
		// behind the scenes, the DragDropOperation holds onto FGraphPinHandles 
		// instead of direct widgets/graph-pins
		for (const TSharedRef<SGraphPin>& PinWidget : InStartingPins)
		{
			PinHandles.Add(PinWidget->GetPinObj());
		}

		return FAutomationGraphDragConnection::New(InGraphPanel, PinHandles);
	}

};

void SEdNode_AutomationGraphNode::Construct(const FArguments& InArgs, UEdNode_AutomationGraphNode* InNode)
{
	GraphNode = InNode;
	UpdateGraphNode();
}

void SEdNode_AutomationGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	
	// Reset variables that are going to be exposed, in case we are refreshing an already set up node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const FSlateBrush* NodeTypeIcon = GetNameIcon();

	FLinearColor TitleShadowColor(0.6f, 0.6f, 0.6f);
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush( "Graph.StateNode.Body" ) )
			.Padding(0)
			.BorderBackgroundColor( this, &SEdNode_AutomationGraphNode::GetBorderBackgroundColor )
			[
				SNew(SOverlay)

				// PIN AREA
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					// Only the output "pin" is selectable.
					SAssignNew(RightNodeBox, SVerticalBox)
				]

				// STATE NAME AREA
				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(10.0f)
				[
					SNew(SBorder)
					.BorderImage( FAppStyle::GetBrush("Graph.StateNode.ColorSpill") )
					.BorderBackgroundColor( TitleShadowColor )
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(10.0f)
					.Visibility(EVisibility::SelfHitTestInvisible)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							// POPUP ERROR MESSAGE
							SAssignNew(ErrorText, SErrorText )
							.BackgroundColor( this, &SEdNode_AutomationGraphNode::GetErrorColor )
							.ToolTipText( this, &SEdNode_AutomationGraphNode::GetErrorMsgToolTip )
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(NodeTypeIcon)
						]
						+SHorizontalBox::Slot()
						.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								SAssignNew(InlineEditableText, SInlineEditableTextBlock)
								.Style( FAppStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText" )
								.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
								.OnVerifyTextChanged(this, &SEdNode_AutomationGraphNode::OnVerifyNameTextChanged)
								.OnTextCommitted(this, &SEdNode_AutomationGraphNode::OnNameTextCommited)
								.IsReadOnly( this, &SEdNode_AutomationGraphNode::IsNameReadOnly )
								.IsSelected(this, &SEdNode_AutomationGraphNode::IsSelectedExclusively)
							]
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								NodeTitle.ToSharedRef()
							]
						]
					]
				]
			]
		];

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreatePinWidgets();
}

void SEdNode_AutomationGraphNode::CreatePinWidgets()
{
	auto* MyNode = CastChecked<UEdNode_AutomationGraphNode>(GraphNode);

	for (int32 PinIdx = 0; PinIdx < MyNode->Pins.Num(); PinIdx++)
	{
		UEdGraphPin* MyPin = MyNode->Pins[PinIdx];
		if (!MyPin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = SNew(SAutomationNodeGraphPin, MyPin);
			
			AddPin(NewPin.ToSharedRef());
		}
	}
}

void SEdNode_AutomationGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bAdvancedParameter = PinObj && PinObj->bAdvancedView;
	if (bAdvancedParameter)
	{
		PinToAdd->SetVisibility( TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced) );
	}

	TSharedPtr<SVerticalBox> PinBox;
	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		PinBox = LeftNodeBox;
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		PinBox = RightNodeBox;
		OutputPins.Add(PinToAdd);
	}

	if (PinBox)
	{
		PinBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			//.Padding(6.0f, 0.0f)
			[
				PinToAdd
			];
	}
}

void SEdNode_AutomationGraphNode::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	auto* MyNode = CastChecked<UEdNode_AutomationGraphNode>(GraphNode);

	if (!MyNode || !MyNode->AutomationNode)
	{
		return;
	}

	FLinearColor MessageColor = FLinearColor(1.f, 0.5f, 0.25f);
	FString NodeMessage = MyNode->AutomationNode->GetMessageText();

	if (!NodeMessage.IsEmpty())
	{
		Popups.Emplace(nullptr, MessageColor, NodeMessage);
	}
}

FSlateColor SEdNode_AutomationGraphNode::GetBorderBackgroundColor() const
{
	auto* MyNode = CastChecked<UEdNode_AutomationGraphNode>(GraphNode);
	
	FLinearColor StateColor_Inactive(0.08f, 0.08f, 0.08f);

	if (MyNode->AutomationNode)
	{
		return MyNode->AutomationNode->GetStateColor();
	}

	return StateColor_Inactive;
}

const FSlateBrush* SEdNode_AutomationGraphNode::GetNameIcon() const
{
	auto* MyNode = CastChecked<UEdNode_AutomationGraphNode>(GraphNode);
	return MyNode->GetNodeIcon();
}

void SEdNode_AutomationGraphNode::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	SGraphNode::OnNameTextCommited(InText, CommitInfo);

	auto* MyNode = CastChecked<UEdNode_AutomationGraphNode>(GraphNode);

	if (MyNode != nullptr && MyNode->AutomationNode != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("AutomationGraphNodeRenameNode", "Automation Graph Node: Rename Node"));
		MyNode->Modify();
		MyNode->AutomationNode->Modify();
		MyNode->AutomationNode->Title = InText;
		UpdateGraphNode();
	}
}

UEdNode_AutomationGraphNode::UEdNode_AutomationGraphNode(const FObjectInitializer& Initializer): Super(Initializer)
{
	bCanRenameNode = true;
}

void UEdNode_AutomationGraphNode::AllocateDefaultPins()
{
	if (Pins.Num() > 0)
	{
		AG_LOG_OBJECT(this, LogAutoGraphEditor, Error, TEXT("Default pins have already been allocated."));
		return;
	}
	
	CreatePin(EGPD_Input, AutomationGraphEditorConstants::PinCategory_MultipleNodes, FName(), TEXT("In"));
	CreatePin(EGPD_Output, AutomationGraphEditorConstants::PinCategory_MultipleNodes, FName(), TEXT("Out"));
}

FText UEdNode_AutomationGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!AutomationNode)
	{
		return Super::GetNodeTitle(TitleType);
	}
	if (!AutomationNode->Title.IsEmpty())
	{
		return AutomationNode->Title;
	}

	return FText::FromString("Unknown");
}

void UEdNode_AutomationGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();
	AutomationNode->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
}

void UEdNode_AutomationGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

FLinearColor UEdNode_AutomationGraphNode::GetBackgroundColor() const
{
	return FLinearColor::Black;
}

UEdGraphPin* UEdNode_AutomationGraphNode::GetInputPin() const
{
	return Pins[0];
}

UEdGraphPin* UEdNode_AutomationGraphNode::GetOutputPin() const
{
	return Pins[1];
}

const FSlateBrush* UEdNode_AutomationGraphNode::GetNodeIcon()
{
	if (AutomationNode.IsA(UCoreAutomationGraphNode::StaticClass()))
	{
		return FAutomationGraphEditorStyle::Get()->GetBrush(TEXT("AutomationGraphEditor.NodeIcon"));
	}
	
	return FAutomationGraphEditorStyle::Get()->GetBrush(TEXT("AutomationGraphEditor.WrenchIcon"));

	// Behavior tree icon
	// return FAppStyle::GetBrush(TEXT("BTEditor.Graph.BTNode.Icon"));
}

#undef LOCTEXT_NAMESPACE