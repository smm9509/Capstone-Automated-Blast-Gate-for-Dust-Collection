# Python script for the test plan of the automated blast gate
import json
import math
import os
import time

import serial


class NanoACS(serial.Serial):
    @property
    def wiper(self) -> int:
        self.write(b":?\n")
        while True:
            line = self.readline().decode().strip()
            if not line:
                raise TimeoutError("wiper: no response from gate Nano")
            try:
                assert line.startswith(";P"), f"wiper: unexpected response: {line!r}"
                return int(line[2:])
            except ValueError:
                print(f"wiper skip: {line!r}")


nanoACS = NanoACS("/dev/ttyUSB0", 115200, timeout=1)
# Opening the port triggers a DTR reset; wait for the Nano to boot and send its
# MOTD, then flush so the version query gets a clean response.
time.sleep(2)
nanoACS.reset_input_buffer()


def main():
    state: dict = {}
    try:
        # setup
        try:
            with open("state.json", "r") as f:
                state = json.load(f)
        except FileNotFoundError:
            with open("state.json", "w") as f:
                json.dump(state, f)
        ino = os.path.join(
            os.path.dirname(__file__), "../test-nanoACS/test-nanoACS.ino"
        )
        with open(ino) as f:
            version_full = next(
                l.split('"')[1] for l in f if l.startswith("#define VERSION")
            )
            version = version_full.split("\\")[0]
        nanoACS.write(b"V")
        response = nanoACS.readline().decode().strip()
        assert response == version.strip(), (
            f"Version mismatch: got {response!r}, expected {version.strip()!r}"
        )

        nanoACS.write(b"O")  # set ACCESS to open signal
        nanoACS.readline()

        # loop
        while True:
            now = time.monotonic_ns()
            PERIOD = 10e9  # 10 seconds
            angle_turns = (now % PERIOD) / PERIOD
            # sin wave
            # value = (math.sin(angle_turns * 2 * math.pi) + 1) / 2 * 100
            # square wave
            value = 100 if angle_turns < 0.5 else 0

            # send value to gate
            nanoACS.write(f":S{int(value)}\n".encode())

            # assert that the response to the write matches the expected value
            response = nanoACS.readline().decode().strip()
            assert response == f";S{int(value)}", (
                f"Write response mismatch: got {response!r}, expected ;S{int(value)}"
            )

            # read value from gate
            pos = nanoACS.wiper
            print(
                f"pos: {pos}\t set: {response} \t time: {now} \t phase: {angle_turns:.2f}"
            )  # evenly spaced columns
            # expecting chaos because that write causes a different response and the response is delayed like 1600ms or so

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
