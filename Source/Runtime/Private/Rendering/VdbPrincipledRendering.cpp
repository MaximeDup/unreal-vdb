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

#include "VdbPrincipledRendering.h"

#include "VdbPrincipledComponent.h"
#include "VdbPrincipledSceneProxy.h"
#include "VdbRenderBuffer.h"
#include "VdbCommon.h"
#include "VdbShaders.h"
#include "VdbComposite.h"
#include "VolumeMesh.h"

#include "Modules\ModuleManager.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "RenderGraphUtils.h"

TAutoConsoleVariable<int32> CVarPathTracingMaxSamplesPerPixel(
	TEXT("r.VdbPrincipled.MaxSamplesPerPixel"),
	-1,
	TEXT("Defines the samples per pixel before resetting the simulation (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

FVdbPrincipledRendering::FVdbPrincipledRendering(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FVdbPrincipledRendering::InitBuffers()
{
	if (VertexBufferRHI == nullptr || !VertexBufferRHI.IsValid())
	{
		// Setup vertex buffer
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(8);

		FVector3f BboxMin(0, 0, 0);
		FVector3f BboxMax(1, 1, 1);

		// Front face
		Vertices[0].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMin.Z, 1.f);	Vertices[0].UV = FVector2f(0.f, 0.f);
		Vertices[1].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMin.Z, 1.f);	Vertices[1].UV = FVector2f(1.f, 0.f);
		Vertices[2].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMin.Z, 1.f);	Vertices[2].UV = FVector2f(0.f, 1.f);
		Vertices[3].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMin.Z, 1.f);	Vertices[3].UV = FVector2f(1.f, 1.f);
		// Back face		   FVector4f
		Vertices[4].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMax.Z, 1.f);	Vertices[0].UV = FVector2f(1.f, 1.f);
		Vertices[5].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMax.Z, 1.f);	Vertices[1].UV = FVector2f(1.f, 0.f);
		Vertices[6].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMax.Z, 1.f);	Vertices[2].UV = FVector2f(0.f, 1.f);
		Vertices[7].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMax.Z, 1.f);	Vertices[3].UV = FVector2f(0.f, 0.f);

		FRHIResourceCreateInfo CreateInfoVB(TEXT("VdbVolumeMeshVB"), &Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfoVB);
	}

	if (IndexBufferRHI == nullptr || !IndexBufferRHI.IsValid())
	{
		// Setup index buffer
		const uint16 Indices[] = { 
			// bottom face
			0, 1, 2,
			1, 3, 2,
			// right face
			1, 5, 3,
			3, 5, 7,
			// front face
			3, 7, 6,
			2, 3, 6,
			// left face
			2, 4, 0,
			2, 6, 4,
			// back face
			0, 4, 5,
			1, 0, 5,
			// top face
			5, 4, 6,
			5, 6, 7 };

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		const uint32 NumIndices = UE_ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		FRHIResourceCreateInfo CreateInfoIB(TEXT("VdbVolumeMeshIB"), &IndexBuffer);
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);
	}
}

void FVdbPrincipledRendering::InitRendering()
{
	check(IsInRenderingThread());

	InitBuffers();
	InitDelegate();
}

void FVdbPrincipledRendering::ReleaseRendering()
{
	check(IsInRenderingThread());

	ReleaseDelegate();
}

void FVdbPrincipledRendering::Init()
{
	if (IsInRenderingThread())
	{
		InitRendering();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				Init();
			});
	}
}

void FVdbPrincipledRendering::Release()
{
	if (IsInRenderingThread())
	{
		ReleaseRendering();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				Release();
			});
	}
}

void FVdbPrincipledRendering::InitDelegate()
{
	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FVdbPrincipledRendering::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterOverlayRenderDelegate(RenderDelegate);
		}
	}
}

void FVdbPrincipledRendering::ReleaseDelegate()
{
	if (RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RendererModule->RemoveOverlayRenderDelegate(RenderDelegateHandle);
		}

		RenderDelegateHandle.Reset();
	}
}

TRDGUniformBufferRef<FVdbPrincipledShaderParams> CreateVdbUniformBuffer(FRDGBuilder& GraphBuilder, const FVdbPrincipledSceneProxy* Proxy, bool UsePathTracing)
{
	FVdbPrincipledShaderParams* UniformParameters = GraphBuilder.AllocParameters<FVdbPrincipledShaderParams>();

	const FVdbPrincipledParams& Params = Proxy->GetParams();

	// Volume Params
	UniformParameters->VdbDensity = Params.VdbDensity->GetBufferSRV();
	UniformParameters->VdbTemperature = Params.VdbTemperature ? Params.VdbTemperature->GetBufferSRV() : UniformParameters->VdbDensity;
	UniformParameters->VolumeScale = Params.IndexSize;
	UniformParameters->VolumeTranslation = Params.IndexMin;
	UniformParameters->VolumeToLocal = Params.IndexToLocal;
	UniformParameters->LocalToWorld = FMatrix44f(Proxy->GetLocalToWorld());
	UniformParameters->WorldToLocal = FMatrix44f(Proxy->GetLocalToWorld().Inverse());
	UniformParameters->SamplesPerPixel = UsePathTracing ? 1 : Params.SamplesPerPixel;
	UniformParameters->StepSize = Params.StepSize;
	UniformParameters->VoxelSize = Params.VoxelSize;
	UniformParameters->MaxRayDepth = Params.MaxRayDepth;
	// Material Params
	auto LinearColorToVector = [](const FLinearColor& Col) { return FVector3f(Col.R, Col.G, Col.B); };
	UniformParameters->Color = LinearColorToVector(Params.Color);
	UniformParameters->DensityMult = Params.DensityMult;
	UniformParameters->Albedo = Params.Albedo;
	UniformParameters->Anisotropy = Params.Anisotropy;
	UniformParameters->EmissionColor = LinearColorToVector(Params.EmissionColor);
	UniformParameters->EmissionStrength = Params.EmissionStrength;
	UniformParameters->BlackbodyTint = LinearColorToVector(Params.BlackbodyTint);
	UniformParameters->BlackbodyIntensity = Params.BlackbodyIntensity;
	UniformParameters->Temperature = Params.Temperature;
	UniformParameters->UseDirectionalLight = Params.UseDirectionalLight;
	UniformParameters->UseEnvironmentLight = Params.UseEnvironmentLight;

	return GraphBuilder.CreateUniformBuffer(UniformParameters);
}

void FVdbPrincipledRendering::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	if (VdbProxies.IsEmpty())
		return;

	SCOPE_CYCLE_COUNTER(STAT_VdbPrincipled_RT);

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);
	const FIntRect& ViewportRect = Parameters.ViewportRect;

	// Sort back to front. Ignore frustum visibility
	TArray<FVdbPrincipledSceneProxy*> SortedVdbProxies = VdbProxies.FilterByPredicate([View](const FVdbPrincipledSceneProxy* Proxy) { return Proxy->IsVisible(View) && !Proxy->IsLevelSet(); });
	SortedVdbProxies.Sort([ViewMat = View->ViewMatrices.GetViewMatrix()](const FVdbPrincipledSceneProxy& Lhs, const FVdbPrincipledSceneProxy& Rhs) -> bool
	{
		const FVector& LeftProxyCenter = Lhs.GetBounds().GetSphere().Center;
		const FVector& RightProxyCenter = Rhs.GetBounds().GetSphere().Center;
		return ViewMat.TransformPosition(LeftProxyCenter).Z > ViewMat.TransformPosition(RightProxyCenter).Z; // front to back
	});

	FRDGTextureDesc TexDesc = Parameters.ColorTexture->Desc;
	TexDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);

	uint32 NumAccumulations = 0;
	const bool UsePathTracing = View->Family->EngineShowFlags.PathTracing;

#if RHI_RAYTRACING
	if (UsePathTracing)
	{
		// Hack and plug ourselves on top of the path tracing renderer
		checkSlow(View->bIsViewInfo);
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);
		FSceneViewState* ViewState = ViewInfo->ViewState;
		if (ViewState)
		{
			NumAccumulations = ViewState->GetPathTracingSampleIndex() ? ViewState->GetPathTracingSampleIndex() - 1u : 0u;
		}
	}
#endif

	FIntPoint RtSize = Parameters.ColorTexture->Desc.Extent;
	bool IsEven = NumAccumulations % 2;

	int32 SamplesPerPixelCVar = CVarPathTracingMaxSamplesPerPixel.GetValueOnRenderThread();
	uint32 MaxSPP = SamplesPerPixelCVar > -1 ? SamplesPerPixelCVar : View->FinalPostProcessSettings.PathTracingSamplesPerPixel;
	MaxSPP = FMath::Max(MaxSPP, 1u);

	for (FVdbPrincipledSceneProxy* Proxy : SortedVdbProxies)
	{
		// Cannot read and write from the same buffer. Use double-buffering rendering.
		FRDGTextureRef VdbCurrRenderTexture = Proxy->GetOrCreateRenderTarget(GraphBuilder, RtSize, IsEven);
		FRDGTextureRef VdbPrevRenderTexture = Proxy->GetOrCreateRenderTarget(GraphBuilder, RtSize, !IsEven);

		if (NumAccumulations < MaxSPP && Proxy->GetParams().VdbDensity)
		{
			TRDGUniformBufferRef<FVdbPrincipledShaderParams> VdbUniformBuffer = CreateVdbUniformBuffer(GraphBuilder, Proxy, UsePathTracing);

			FVdbPrincipledPS::FParameters* ParametersPS = GraphBuilder.AllocParameters<FVdbPrincipledPS::FParameters>();
			ParametersPS->View = View->ViewUniformBuffer;
			ParametersPS->SceneDepthTexture = Parameters.DepthTexture;
			ParametersPS->PrevAccumTex = VdbPrevRenderTexture;
			ParametersPS->NumAccumulations = NumAccumulations;
			ParametersPS->VdbGlobalParams = VdbUniformBuffer;
			ParametersPS->DisplayBounds = Proxy->GetDisplayBounds();
			ParametersPS->RenderTargets[0] = FRenderTargetBinding(VdbCurrRenderTexture, ERenderTargetLoadAction::EClear);

			FVdbPrincipledPS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FVdbPrincipledPS::FPathTracing>(UsePathTracing);
			PermutationVector.Set<FVdbPrincipledPS::FUseTemperature>(Proxy->GetParams().VdbTemperature != nullptr);

			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FVdbPrincipledVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FVdbPrincipledPS> PixelShader(GlobalShaderMap, PermutationVector);

			ClearUnusedGraphResources(PixelShader, ParametersPS);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("VdbPrincipledRendering"),
				ParametersPS,
				ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[this, ParametersPS, VertexShader, PixelShader, ViewportRect, Proxy, View](FRHICommandList& RHICmdList)
				{
					FVdbPrincipledVS::FParameters ParametersVS;
					ParametersVS.View = View->ViewUniformBuffer;
					ParametersVS.VdbGlobalParams = ParametersPS->VdbGlobalParams;

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Less>::GetRHI();

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *ParametersPS);

					RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
					RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
					RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, 8, 0, 12, 1);
				});

			// Optional denoising (disabled with path tracing)
			if (!UsePathTracing)
			{
				VdbCurrRenderTexture = VdbDenoiser::ApplyDenoising(GraphBuilder, VdbCurrRenderTexture, View, Parameters.ViewportRect, DenoiserMethod);
			}
		}

		// Composite VDB offscreen rendering onto back buffer
		VdbComposite::CompositeFullscreen(GraphBuilder, VdbCurrRenderTexture, Parameters.ColorTexture, View);
	}
}

void FVdbPrincipledRendering::AddVdbProxy(FVdbPrincipledSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FAddVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(VdbProxies.Find(Proxy) == INDEX_NONE);
			VdbProxies.Emplace(Proxy);
		});
}

void FVdbPrincipledRendering::RemoveVdbProxy(FVdbPrincipledSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FRemoveVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			auto Idx = VdbProxies.Find(Proxy);
			if (Idx != INDEX_NONE)
			{
				VdbProxies.Remove(Proxy);
			}
		});
}

void FVdbPrincipledRendering::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	// Reset visibility on all registered FVdbProxies, before SceneVisibility is computed 
	for (FVdbPrincipledSceneProxy* Proxy : VdbProxies)
	{
		Proxy->ResetVisibility();
	}
}
