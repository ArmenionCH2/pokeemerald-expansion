#include "global.h"
#include "atlas_recipe.h"
/* Expansion owns every flash sector. Regenerate the optional world recipe
 * from the seed; never write Hall of Fame, Trainer Hill or battle sectors. */
bool8 AtlasRecipeIO_Read(u8 slot, u8 *data) { return FALSE; }
bool8 AtlasRecipeIO_Write(u8 slot, const u8 *data) { return FALSE; }
