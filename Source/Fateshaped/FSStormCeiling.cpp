#include "FSStormCeiling.h"

#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
constexpr double CellSize = 65.0;
constexpr double BillowSpacing = 345.0;
const FIntPoint Corners[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
const FIntPoint EdgeNeighbors[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

FVector2D CornerPosition(FIntPoint Corner)
{
    return FVector2D(Corner.X * CellSize, Corner.Y * CellSize);
}

double BillowDepth(FVector2D Position)
{
    const FVector2D Grid = Position / BillowSpacing;
    const FIntPoint Cell(FMath::FloorToInt(Grid.X), FMath::FloorToInt(Grid.Y));
    double Depth = 0.0;
    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            const FIntPoint Key = Cell + FIntPoint(X, Y);
            FRandomStream Random(static_cast<int32>(GetTypeHash(Key)));
            const FVector2D Offset(Random.FRandRange(0.15f, 0.85f), Random.FRandRange(0.15f, 0.85f));
            const FVector2D Center = FVector2D(Key.X, Key.Y) + Offset;
            const double Radius = Random.FRandRange(0.50f, 0.80f);
            const double Weight = FMath::Exp(-2.1 * (Grid - Center).SizeSquared() / FMath::Square(Radius));
            Depth += Weight * Random.FRandRange(90.0f, 205.0f);
        }
    }
    return Depth + 42.0 * FMath::PerlinNoise2D(Position / 150.0) + 16.0 * FMath::PerlinNoise2D(Position / 70.0);
}
}

void FStormCeiling::Cover(FVector Center, FVector Forward, FVector2D HalfSize, double WallTop)
{
    const FVector2D At(Center.X, Center.Y);
    const FVector2D Axis = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
    const FVector2D Side(-Axis.Y, Axis.X);
    // Include edge cells and a small overlap at doorway seams.
    const FVector2D Padded = HalfSize + FVector2D(90.0);
    const FVector2D Extent(FMath::Abs(Axis.X) * Padded.X + FMath::Abs(Side.X) * Padded.Y,
        FMath::Abs(Axis.Y) * Padded.X + FMath::Abs(Side.Y) * Padded.Y);
    const FIntPoint First(FMath::FloorToInt((At.X - Extent.X) / CellSize),
        FMath::FloorToInt((At.Y - Extent.Y) / CellSize));
    const FIntPoint Last(FMath::FloorToInt((At.X + Extent.X) / CellSize),
        FMath::FloorToInt((At.Y + Extent.Y) / CellSize));
    for (int32 X = First.X; X <= Last.X; ++X)
    {
        for (int32 Y = First.Y; Y <= Last.Y; ++Y)
        {
            const FIntPoint Key(X, Y);
            const FVector2D Delta = CornerPosition(Key) + FVector2D(CellSize * 0.5) - At;
            if (FMath::Abs(FVector2D::DotProduct(Delta, Axis)) > Padded.X
                || FMath::Abs(FVector2D::DotProduct(Delta, Side)) > Padded.Y)
            {
                continue;
            }
            double& Height = RoofCells.FindOrAdd(Key, WallTop);
            Height = FMath::Min(Height, WallTop);
        }
    }
}

void FStormCeiling::RegisterCloud(AActor& Owner, UMaterialInterface& Material) const
{
    if (RoofCells.IsEmpty()) return;

    auto* Cloud = NewObject<UProceduralMeshComponent>(&Owner, TEXT("StormCloudCanopy"));
    Cloud->SetupAttachment(Owner.GetRootComponent());
    Cloud->SetCanEverAffectNavigation(false);
    Cloud->SetCollisionProfileName(TEXT("BlockAll"));
    Cloud->SetGenerateOverlapEvents(false);
    Cloud->SetCastShadow(false);
    Cloud->RegisterComponent();

    const FTransform Frame = Cloud->GetComponentTransform();
    TMap<FIntPoint, int32> Indices;
    TMap<int32, int32> LowerEdgeIndices;
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<int32> Triangles;
    const auto VertexAt = [&](FIntPoint Corner)
    {
        if (const int32* Existing = Indices.Find(Corner)) return *Existing;
        double Total = 0.0;
        int32 Samples = 0;
        for (const FIntPoint Offset : Corners)
        {
            if (const double* Height = RoofCells.Find(Corner - Offset))
            {
                Total += *Height;
                ++Samples;
            }
        }
        const FVector2D At = CornerPosition(Corner);
        const double Depth = 240.0 * (1.0 - FMath::Exp(-BillowDepth(At) / 190.0));
        const double Height = Total / Samples + 52.0 - Depth;
        const int32 Index = Vertices.Add(Frame.InverseTransformPosition(FVector(At, Height)));
        Normals.Add(FVector::ZeroVector);
        Indices.Add(Corner, Index);
        return Index;
    };
    const auto AddTriangle = [&](int32 A, int32 B, int32 C)
    {
        Triangles.Append({A, B, C});
        const FVector Normal = FVector::CrossProduct(Vertices[C] - Vertices[A], Vertices[B] - Vertices[A]);
        Normals[A] += Normal;
        Normals[B] += Normal;
        Normals[C] += Normal;
    };
    const auto LowerEdge = [&](int32 Upper)
    {
        if (const int32* Existing = LowerEdgeIndices.Find(Upper)) return *Existing;
        const FVector At = Vertices[Upper];
        const double Drop = 176.0 + 36.0 * FMath::PerlinNoise2D(FVector2D(At) / 190.0);
        const int32 Index = Vertices.Add(At + FVector(0.0, 0.0, -Drop));
        Normals.Add(FVector::ZeroVector);
        LowerEdgeIndices.Add(Upper, Index);
        return Index;
    };
    for (const TPair<FIntPoint, double>& Cell : RoofCells)
    {
        const int32 Quad[] = {VertexAt(Cell.Key), VertexAt(Cell.Key + Corners[1]),
            VertexAt(Cell.Key + Corners[2]), VertexAt(Cell.Key + Corners[3])};
        AddTriangle(Quad[0], Quad[1], Quad[2]);
        AddTriangle(Quad[0], Quad[2], Quad[3]);
        for (int32 Edge = 0; Edge < UE_ARRAY_COUNT(EdgeNeighbors); ++Edge)
        {
            if (RoofCells.Contains(Cell.Key + EdgeNeighbors[Edge])) continue;
            // Bring the cloud rim down to the wall top so angled views stay inside the Crypt.
            const int32 A = Quad[Edge];
            const int32 B = Quad[(Edge + 1) % UE_ARRAY_COUNT(EdgeNeighbors)];
            const int32 LowerA = LowerEdge(A);
            const int32 LowerB = LowerEdge(B);
            AddTriangle(B, A, LowerA);
            AddTriangle(B, LowerA, LowerB);
        }
    }
    for (FVector& Normal : Normals) Normal.Normalize();
    Cloud->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, {}, {}, {}, true);
    Cloud->SetMaterial(0, &Material);
}
