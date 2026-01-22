// HealthBarWidget.h - Health Bar Widget Header
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HealthBarWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAGISTERKABKONKEL_API UHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UHealthBarWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;

public:
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* HealthProgressBar;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* HealthText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar Settings")
    bool bShowHealthText = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar Settings")
    FLinearColor HealthyColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar Settings")
    FLinearColor DamagedColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar Settings")
    FLinearColor CriticalColor = FLinearColor::Red;

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    void UpdateHealthBar(float HealthPercentage, int32 CurrentHealth, int32 MaxHealth);

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    void SetHealthBarColor(FLinearColor NewColor);

    UFUNCTION(BlueprintCallable, Category = "Health Bar")
    void SetShowHealthText(bool bShow);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health Bar")
    FLinearColor GetHealthColor(float HealthPercentage) const;

protected:
    
};