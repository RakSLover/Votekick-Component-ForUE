
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoteKickComponent.generated.h"



UENUM(BlueprintType)
enum class EVoteStatus : uint8
{
    Inactive,
    InProgress,
    Passed,
    Failed
};

USTRUCT(BlueprintType)
struct FVoteKickData
{

    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    APlayerState* TargetPlayer;

    UPROPERTY(BlueprintReadOnly)
    APlayerState* InitiatorPlayer;

    UPROPERTY(BlueprintReadOnly)
    float StartTime;

    UPROPERTY(BlueprintReadOnly)
    float Duration;

    UPROPERTY(BlueprintReadOnly)
    TArray<APlayerState*> YesVotes;

    UPROPERTY(BlueprintReadOnly)
    TArray<APlayerState*> NoVotes;

    UPROPERTY(BlueprintReadOnly)
    EVoteStatus Status;

    FVoteKickData()
    {
        TargetPlayer = nullptr;
        InitiatorPlayer = nullptr;
        StartTime = 0.f;
        Duration = 0.f;
        Status = EVoteStatus::Inactive;
    }
};

// ���������� ���������
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoteStartedSignature, const FVoteKickData&, VoteData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVoteProgressSignature, const FVoteKickData&, VoteData, float, TimeRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVoteResultSignature, bool, bPassed, const FString&, Reason, APlayerController*, Target);



UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GAME_API UVoteKickComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UVoteKickComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


public:
    // ��������� �����������
    UPROPERTY(EditDefaultsOnly, Category = "Vote Settings")
    float VoteCooldown = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Vote Settings")
    float VoteDuration = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Vote Settings")
    float RequiredPercent = 0.33f;

    // �������� ��� UI
    UPROPERTY(BlueprintAssignable, Category = "Vote Events")
    FVoteStartedSignature OnVoteStarted;

    UPROPERTY(BlueprintAssignable, Category = "Vote Events")
    FVoteProgressSignature OnVoteProgress;

    UPROPERTY(BlueprintAssignable, Category = "Vote Events")
    FVoteResultSignature OnVoteResult;


    UFUNCTION(BlueprintCallable)
    void StartVoteKick(APlayerState* Initiator, APlayerState* Target);

  
    UFUNCTION(BlueprintCallable)
    void CastVote(APlayerState* Voter, bool bVoteYes);


    UFUNCTION(BlueprintPure, Category = "Vote System")
    FVoteKickData GetCurrentVoteData() const { return ActiveVote; }

    // ����� �� ������ �����������
    UFUNCTION(BlueprintPure, Category = "Vote System")
    bool CanStartVote(APlayerState* Initiator, APlayerState* Target) const;

private:
    // ������� �������� �����������
    UPROPERTY(ReplicatedUsing = OnRep_ActiveVote)
    FVoteKickData ActiveVote;

    // ����� ���������� �����������
    float LastVoteTime;

    // ������ �����������
    FTimerHandle VoteTimerHandle;

    // ���������� ������ �����������
    UFUNCTION()
    void OnRep_ActiveVote();

    // ���������� �����������
    void CompleteVote();

    // �������� ����� ���������� �������
    int32 GetTotalPlayers() const;

    // ������� ������
    void KickPlayer(APlayerState* PlayerToKick);

    // �������� ������� ����������
    FString GetVoteResultReason(bool bPassed) const;


};
