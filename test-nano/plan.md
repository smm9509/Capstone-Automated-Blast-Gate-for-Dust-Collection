Author: Claude Opus 4.6
# blastGate rewrite plan

## Abstractable Components

### 1. PinGroup structs + `.setup()`

Group pins by physical subsystem. Each struct owns its pin numbers AND its `pinMode()` calls.

- **MotorDriver** — `ENA`, `IN1`, `IN2`. Absorbs `motorExtend()`, `motorRetract()`, `motorStop()` as methods. Direction becomes an enum. One method: `drive(Direction dir, uint8_t speed)` + `stop()`.
- **IndicatorPins** — `LED_RED`, `LED_YLW`, `LED_GRN`. Method to set pattern from state: `setFromState(State s)`. Currently wired + pinModed but never driven in loop — this fixes that.
- **JogPins** — `JOG_OPEN`, `JOG_CLOSE`. `readOpen()`, `readClose()`. Active-low logic hidden inside.
- **PositionSensor** — see section 2.
- **InterruptInput** — see section 3.

Serial (RX/TX) is not abstracted into a struct. `Serial` *is* the interface. Baud rate becomes a named constant:
```cpp
static const uint32_t SERIAL_BAUD = 4800;
```

Each struct gets a `.setup()` that calls its `pinMode`/`attachInterrupt`. Top-level `setup()` just calls each group's `.setup()`.

### 2. PositionSensor (wraps wiper + calibration)

Owns `wiperMin`, `wiperMax`, the EEPROM magic/address/struct.

- `readRaw()` — raw ADC value.
- `readPercent()` — does `analogRead` + `map` in one place.
- `isCalibrated()` — checks EEPROM magic.
- `calibrate(MotorDriver&, JogPins&)` — the interactive jog calibration routine, extracted from setup. Takes motor and jog as dependencies since it drives during calibration.
- `loadCalibration()` / `saveCalibration()` — EEPROM ops.
- Compile-time skip and hardcoded fallback become a CalibrationMode enum or constructor option.

### 3. InterruptInput (E_STOP + ACS_ACCESS)

Both interrupt pins follow the same pattern: pin triggers ISR, ISR sets flag, loop checks and clears flag.

```cpp
struct InterruptInput {
    uint8_t pin;
    volatile bool flag;
    uint8_t mode;         // FALLING, CHANGE, etc.
    void (*isr)();        // pointer to ISR function

    void setup();         // pinMode + attachInterrupt
    bool checkAndClear(); // atomically read & reset flag
};
```

Two instances: one for E_STOP (FALLING, INPUT_PULLUP), one for ACS_ACCESS (CHANGE, INPUT). `checkAndClear()` wraps `cli/sei` if true atomicity is needed (on AVR single-byte volatile is safe in practice, but document the assumption).

### 4. MotorController (the P-loop)

- Owns `kp`, `minSpeed`, `deadband`.
- `update(int targetPercent, PositionSensor& sensor)` → computes error, calls into MotorDriver.
- Speed measurement: store `prevPos` + `prevMicros`, differentiate for counts/sec. Feeds stall detection.
- Returns a status: REACHED_TARGET, STILL_MOVING, STALLED.

### 5. State Machine

#### Current states
`IDLE`, `MOVING`, `ESTOP`

#### Proposed states
| State | Entry condition | Behavior | Exit condition |
|---|---|---|---|
| `IDLE` | target reached, or startup after calibration | motor stopped, LEDs yellow, waiting for command | command received or ACS edge |
| `MOVING` | setpoint changed | P-loop drives motor, LEDs green/red by direction | target reached → IDLE, or estop/lockout |
| `ESTOP` | estop ISR fires | motor stops immediately, LEDs red | transitions to LOCKOUT |
| `LOCKOUT` | entered from ESTOP, or administrative disable | motor stopped, all commands rejected, LEDs red blinking | deliberate reset (serial `:R\n` or button combo) |
| `CALIBRATING` | first boot (no EEPROM magic) or serial `:C\n` | interactive jog calibration, runs in main loop instead of blocking setup | calibration complete → IDLE |
| `SERIAL_BUSY` | (optional) if serial processing ever needs multiple loop cycles | holds current motor state, signals "processing" | command fully parsed → previous state or new target |

`isNewState` pattern → formalize with `onEnter()` / `onExit()` hooks per state. Transition table if it gets complex.

#### Open: ESTOP vs LOCKOUT behavior
- ESTOP = immediate motor kill. Comment says "retracts actuator" but code just calls `motorStop()`. Pick one and document it.
- LOCKOUT = sustained disable. Requires explicit human action to re-enable.
- Does ESTOP auto-transition to LOCKOUT, or does it stay in ESTOP until the button is released and then go to LOCKOUT?

### 6. Serial Protocol — Framed Commands

#### Problem with current approach
- No start delimiter: can't distinguish message start from mid-noise.
- `Serial.parseInt()` blocks up to 1 second.
- Peek-and-branch on `?` vs `isDigit()` is ambiguous and CPU-wasteful.

#### Framing

| Byte | Role | ASCII |
|---|---|---|
| `:` | **START** — parser latches, begins accumulating | 0x3A |
| `\n` | **END** — parser dispatches complete command | 0x0A |

Anything outside a `:`...`\n` frame is discarded. Free noise rejection.

#### Parser state machine
```
WAITING  ──see ':'──▶  ACCUMULATING  ──see '\n'──▶  DISPATCH  ──▶  WAITING
                        │                                          ▲
                        └──buffer full──▶  DISCARD  ────────────────┘
anything in WAITING that isn't ':' → ignore
```

Non-blocking: one byte consumed per loop iteration max. At 4800 baud (~1 byte/2ms) the control loop stays responsive.

#### Command set

| Command | Frame | Description |
|---|---|---|
| Query position | `:?\n` | Returns current percent |
| Set target | `:S75\n` | Set target to 75% (0-100) |
| Calibrate | `:C\n` | Enter CALIBRATING state |
| Reset lockout | `:R\n` | Exit LOCKOUT → IDLE |
| Dump status | `:D\n` | Report state, position, setpoint, speed |

#### Why `:`
Printable, not a digit, not `\n`, rare in line noise, visually distinct in serial monitor. Easy to type.

#### Re: PMBus compatibility
PMBus is SMBus/I2C, not UART. The ESP32 link is UART at 4800 baud. PMBus's command/response addressing pattern could inspire the protocol, but the electrical layer is incompatible. If I2C is ever needed between boards, that's a separate bus. For UART: this framed ASCII protocol is simpler and sufficient.

### 7. LED / Status Indicator

Map states to LED patterns. Lookup table or function:

| State | RED | YLW | GRN |
|---|---|---|---|
| IDLE | off | on | off |
| MOVING (extend) | off | off | on |
| MOVING (retract) | on | off | off |
| ESTOP | on | off | off |
| LOCKOUT | blink | off | off |
| CALIBRATING | off | blink | off |

Currently not implemented in `loop()` at all — LEDs are set up but never written after `setup()`.

## Bugs / Debt to fix in rewrite

| Issue | Where | Fix |
|---|---|---|
| `int kp = 1.5` truncates to 1 | control vars | change to `float` or fixed-point |
| `kp` declared but never used in calculation | control loop | the speed calc uses `map()` not `kp * error` — decide which approach you actually want and use `kp` or delete it |
| `deadband = 0` makes deadband checks no-ops | control loop | set a real deadband (2-5 counts?) or remove the branching |
| `currentPos` read at END of loop, control logic uses stale value | end of `loop()` | read sensor FIRST, then run control |
| `Serial.parseInt()` blocks up to 1 second | serial handler | replace with non-blocking framed parser (section 6) |
| LEDs wired + pinModed but never driven in loop | indicators | implement section 7 |
| `error`/`speed` are globals but only used in MOVING | scoping | make local to MotorController |
| `openSetpoint` written by both serial and ACS_ACCESS — no priority defined | setpoint management | decide who wins, document it |
| `map(abs(error), 0, 100, ...)` uses 100 not wiper range | MOVING speed calc | intentional tuning knob or bug? 100 means full speed kicks in early. Document or fix. |

## Open Questions

1. **Lockout reset mechanism** — button combo? Serial `:R\n`? Both? Physical reset button on the board?
2. **Stall detection** — if motor is running but position isn't changing, what should happen? Speed measurement feeds this. Timeout to ESTOP? Retry? Just flag it over serial?
3. **ESTOP behavior** — retract to closed, or freeze in place? Comment says "retracts" but code calls `motorStop()`. For a blast gate on a dust collection system, closed-on-estop might be wrong (traps dust pressure). Decide.
4. **ACS_ACCESS vs serial priority** — who wins when both set a target? Last-write-wins? ACS overrides serial? Serial overrides ACS?
Liz: serial is more expensive, sets the preference. ACS sets the binary state, open or 0% close.
5. **BUSIO1 for Tom** — binary open/closed, or analog percent? What pin? When does this need to exist?
6. **Calibration without serial** — the TODO says "get user's attention even if serial not connected." LED morse code C? Blink pattern? The CALIBRATING state makes this possible but the UX needs defining.
7. **Response framing** — should responses FROM the Nano also be framed (`:R<data>\n`)? Or is unframed `Serial.println()` fine for the return direction? If the ESP32 needs to parse responses reliably, frame both directions.
Liz: YES YES the recipient is actually a lab test bench with automation, needs to be able to parse the data easily. To keep shit simple, use `;` as the response header and `\n` as the response endcap. 

## File Structure (proposed)

```
blastGate/
  blastGate.ino          // setup(), loop(), state machine, glue
  no header files --Liz
```

All `.h` with inline implementations (Arduino IDE compiles all `.ino` and `.h` in the sketch folder). No `.cpp` files needed unless you want them — keeps it simple for the Arduino toolchain.

Alternative: everything in one file with clear `//========` section dividers if multi-file Arduino builds annoy you. The abstractions still apply, just scoped with structs in a single translation unit.
```

## Features to save until Next Week --Liz
* obstruction/stall detection
* e-stop
* advanced calibration (just use constants we already have)
