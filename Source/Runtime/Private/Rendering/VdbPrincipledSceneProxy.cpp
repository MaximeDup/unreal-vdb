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

#include "VdbPrincipledSceneProxy.h"
#include "VdbAssetComponent.h"
#include "VdbPrincipledComponent.h"
#include "VdbVolumeStatic.h"
#include "VdbCommon.h"
#include "VolumeRuntimeModule.h"
#include "Rendering/VdbPrincipledRendering.h"

#include "RenderTargetPool.h"

FVdbPrincipledSceneProxy::FVdbPrincipledSceneProxy(const UVdbAssetComponent* AssetComponent, const UVdbPrincipledComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, DisplayBounds(InComponent->DisplayBounds)
{
	LevelSet = AssetComponent->GetVdbClass() == EVdbClass::SignedDistance;
	
	const FVolumeRenderInfos* RenderInfosDensity = AssetComponent->GetRenderInfos(AssetComponent->PrimaryVolume);
	const FVolumeRenderInfos* RenderInfosTemperature = AssetComponent->GetRenderInfos(AssetComponent->SecondaryVolume);

	Params.VdbDensity = RenderInfosDensity->GetRenderResource();
	Params.VdbTemperature = RenderInfosTemperature ? RenderInfosTemperature->GetRenderResource() : nullptr;
	Params.IndexMin = RenderInfosDensity->GetIndexMin();
	Params.IndexSize = RenderInfosDensity->GetIndexSize();
	Params.IndexToLocal = RenderInfosDensity->GetIndexToLocal();
	Params.MaxRayDepth = InComponent->MaxRayDepth;
	Params.SamplesPerPixel = InComponent->SamplesPerPixel;
	Params.StepSize = InComponent->StepSize;
	Params.VoxelSize = AssetComponent->PrimaryVolume->GetVoxelSize();
	Params.Color = InComponent->Color;
	Params.DensityMult = InComponent->DensityMultiplier;
	Params.Albedo = InComponent->Albedo;
	Params.Anisotropy = InComponent->Anisotropy;
	Params.EmissionStrength = InComponent->EmissionStrength;
	Params.EmissionColor = InComponent->EmissionColor;
	Params.BlackbodyIntensity = InComponent->BlackbodyIntensity;
	Params.BlackbodyTint = InComponent->BlackbodyTint;
	Params.Temperature = InComponent->Temperature;
	Params.UseDirectionalLight = InComponent->UseDirectionalLight ? 1.0f : 0.0f;
	Params.UseEnvironmentLight = InComponent->UseEnvironmentLight ? 1.0f : 0.0f;

	VdbRenderMgr = FVolumeRuntimeModule::GetRenderPrincipledMgr();
}

void FVdbPrincipledSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	check(IsInRenderingThread());

	if (!Params.VdbDensity)
		return;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)))
		{
			VisibleViews.Add(View);

			// Only render Bounds
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			RenderBounds(PDI, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		}
	}
}

FPrimitiveViewRelevance FVdbPrincipledSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	return Result;
}

SIZE_T FVdbPrincipledSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FVdbPrincipledSceneProxy::CreateRenderThreadResources()
{
	FPrimitiveSceneProxy::CreateRenderThreadResources();

	VdbRenderMgr->AddVdbProxy(this);
}

void FVdbPrincipledSceneProxy::DestroyRenderThreadResources()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();

	VdbRenderMgr->RemoveVdbProxy(this);
}

FRDGTextureRef FVdbPrincipledSceneProxy::GetOrCreateRenderTarget(FRDGBuilder& GraphBuilder, const FIntPoint& RtSize, bool EvenFrame)
{
	if (!OffscreenRenderTarget[EvenFrame].IsValid() || OffscreenRenderTarget[EvenFrame]->GetDesc().Extent != RtSize)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		const FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			RtSize,
			PF_A16B16G16R16,
			FClearValueBinding(FLinearColor::Transparent),
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable,
			false);

		for (int idx = 0; idx < 2; ++idx)
		{
			FString DebugName = FString::Printf(TEXT("VdbRenderTarget_%d"), idx);
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OffscreenRenderTarget[idx], *DebugName);
			ensure(OffscreenRenderTarget[idx].IsValid());
		}
	}

	return GraphBuilder.RegisterExternalTexture(OffscreenRenderTarget[EvenFrame]);
}

void FVdbPrincipledSceneProxy::Update(const FMatrix44f& InIndexToLocal, const FVector3f& InIndexMin, const FVector3f& InIndexSize, FVdbRenderBuffer* DensityBuffer, FVdbRenderBuffer* TemperatureBuffer)
{
	Params.VdbDensity = DensityBuffer;
	Params.IndexMin = InIndexMin;
	Params.IndexSize = InIndexSize;
	Params.IndexToLocal = InIndexToLocal;
	Params.VdbTemperature = TemperatureBuffer;
}
