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
#include "Components/PrimitiveComponent.h"

#include "VdbPrincipledComponent.generated.h"

class UVdbSequenceComponent;
class UVdbAssetComponent;

// If you do not care about Unreal features integration, I recommend using this "Principled" component.
// It allows you to experiment with OpenVDB / NanoVDB rendering, without having to worry 
// about most Unreal compatibilities. 
// 
// These NanoVDBs are rendered at the end of the graphics pipeline, just before the Post Processes. 
// 
// This cannot be used in production, this is only used for research and experimentation purposes. It will 
// probably be incompatible with a lot of other Unreal features (but we don't care).
// Also note that this component can hack into the Unreal's path-tracer, and render high quality images.
// We made the delibarate choice to only handle NanoVDB FogVolumes with this component, because they benefit most 
// from experimentation and path-tracers, and are still an active research area (offline and realtime).
UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, VirtualTexture), meta = (BlueprintSpawnableComponent))
class UVdbPrincipledComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbPrincipledComponent();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VdbAssetComponent"), Category = Volume)
	void SetVdbAssets(UVdbAssetComponent* Comp);

	//----------------------------------------------------------------------------
	// Volume Attributes
	//----------------------------------------------------------------------------

	// Max number of ray bounces
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (ClampMin = "1", UIMin = "1", ClampMax = "50", UIMax = "20"))
	int32 MaxRayDepth = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (ClampMin = "1", UIMin = "1"))
	int32 SamplesPerPixel = 1;

	// Volume local step size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (ClampMin = "0.5", UIMin = "0.5"))
	float StepSize = 8.0f;

	//-------------------------------------------------------------------------------
	//    Principled Volume Shader Options, inspired by these two sources:
	// https://docs.arnoldrenderer.com/display/A5AFMUG/Standard+Volume
	// https://docs.blender.org/manual/en/latest/render/shader_nodes/shader/volume_principled.html
	//----------------------------------------------------------------------------

	// Volume scattering color. This acts as a multiplier on the scatter color, to texture the color of the volume.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume")
	FLinearColor Color = FLinearColor(10.0, 10.0, 10.0, 1.0);

	// Density multiplier of the volume, modulating VdbDensity values 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (ClampMin = "0.00001", UIMin = "0.00001"))
	float DensityMultiplier = 1.0;

	// Describes the probability of scattering (versus absorption) at a scattering event. Between 0 and 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Albedo = 0.8;

	// Backward or forward scattering direction (aka directional bias).
	// The default value of zero gives isotropic scattering so that light is scattered evenly in all directions. 
	// Positive values bias the scattering effect forwards, in the direction of the light, while negative values bias the scattering backward, toward the light. 
	// This shader uses the Henyey-Greenstein phase function.
	// Note that values very close to 1.0 (above 0.95) or -1.0 (below - 0.95) will produce scattering that is so directional that it will not be very visible from most angles, so such values are not recommended.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float Anisotropy = 0.0;

	// Amount of light to emit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EmissionStrength = 0.0;

	// Emission color tint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume")
	FLinearColor EmissionColor = FLinearColor(1.0, 1.0, 1.0, 1.0);

	// Blackbody emission for fire. Set to 1 for physically accurate intensity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlackbodyIntensity = 1.0;

	// Color tint for blackbody emission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume")
	FLinearColor BlackbodyTint = FLinearColor(1.0, 1.0, 1.0, 1.0);

	// Temperature in kelvin for blackbody emission, higher values emit more.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|Principled Volume", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "6500", UIMax = "6500"))
	float Temperature = 1500.0;


	//----------------------------------------------------------------------------
	// Debug options (by order of priority)
	//----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|DebugDisplay")
	bool UseDirectionalLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|DebugDisplay")
	bool UseEnvironmentLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|DebugDisplay")
	bool DisplayBounds = false;

	//----------------------------------------------------------------------------

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const  override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual bool SupportsStaticLighting() const override { return false; }
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	void UpdateSceneProxy(uint32 FrameIndex);

private:

	UVdbAssetComponent* VdbAssets;
};
