# Python script for the test plan of the automated blast gate
import json
import os
from tkinter.constants import N

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
        nanoACS.open()
        nanoACS.write(b"H")
        assert nanoACS.read(4) == "Mock", "Help MOTD response not received"

        # loop
        while True:
            raise NotImplementedError

    finally:
        nanoACS.close()


if __name__ == "__main__":
    main()
