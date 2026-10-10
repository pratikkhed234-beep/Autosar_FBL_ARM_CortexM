#ifndef BOOTMANAGER_H
#define BOOTMANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define BOOT_META_ADDRESS       (0x08008000UL)
#define APP_VALID_MAGIC         (0x5A5A5A5AUL)

typedef struct {
    uint32_t magic;
    uint32_t app_size;
    uint32_t crc32;
    uint32_t reserved;
} BootMetaHeader_t;

bool BootManager_CheckAppIntegrity(void);
void BootManager_JumpToApplication(void);
uint32_t BootManager_CalculateCRC32(const uint8_t *data, uint32_t length);

#endif /* BOOTMANAGER_H */
