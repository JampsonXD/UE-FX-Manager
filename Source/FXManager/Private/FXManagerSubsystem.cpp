// Fill out your copyright notice in the Description page of Project Settings.


#include "FXManagerSubsystem.h"
#include "GameplayTagAssetInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"


const TMap<EAttachmentRule, EAttachLocation::Type> UFXManagerSubsystem::AttachmentMap =
{
	{EAttachmentRule::SnapToTarget, EAttachLocation::Type::SnapToTarget},
	{EAttachmentRule::KeepRelative, EAttachLocation::Type::KeepRelativeOffset},
	{EAttachmentRule::KeepWorld, EAttachLocation::Type::KeepWorldPosition}
};

bool UFXManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UFXManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UFXManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

UFXManagerSubsystem* UFXManagerSubsystem::GetFXManager()
{
	if(GEngine)
	{
		return GEngine->GetEngineSubsystem<UFXManagerSubsystem>();
	}

	return nullptr;
}

FActiveEffectPackHandle UFXManagerSubsystem::PlayEffectAtLocation(AActor* SourceActor, AActor* TargetActor,
                                                         const FEffectPack& EffectPack, EEffectActivationType ActivationType, FTransform Transform)
{
	if(!EffectPack.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Effect Pack is Empty."))
		return FActiveEffectPackHandle();
	}

	if(!SourceActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Source Actor is invalid!"))
		return FActiveEffectPackHandle();
	}

	FActiveEffectPack ActivePack = FActiveEffectPack(GenerateNewActivePackId(), SourceActor, TargetActor, nullptr, ActivationType);

	const FGameplayTagContainer SourceTags = GetActorTags(SourceActor);
	const FGameplayTagContainer TargetTags = GetActorTags(TargetActor);

	for(const FVFXData& VfxData : EffectPack.VFXData)
	{
		/* Go to the next effect data if this one is unable to play */
		if(!VfxData.CanPlay(SourceTags, TargetTags))
		{
			continue;
		}

		ActivePack.AddActiveVFX(SpawnVFXDataAtLocation(VfxData, SourceActor, Transform), VfxData.AccessTag);
	}

	for(const FSFXData& SfxData : EffectPack.SFXData)
	{
		/* Go to the next effect data if this one is unable to play */
		if (!SfxData.CanPlay(SourceTags, TargetTags))
		{
			continue;
		}

		ActivePack.AddActiveSound(SpawnSFXDataAtLocation(SfxData, SourceActor, Transform), SfxData.AccessTag);
	}

	if(!ActivePack.IsActive())
	{
		return FActiveEffectPackHandle();
	}

	ActivationType == EEffectActivationType::Active ? ActiveEffectPacks.Add(ActivePack) : AddInstantPack(SourceActor, ActivePack);
	return ActivePack.CreateHandle();
}

FActiveEffectPackHandle UFXManagerSubsystem::PlayEffectAttached(AActor* SourceActor, AActor* TargetActor,
	USceneComponent* AttachComponent, const FEffectPack& EffectPack, EEffectActivationType ActivationType)
{
	if (!EffectPack.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Effect Pack is Empty."))
			return FActiveEffectPackHandle();
	}

	if (!SourceActor || !AttachComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Source Actor or Attach Component is invalid!"))
			return FActiveEffectPackHandle();
	}

	FActiveEffectPack ActivePack = FActiveEffectPack(GenerateNewActivePackId(), SourceActor, TargetActor, AttachComponent, ActivationType);

	const FGameplayTagContainer SourceTags = GetActorTags(SourceActor);
	const FGameplayTagContainer TargetTags = GetActorTags(TargetActor);

	for (const FVFXData& VfxData : EffectPack.VFXData)
	{
		/* Go to the next effect data if this one is unable to play */
		if (!VfxData.CanPlay(SourceTags, TargetTags))
		{
			continue;
		}

		ActivePack.AddActiveVFX(SpawnVFXDataAtComponent(VfxData, SourceActor, AttachComponent), VfxData.AccessTag);
	}

	for (const FSFXData& SfxData : EffectPack.SFXData)
	{
		/* Go to the next effect data if this one is unable to play */
		if (!SfxData.CanPlay(SourceTags, TargetTags))
		{
			continue;
		}

		ActivePack.AddActiveSound(SpawnSFXDataAtComponent(SfxData, SourceActor, AttachComponent), SfxData.AccessTag);
	}

	if (!ActivePack.IsActive())
	{
		return FActiveEffectPackHandle();
	}

	ActivationType == EEffectActivationType::Active ? ActiveEffectPacks.Add(ActivePack) : AddInstantPack(SourceActor, ActivePack);
	return ActivePack.CreateHandle();
}

void UFXManagerSubsystem::StopActivePack(const FActiveEffectPackHandle& Handle)
{
	for(auto Iterator = ActiveEffectPacks.CreateIterator(); Iterator; ++Iterator)
	{
		FActiveEffectPack& Pack = ActiveEffectPacks[Iterator.GetIndex()];
		if(Pack.Id == Handle.GetId())
		{
			Pack.Invalidate();
			Iterator.RemoveCurrent();
			return;
		}
	}
}

void UFXManagerSubsystem::StopActivePacks(const TArray<FActiveEffectPackHandle>& Handles)
{
	/* Cache all Handle Id's using a Set, this keeps us from having to iterate over our active effect pack
	 * array for each handle, O(n) instead of O(n^2) */
	TSet<int> PackIdsToRemove;

	// Iterate through all our active effect packs and cache their Id's
	for(int i = 0; i < Handles.Num(); ++i)
	{
		const FActiveEffectPackHandle& Handle = Handles[i];
		PackIdsToRemove.Add(Handle.GetId());
	}

	// Check if the handles Id was in our cached set, invalidate the pack, and remove the current index
	for(auto Iterator = ActiveEffectPacks.CreateIterator(); Iterator; ++Iterator)
	{
		FActiveEffectPack& Pack = ActiveEffectPacks[Iterator.GetIndex()];
		if(PackIdsToRemove.Contains(Pack.Id))
		{
			Pack.Invalidate();
			Iterator.RemoveCurrent();
		}
	}
}

UFXSystemComponent* UFXManagerSubsystem::GetVfxSystemComponentByTag(const FActiveEffectPackHandle& Handle,
	FGameplayTag Tag)
{
	return Internal_GetVfxSystemComponent(Handle, [Tag](const FActiveEffect<UFXSystemComponent*>& ActiveEffect)
	{
		return ActiveEffect.AccessTag == Tag;
	});
}

UAudioComponent* UFXManagerSubsystem::GetSfxSystemComponentByTag(const FActiveEffectPackHandle& Handle,
	FGameplayTag Tag)
{
	return Internal_FindSfxSystemComponent(Handle, [Tag](const FActiveEffect<UAudioComponent*>& ActiveEffect)
	{
		return ActiveEffect.AccessTag == Tag;
	});
}

UFXSystemComponent* UFXManagerSubsystem::SpawnVFXDataAtLocation(const FVFXData VFXData, const AActor* SourceActor, const FTransform& Transform) const
{

	UFXSystemAsset* Asset = VFXData.ParticleSystem;
	if(!Asset)
	{
		return nullptr;
	}

	const FVector Location = Transform.GetLocation() + VFXData.AttachmentData.RelativeTransform.GetLocation();
	const FRotator Rotation = FRotator(Transform.GetRotation() + VFXData.AttachmentData.RelativeTransform.GetRotation());
	const FVector Scale = Transform.GetScale3D() * VFXData.AttachmentData.RelativeTransform.GetScale3D();

	if(UParticleSystem* Cascade = Cast<UParticleSystem>(Asset))
	{
		return UGameplayStatics::SpawnEmitterAtLocation
		(SourceActor, Cascade, Location, Rotation, Scale, true);
	}

	if(UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Asset))
	{
		return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAtLocation(SourceActor, Niagara, 
			Location, Rotation, Scale, true, true));
	}

	return nullptr;
}

UAudioComponent* UFXManagerSubsystem::SpawnSFXDataAtLocation(const FSFXData SFXData, const AActor* SourceActor, const FTransform& Transform) const
{

	USoundBase* Asset = SFXData.Sound;
	if(!Asset)
	{
		return nullptr;
	}

	if(SFXData.AudioType == EAudioType::TwoDimensional)
	{
		return UGameplayStatics::SpawnSound2D(SourceActor, Asset);
	}

	if(SFXData.AudioType == EAudioType::ThreeDimensional)
	{
		const FVector Location = Transform.GetLocation() + SFXData.AttachmentData.RelativeTransform.GetLocation();
		const FRotator Rotation = FRotator(Transform.GetRotation() + SFXData.AttachmentData.RelativeTransform.GetRotation());
		return UGameplayStatics::SpawnSoundAtLocation(SourceActor, Asset, Location, Rotation);
	}

	return nullptr;
}

UFXSystemComponent* UFXManagerSubsystem::SpawnVFXDataAtComponent(const FVFXData VFXData, const AActor* SourceActor,
	USceneComponent* AttachComponent) const
{

	UFXSystemAsset* Asset = VFXData.ParticleSystem;
	if (!Asset)
	{
		return nullptr;
	}

	/* If our attach type is at socket location, return our effect at location instead of trying to attach */
	if(VFXData.AttachmentData.AttachType == EAttachType::AtSocketLocation)
	{
		return SpawnVFXDataAtLocation(VFXData, SourceActor, AttachComponent->GetSocketTransform(VFXData.AttachmentData.SocketName));
	}

	const FTransform RelativeTransform = VFXData.GetRelativeTransform();
	const EAttachLocation::Type AttachRule = GetAttachLocationType(VFXData.AttachmentData.AttachmentRule);
	

	if (UParticleSystem* Cascade = Cast<UParticleSystem>(Asset))
	{
		return UGameplayStatics::SpawnEmitterAttached(Cascade, AttachComponent, VFXData.AttachmentData.SocketName, RelativeTransform.GetLocation(),
			FRotator(RelativeTransform.GetRotation()), RelativeTransform.GetScale3D(), AttachRule, true, EPSCPoolMethod::AutoRelease, true);
	}

	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Asset))
	{
		return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAttached(Niagara, AttachComponent, VFXData.AttachmentData.SocketName,
			RelativeTransform.GetLocation(), FRotator(RelativeTransform.GetRotation()),
			AttachRule, true, true, ENCPoolMethod::AutoRelease));
	}

	return nullptr;
}

UAudioComponent* UFXManagerSubsystem::SpawnSFXDataAtComponent(const FSFXData SFXData, const AActor* SourceActor,
	USceneComponent* AttachComponent) const
{

	USoundBase* Asset = SFXData.Sound;
	if (!Asset)
	{
		return nullptr;
	}

	/* If our attach type is at socket location or we are playing a generic two dimensional sound, play it at location instead of attached */
	if (SFXData.AttachmentData.AttachType == EAttachType::AtSocketLocation || SFXData.AudioType == EAudioType::TwoDimensional)
	{
		return SpawnSFXDataAtLocation(SFXData, SourceActor, AttachComponent->GetSocketTransform(SFXData.AttachmentData.SocketName));
	}

	const FTransform RelativeTransform = SFXData.GetRelativeTransform();
	const EAttachLocation::Type AttachRule = GetAttachLocationType(SFXData.AttachmentData.AttachmentRule);

	return UGameplayStatics::SpawnSoundAttached(Asset, AttachComponent, SFXData.AttachmentData.SocketName,
		RelativeTransform.GetLocation(), FRotator(RelativeTransform.GetRotation()), AttachRule, false);

}


FActiveEffectPack& UFXManagerSubsystem::GetActivePack(const FActiveEffectPackHandle& Handle)
{
	FActiveEffectPack TempPack;
	if(!Handle.IsValid())
	{
		return TempPack;
	}
	
	/* Get our active or instant pack array based on the activation data from our handle */
	TArray<FActiveEffectPack>& Array = Handle.GetPackType() == EEffectActivationType::Active ? ActiveEffectPacks : InstantEffectPacks;
	for (auto Iterator = Array.CreateConstIterator(); Iterator; ++Iterator)
	{
		FActiveEffectPack& Pack = Array[Iterator.GetIndex()];
		if (Pack.Id == Handle.GetId())
		{
			return Pack;
		}
	}
	
	return TempPack;
}

void UFXManagerSubsystem::AddInstantPack(const UObject* WorldContextObject, const FActiveEffectPack& ActivePack)
{
	InstantEffectPacks.Add(ActivePack);
	if(!WorldContextObject->GetWorld()->GetTimerManager().IsTimerActive(InstantPackTimerHandle))
	{
		InstantPackTimerHandle = WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UFXManagerSubsystem::ClearInstantPacks);
	}
}

void UFXManagerSubsystem::ClearInstantPacks()
{
	InstantEffectPacks.Empty();
}

int UFXManagerSubsystem::GenerateNewActivePackId()
{
	return Internal_NextId++;
}

FGameplayTagContainer UFXManagerSubsystem::GetActorTags(const AActor* Actor) const
{
	FGameplayTagContainer Container = FGameplayTagContainer::EmptyContainer;

	if(const IGameplayTagAssetInterface* Interface = Cast<IGameplayTagAssetInterface>(Actor))
	{
		Interface->GetOwnedGameplayTags(Container);
	}

	return Container;
}

EAttachLocation::Type UFXManagerSubsystem::GetAttachLocationType(const EAttachmentRule& Rule)
{
	return AttachmentMap.FindRef(Rule);
}
