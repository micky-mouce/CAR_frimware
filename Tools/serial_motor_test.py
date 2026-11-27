"""Windows interactive C30D four-wheel motor serial test."""

import argparse
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ModuleNotFoundError:
    print("缺少 pyserial，请先运行：", file=sys.stderr)
    print("  py -m pip install pyserial", file=sys.stderr)
    print("如果电脑没有 py 命令，请把 py 换成 python。", file=sys.stderr)
    raise SystemExit(2)


class MotorSerialTest:
    def __init__(self, port, baud, rpm, verbose):
        self.serial_port = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
            write_timeout=0.2,
        )
        self.running = threading.Event()
        self.stop_requested = threading.Event()
        self.rpm = int(rpm)
        self.verbose = verbose
        self.rx_buffer = bytearray()
        self.latest_fields = {}
        self.latest_status_time = 0.0
        self.last_pong_time = 0.0
        self.last_send = 0.0
        self.last_state = None
        self.print_lock = threading.Lock()
        self.serial_write_lock = threading.Lock()

    def output(self, text):
        with self.print_lock:
            print(text, flush=True)

    def send(self, text):
        with self.serial_write_lock:
            self.serial_port.write((text + "\n").encode("ascii"))

    def read_feedback(self):
        waiting = self.serial_port.in_waiting
        if waiting:
            self.rx_buffer.extend(self.serial_port.read(waiting))

        while b"\n" in self.rx_buffer:
            raw, _, remainder = self.rx_buffer.partition(b"\n")
            self.rx_buffer = bytearray(remainder)
            line = raw.decode("ascii", errors="replace").strip()
            if not line:
                continue

            if line == "PONG":
                self.last_pong_time = time.monotonic()
                if self.verbose:
                    self.output("RX PONG")
                continue

            fields = {}
            for item in line.split(","):
                key, separator, value = item.partition("=")
                if separator:
                    fields[key] = value

            state = fields.get("STATE")
            if state is not None:
                self.latest_fields = fields
                self.latest_status_time = time.monotonic()
                self.last_state = state

                if self.verbose:
                    self.output(f"RX {line}")

    def print_status(self):
        fields = self.latest_fields
        if not fields or (time.monotonic() - self.latest_status_time > 1.0):
            self.output("尚未收到 STM32 状态，请检查板子供电和串口连接。")
            return

        rpm = ",".join(
            f"{wheel}{fields.get(f'RPM_{wheel}', '?')}"
            for wheel in ("A", "B", "C", "D")
        )
        pwm = ",".join(
            f"{wheel}{fields.get(f'PWM_{wheel}', '?')}"
            for wheel in ("A", "B", "C", "D")
        )
        self.output("")
        self.output(
            f"状态={fields.get('STATE', '?')}  "
            f"原因={fields.get('REASON', '?')}  "
            f"目标={fields.get('TARGET_RPM', '?')}rpm"
        )
        self.output(f"实测转速: {rpm}    PWM: {pwm}")

        if (
            fields.get("STATE") == "STOP"
            and fields.get("REASON") == "HW_DISABLED"
        ):
            self.output("提示：请打开 C30D 板的电机使能开关 PD3。")

    def input_loop(self):
        self.output("")
        self.output("命令: r=运行  s=停止  p=PING  run 50=设置50rpm")
        self.output("      status=查看状态  v=切换完整日志  q=退出")

        while not self.stop_requested.is_set():
            try:
                command = input("> ").strip().lower()
            except EOFError:
                self.stop_requested.set()
                break

            if not command:
                continue

            if command in ("r", "run"):
                self.running.set()
                self.output(f"运行，目标 {self.rpm} rpm")
                time.sleep(1.0)
                self.print_status()
            elif command in ("s", "stop", "stp"):
                self.running.clear()
                self.output("停止")
                time.sleep(0.3)
                self.print_status()
            elif command in ("p", "ping"):
                self.last_pong_time = 0.0
                self.send("PING")
                time.sleep(0.2)
                if time.monotonic() - self.last_pong_time < 0.5:
                    self.output("PING -> PONG，通信正常。")
                else:
                    self.output("未收到 PONG，请检查串口连接。")
            elif command in ("status", "st"):
                self.print_status()
            elif command in ("v", "verbose"):
                self.verbose = not self.verbose
                self.output(
                    "完整状态日志：已开启"
                    if self.verbose
                    else "完整状态日志：已关闭"
                )
            elif command.startswith("run "):
                try:
                    rpm = int(command.split(maxsplit=1)[1])
                except ValueError:
                    self.output("请输入 20～80 之间的整数")
                    continue
                if 20 <= rpm <= 80:
                    self.rpm = rpm
                    self.running.set()
                    self.output(f"运行，目标 {self.rpm} rpm")
                    time.sleep(1.0)
                    self.print_status()
                else:
                    self.output("目标转速必须在 20～80 rpm")
            elif command in ("q", "quit", "exit"):
                self.stop_requested.set()
                break
            else:
                self.output("无效命令，请输入 r、s、p、run 50、status、v 或 q")

    def communication_loop(self):
        while not self.stop_requested.is_set():
            now = time.monotonic()
            if now - self.last_send >= 0.1:
                if self.running.is_set():
                    self.send(f"RUN,{self.rpm}")
                else:
                    self.send("STOP")
                self.last_send = now

            try:
                self.read_feedback()
            except serial.SerialException as error:
                self.stop_requested.set()
                self.output(f"\n串口连接中断：{error}")
                self.output("请关闭其他串口程序，并重新连接 USB 数据线。")
                break
            time.sleep(0.01)

    def run(self):
        self.serial_port.reset_input_buffer()
        self.serial_port.reset_output_buffer()

        communication_thread = threading.Thread(
            target=self.communication_loop,
            daemon=True,
        )
        communication_thread.start()

        try:
            self.input_loop()
        except KeyboardInterrupt:
            self.output("\n收到 Ctrl+C，准备停车")
        finally:
            self.running.clear()
            self.stop_requested.set()
            for _ in range(3):
                try:
                    self.send("STOP")
                except serial.SerialException:
                    break
                time.sleep(0.03)
            communication_thread.join(timeout=0.5)
            try:
                self.serial_port.close()
            except serial.SerialException:
                pass
            self.output("串口已关闭，STM32 已收到最后 STOP 指令")


def choose_port(requested_port):
    ports = list(list_ports.comports())
    if requested_port:
        return requested_port

    candidates = [
        port.device
        for port in ports
        if "CH9102" in (port.description or "").upper()
        or "CH343" in (port.description or "").upper()
        or "USB-SERIAL" in (port.description or "").upper()
        or "USB 串行设备" in (port.description or "")
    ]
    if len(candidates) == 1:
        return candidates[0]

    print("可用串口:")
    for port in ports:
        print(f"  {port.device}: {port.description}")
    raise SystemExit("请用 --port COMx 指定串口")


def main():
    parser = argparse.ArgumentParser(
        description="C30D four-wheel motor serial test for Windows"
    )
    parser.add_argument("--port", help="Windows serial port, for example COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rpm", type=int, default=40)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--list", action="store_true", help="list serial ports")
    args = parser.parse_args()

    if args.list:
        for port in list_ports.comports():
            print(f"{port.device}: {port.description}")
        return

    if not 20 <= args.rpm <= 80:
        raise SystemExit("--rpm must be between 20 and 80")

    port = choose_port(args.port)
    print(f"打开 {port}，{args.baud} bps，8N1")
    try:
        test = MotorSerialTest(port, args.baud, args.rpm, args.verbose)
    except serial.SerialException as error:
        print(f"无法打开 {port}：{error}", file=sys.stderr)
        print("请关闭串口助手或其他占用该 COM 口的程序后重试。", file=sys.stderr)
        return 2

    test.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
