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

class FVdbRenderBuffer;
class FVdbPrincipledRendering;

struct FVdbPrincipledParams
{
	FVdbRenderBuffer* VdbDensity;
	FVdbRenderBuffer* VdbTemperature;
	FVector3f IndexMin;
	FVector3f IndexSize;
	FMatrix44f IndexToLocal;
	uint32 MaxRayDepth;
	uint32 SamplesPerPixel;
	float StepSize;
	float VoxelSize;
	FLinearColor Color;
	float DensityMult;
	float Albedo;
	float Anisotropy;
	float EmissionStrength;
	FLinearColor EmissionColor;
	FLinearColor BlackbodyTint;
	float BlackbodyIntensity;
	float Temperature;
	float UseDirectionalLight;
	float UseEnvironmentLight;
};

// Render Thread equivalent of VdbPrincipledComponent
class FVdbPrincipledSceneProxy : public FPrimitiveSceneProxy
{
public:
	FVdbPrincipledSceneProxy(const class UVdbAssetComponent* AssetComponent, const class UVdbPrincipledComponent* InComponent);
	virtual ~FVdbPrincipledSceneProxy() = default;

	void UpdateProperties_RenderThread();

	const FVdbPrincipledParams& GetParams() const { return Params; }
	bool GetDisplayBounds() const { return DisplayBounds; }
	bool IsLevelSet() const { return LevelSet; }

	FRDGTextureRef GetOrCreateRenderTarget(FRDGBuilder& GraphBuilder, const FIntPoint& RtSize, bool EvenFrame);

	void ResetVisibility() { VisibleViews.Empty(4); }
	bool IsVisible(const FSceneView* View) const { return VisibleViews.Find(View) != INDEX_NONE; }
	void Update(const FMatrix44f& InIndexToLocal, const FVector3f& InIndexMin, const FVector3f& InIndexSize, FVdbRenderBuffer* DensityBuffer, FVdbRenderBuffer* TemperatureBuffer);

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

	TSharedPtr<FVdbPrincipledRendering, ESPMode::ThreadSafe> VdbRenderMgr;
	FVdbPrincipledParams Params;
	bool DisplayBounds;
	bool LevelSet;

	// RTs per proxy, for easier translucency support
	TRefCountPtr<IPooledRenderTarget> OffscreenRenderTarget[2];

	mutable TArray<const FSceneView*> VisibleViews;
};
