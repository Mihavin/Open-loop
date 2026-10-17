# Motion-control corrections and validation

Review basis: `Mihavin/Open-loop` commit `1813150a2547496dd6d3d6402fcc951a5b015627`,
FRW-4.0.0 schematic/board and the revision 2 engineering review. Requirements are
15 V / 300 W, a 5065 140 kV motor, 2:1 motor-to-shaft reduction and **1,000 shaft
rpm**. This change addresses all four critical and six high firmware findings.
It does not release the hardware for production.

## Finding coverage

| Finding | Implemented correction | Validation |
| --- | --- | --- |
| F01, critical: DRV transaction corruption | Native 16-bit transmit/receive objects with `Size=1`; verify registers 2-6, lock state and fault status. Configuration/readback failures inhibit motion. The communication helper no longer temporarily changes protection settings. | Production driver runs against a mock SPI register bank; frame count, data ordering, altered register, stuck MISO and timeout cases. |
| F02, critical: faults do not stop both axes | Critical reports immediately clear TIM1 MOE, disable BLDC enable, disable the stepper and inhibit its timer without HAL delays or RTOS calls. TIM1 active-low break is enabled with automatic output recovery disabled. Limits are handled in EXTI before task notification and also polled to catch a switch held at boot. DRV status is checked by MotorTask; TMC status by StepperTask. | Fault register/output assertions, held-limit rejection, arm-after-fault rejection; physical break/latency measurement still required. |
| F03, critical: startup/exception energization | No automatic arm; enabling autostart now fails compilation. PWM channels start behind a disabled driver, then MOE stays off during configuration/calibration. Every arm reconfigures and verifies the DRV because sleeping it resets registers. Reset, exception, allocation failure and assertion paths inhibit outputs. Stepper ENN initializes inactive. Independent watchdog is fed only with fresh Motor/HID/Stepper health. | Startup ordering, zero-output idle, failed re-arm, request precedence and watchdog-health scenarios. |
| F11, critical: operator counter rollover | Wrap-aware delta for the actual 10,000-count modulus, signed bounded setpoints and no raw-counter commands. NaN/infinite input is rejected or zeroed before conversion. API input explicitly owns the command until an idle handover to the knob. | All 10,000 counter positions tested in each direction; bounds, nonfinite inputs and ownership tests. |
| F04, high: stepper clocks and SPI | MCO2 is 12.5 MHz; SPI3 mode 3 at approximately 1.004 MHz. Driver rejects the wrong SPI mode and checks IOIN version, GCONF and CHOPCONF before use. | Datasheet clock inequalities, production initialization/readback under the mock HAL and full ARM compilation. |
| F05, high: disrupted STEP pulses / estimated position | TIM12 is a free-running 1 MHz compare timebase. Hardware toggle deadlines preserve pulse phase; ISR counts rising edges and stops finite moves at the target after completing the pulse. Position comes from emitted edges, not integrated velocity. Direction changes wait for the prior pulse train to stop. Late compare service latches a fault. | Exact finite counts at 10/100/1,000/2,000 Hz, forward/reverse moves, timer wrap, repeated frequency changes and decelerating stop. |
| F06, high: control scaling | Nominal bus 15 V, ADC reference 3.0 V, 16-bit ADC; current-loop dt derived from the timer clock/configuration, slow-loop dt from elapsed time. ADC1/2/3 completions rendezvous before consuming a current sample; invalid offsets, missing/stale conversions and excessive measured current inhibit motion. Motor speed request is limited to 2,000 rpm. Electrical angle remains continuous over both signed and hardware encoder wraps. | Numeric dt, current/ADC fault, input limit and rotor-wrap tests. Actual current calibration, sampling window and regulator tuning require measurements. |
| F07, high: unsafe blocking mode change | UI mode change requests STOP, waits without blocking EXTI for absolute speed <=10 motor rpm for 100 ms, then changes mode while disarmed. A 2 s timeout latches a fault. It neither zeroes the encoder nor automatically re-arms. Moving mode/zero requests are rejected by the motor API. | Moving-mode/zero rejection and STOP/DISARM versus ARM precedence; physical UI/standstill timing remains a bench check. |
| F08, high: competing owners / incomplete restart | UI stepper operations use a bounded queue consumed only by StepperTask; snapshots are published separately. Queue overflow inhibits motion. STOP inhibits PWM while keeping acquisition and encoder running, so a subsequent explicit arm has a complete control chain. | Queued command ordering, no mutation before owner service, queue overflow, API ownership and stop/re-arm scenarios. |
| F09, high: memory configuration | QSPI aperture, MPU extent and linker region agree at 32 MiB. SDRAM is Normal, non-cacheable, non-bufferable, execute-never memory. | Capacity contracts, full compile/link and SDRAM timing contracts; physical memory stress/boundary tests remain required. |

Additional firmware corrections cover the false fan interrupt (H04), the SDRAM
timing violation (H05) and current protection configuration (H03/H09). The fan net
is a PWM command monitor and no longer reports ordinary PWM edges as a fault.
SDRAM tRAS/tRC/tWR are 5/7/3 cycles at 96.875 MHz, including the FMC interdependent
constraints. DRV protection uses latched shutdown, 60 mV VDS threshold, 2 us
deglitch, minimum 0.25 V shunt threshold and 120/240 mA source/sink gate drive.
A separate 3.2 A measured phase-current trip operates below the nominal 7.81 A
linear measurement range. Those hardware thresholds are not precise current
limits or a demonstrated short-circuit rating.

## Operating changes to account for

- Startup performs offset calibration and remains disarmed. An explicit
  `UI_BldcArm()` / `MotorFOC_RequestArm()` is required; arming includes the existing
  700 ms alignment operation, which can move the motor. Production run-button
  integration belongs to the previously incomplete UI (F10).
- Normal STOP/DISARM removes torque and coasts. It keeps the DRV awake and forces
  all six PWM inputs low through TIM1 off-state settings, retaining live ADC and
  encoder feedback. A critical fault additionally removes both driver enables.
  Mechanical stopping distance is not implied by electrical inhibition.
- Critical faults latch until MCU reset or power cycle. `Fault_Clear()` only
  acknowledges noncritical bits. Neither releasing a limit nor clearing a
  display message restarts motion. Limit inputs are treated as active-low, as
  indicated by the existing falling-edge configuration; verify the harness and
  inactive logic level.
- Existing BLDC APIs continue to use **motor** rpm and absolute motor encoder
  counts/revolutions. The knob's speed increment is one **shaft** rpm, converted
  by the 2:1 ratio. Its position increment is 0.1 shaft revolution.
- The 1.6 A d/q command limit, 3.2 A measured-current trip, 500 motor-rpm/s speed
  ramp and +/-10 motor-revolution position window are conservative bring-up
  policies, not qualified machine ratings. The position window is centered on
  the location at a disarmed mode change. The stepper target bound is +/-1,000,000
  emitted steps. Replace these bounds with approved machine limits after homing
  and mechanical validation; they do not implement a physical travel sensor.
- The compare engine accepts 8-2,000 pulses/s. Lower nonzero ramp-tail frequencies
  use 8 Hz; position remains tied to emitted pulses. The old timed
  `TMC2130_MoveBlocking()` API returns `HAL_ERROR`; use the queued counted-motion
  APIs. This API had no production callers in the reviewed application.
- The existing 32-bit public position API still needs managed homing/zeroing for
  long-running absolute motion. Electrical phase and speed deltas handle counter
  wrap; this does not make the public position an unlimited multi-turn value.

## Production-readiness follow-up, 17 October 2026

| Area | Change | Still needs |
| --- | --- | --- |
| Current sampling | The injected ADC trigger (TIM1 OC4REF, PWM mode 2) now fires `MOTOR_FOC_ADC_TRIGGER_LEAD_COUNTS` (300) before the counter peak, so the sample is centred on the low-side conduction window. Previously it fired about 3.2 us after the peak, outside the window above about 75% duty. Set at `MotorFOC_Start()`, so CubeMX regeneration of `tim.c` cannot undo it. | Scope CSA output against TRGO2 and set the lead to half the real injected sampling time. |
| Duty limit | Phase duty is capped at `MOTOR_FOC_DUTY_MAX` (0.95) for bootstrap refresh and a minimum low-side window. The current-loop voltage limit is derived from it (0.52 x Vbus instead of 0.577 x Vbus), so there is no clamp distortion or windup. | About 10% less voltage headroom; confirm the speed target at minimum supply. |
| Phase reconstruction | A phase whose duty exceeded `MOTOR_FOC_SAMPLE_DUTY_MAX` (0.88) when it was sampled is rebuilt from the other two. If two phases are untrusted, the previous currents are held, and more than four consecutive held periods latch `FAULT_CURRENT`. Raw-range checks apply only to trusted phases. | Validate the threshold with the measured sampling window. |
| Interrupt priority | ADC_IRQn and ADC3_IRQn run at priority 2 (`MOTOR_FOC_ADC_IRQ_PRIORITY`), above every other peripheral interrupt (5), so display/DMA/SPI/EXTI handlers cannot delay the current loop or provoke false `FAULT_ADC`. The ADC callback uses no FreeRTOS API. | Measure ISR execution time and jitter under full GUI load. |
| QSPI | The flash must answer with a 256 Mbit Cypress/Infineon JEDEC ID, and reset, init and memory-mapped mode must succeed. Otherwise `Error_Handler()` latches outputs off, instead of a later bus fault inside TouchGFX. | None. |
| HAL asserts | `assert_failed()` now calls `Error_Handler()` instead of silently continuing. | None. |
| Build | `tools/build_firmware.py --config release` builds `-O2 -DNDEBUG`. Sources come from `tools/firmware_sources.txt`, not the IDE's `Debug/objects.list`. `Debug/`, `.metadata/`, object files, `Middlewares/ST/touchgfx_backup/` and `backup_FRW4_0_RTOS.ioc` are no longer tracked. | Ship the release image only after the timing above is measured on it. |

Verified on 17 October 2026: native scenarios (now nine groups) pass with GCC `-Werror`. Both debug and release configurations compile all 140 translation units and link with arm-none-eabi-gcc 13.2.1. The only warnings are the same unused-parameter ones in generated code.

## Reproduce software validation

From `FRW4_0_RTOS`, with Python 3.9+ and a native GCC:

```text
python tests/run_tests.py --cc gcc
```

This compiles the actual driver, motor, motion, fault and UI implementations
against deterministic fake peripherals with `-Wall -Wextra -Werror`. It runs
eight behavioral scenario groups plus four independent configuration tests.
It also verifies that a build attempting to enable automatic arming is rejected.
The tests cover logic/register interactions; they do not emulate the motor,
analog circuit, interrupt execution time or electrical pin waveforms.

For the full firmware, supply the directory containing GNU Arm compiler tools:

```text
python tools/build_firmware.py --toolchain <arm-toolchain-bin-directory> [--config release]
```

The build compiles all 140 production translation units listed in the project's
IDE manifest and links the image using the checked-in libraries and linker
script. It uses fresh objects under `build-validation/`; it never links old
`Debug` objects or follows the original developer-specific absolute paths.
No compiler download, flashing or hardware operation is performed by either
script. `build-validation/build-results.json` contains full compiler diagnostics.
The committed `validation-results.json` records the completed checks and source
hashes without local-machine paths. Large generated images/binaries are excluded.

## Completed validation, 16 October 2026

| Check | Result |
| --- | --- |
| Native production-code regression executable, MinGW GCC 13.2.0 | All eight scenario groups passed; compile warnings treated as errors. |
| Encoder rollover coverage | 20,000 adjacent forward/reverse transitions plus both 32-bit rotor counter boundaries passed. |
| Configuration contracts | Four tests passed: SDRAM minima/FMC constraints, NOR capacity, stepper clock/SPI timing and output/break defaults. |
| GNU Tools for STM32 GCC/G++ 14.3.1 | All 140 C/C++/startup translation units compiled; final firmware linked successfully. |
| Diff integrity | `git diff --check` passed. |

The full ARM build reports unused-parameter warnings from generated/template
code and headers. These are recorded in the build diagnostics; the result is
not a claim of a warning-free ARM build.

## Hardware validation still required before powered release

1. Capture all six gate signals, EN, nFAULT/BKIN, STEP and DIR during startup,
   calibration, arm, normal stop, held/edge limits, reset, brownout and injected
   CPU/task failures. Confirm actual off-state levels, direction setup, pulse
   widths and worst shutdown latency. Validate watchdog timeout over LSI
   tolerance. Repeat with the debugger attached; production watchdog is not
   intentionally frozen.
2. Capture DRV and TMC serial transactions/readbacks under switching load. Check
   the new gate currents, dead time, 60 mV threshold, deglitch and latched
   protection against ringing, false trips and device SOA before raising load.
   The minimum shunt hardware threshold is still 62.5 A at 4 mOhm; it cannot
   replace application current limiting or input fault-energy protection.
3. Measure current against an independent reference. Verify ADC settling and
   valid low-side-shunt sampling windows at every allowed duty, ISR timing under
   maximum GUI/memory load, sensor-loss response, actual pole pairs and encoder
   CPR. Retune control loops after confirming units and actual sample cadence.
   Simultaneous ADC completion does not establish a valid analog sampling window.
4. Verify loaded 1,000 shaft rpm at minimum supply voltage, current/temperature
   limits, normal coast-down time and regeneration/brake behavior. The bus model
   remains nominal 15 V because the present board lacks a validated bus-voltage
   measurement path. The 300 W PSU rating does not prove 300 W motor capability.
5. Stress SDRAM and QSPI across temperature while the display and both motors
   operate; test NOR above 16 MiB and at the 32 MiB boundary with the actual
   external loader/assets. SDRAM CKE remains on restricted PC3_C because changing
   its pin assignment requires a matching PCB revision. The obsolete SDRAM,
   fan rotation feedback, brake energy, routing, purchasing BOM and manufacturing
   findings remain hardware/sourcing work outside this firmware PR.

After CubeMX regeneration, rerun both scripts and inspect the custom control
paths, ADC3 interrupt and compare-engine configuration. The `.ioc` carries the
changed pin defaults, SPI, clock, break and memory settings, but generation is
not a substitute for reviewing the custom application code.
