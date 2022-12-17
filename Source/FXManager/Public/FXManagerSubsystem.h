// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FXTypes.h"
#include "UObject/NoExportTypes.h"
#include "FXManagerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class FXMANAGER_API UFXManagerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()


	// Begin Subsystem Overrides
public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// End Subsystem Overrides

private:

	/* What our next Id should be for any active pack */
	int Internal_NextId = 0;

	/* Array of active effects we are managing */
	TArray<FActiveEffectPack> ActiveEffectPacks;

	/* Array of internal effect packs that will be removed on the next tick */
	TArray<FActiveEffectPack> InstantEffectPacks;

	FTimerHandle InstantPackTimerHandle;

	/* Maps attachment rules to attach location types */
	static const TMap<EAttachmentRule, EAttachLocation::Type> AttachmentMap;

public:

	/* Returns a pointer to our instanced FX manager subsystem, nullptr if the global engine pointer is invalid */
	static UFXManagerSubsystem* GetFXManager();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX Manager")
	FActiveEffectPackHandle PlayEffectAtLocation(AActor* SourceActor, AActor* TargetActor,
		const FEffectPack& EffectPack, EEffectActivationType ActivationType = EEffectActivationType::Instant,
		FTransform Transform = FTransform());

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX Manager")
	FActiveEffectPackHandle PlayEffectAttached(AActor* SourceActor, AActor* TargetActor,
		USceneComponent* AttachComponent, const FEffectPack& EffectPack,
		EEffectActivationType ActivationType = EEffectActivationType::Instant);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX Manager")
	void StopActivePack(const FActiveEffectPackHandle& Handle);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX MAnager")
	void StopActivePacks(const TArray<FActiveEffectPackHandle>& Handles);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX Manager")
	UFXSystemComponent* GetVfxSystemComponentByTag(const FActiveEffectPackHandle& Handle, FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "FX Manager")
	UAudioComponent* GetSfxSystemComponentByTag(const FActiveEffectPackHandle& Handle, FGameplayTag Tag);

private:

	UFXSystemComponent* SpawnVFXDataAtLocation(const FVFXData VFXData, const AActor* SourceActor, const FTransform& Transform) const;

	UAudioComponent* SpawnSFXDataAtLocation(const FSFXData SFXData, const AActor* SourceActor, const FTransform& Transform) const;

	UFXSystemComponent* SpawnVFXDataAtComponent(const FVFXData VFXData, const AActor* SourceActor, USceneComponent* AttachComponent) const;

	UAudioComponent* SpawnSFXDataAtComponent(const FSFXData SFXData, const AActor* SourceActor, USceneComponent* AttachComponent) const;

	FActiveEffectPack& GetActivePack(const FActiveEffectPackHandle& Handle);

	/* Adds packs with an instant activation for the next tick so that they can be modified if necessary */
	void AddInstantPack(const UObject* WorldContextObject, const FActiveEffectPack& ActivePack);

	void ClearInstantPacks();

	/* Increments our internal id counter and returns the next id */
	int GenerateNewActivePackId();

	/* Returns actor tags from the IGameplayTagInterface, if implemented by the passed in actor */
	FGameplayTagContainer GetActorTags(const AActor* Actor) const;

	/* Converts an attachment rule to the needed attach location type enumerator */
	static EAttachLocation::Type GetAttachLocationType(const EAttachmentRule& Rule);

	template<typename Predicate>
	UFXSystemComponent* Internal_GetVfxSystemComponent(const FActiveEffectPackHandle& Handle, Predicate Pred)
	{
		const FActiveEffectPack& Pack = GetActivePack(Handle);
		if(!Pack.IsValid())
		{
			return nullptr;
		}

		// Try finding an active effect that matches our tag
		// Return the object within our found active effect if it exists, otherwise a null pointer
		if(const FActiveEffect<UFXSystemComponent*>* FoundValue = Pack.ActiveFXSystemComponents.FindByPredicate(Pred))
		{
			return (*FoundValue).Object;
		}

		return nullptr;
	}

	template<typename Predicate>
	UAudioComponent* Internal_FindSfxSystemComponent(const FActiveEffectPackHandle& Handle,
	Predicate Pred)
	{
		const FActiveEffectPack& Pack = GetActivePack(Handle);
		if(!Pack.IsValid())
		{
			return nullptr;
		}

		// Try finding an active effect that matches our tag
		// Return the object within our found active effect if it exists, otherwise a null pointer
		if(const FActiveEffect<UAudioComponent*>* FoundValue = Pack.ActiveSoundComponents.FindByPredicate(Pred))
		{
			return (*FoundValue).Object;
		}

		return nullptr;
	}
};
