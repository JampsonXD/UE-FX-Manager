// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "FXTypes.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EAttachType : uint8
{
	AtSocketLocation,
	AttachToSocket
};

UENUM(BlueprintType)
enum class EAudioType : uint8
{
	TwoDimensional,
	ThreeDimensional
};

USTRUCT(BlueprintType)
struct FAttachData
{
	GENERATED_BODY()

	FAttachData()
	{
		RelativeTransform = FTransform::Identity;
		AttachType = EAttachType::AtSocketLocation;
		SocketName = "root";
		AttachmentRule = EAttachmentRule::SnapToTarget;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FTransform RelativeTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TEnumAsByte<EAttachType> AttachType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SocketName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "AttachType == EAttachType::AttachToSocket", EditConditionHides))
	TEnumAsByte<EAttachmentRule> AttachmentRule;
	
};

/* Contains Tag Requirements for both a Source and Target Actor */
USTRUCT(BlueprintType)
struct FTagRequirements
{
	GENERATED_BODY()

	FTagRequirements()
	{
		SourceRequiredTags = FGameplayTagContainer::EmptyContainer;
		TargetRequiredTags = FGameplayTagContainer::EmptyContainer;
		SourceBlockingTags = FGameplayTagContainer::EmptyContainer;
		TargetBlockingTags = FGameplayTagContainer::EmptyContainer;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer SourceRequiredTags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer SourceBlockingTags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer TargetRequiredTags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer TargetBlockingTags;

	/* Checks if passed in owning tags meet passed in required and blocking tag requirements  */
	bool MeetsTagCriteria(const FGameplayTagContainer& OwningTags, const FGameplayTagContainer& RequiredToCheckAgainst, const FGameplayTagContainer& BlockingToCheckAgainst) const
	{
		bool bMeetsRequired = true;
		bool bMeetsBlocking = true;

		if(!SourceRequiredTags.IsEmpty())
		{
			bMeetsRequired = OwningTags.HasAllExact(RequiredToCheckAgainst);
		}

		if(!SourceBlockingTags.IsEmpty())
		{
			bMeetsBlocking = !OwningTags.HasAnyExact(BlockingToCheckAgainst);
		}

		return bMeetsRequired && bMeetsBlocking;
	}

	/* Helper functions for testing source and target tags against our tag criteria */
	bool MeetsSourceTagCriteria(const FGameplayTagContainer& SourceTags) const { return MeetsTagCriteria(SourceTags, SourceRequiredTags, SourceBlockingTags); }

	bool MeetsTargetTagCriteria(const FGameplayTagContainer& TargetTags) const { return MeetsTagCriteria(TargetTags, TargetRequiredTags, TargetBlockingTags); }

	bool MeetsSourceAndTargetCriteria(const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags) const
	{ return MeetsSourceTagCriteria(SourceTags) && MeetsTargetTagCriteria(TargetTags); }
};

USTRUCT(BlueprintType)
struct FFXData
{
	GENERATED_BODY()

	virtual ~FFXData() = default;

	// Tag that can be used to access the spawned FFXData once the FX Manager spawns the Effect
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Categories = "Effect"))
	FGameplayTag AccessTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAttachData AttachmentData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FTagRequirements TagRequirements;

	virtual FTransform GetRelativeTransform() const { return AttachmentData.RelativeTransform; }

	virtual bool CanPlay(const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags) const
	{
		return TagRequirements.MeetsSourceAndTargetCriteria(SourceTags, TargetTags);
	}
};

USTRUCT(BlueprintType)
struct FVFXData : public FFXData
{
	GENERATED_BODY()

	FVFXData()
	{
		ParticleSystem = nullptr;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UFXSystemAsset* ParticleSystem;

};

USTRUCT(BlueprintType)
struct FSFXData : public FFXData
{
	GENERATED_BODY()

	FSFXData()
	{
		Sound = nullptr;
		AudioType = EAudioType::TwoDimensional;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* Sound;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TEnumAsByte<EAudioType> AudioType;
};

USTRUCT(BlueprintType)
struct FEffectPack
{
	GENERATED_BODY()
	virtual ~FEffectPack() = default;

	FEffectPack()
	{

	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FVFXData> VFXData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FSFXData> SFXData;

	virtual bool HasSFX() const { return SFXData.Num() > 0; }
	virtual bool HasVFX() const { return VFXData.Num() > 0; }

	/* Effect pack is valid if we have any Sound Effects or Visual Effects*/
	virtual bool IsValid() const { return HasSFX() || HasVFX(); }

};

UENUM(BlueprintType)
enum class EEffectActivationType : uint8
{
	None,
	Instant,
	Active
};

USTRUCT(BlueprintType)
struct FActiveEffectPackHandle
{
	GENERATED_BODY()

	FActiveEffectPackHandle(): Id(-1), ActivationType(EEffectActivationType::None) {}

	FActiveEffectPackHandle(int InId, EEffectActivationType InActivationType)
	{
		Id = InId;
		ActivationType = InActivationType;
	}

	int GetId() const { return Id; }

	EEffectActivationType GetPackType() const { return ActivationType; }

	bool IsValid() const { return Id != -1; }

private:

	int Id;

	EEffectActivationType ActivationType;
};

template<class T>
struct FActiveEffect
{
	FActiveEffect(T InObject, FGameplayTag InTag)
	{
		Object = InObject;
		AccessTag = InTag;
	}
	
	FGameplayTag AccessTag;
	T Object;
	
};

USTRUCT()
struct FActiveEffectPack
{
	GENERATED_BODY()

	FActiveEffectPack()
	{
		Id = -1;
		SourceActor = nullptr;
		TargetActor = nullptr;
		AttachComponent = nullptr;
		ActivationType = EEffectActivationType::None;
	}

	FActiveEffectPack(int InId, AActor* InSourceActor, AActor* InTargetActor, USceneComponent* InAttachComponent, EEffectActivationType InActivationType)
	{
		Id = InId;
		ActivationType = InActivationType;
		SourceActor = InSourceActor;
		TargetActor = InTargetActor;
		AttachComponent = InAttachComponent;
	}

	bool operator==(const FActiveEffectPackHandle& Other) const { return Id == Other.GetId(); }
	bool operator!=(const FActiveEffectPackHandle& Other) const { return !(*this == Other); }

	int Id;
	EEffectActivationType ActivationType;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<USceneComponent> AttachComponent;
	TArray<FActiveEffect<UFXSystemComponent*>> ActiveFXSystemComponents;
	TArray<FActiveEffect<UAudioComponent*>> ActiveSoundComponents;

	void AddActiveVFX(UFXSystemComponent* VFX, FGameplayTag AccessTag) { ActiveFXSystemComponents.Add( FActiveEffect(VFX, AccessTag)); }
	void AddActiveSound(UAudioComponent* Sound, FGameplayTag AccessTag) { ActiveSoundComponents.Add( FActiveEffect(Sound, AccessTag)); }

	bool HasVFX() const { return ActiveFXSystemComponents.Num() > 0; }
	bool HasSFX() const { return ActiveSoundComponents.Num() > 0; }
	bool IsValid() const { return Id > -1; }
	bool IsActive() const { return IsValid() && (HasSFX() || HasVFX()); }

	/* Creates a handle to this active effect using its id */
	FActiveEffectPackHandle CreateHandle() const { return FActiveEffectPackHandle(Id, ActivationType); }

	void Invalidate()
	{
		DeactivateFXSystems();
		DeactivateSFXSystems();
	}

	void DeactivateFXSystems()
	{
		if (ActiveFXSystemComponents.IsEmpty()) return;

		for(const FActiveEffect<UFXSystemComponent*>& Effect : ActiveFXSystemComponents)
		{
			if(Effect.Object)
			{
				Effect.Object->Deactivate();
			}
		}

		ActiveFXSystemComponents.Empty();
	}

	void DeactivateSFXSystems()
	{
		if (ActiveSoundComponents.IsEmpty()) return;

		for(const FActiveEffect<UAudioComponent*>& Effect: ActiveSoundComponents)
		{
			if(Effect.Object)
			{
				Effect.Object->Deactivate();
			}
		}

		ActiveSoundComponents.Empty();
	}
};
