
#include "VoteKick/VoteKickComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"


UVoteKickComponent::UVoteKickComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

}



void UVoteKickComponent::BeginPlay()
{
	Super::BeginPlay();
    LastVoteTime = -VoteCooldown;
}

void UVoteKickComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVoteKickComponent, ActiveVote);
}


void UVoteKickComponent::StartVoteKick(APlayerState* Initiator, APlayerState* Target)
{

    if (!CanStartVote(Initiator, Target))
        return;

    // Создаем новое голосование
    ActiveVote = FVoteKickData();
    ActiveVote.TargetPlayer = Target;
    ActiveVote.InitiatorPlayer = Initiator;
    ActiveVote.StartTime = GetWorld()->GetTimeSeconds();
    ActiveVote.Duration = VoteDuration;
    ActiveVote.Status = EVoteStatus::InProgress;

    LastVoteTime = GetWorld()->GetTimeSeconds();


    // Инициатор голосует ЗА
    if (Initiator && !ActiveVote.YesVotes.Contains(Initiator))
    {
        ActiveVote.YesVotes.Add(Initiator);
    }
    // Цель голосует ПРОТИВ
    if (Target && !ActiveVote.NoVotes.Contains(Target))
    {
        ActiveVote.NoVotes.Add(Target);
    }


    // Запускаем таймер
    GetWorld()->GetTimerManager().SetTimer(
        VoteTimerHandle,
        this,
        &UVoteKickComponent::CompleteVote,
        VoteDuration,
        false
    );

    // Уведомляем клиентов
    OnVoteStarted.Broadcast(ActiveVote);
}

bool UVoteKickComponent::CanStartVote(APlayerState* Initiator, APlayerState* Target) const
{
    if (!Initiator || !Target)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("votekick: not valid initiator or target"));
        return false;
    }
    if (ActiveVote.Status == EVoteStatus::InProgress)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("votekick: in progress"));
        return false;
    }
    // Проверка кд между голосованиями
    if (GetWorld()->GetTimeSeconds() - LastVoteTime < VoteCooldown)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("votekick: in cooldown"));
        return false;
    }

    if (ActiveVote.TargetPlayer == Target)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("votekick: has vote"));
        return false;
    }
        

    // Нельзя против себя
    if (Initiator == Target)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("votekick: initiator = target"));
        return false;

    }

    return true;
}

void UVoteKickComponent::CastVote(APlayerState* Voter, bool bVoteYes)
{



    // Проверяем, что голосование еще активно и указатели валидны
    if (ActiveVote.Status != EVoteStatus::InProgress || !Voter || !IsValid(Voter))
        return;

    // Проверяем, что целевой игрок и инициатор еще валидны
    if (!IsValid(ActiveVote.TargetPlayer) || !IsValid(ActiveVote.InitiatorPlayer))
    {
        // Если кто-то из ключевых игроков отключился, отменяем голосование
        CompleteVote();
        return;
    }


    if (ActiveVote.Status != EVoteStatus::InProgress || !Voter)
        return;

    // Инициатор и цель не могут менять свои голоса
    if (Voter == ActiveVote.InitiatorPlayer || Voter == ActiveVote.TargetPlayer)
        return;

    // Проверяем, не голосовал ли уже
    if (ActiveVote.YesVotes.Contains(Voter) || ActiveVote.NoVotes.Contains(Voter))
        return;

    if (bVoteYes)
        ActiveVote.YesVotes.Add(Voter);
    else
        ActiveVote.NoVotes.Add(Voter);

    // Обновляем прогресс
    float TimeRemaining = FMath::Max(0.f, ActiveVote.Duration - (GetWorld()->GetTimeSeconds() - ActiveVote.StartTime));
    OnVoteProgress.Broadcast(ActiveVote, TimeRemaining);
}

void UVoteKickComponent::OnRep_ActiveVote()
{
    if (ActiveVote.Status == EVoteStatus::InProgress)
    {
        OnVoteStarted.Broadcast(ActiveVote);
    }
}

void UVoteKickComponent::CompleteVote()
{
    int32 TotalPlayers = GetTotalPlayers();
    int32 VotedYes = ActiveVote.YesVotes.Num();
    int32 VotedNo = ActiveVote.NoVotes.Num();
    int32 TotalVoted = VotedYes + VotedNo;

    bool bPassed = false;

    if (TotalVoted > 0)
    {
        float YesPercentage = (float)VotedYes / TotalPlayers;
        bPassed = YesPercentage >= RequiredPercent;
    }

    ActiveVote.Status = bPassed ? EVoteStatus::Passed : EVoteStatus::Failed;

    if (bPassed && ActiveVote.TargetPlayer)
    {
        KickPlayer(ActiveVote.TargetPlayer);
    }

    APlayerController* TargetController = ActiveVote.TargetPlayer->GetPlayerController();

    FString Reason = GetVoteResultReason(bPassed);
    OnVoteResult.Broadcast(bPassed, Reason, TargetController);

    // Очищаем таймер
    GetWorld()->GetTimerManager().ClearTimer(VoteTimerHandle);


    // Очищаем массивы голосов
    ActiveVote.YesVotes.Empty();
    ActiveVote.NoVotes.Empty();

    // Сбрасываем указатели на игроков
    ActiveVote.TargetPlayer = nullptr;
    ActiveVote.InitiatorPlayer = nullptr;

    // Сбрасываем статус
    ActiveVote.Status = EVoteStatus::Inactive;

    // Сбрасываем время и длительность
    ActiveVote.StartTime = 0.f;
    ActiveVote.Duration = 0.f;


}

int32 UVoteKickComponent::GetTotalPlayers() const
{
    if (AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner()))
    {
        return GameState->PlayerArray.Num();
    }
    return 0;
}

void UVoteKickComponent::KickPlayer(APlayerState* PlayerToKick)
{
    if (PlayerToKick && PlayerToKick->GetOwner())
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(PlayerToKick->GetOwner()))
        {
            FString KickReason = TEXT("Kicked by vote");
            PlayerController->ClientWasKicked(FText::FromString(KickReason));
        }
    }
}

FString UVoteKickComponent::GetVoteResultReason(bool bPassed) const
{
    if (bPassed)
    {
        return FString::Printf(TEXT("Vote passed (%d for, %d against)"), ActiveVote.YesVotes.Num(), ActiveVote.NoVotes.Num());
    }
    else
    {
        if (ActiveVote.YesVotes.Num() + ActiveVote.NoVotes.Num() == 0)
            return TEXT("Vote failed - no one voted");

        else
            FString::Printf(TEXT("Vote failed (%d for, %d against, needed %.0f%%)"), ActiveVote.YesVotes.Num(), ActiveVote.NoVotes.Num(), RequiredPercent * 100);

    }
}
