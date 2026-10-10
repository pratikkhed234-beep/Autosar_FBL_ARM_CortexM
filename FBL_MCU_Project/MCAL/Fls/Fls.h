#ifndef FLS_H
#define FLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef E_OK
#define E_OK        (0U)
#endif

#ifndef E_NOT_OK
#define E_NOT_OK    (1U)
#endif

#define FLS_OK      E_OK
#define FLS_NOT_OK  E_NOT_OK

/* Application Flash Partition Boundaries (Matches bootloader_partition.ld) */
#define FLS_APP_BASE_ADDRESS    (0x08008800UL)
#define FLS_APP_END_ADDRESS     (0x0807FFFFUL)

typedef uint8_t Std_ReturnType;
typedef uint8_t Fls_ReturnType;
typedef uint32_t Fls_AddressType;
typedef uint32_t Fls_LengthType;

typedef enum {
    FLS_JOB_NONE = 0,
    FLS_JOB_ERASE,
    FLS_JOB_WRITE
} Fls_JobType;

void Fls_Init(void);
Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8_t* SourceAddressPtr, Fls_LengthType Length);
Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8_t* TargetAddressPtr, Fls_LengthType Length);
Fls_JobType Fls_GetJobResult(void);

#endif /* FLS_H */
