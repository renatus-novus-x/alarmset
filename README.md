# alarmset

[English](README.md) | [日本語](README.ja.md)

<p align="center">
  <strong><a href="https://uraraworks.github.io/WebX68k/?cpu=10&ram=12&fd1=https://raw.githubusercontent.com/renatus-novus-x/alarmset/main/dist/alarmset.zip&run=1">Launch alarmset in WebX68k</a></strong>
</p>

A minimal Human68k utility for configuring RTC alarm-based scheduled power-on
on the Sharp X68000.

## Usage

```text
alarmset                 Show status and usage
alarmset status          Show the saved alarm
alarmset HH:MM           Set a daily alarm
alarmset DAY HH:MM       Set a weekly alarm (sun, mon, ..., sat)
alarmset DATE HH:MM      Set a monthly alarm (DATE = 1..31)
alarmset off             Disable the alarm
alarmset -?              Show usage
alarmset --help          Show usage
```

Examples:

```text
alarmset 07:30
alarmset mon 06:45
alarmset 15 08:00
alarmset off
```

The command uses the X68000 ROM IOCS `_ALARMMOD`, `_ALARMSET`, and
`_ALARMGET` calls. A new schedule is configured for normal boot, without TV
control or an automatic power-off timer. The setting is retained in the
battery-backed SRAM.

After setting an alarm, leave the rear power switch on and turn the machine off
through its supported soft-power path. RTC wake-up depends on the machine's RTC,
standby power, and power-control circuitry. Replacement power supplies and
emulators may behave differently. Confirm the clock and alarm status before
relying on unattended startup.

Running the generated disk image invokes `alarmset.x` without arguments from
`AUTOEXEC.BAT`. This only displays status and usage; it never changes the
alarm.

## Build

Build under WSL or Linux with the elf2x68k toolchain installed and
`m68k-xelf-gcc` available in `PATH`.

Required host tools:

```sh
sudo apt install python3 curl unar
```

From the repository root:

```sh
cd src
make
```

The Makefile downloads Human68k 3.02 and a pinned `xdftool.py`, then creates:

```text
src/alarmset.x     Human68k executable
src/alarmset.xdf   Bootable Human68k disk image
dist/alarmset.zip  Distribution archive with the Human68k license
```

Inspect the generated disk image with:

```sh
make check-xdf
```

Remove generated files with `make clean`, or also remove downloaded support
files with `make distclean`.
