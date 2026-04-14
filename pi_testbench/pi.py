# Python script for the test plan of the automated blast gate
import json
import os
from tkinter.constants import N

import serial

nanoACS = serial.Serial("/dev/ttyUSB0", 115200, timeout=1)
state: dict = {}


def main():
    try:
        # setup
        with open("state.json", "r") as f:
            state = json.load(f)
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
