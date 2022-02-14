// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include  "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"


// Sets default values
AShooterCharacter::AShooterCharacter() :
	// Turn and Lookup
	BaseTurnRate(45.f),
	BaseLookupRate(45.f),
	// Relaxed?
	bIsRelaxed(true)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true; // Will be changed later
	CameraBoom->SocketOffset = FVector{ 0.f, 50.f, 50.f };

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false; // Change later

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator{ 0.f, 540.f, 0.f };

	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.f)
	{
		FRotator ControlRotation{ Controller->GetControlRotation() };
		FRotator YawRotation{ 0.f, ControlRotation.Yaw, 0.f };
		FVector ForwardDirection{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };

		AddMovementInput(ForwardDirection, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.f)
	{
		FRotator ControlRotation{ Controller->GetControlRotation() };
		FRotator YawRotation{ 0.f, ControlRotation.Yaw, 0.f };
		FVector ForwardDirection{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };

		AddMovementInput(ForwardDirection, Value);
	}
}

void AShooterCharacter::TurnRightAtRate(float Rate)
{
	AddControllerYawInput(BaseTurnRate * Rate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(BaseLookupRate * Rate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::FireWeapon()
{
	bIsRelaxed = false;

	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}

	const USkeletalMeshSocket* WeaponLSocket = GetMesh()->GetSocketByName("weapon_L_Socket");
	const USkeletalMeshSocket* WeaponRSocket = GetMesh()->GetSocketByName("weapon_R_Socket");

	if (WeaponLSocket && WeaponRSocket)
	{
		FTransform WeaponLTransform = WeaponLSocket->GetSocketTransform(GetMesh());
		FTransform WeaponRTransform = WeaponRSocket->GetSocketTransform(GetMesh());

		FVector MuzzleFlashLeftLocation{ 0.f, 0.f,0.f };
		FVector MuzzleFlashRightLocation{ 0.f, 0.f,0.f };

		if (GetCharacterMovement()->GetCurrentAcceleration().Size() <= 0.f)
		{
			MuzzleFlashLeftLocation = WeaponLTransform.GetLocation();
			MuzzleFlashRightLocation = WeaponRTransform.GetLocation();
		}
		else
		{
			MuzzleFlashLeftLocation = WeaponLTransform.GetLocation() + GetCharacterMovement()->Velocity / 20.f;
			MuzzleFlashRightLocation = WeaponRTransform.GetLocation() + GetCharacterMovement()->Velocity / 20.f;
		}

		// Fire Hit Test

		FHitResult FireLeftHit;
		FHitResult FireRightHit;

		// Left Weapon Socket Trace
		FVector LStart{ MuzzleFlashLeftLocation };
		FQuat LRotation{ WeaponLTransform.GetRotation() };
		FVector LRotationAxis{ LRotation.GetAxisX() };
		FVector LEnd{ LStart + LRotationAxis * 50'000.f };

		// Right Weapon Socket Trace
		FVector RStart{ MuzzleFlashRightLocation };
		FQuat RRotation{ WeaponRTransform.GetRotation() };
		FVector RRotationAxis{ RRotation.GetAxisX() };
		FVector REnd{ RStart + RRotationAxis * 50'000.f };
		
		FVector LBeamEnd{ LEnd };
		FVector RBeamEnd{ REnd };

		// Both Line Traces by socket
		GetWorld()->LineTraceSingleByChannel(
			FireLeftHit,
			LStart,
			LEnd,
			ECollisionChannel::ECC_Visibility
		);

		GetWorld()->LineTraceSingleByChannel(
			FireRightHit,
			RStart,
			REnd,
			ECollisionChannel::ECC_Visibility
		);

		// Spawn Muzzle Flash
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAttached(
				MuzzleFlash,
				RootComponent,
				NAME_None,
				MuzzleFlashLeftLocation,
				WeaponLTransform.GetRotation().Rotator(),
				EAttachLocation::KeepWorldPosition
			);
			UGameplayStatics::SpawnEmitterAttached(
				MuzzleFlash,
				RootComponent,
				NAME_None,
				MuzzleFlashRightLocation,
				WeaponRTransform.GetRotation().Rotator(),
				EAttachLocation::KeepWorldPosition
			);
		}

		// Spawn Impact Particles
		if (ImpactParticles)
		{
			if (FireLeftHit.bBlockingHit)
			{
				LEnd = FireLeftHit.Location;

				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					FireLeftHit.Location
				);
			}

			if (FireRightHit.bBlockingHit)
			{
				REnd = FireRightHit.Location;

				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					FireRightHit.Location
				);
			}
		}

		if (BeamParticles)
		{
			UParticleSystemComponent* LBeamParticles = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				BeamParticles,
				LStart
			);


			UParticleSystemComponent* RBeamParticles = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				BeamParticles,
				RStart
			);


			if (LBeamParticles && RBeamParticles)
			{
				LBeamParticles->SetVectorParameter("Target", LEnd);
				RBeamParticles->SetVectorParameter("Target", REnd);
			}
		}
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (AnimInstance && DoubleFireMontage)
	{
		AnimInstance->Montage_Play(DoubleFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartDoubleFire"));
	}

	GetWorldTimerManager().SetTimer(
		RelaxedTimerHandle,
		this,
		&AShooterCharacter::ResetRelaxedTimer,
		2.f // Fire -> Relax cooldown
	);
}

void AShooterCharacter::ResetRelaxedTimer()
{
	bIsRelaxed = true;
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);

	PlayerInputComponent->BindAxis("TurnRight", this, &AShooterCharacter::TurnRightAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUpAtRate);

	PlayerInputComponent->BindAxis("MouseTurn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("MouseLookUp", this, &AShooterCharacter::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::FireWeapon);
}

