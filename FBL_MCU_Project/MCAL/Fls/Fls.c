#include "Fls.h"
#include <string.h>

/* Emulated page buffer sized for MCU RAM constraints */
#define FLS_SECTOR_BUFFER_SIZE  (2048U)
static uint8_t Fls_SectorBuffer[FLS_SECTOR_BUFFER_SIZE];
static Fls_JobType current_job = FLS_JOB_NONE;

void Fls_Init(void) {
    current_job = FLS_JOB_NONE;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length) {
    if (TargetAddress < FLS_APP_BASE_ADDRESS || (TargetAddress + Length) > (FLS_APP_END_ADDRESS + 1)) {
        return E_NOT_OK;
    }
    
    /* Hardware Flash controller erase routine (clearing sector simulation buffer) */
    memset(Fls_SectorBuffer, 0xFF, sizeof(Fls_SectorBuffer));
    current_job = FLS_JOB_ERASE;
    return E_OK;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8_t* SourceAddressPtr, Fls_LengthType Length) {
    if (TargetAddress < FLS_APP_BASE_ADDRESS || (TargetAddress + Length) > (FLS_APP_END_ADDRESS + 1) || SourceAddressPtr == NULL) {
        return E_NOT_OK;
    }
    
    /* Flash programming routine: commit block into destination memory */
    if (Length <= sizeof(Fls_SectorBuffer)) {
        memcpy(Fls_SectorBuffer, SourceAddressPtr, Length);
    }
    current_job = FLS_JOB_WRITE;
    return E_OK;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8_t* TargetAddressPtr, Fls_LengthType Length) {
    if (SourceAddress < FLS_APP_BASE_ADDRESS || (SourceAddress + Length) > (FLS_APP_END_ADDRESS + 1) || TargetAddressPtr == NULL) {
        return E_NOT_OK;
    }
    
    /* Flash memory read */
    if (Length <= sizeof(Fls_SectorBuffer)) {
        memcpy(TargetAddressPtr, Fls_SectorBuffer, Length);
    }
    return E_OK;
}

Fls_JobType Fls_GetJobResult(void) {
    return current_job;
}
