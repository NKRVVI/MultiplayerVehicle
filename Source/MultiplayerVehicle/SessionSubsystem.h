// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SessionSubsystem.generated.h"

/**
 * Hosts and joins sessions through whichever online subsystem is the default (Steam, or Null for LAN).
 */
UCLASS()
class MULTIPLAYERVEHICLE_API USessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//Creates a session and opens MapName as a listen server. Destroys any existing session first.
	UFUNCTION(BlueprintCallable, Category = "Session")
	void HostSession(int32 MaxPlayers = 4, const FString& MapName = TEXT("/Game/CarArena"));

	//Searches for sessions of this game and joins the first one that has room.
	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindAndJoinSession();

	//Leaves / tears down the current session.
	UFUNCTION(BlueprintCallable, Category = "Session")
	void LeaveSession();

private:
	void CreateSessionInternal();

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnLeaveDestroyComplete(FName SessionName, bool bWasSuccessful);
	void TravelToMenu();
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	//network debugging functions
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	void ShowMessage(const FString& Message, const FColor& Color = FColor::Yellow) const;

	/*Session tags*/
	static const FName GameKeyName;
	static const FString GameKeyValue;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle LeaveDestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;

	//Host request that's waiting for an old session to finish being destroyed
	bool bCreateAfterDestroy = false;
	//Join request that's waiting for a stale session (e.g. after the host quit) to finish being destroyed
	bool bFindAfterDestroy = false;
	bool bLeaving = false;
	int32 PendingMaxPlayers = 4;
	FString PendingMapName;
};
