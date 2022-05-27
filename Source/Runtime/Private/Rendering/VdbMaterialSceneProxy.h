// Copyright 2022 Eidos-Montreal / Eidos-Sherbrooke

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once 

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"

class UVdbAssetComponent;
class UVdbMaterialComponent;

// Render Thread equivalent of VdbMaterialComponent
class FVdbMaterialSceneProxy : public FPrimitiveSceneProxy
{
public:
	FVdbMaterialSceneProxy(const UVdbAssetComponent* AssetComponent, const UVdbMaterialComponent* InComponent);
	virtual ~FVdbMaterialSceneProxy() = default;

	FVector3f GetIndexMin() const { return IndexMin; }
	FVector3f GetIndexSize() const { return IndexSize; }
	FIntVector4 GetCustomIntData0() const { return CustomIntData0; }
	FVector4f GetCustomFloatData0() const { return CustomFloatData0; }
	FVector4f GetCustomFloatData1() const { return CustomFloatData1; }
	const FMatrix44f& GetIndexToLocal() const { return IndexToLocal; }
	class UMaterialInterface* GetMaterial() const { return Material; }
	const class FVdbRenderBuffer* GetPrimaryRenderResource() const { return PrimaryRenderBuffer; }
	const class FVdbRenderBuffer* GetSecondaryRenderResource() const { return SecondaryRenderBuffer; }
	
	bool IsLevelSet() const { return LevelSet; }
	void ResetVisibility() { VisibleViews.Empty(4); }
	bool IsVisible(const FSceneView* View) const { return VisibleViews.Find(View) != INDEX_NONE; }
	void Update(const FMatrix44f& IndexToLocal, const FVector3f& IndexMin, const FVector3f& IndexSize, FVdbRenderBuffer* PrimaryRenderBuffer, FVdbRenderBuffer* SecondaryRenderBuffer);

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	//~ End FPrimitiveSceneProxy Interface

private:
	
	TSharedPtr<class FVdbMaterialRendering, ESPMode::ThreadSafe> VdbMaterialRenderExtension;

	// Fixed attributes
	const UVdbMaterialComponent* VdbMaterialComponent = nullptr;
	class UMaterialInterface* Material = nullptr;
	bool LevelSet;

	FIntVector4 CustomIntData0;
	FVector4f CustomFloatData0;
	FVector4f CustomFloatData1;

	FVdbRenderBuffer* PrimaryRenderBuffer;
	FVdbRenderBuffer* SecondaryRenderBuffer;
	FVector3f IndexMin;
	FVector3f IndexSize;
	FMatrix44f IndexToLocal;

	mutable TArray<const FSceneView*> VisibleViews;
};