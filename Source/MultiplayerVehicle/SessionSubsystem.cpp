// Fill out your copyright notice in the Description page of Project Settings.

#include "SessionSubsystem.h"

#include "GameMapsSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"

const FName USessionSubsystem::GameKeyName = TEXT("GAME_KEY");
const FString USessionSubsystem::GameKeyValue = TEXT("MultiplayerVehicle");

void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
	{
		SessionInterface = OnlineSub->GetSessionInterface();
		UE_LOG(LogTemp, Log, TEXT("SessionSubsystem using online subsystem: %s"), *OnlineSub->GetSubsystemName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SessionSubsystem: no online subsystem available"));
	}

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &USessionSubsystem::OnNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &USessionSubsystem::OnTravelFailure);
	}
}

void USessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	SessionInterface.Reset();
	SessionSearch.Reset();
	Super::Deinitialize();
}

void USessionSubsystem::HostSession(int32 MaxPlayers, const FString& MapName)
{
	if (!SessionInterface.IsValid())
	{
		ShowMessage(TEXT("Host failed: no online subsystem"), FColor::Red);
		return;
	}

	PendingMaxPlayers = MaxPlayers;
	PendingMapName = MapName;

	// Only one session per name can exist, so a leftover one has to be destroyed first.
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		bCreateAfterDestroy = true;
		DestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionComplete));
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	CreateSessionInternal();
}

void USessionSubsystem::CreateSessionInternal()
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = PendingMaxPlayers;
	Settings.bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == NULL_SUBSYSTEM;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(GameKeyName, GameKeyValue, EOnlineDataAdvertisementType::ViaOnlineService);

	CreateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnCreateSessionComplete));

	if (!SessionInterface->CreateSession(0, NAME_GameSession, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		ShowMessage(TEXT("CreateSession failed to start"), FColor::Red);
	}
}

void USessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);

	if (bCreateAfterDestroy)
	{
		bCreateAfterDestroy = false;
		CreateSessionInternal();
	}
	else if (bFindAfterDestroy)
	{
		bFindAfterDestroy = false;
		FindAndJoinSession();
	}
}

void USessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);

	if (!bWasSuccessful)
	{
		ShowMessage(TEXT("Failed to create session"), FColor::Red);
		return;
	}

	ShowMessage(TEXT("Session created, opening map as listen server"), FColor::Green);
	UGameplayStatics::OpenLevel(this, FName(*PendingMapName), true, TEXT("listen"));
}

void USessionSubsystem::FindAndJoinSession()
{
	if (!SessionInterface.IsValid())
	{
		ShowMessage(TEXT("Join failed: no online subsystem"), FColor::Red);
		return;
	}

	// If the host quit on us, the client-side session is still registered and JoinSession would fail with AlreadyInSession.
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		ShowMessage(TEXT("Clearing stale session before searching..."));
		bFindAfterDestroy = true;
		DestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionComplete));
		if (!SessionInterface->DestroySession(NAME_GameSession))
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
			bFindAfterDestroy = false;
			ShowMessage(TEXT("DestroySession failed to start"), FColor::Red);
		}
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == NULL_SUBSYSTEM;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(GameKeyName, GameKeyValue, EOnlineComparisonOp::Equals);

	FindHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnFindSessionsComplete));

	ShowMessage(TEXT("Searching for sessions..."));
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		ShowMessage(TEXT("FindSessions failed to start"), FColor::Red);
	}
}

void USessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		ShowMessage(TEXT("Session search failed"), FColor::Red);
		return;
	}

	const FUniqueNetIdPtr LocalId = IOnlineSubsystem::Get()->GetIdentityInterface()->GetUniquePlayerId(0);

	for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
	{
		if (!Result.IsValid() || Result.Session.NumOpenPublicConnections <= 0)
		{
			continue;
		}
		// Don't join our own lobby.
		if (LocalId.IsValid() && Result.Session.OwningUserId.IsValid() && *Result.Session.OwningUserId == *LocalId)
		{
			continue;
		}

		JoinHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnJoinSessionComplete));

		ShowMessage(FString::Printf(TEXT("Joining session hosted by %s"), *Result.Session.OwningUserName));
		if (!SessionInterface->JoinSession(0, NAME_GameSession, Result))
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			ShowMessage(TEXT("JoinSession failed to start"), FColor::Red);
		}
		return;
	}

	ShowMessage(FString::Printf(TEXT("No joinable sessions found (%d results)"), SessionSearch->SearchResults.Num()), FColor::Red);
}

void USessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		ShowMessage(FString::Printf(TEXT("Join failed (%d)"), static_cast<int32>(Result)), FColor::Red);
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
	{
		ShowMessage(TEXT("Could not resolve connect string"), FColor::Red);
		return;
	}

	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		ShowMessage(FString::Printf(TEXT("Traveling to %s"), *ConnectString), FColor::Green);
		PC->ClientTravel(ConnectString, TRAVEL_Absolute);
	}
}

void USessionSubsystem::LeaveSession()
{
	if (bLeaving)
	{
		return;
	}

	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		bLeaving = true;
		LeaveDestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnLeaveDestroyComplete));
		if (!SessionInterface->DestroySession(NAME_GameSession))
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveDestroyHandle);
			bLeaving = false;
			ShowMessage(TEXT("DestroySession failed to start"), FColor::Red);
		}
		return;
	}
}

void USessionSubsystem::OnLeaveDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LeaveDestroyHandle);
	bLeaving = false;

	if (!bWasSuccessful)
	{
		ShowMessage(TEXT("DestroySession reported failure, leaving anyway"), FColor::Red);
	}

	TravelToMenu();
}

void USessionSubsystem::TravelToMenu()
{
	// Leaving the map is what actually shuts down the listen server (dropping its clients) or disconnects a client.
	UGameplayStatics::OpenLevel(this, FName(*UGameMapsSettings::GetGameDefaultMap()));
}

void USessionSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	ShowMessage(FString::Printf(TEXT("Network failure: %s (%s)"), ENetworkFailure::ToString(FailureType), *ErrorString), FColor::Red);
}

void USessionSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	ShowMessage(FString::Printf(TEXT("Travel failure: %s (%s)"), ETravelFailure::ToString(FailureType), *ErrorString), FColor::Red);
}

void USessionSubsystem::ShowMessage(const FString& Message, const FColor& Color) const
{
	UE_LOG(LogTemp, Log, TEXT("SessionSubsystem: %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, Color, Message);
	}
}
