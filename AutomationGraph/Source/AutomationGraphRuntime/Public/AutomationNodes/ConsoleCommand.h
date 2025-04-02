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
#include "Foundation/AutomationGraphNode.h"

#include "ConsoleCommand.generated.h"

UCLASS(Abstract)
class AUTOMATIONGRAPHRUNTIME_API UAGN_ConsoleCommandBase : public UCoreAutomationGraphNode
{
	GENERATED_BODY()

public:
	//~UAutomationGraphNode interface.
	virtual bool Initialize(UWorld* NewWorld) override;
	//~End UAutomationGraphNode interface.
	
protected:
	//~UAutomationGraphNode interface.
	virtual EAutomationGraphNodeState ActivateInternal(float DeltaSeconds) override;
	//~End UAutomationGraphNode interface.
	
	virtual FString GetCommand() { return FString(); }

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld;
};

UCLASS(meta=( DisplayName="Console Command" ))
class AUTOMATIONGRAPHRUNTIME_API UAGN_ConsoleCommand : public UAGN_ConsoleCommandBase
{
	GENERATED_BODY()

public:
	UAGN_ConsoleCommand(const FObjectInitializer& Initializer);
	
protected:
	virtual FString GetCommand() override { return Command; }
	virtual FText GetNodeCategory() override { return FAutomationGraphNodeCategory::Util; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Command;
};