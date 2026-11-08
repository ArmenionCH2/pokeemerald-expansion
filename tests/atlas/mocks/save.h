#ifndef ATLAS_MOCK_SAVE_H
#define ATLAS_MOCK_SAVE_H
#define SECTOR_DATA_SIZE 3968
#define SECTOR_SIZE 4096
struct SaveSector {u8 data[3968], unused[116];u16 id,checksum;u32 signature,counter;};
extern struct SaveSector gSaveDataBuffer;
#endif
