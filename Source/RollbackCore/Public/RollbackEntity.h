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
	/**
	 * Frame convention: the manager calls SaveRollbackState(Frame) BEFORE RollbackTick(Frame),
	 * for every entity, so the snapshot stored under Frame is the state Frame's input is applied
	 * to (the end of Frame - 1). LoadRollbackState(Frame) must restore exactly that; the manager
	 * then replays Frame itself. An entity that keeps its own history must use the same rule.
	 */
	virtual void SaveRollbackState(int32 Frame) = 0;
	virtual void LoadRollbackState(int32 Frame) = 0;
};