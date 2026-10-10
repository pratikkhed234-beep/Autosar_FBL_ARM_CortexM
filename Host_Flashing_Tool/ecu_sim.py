import time
import can
import isotp

def run_simulated_ecu():
    print("[ECU Sim] Initializing Virtual ECU Node on channel 'virtual_ch'...")
    bus = can.interface.Bus(channel='virtual_ch', interface='virtual')
    
    # ECU receives on 0x7E0 (Tester TX) and transmits on 0x7E8 (Tester RX)
    tp_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=0x7E0, txid=0x7E8)
    stack = isotp.CanStack(bus=bus, address=tp_addr)
    stack.start()

    session = 0x01
    security_unlocked = False
    active_seed = bytes([0x12, 0x34, 0x56, 0x78])

    print("[ECU Sim] Listening for UDS diagnostic requests...")
    try:
        while True:
            # stack.start() runs in background; do not call stack.process()
            if stack.available():
                payload = stack.recv()
                if not payload:
                    continue

                sid = payload[0]

                # 0x10 Diagnostic Session Control
                if sid == 0x10:
                    sub_func = payload[1]
                    session = sub_func
                    print(f"[ECU Sim] Session transition to: 0x{sub_func:02X}")
                    stack.send(bytes([0x50, sub_func, 0x00, 0x32, 0x01, 0xF4]))

                # 0x27 Security Access
                elif sid == 0x27:
                    sub_func = payload[1]
                    if sub_func == 0x01:
                        print("[ECU Sim] Seed requested, sending seed...")
                        stack.send(bytes([0x67, 0x01]) + active_seed)
                    elif sub_func == 0x02:
                        received_key = payload[2:]
                        expected_key = bytes([b ^ 0x5A for b in active_seed])
                        if received_key == expected_key:
                            security_unlocked = True
                            print("[ECU Sim] Security Access UNLOCKED!")
                            stack.send(bytes([0x67, 0x02]))
                        else:
                            print("[ECU Sim] Security Key Invalid!")
                            stack.send(bytes([0x7F, 0x27, 0x35]))

                # 0x31 Routine Control (Erase)
                elif sid == 0x31:
                    if not security_unlocked:
                        stack.send(bytes([0x7F, 0x31, 0x33]))
                    else:
                        print("[ECU Sim] Routine 0xFF00: Memory Erase executed.")
                        stack.send(bytes([0x71, 0x01, 0xFF, 0x00]))

                # 0x34 Request Download
                elif sid == 0x34:
                    print("[ECU Sim] Request Download accepted (Max block: 256 bytes)")
                    stack.send(bytes([0x74, 0x20, 0x01, 0x00]))

                # 0x36 Transfer Data
                elif sid == 0x36:
                    block_seq = payload[1]
                    data_len = len(payload) - 2
                    print(f"[ECU Sim] Flashed block {block_seq} ({data_len} bytes)")
                    stack.send(bytes([0x76, block_seq]))

                # 0x37 Request Transfer Exit
                elif sid == 0x37:
                    print("[ECU Sim] Transfer exit acknowledged.")
                    stack.send(bytes([0x77]))

                # 0x11 ECU Reset
                elif sid == 0x11:
                    print("[ECU Sim] ECU Reset requested! Resetting node...")
                    stack.send(bytes([0x51, payload[1]]))
                    time.sleep(0.1)
                    break

            time.sleep(0.01)
    finally:
        stack.stop()
        bus.shutdown()
        print("[ECU Sim] Node shutdown complete.")

if __name__ == "__main__":
    run_simulated_ecu()
