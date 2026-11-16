#include "FSProfileStorageLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
// This is deliberately limited to the v1 Blueprint profile written by UE 5.8.
// Unknown formats are preserved for recovery, never passed to the legacy loader.
struct FProfileReader
{
    const TArray<uint8>& Bytes;
    int64 Position = 0;

    bool Skip(int64 Count)
    {
        if (Count < 0 || Count > Bytes.Num() - Position) return false;
        Position += Count;
        return true;
    }

    template<typename T> bool Read(T& Value)
    {
        if (sizeof(T) > Bytes.Num() - Position) return false;
        FMemory::Memcpy(&Value, Bytes.GetData() + Position, sizeof(T));
        Position += sizeof(T);
        return true;
    }

    bool String(FString& Value, int32 MaxCharacters)
    {
        int32 Length;
        if (!Read(Length)) return false;
        const int64 Count = FMath::Abs(static_cast<int64>(Length));
        const int32 Width = Length < 0 ? 2 : 1;
        if (Count > MaxCharacters || Count * Width > Bytes.Num() - Position) return false;
        Value.Reset();
        if (Count == 0) return true;
        Value.Reserve(static_cast<int32>(Count));
        for (int64 Index = 0; Index < Count; ++Index)
        {
            uint16 Character = 0;
            if (Width == 2)
            {
                if (!Read(Character)) return false;
            }
            else
            {
                uint8 Byte;
                if (!Read(Byte)) return false;
                Character = Byte;
            }
            if (Index == Count - 1)
            {
                if (Character != 0) return false;
            }
            else
            {
                if (Character == 0) return false;
                Value.AppendChar(static_cast<TCHAR>(Character));
            }
        }
        return true;
    }
};

bool ReadProfile(const TArray<uint8>& Bytes, USaveGame& Profile)
{
    if (Bytes.Num() < 40 || Bytes.Num() > 1024 * 1024) return false;
    FProfileReader Reader{Bytes};
    int32 Magic, Format, UE4, UE5;
    if (!Reader.Read(Magic) || Magic != 0x53415647 || !Reader.Read(Format) || Format != 3
        || !Reader.Read(UE4) || UE4 != 522 || !Reader.Read(UE5) || UE5 != 1018
        || !Reader.Skip(10)) return false; // FEngineVersion: three uint16s + uint32.
    FString Branch, ClassPath;
    int32 CustomFormat, CustomCount;
    if (!Reader.String(Branch, 1023) || !Reader.Read(CustomFormat) || CustomFormat != 3
        || !Reader.Read(CustomCount) || CustomCount < 0 || CustomCount > 1024
        || !Reader.Skip(static_cast<int64>(CustomCount) * 20)
        || !Reader.String(ClassPath, 1023) || !ClassPath.Equals(Profile.GetClass()->GetPathName(), ESearchCase::CaseSensitive)) return false;
    uint8 Control;
    if (!Reader.Read(Control) || Control != 0) return false;

    TSet<FString> Seen;
    for (int32 Index = 0; Index < 128; ++Index)
    {
        FString Name, Type;
        if (!Reader.String(Name, 1023)) return false;
        if (Name == TEXT("None"))
        {
            int32 HasObjectGuid;
            return Reader.Read(HasObjectGuid) && HasObjectGuid == 0 && Reader.Position == Bytes.Num();
        }
        if (Seen.Contains(Name)) return false;
        Seen.Add(Name);
        const FProperty* Property = FindFProperty<FProperty>(Profile.GetClass(), *Name);
        int32 Parameters = 0;
        int32 Size = 0;
        uint8 Flags = 0;
        if (!Property || Property->ArrayDim != 1 || !Name.Equals(Property->GetName(), ESearchCase::CaseSensitive)
            || !Reader.String(Type, 1023) || !Reader.Read(Parameters) || Parameters != 0
            || !Reader.Read(Size) || Size < 0 || !Reader.Read(Flags) || (Flags & ~0x13) != 0) return false;
        if (Flags & 0x01)
        {
            int32 ArrayIndex;
            if (!Reader.Read(ArrayIndex) || ArrayIndex != 0) return false;
        }
        if ((Flags & 0x02) && !Reader.Skip(16)) return false;
        const int64 Start = Reader.Position;
        if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
        {
            int32 Value;
            if (Type != TEXT("IntProperty") || Size != sizeof(Value) || (Flags & 0x10) || !Reader.Read(Value)) return false;
            IntProperty->SetPropertyValue_InContainer(&Profile, Value);
        }
        else if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
        {
            FString Value;
            if (Type != TEXT("StrProperty") || (Flags & 0x10) || !Reader.String(Value, Size)) return false;
            StringProperty->SetPropertyValue_InContainer(&Profile, Value);
        }
        else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            if (Type != TEXT("BoolProperty") || Size != 0) return false;
            BoolProperty->SetPropertyValue_InContainer(&Profile, (Flags & 0x10) != 0);
        }
        else
        {
            return false;
        }
        if (Reader.Position - Start != Size) return false;
    }
    return false;
}
}

USaveGame* UFSProfileStorageLibrary::LoadCheckedProfile(const FString& SlotName,
    TSubclassOf<USaveGame> ProfileClass, FString& FailureReason)
{
    FailureReason.Reset();
    TArray<uint8> Bytes;
    if (!ProfileClass || !UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, 0))
    {
        FailureReason = TEXT("Profile could not be read. Original file preserved.");
        return nullptr;
    }
    USaveGame* Profile = UGameplayStatics::CreateSaveGameObject(ProfileClass);
    if (!Profile || !ReadProfile(Bytes, *Profile))
    {
        FailureReason = TEXT("Invalid or unsupported save envelope. Original file preserved.");
        return nullptr;
    }
    return Profile;
}
