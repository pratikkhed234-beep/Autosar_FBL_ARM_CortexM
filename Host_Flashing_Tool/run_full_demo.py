import time
import queue
import threading

class VirtualCanBus:
    """Zero-dependency in-memory CAN bus router."""
    def __init__(self):
        self.ecu_rx_queue = queue.Queue()
        self.tester_rx_queue = queue.Queue()

    def send_from_tester(self, payload: bytes):
        self.ecu_rx_queue.put(payload)

    def send_from_ecu(self, payload: bytes):
        self.tester_rx_queue.put(payload)

    def recv_ecu(self, timeout=1.0):
        try:
            return self.ecu_rx_queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def recv_tester(self, timeout=2.0):
        try:
            return self.tester_rx_queue.get(timeout=timeout)
        except queue.Empty:
            return None

def ecu_server_worker(bus: VirtualCanBus, stop_evt: threading.Event):
    session = 0x01
    security_unlocked = False
    active_seed = bytes([0x12, 0x34, 0x56, 0x78])

    print("[ECU Node] Online and listening on CAN ID 0x7E0...")

    while not stop_evt.is_set():
        payload = bus.recv_ecu(timeout=0.05)
        if not payload:
            continue

        sid = payload[0]

        # 0x10 Diagnostic Session Control
        if sid == 0x10:
            sub_func = payload[1]
            session = sub_func
            print(f"  [ECU BSW] Transition to Session 0x{sub_func:02X}")
            bus.send_from_ecu(bytes([0x50, sub_func, 0x00, 0x32, 0x01, 0xF4]))

        # 0x27 Security Access
        elif sid == 0x27:
            sub_func = payload[1]
            if sub_func == 0x01:
                print("  [ECU BSW] Generating Seed...")
                bus.send_from_ecu(bytes([0x67, 0x01]) + active_seed)
            elif sub_func == 0x02:
                received_key = payload[2:]
                expected_key = bytes([b ^ 0x5A for b in active_seed])
                if received_key == expected_key:
                    security_unlocked = True
                    print("  [ECU BSW] Key Authenticated! Security UNLOCKED.")
                    bus.send_from_ecu(bytes([0x67, 0x02]))
                else:
                    bus.send_from_ecu(bytes([0x7F, 0x27, 0x35]))

        # 0x31 Routine Control (Erase Flash)
        elif sid == 0x31:
            if not security_unlocked:
                bus.send_from_ecu(bytes([0x7F, 0x31, 0x33]))
            else:
                print("  [ECU BSW] Executing Fls_Erase() on Application Partition...")
                bus.send_from_ecu(bytes([0x71, 0x01, 0xFF, 0x00]))

        # 0x34 Request Download
        elif sid == 0x34:
            print("  [ECU BSW] Request Download approved (Max Block: 256 bytes).")
            bus.send_from_ecu(bytes([0x74, 0x20, 0x01, 0x00]))

        # 0x36 Transfer Data
        elif sid == 0x36:
            block_seq = payload[1]
            chunk_len = len(payload) - 2
            print(f"  [ECU BSW] Fls_Write() executed for Block {block_seq} ({chunk_len} bytes).")
            bus.send_from_ecu(bytes([0x76, block_seq]))

        # 0x37 Request Transfer Exit
        elif sid == 0x37:
            print("  [ECU BSW] Transfer Exit completed.")
            bus.send_from_ecu(bytes([0x77]))

        # 0x11 ECU Reset
        elif sid == 0x11:
            print("  [ECU BSW] Triggering ECU System Reset (NVIC_SystemReset)...")
            bus.send_from_ecu(bytes([0x51, payload[1]]))
            time.sleep(0.1)
            break

def run_reprogramming_demo():
    print("=" * 65)
    print(" AUTOSAR FLASH BOOTLOADER: END-TO-END UDS REPROGRAMMING DEMO ")
    print("=" * 65)

    bus = VirtualCanBus()
    stop_evt = threading.Event()

    ecu_thread = threading.Thread(target=ecu_server_worker, args=(bus, stop_evt), daemon=True)
    ecu_thread.start()

    time.sleep(0.2)

    try:
        # Step 1: Extended Session
        print("\n[Step 1] Tester -> 0x10 03 (DiagnosticSessionControl: Extended)")
        bus.send_from_tester(bytes([0x10, 0x03]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x50, f"Failed at Step 1: {resp}"
        print(f"         ECU -> Positive Response: {resp.hex(' ')}")

        # Step 2: Programming Session
        print("\n[Step 2] Tester -> 0x10 02 (DiagnosticSessionControl: Programming)")
        bus.send_from_tester(bytes([0x10, 0x02]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x50, f"Failed at Step 2: {resp}"
        print(f"         ECU -> Positive Response: {resp.hex(' ')}")

        # Step 3: Security Access (Request Seed)
        print("\n[Step 3] Tester -> 0x27 01 (SecurityAccess: Request Seed)")
        bus.send_from_tester(bytes([0x27, 0x01]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x67, f"Failed at Step 3: {resp}"
        seed = resp[2:]
        print(f"         ECU -> Seed Received: {seed.hex(' ')}")

        # Step 4: Security Access (Send Key)
        key = bytes([b ^ 0x5A for b in seed])
        print(f"\n[Step 4] Tester -> 0x27 02 (SecurityAccess: Send Key {key.hex(' ')})")
        bus.send_from_tester(bytes([0x27, 0x02]) + key)
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x67, f"Failed at Step 4: {resp}"
        print(f"         ECU -> Security Access Granted: {resp.hex(' ')}")

        # Step 5: Routine Control (Erase Memory)
        print("\n[Step 5] Tester -> 0x31 01 FF 00 (RoutineControl: Erase Memory)")
        bus.send_from_tester(bytes([0x31, 0x01, 0xFF, 0x00]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x71, f"Failed at Step 5: {resp}"
        print(f"         ECU -> Erase Acknowledged: {resp.hex(' ')}")

        # Step 6: Request Download
        print("\n[Step 6] Tester -> 0x34 (RequestDownload: Address 0x08008800, Size 512B)")
        addr_and_len = bytes([0x08, 0x00, 0x88, 0x00, 0x00, 0x00, 0x02, 0x00])
        bus.send_from_tester(bytes([0x34, 0x00, 0x44]) + addr_and_len)
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x74, f"Failed at Step 6: {resp}"
        print(f"         ECU -> Download Ready: {resp.hex(' ')}")

        # Step 7: Transfer Data (Chunk 1)
        print("\n[Step 7] Tester -> 0x36 01 (TransferData: Stream 64-byte Chunk)")
        chunk = bytes([0xAA] * 64)
        bus.send_from_tester(bytes([0x36, 0x01]) + chunk)
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x76, f"Failed at Step 7: {resp}"
        print(f"         ECU -> Block 1 Flashed: {resp.hex(' ')}")

        # Step 8: Request Transfer Exit
        print("\n[Step 8] Tester -> 0x37 (RequestTransferExit)")
        bus.send_from_tester(bytes([0x37]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x77, f"Failed at Step 8: {resp}"
        print(f"         ECU -> Transfer Exit OK: {resp.hex(' ')}")

        # Step 9: ECU Hard Reset
        print("\n[Step 9] Tester -> 0x11 01 (ECUReset: Hard Reset)")
        bus.send_from_tester(bytes([0x11, 0x01]))
        resp = bus.recv_tester()
        assert resp and resp[0] == 0x51, f"Failed at Step 9: {resp}"
        print(f"         ECU -> Reset Executed: {resp.hex(' ')}")

        print("\n" + "=" * 65)
        print(" SUCCESS: ALL 9 UDS STAGES COMPLETED & POSITIVELY ACKNOWLEDGED ")
        print("=" * 65)

    finally:
        stop_evt.set()

if __name__ == "__main__":
    run_reprogramming_demo()
