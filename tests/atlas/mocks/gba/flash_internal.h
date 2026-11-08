#ifndef ATLAS_MOCK_FLASH_INTERNAL_H
#define ATLAS_MOCK_FLASH_INTERNAL_H
extern u16 (*ProgramFlashByte)(u16,u32,u8);
void ReadFlash(u16 sector,u32 offset,u8 *data,u32 size);
#endif
