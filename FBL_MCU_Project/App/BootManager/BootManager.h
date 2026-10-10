#ifndef BOOT_MANAGER_H
#define BOOT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define APP_START_ADDRESS       (0x08008800UL)
#define BOOT_FLAG_VALID_APP     (0xA5A55A5AUL)

typedef struct {
    uint32_t app_valid_flag;
    uint32_t app_size;
    uint32_t app_crc32;
    uint32_t reserved;
} BootMetaHeader_t;

void BootManager_Init(void);
void BootManager_Process(void);
bool BootManager_CheckAppIntegrity(void);
void BootManager_JumpToApplication(void);

#endif /* BOOT_MANAGER_H */
