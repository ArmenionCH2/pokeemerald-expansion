#ifndef GUARD_ATLAS_RECIPE_H
#define GUARD_ATLAS_RECIPE_H
/* Two optional two-sector caches; native player saves remain authoritative. */
#define ATLAS_RECIPE_BYTES 7936
#define ATLAS_RECIPE_HEADER 128
extern u8 gAtlasRecipeStatus;
extern u16 gAtlasRecipeWrites;
void AtlasRecipe_Reset(void);
bool8 AtlasRecipe_Build(u8 *data, u32 counter);
bool8 AtlasRecipe_Valid(const u8 *data);
bool8 AtlasRecipe_Load(u32 seed);
bool8 AtlasRecipe_VerifyRestore(void);
void AtlasRecipe_Save(u32 counter);
bool8 AtlasRecipeIO_Read(u8 slot, u8 *data);
bool8 AtlasRecipeIO_Write(u8 slot, const u8 *data);
#endif
