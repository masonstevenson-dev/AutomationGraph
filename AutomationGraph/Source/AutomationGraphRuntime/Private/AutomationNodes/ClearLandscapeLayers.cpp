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

#include "AutomationNodes/ClearLandscapeLayers.h"

#include "AutomationGraphRuntimeLoggingDefs.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "Macros/AutomationGraphLoggingMacros.h"

UAGN_ClearLandscapeLayers::UAGN_ClearLandscapeLayers(const FObjectInitializer& Initializer): Super(Initializer)
{
	Title = FText::FromString("ClearLandscapeLayers");
}

EAutomationGraphNodeState UAGN_ClearLandscapeLayers::ActivateInternal(float DeltaSeconds)
{
	if (!TargetLandscape.IsValid())
	{
		return SetState(EAutomationGraphNodeState::Error);
	}
	if (EditLayers.IsEmpty() || PaintLayers.IsEmpty())
	{
		return SetState(EAutomationGraphNodeState::Error);
	}

	// Standard activation, ensures the node is active past this block.
	{
		EAutomationGraphNodeState CurrentState = GetState();
		if (CurrentState == EAutomationGraphNodeState::Standby)
		{
			return SetState(EAutomationGraphNodeState::Active);
		}
		if (CurrentState != EAutomationGraphNodeState::Active)
		{
			return CurrentState;
		}
	}
	
	ALandscape* LandscapePtr = TargetLandscape.Get();
	TMap<FName, ULandscapeLayerInfoObject*> LayerInfoByName;

	ULandscapeInfo* LandscapeInfo = LandscapePtr->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("LandscapeInfo is invalid."));
		return SetState(EAutomationGraphNodeState::Error);
	}
	for (FName PaintLayerName : PaintLayers)
	{
		ULandscapeLayerInfoObject* LayerInfo = LandscapeInfo->GetLayerInfoByName(PaintLayerName);
		if (!LayerInfo)
		{
			AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("unknown paint layer \"%s\""), *PaintLayerName.ToString());
			return SetState(EAutomationGraphNodeState::Error);
		}
		LayerInfoByName.Add(PaintLayerName, LayerInfo);
	}

	for (FName EditLayerName : EditLayers)
	{
		int32 EditLayerIndex = LandscapePtr->GetLayerIndex(EditLayerName);
		if (EditLayerIndex == INDEX_NONE)
		{
			AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("unknown edit layer \"%s\""), *EditLayerName.ToString());
			return SetState(EAutomationGraphNodeState::Error);
		}

		for (FName PaintLayerName : PaintLayers)
		{
			LandscapePtr->ClearPaintLayer(EditLayerIndex, LayerInfoByName[PaintLayerName]);
		}
	}
	
	return SetState(EAutomationGraphNodeState::Finished);
}

bool UAGN_ClearLandscapeLayers::Initialize(UWorld* World)
{
	if (!World)
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Failed to initialize ClearLandscapeLayers node: world is invalid."));
		SetState(EAutomationGraphNodeState::Error);
		return false;
	}

	for (TActorIterator<ALandscape> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* CurrentActor = *ActorItr;
		auto* LandscapeActor = Cast<ALandscape>(CurrentActor);

		if (!LandscapeActor)
		{
			continue;
		}

		// Note: For now, we just grab the first landscape we can find. Technically a world can have more than one
		//       Landscape, although this generally is not recommended.
		TargetLandscape = LandscapeActor;
		break;
	}

	if (!TargetLandscape.IsValid())
	{
		AG_LOG_OBJECT(this, LogAutoGraphRuntime, Error, TEXT("Failed to initialize ClearLandscapeLayers node: Landscape is missing."));
		SetState(EAutomationGraphNodeState::Error);
		return false;
	}

	return Super::Initialize(World);
}
