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
#include "VdbCommon.h"

#include "VdbMaterialComponent.generated.h"

class UVdbAssetComponent;

UCLASS(Blueprintable)
class UVdbMaterialComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UVdbMaterialComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual bool SupportsStaticLighting() const override { return false; }
	//~ End UPrimitiveComponent Interface.

	// Must be a Volume domain material.
	UPROPERTY(EditAnywhere, Category = Volume)
	TObjectPtr<class UMaterialInterface> Material;

	// Max number of ray bounces
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRayDepth = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Volume, meta = (ClampMin = "1", UIMin = "1"))
	int32 SamplesPerPixel = 1;

	// Raymarching step distance, in local space. The smaller the more accurate, but also the more expensive. Only use small values 
	// to capture small missing features. It is recommended to keep this multiplier as high as possible for better performance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume|FogVolume", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float LocalStepSize = 4.f;

	// Shadow raymarching step distance multiplier. It represents a multiple of LocalStepSize.
	// It is recommended to keep this multiplier as high as possible for better performance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume|FogVolume", meta = (UIMin = "1.0", ClampMin = "1.0"))
	float ShadowStepSizeMultiplier = 5.f;

	// Describes the probability of scattering (versus absorption) at a scattering event. Between 0 and 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|FogVolume", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Albedo = 0.8;

	// Backward or forward scattering direction (aka directional bias).
	// The default value of zero gives isotropic scattering so that light is scattered evenly in all directions. 
	// Positive values bias the scattering effect forwards, in the direction of the light, while negative values bias the scattering backward, toward the light. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|FogVolume", meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "1.0", UIMax = "1.0"))
	float Anisotropy = 0.0;

	// Temperature in kelvin for blackbody emission, higher values emit more.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|FogVolume", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6500"))
	float BlackbodyTemperature = 1500.0;

	// Blackbody emission for fire. Set to 1 for physically accurate intensity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume|FogVolume", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlackbodyIntensity = 1.0;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const  override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	void UpdateSceneProxy(uint32 FrameIndex);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VdbAssetComponent"), Category = Volume)
	void SetVdbAssets(UVdbAssetComponent* Comp);

	private:
		UVdbAssetComponent* VdbAssets;
};
