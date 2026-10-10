import zlib
import time
import sys

SECURITY_KEY_MASK = 0x4A92B351

def calculate_key(seed: int) -> int:
    key = (seed ^ SECURITY_KEY_MASK) & 0xFFFFFFFF
    key = ((key << 5) & 0xFFFFFFFF) | (key >> 27)
    key = (key ^ 0xA5A5A5A5) & 0xFFFFFFFF
    return key

def run_flashing_demo():
    print("=" * 70)
    print("AUTOSAR FLASH BOOTLOADER - PRODUCTION FLASHING SEQUENCE DEMO")
    print("=" * 70)

    # 1. DiagnosticSessionControl (0x10 02)
    print("\n[STAGE 1] Switching to Programming Session (0x10 02)...")
    time.sleep(0.04)
    print("  -> TX: 10 02")
    print("  <- RX: 50 02 00 32 01 F4 (Session Switched: Programming)")

    # 2. SecurityAccess: Request Dynamic Seed (0x27 01)
    print("\n[STAGE 2] Requesting Dynamic Security Access Seed (0x27 01)...")
    time.sleep(0.04)
    print("  -> TX: 27 01")
    # Simulated PRNG output from ECU
    simulated_seed = 0x3E8B1A24
    seed_bytes = simulated_seed.to_bytes(4, byteorder='big')
    print(f"  <- RX: 67 01 {seed_bytes.hex(' ')} (Dynamic Seed Generated)")

    # 3. SecurityAccess: Calculate and Send Key (0x27 02)
    computed_key = calculate_key(simulated_seed)
    key_bytes = computed_key.to_bytes(4, byteorder='big')
    print(f"\n[STAGE 3] Computing Key via Mask/Rotate Algorithm: 0x{computed_key:08X}...")
    time.sleep(0.04)
    print(f"  -> TX: 27 02 {key_bytes.hex(' ')}")
    print("  <- RX: 67 02 (Security Unlocked: Level 1 Access Granted)")

    # 4. RoutineControl Erase Flash (0x31 01 FF 00)
    print("\n[STAGE 4] Executing RoutineControl: Erase Application Partition (0x31 01 FF 00)...")
    time.sleep(0.04)
    print("  -> TX: 31 01 FF 00")
    print("  <- RX: 71 01 FF 00 (Flash Sector Erase Successful: Status 0x00)")

    # 5. RequestDownload (0x34)
    payload = bytes([i % 256 for i in range(128)])
    app_addr = 0x08008800
    app_size = len(payload)
    print(f"\n[STAGE 5] Requesting Download to Flash (0x34) [Addr: 0x{app_addr:08X}, Size: {app_size} bytes]...")
    time.sleep(0.04)
    print("  -> TX: 34 00 44 08 00 88 00 00 00 00 80")
    print("  <- RX: 74 20 01 00 (Download Accepted, Max Block Size = 256 bytes)")

    # 6. TransferData (0x36)
    print(f"\n[STAGE 6] Transferring Firmware Payload (0x36, Block #1, {len(payload)} bytes)...")
    time.sleep(0.04)
    print(f"  -> TX: 36 01 {payload[:8].hex(' ')} ... [+{len(payload)-8} bytes]")
    print("  <- RX: 76 01 (Block #1 Written to Flash Successfully)")

    # 7. RoutineControl: Verify CRC32 (0x31 01 02 02)
    crc32_val = zlib.crc32(payload) & 0xFFFFFFFF
    crc_bytes = crc32_val.to_bytes(4, byteorder='big')
    print(f"\n[STAGE 7] Verifying Application Integrity: CRC32 = 0x{crc32_val:08X} (0x31 01 02 02)...")
    time.sleep(0.04)
    print(f"  -> TX: 31 01 02 02 {crc_bytes.hex(' ')}")
    print("  <- RX: 71 01 02 02 00 (Memory Integrity Verified: Checksum Matches)")

    # 8. RequestTransferExit (0x37)
    print("\n[STAGE 8] Finalizing Programming: Request Transfer Exit (0x37)...")
    time.sleep(0.04)
    print("  -> TX: 37")
    print("  <- RX: 77 (Transfer Session Closed)")

    # 9. ECUReset (0x11 01)
    print("\n[STAGE 9] Triggering Target Hardware Reset (0x11 01: Hard Reset)...")
    time.sleep(0.04)
    print("  -> TX: 11 01")
    print("  <- RX: 51 01 (Reset Acknowledged - Resetting Controller)")

    print("\n" + "=" * 70)
    print("FLASHING COMPLETE: BOOTLOADER VERIFIED INTEGRITY & EXECUTED APP")
    print("=" * 70)

if __name__ == "__main__":
    run_flashing_demo()
