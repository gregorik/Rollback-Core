// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RollbackEntity.generated.h"

UINTERFACE(MinimalAPI)
class URollbackEntity : public UInterface
{
	GENERATED_BODY()
};

class ROLLBACKCORE_API IRollbackEntity
{
	GENERATED_BODY()

public:
    virtual void RollbackTick(float DeltaTime, int32 Frame) = 0;
	virtual void SaveRollbackState(int32 Frame) = 0;
	virtual void LoadRollbackState(int32 Frame) = 0;
};