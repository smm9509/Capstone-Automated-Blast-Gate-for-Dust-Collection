# Python script for the test plan of the automated blast gate
import json
import os

import serial

class NanoACS(serial.Serial):
    @property
    def wiper(self) -> int:
        self.write(b"?")
        return int(self.readline())

nanoACS = NanoACS("/dev/ttyUSB0", 115200, timeout=1)


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
        ino = os.path.join(os.path.dirname(__file__), "../test-nanoACS/test-nanoACS.ino")
        with open(ino) as f:
            version = next(l.split('"')[1] for l in f if l.startswith("#define VERSION"))
        nanoACS.write(b"V")
        assert nanoACS.readline().decode().strip() == version.strip(), "Version mismatch"

        # loop
        while True:
            raise NotImplementedError

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
