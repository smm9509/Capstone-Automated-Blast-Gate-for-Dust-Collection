# Python script for the test plan of the automated blast gate
import json
import os
import time

import serial


class NanoACS(serial.Serial):
    @property
    def wiper(self) -> int:
        self.write(b"?")
        return int(self.readline())


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

        # loop
        while True:
            raise NotImplementedError

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
