#pragma once

#include "CoreMinimal.h"

namespace FSDungeon
{
inline const FIntPoint Directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
inline const FIntPoint NormalHalfSize(1, 1);
inline const FIntPoint BossHalfSize(1, 1);
inline const FIntPoint ExitHalfSize(1, 1);
inline constexpr int32 MinStraightTilesBetweenCorners = 2;

enum class ERoomRole : uint8
{
    Passage, Entrance, Guardian, Boss, Spikes, Poison, Cache, Exit
};

struct FRoom
{
    FIntPoint Center;
    FIntPoint HalfSize;
    ERoomRole Role = ERoomRole::Passage;
};

struct FPlan
{
    TArray<FRoom> Rooms;
    TSet<FIntPoint> Corridors;
    TSet<FIntPoint> Tiles;
    FIntPoint GateTile = FIntPoint::ZeroValue;
    FIntPoint ExitDirection = FIntPoint::ZeroValue;
    int32 Shortcuts = 0;
    int32 Corners = 0;
    int32 Junctions = 0;
};

FIntPoint DoorTile(const FRoom& Room, FIntPoint Direction);
uint8 RoomOpenings(const FPlan& Plan, const FRoom& Room);
uint8 CorridorOpenings(const FPlan& Plan, FIntPoint Tile);
bool IsStraight(uint8 Openings);
bool HasSpacedCorners(const FPlan& Plan);
bool Generate(int32 Seed, FPlan& OutPlan);
}
