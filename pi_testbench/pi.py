# Python script for the test plan of the automated blast gate
import datetime
import math
import os
import time

import serial

test_version = "0.0.7_shortpot"


class NanoACS(serial.Serial):
    csv_monolog = None  # set after opening log file

    def _readline_filtered(self) -> str:
        """Read a line, intercepting ;I idle-transition events and logging them."""
        while True:
            line = self.readline().decode("utf-8", errors="replace").strip()
            if line == ";I":
                now = time.monotonic_ns()
                print(f"idle\t\t\t\t time: {now}")
                if self.csv_monolog:
                    self.csv_monolog.write(
                        f"IDLE,,{now},,{datetime.datetime.now().isoformat()}\n"
                    )
                    self.csv_monolog.flush()
                continue
            return line

    @property
    def wiper(self) -> int | None:
        self.write(b":?\n")
        while True:
            line = self._readline_filtered()
            if not line:
                raise TimeoutError("wiper: no response from gate Nano")
            try:
                assert line.startswith(";P"), f"wiper: unexpected response: {line!r}"
                return int(line[2:])
            except ValueError:
                print(f"wiper skip: {line!r}")
            except AssertionError:
                print(f"wiper serial noise: {line!r}")
                return None


nanoACS = NanoACS("/dev/ttyUSB0", 115200, timeout=1)
# Opening the port triggers a DTR reset; wait for the Nano to boot and send its
# MOTD, then flush so the version query gets a clean response.
time.sleep(2)
nanoACS.reset_input_buffer()


def main():
    state: dict = {}
    try:
        # setup
        """
        try:
            with open("state.json", "r") as f:
                state = json.load(f)
        except FileNotFoundError:
            with open("state.json", "w") as f:
                json.dump(state, f)
        """
        ino = os.path.join(
            os.path.dirname(__file__), "../test-nanoACS/test-nanoACS.ino"
        )
        # monolog, append lines to csv file
        csv_monolog = open(f"monolog_{test_version}.csv", "a")
        nanoACS.csv_monolog = csv_monolog

        with open(ino) as f:
            version_full = next(
                l.split('"')[1] for l in f if l.startswith("#define VERSION")
            )
            version = version_full.split("\\")[0]
        nanoACS.write(b"V")
        response = nanoACS.readline().decode("utf-8", errors="replace").strip()
        assert response == version.strip(), (
            f"Version mismatch: got {response!r}, expected {version.strip()!r}"
        )

        nanoACS.write(b"O")  # set ACCESS to open signal
        assert (
            "ACCESS is now HIGH."
            == nanoACS.readline().decode("utf-8", errors="replace").strip()
        )  # ACS response which confirms the ACCESS signal was set HIGH

        # loop
        while True:
            now = time.monotonic_ns()
            PERIOD = 10e9  # 10 seconds
            angle_turns = (now % PERIOD) / PERIOD
            # sin wave
            # value = (math.sin(angle_turns * 2 * math.pi) + 1) / 2 * 100
            # square wave
            square_max, square_min = (5, 70)
            value = square_max if angle_turns < 0.5 else square_min

            # send value to gate
            nanoACS.write(f":S{int(value)}\n".encode())

            # assert that the response to the write matches the expected value
            response = nanoACS._readline_filtered()
            try:
                assert response == f";S{int(value)}", (
                    f"Write response mismatch: got {response!r}, expected ;S{int(value)}"
                )
                response_digits = response[2:]
            except AssertionError:
                response = None
                response_digits = None

            # read value from gate
            pos = nanoACS.wiper
            print(  # debug info should probably also be saved to csv log
                f"pos: {pos}\t set: {response} \t time: {now} \t phase: {
                    angle_turns:.2f}"
            )
            csv_monolog.write(
                f"{pos},{response_digits},{now},{angle_turns:.2f},{
                    datetime.datetime.now().isoformat()
                }\n"
            )
            csv_monolog.flush()

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
