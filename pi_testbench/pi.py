# Python script for the test plan of the automated blast gate
import json
import os

import serial

nanoACS = serial.Serial("/dev/ttyUSB0", 115200, timeout=1)


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
        nanoACS.write(b"H")
        assert nanoACS.read(4) == b"Mock", "Help MOTD response not received"

        # loop
        while True:
            raise NotImplementedError

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
