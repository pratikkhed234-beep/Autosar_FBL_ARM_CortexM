import time
import can
import isotp

def run_tester():
    print("[Tester] Initializing Diagnostic Client...")
    bus = can.interface.Bus(channel='virtual_ch', interface='virtual')
    
    # Tester sends on 0x7E0 and receives on 0x7E8
    tp_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=0x7E8, txid=0x7E0)
    stack = isotp.CanStack(bus=bus, address=tp_addr)
    stack.start()

    time.sleep(0.5)

    try:
        # 1. Diagnostic Session Control -> Extended (0x10 03)
        print("\n[Step 1] Requesting Extended Session (0x10 03)...")
        stack.send(bytes([0x10, 0x03]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x50, f"Failed: {resp}"
        print(f"  -> Success: {resp.hex(' ')}")

        # 2. Diagnostic Session Control -> Programming (0x10 02)
        print("\n[Step 2] Requesting Programming Session (0x10 02)...")
        stack.send(bytes([0x10, 0x02]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x50, f"Failed: {resp}"
        print(f"  -> Success: {resp.hex(' ')}")

        # 3. Security Access -> Request Seed (0x27 01)
        print("\n[Step 3] Requesting Security Seed (0x27 01)...")
        stack.send(bytes([0x27, 0x01]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x67, f"Failed: {resp}"
        seed = resp[2:]
        print(f"  -> Received Seed: {seed.hex(' ')}")

        # Compute Key: Seed XOR 0x5A
        key = bytes([b ^ 0x5A for b in seed])
        print(f"\n[Step 4] Sending Computed Key (0x27 02 {key.hex(' ')})...")
        stack.send(bytes([0x27, 0x02]) + key)
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x67, f"Failed: {resp}"
        print(f"  -> Security Unlocked: {resp.hex(' ')}")

        # 4. Routine Control -> Erase Flash (0x31 01 FF 00)
        print("\n[Step 5] Triggering Flash Erase Routine (0x31 01 FF 00)...")
        stack.send(bytes([0x31, 0x01, 0xFF, 0x00]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x71, f"Failed: {resp}"
        print(f"  -> Flash Erased: {resp.hex(' ')}")

        # 5. Request Download (0x34)
        print("\n[Step 6] Requesting Download Address 0x08008800 (0x34)...")
        addr_and_len = bytes([0x08, 0x00, 0x88, 0x00, 0x00, 0x00, 0x02, 0x00])
        stack.send(bytes([0x34, 0x00, 0x44]) + addr_and_len)
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x74, f"Failed: {resp}"
        print(f"  -> Download Accepted: {resp.hex(' ')}")

        # 6. Transfer Data (0x36)
        print("\n[Step 7] Streaming Firmware Data Block 1 (0x36 01)...")
        dummy_data = bytes([0xAA] * 64)
        stack.send(bytes([0x36, 0x01]) + dummy_data)
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x76, f"Failed: {resp}"
        print(f"  -> Block Flashed: {resp.hex(' ')}")

        # 7. Request Transfer Exit (0x37)
        print("\n[Step 8] Finalizing Transfer (0x37)...")
        stack.send(bytes([0x37]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x77, f"Failed: {resp}"
        print(f"  -> Transfer Closed: {resp.hex(' ')}")

        # 8. ECU Hard Reset (0x11 01)
        print("\n[Step 9] Triggering ECU Reset (0x11 01)...")
        stack.send(bytes([0x11, 0x01]))
        resp = stack.recv(timeout=2.0)
        assert resp and resp[0] == 0x51, f"Failed: {resp}"
        print(f"  -> ECU Reset Acknowledged: {resp.hex(' ')}")

        print("\n=======================================================")
        print("  END-TO-END UDS REPROGRAMMING VERIFIED SUCCESSFULLY!  ")
        print("=======================================================")

    finally:
        stack.stop()
        bus.shutdown()

if __name__ == "__main__":
    run_tester()
