#!/bin/bash
# Flash CrossPoint (Korean fork) + MicroSlate as a dual boot on an Xteink X4.
#
# Uses the Korean fork's partition table (two 0x6A0000 app slots):
#   0x8000    partition table   (from the CrossPoint repo's partitions.csv)
#   0x10000   app0  CrossPoint  (patched firmware.bin from a gh_release build)
#   0x6B0000  app1  MicroSlate  (.pio/build/xteink_x4/firmware.bin)
#   0xE000    otadata erased, so the device boots CrossPoint first
# The bootloader at 0x0 and NVS (settings, Bluetooth pairing) are left alone,
# the same as the crosspoint.kr web flasher.
#
# usage: scripts/dualboot-ko.sh <crosspoint-repo> [serial-port]
#        DRY_RUN=1 scripts/dualboot-ko.sh ...   (check sizes, print the command)
set -euo pipefail

CROSSPOINT=${1:?usage: $0 <crosspoint-repo> [serial-port]}
PORT=${2:-}
HERE="$(cd "$(dirname "$0")/.." && pwd)"
PIO_HOME="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
PY="$PIO_HOME/penv/bin/python"
ESPTOOL="$PIO_HOME/packages/tool-esptoolpy/esptool.py"
GEN_PART="$PIO_HOME/packages/framework-espidf/components/partition_table/gen_esp32part.py"

CP_BIN="$CROSSPOINT/.pio/build/gh_release/firmware.bin"
MS_BIN="$HERE/.pio/build/xteink_x4/firmware.bin"
SLOT_SIZE=$((0x6A0000))

for f in "$CP_BIN" "$MS_BIN" "$CROSSPOINT/partitions.csv"; do
  [ -f "$f" ] || { echo "missing: $f" >&2; exit 1; }
done
grep -q "app1, *app, *ota_1, *0x6B0000, *0x6A0000" <(tr -d ' ' < "$CROSSPOINT/partitions.csv" | sed 's/,/, /g') \
  || { echo "unexpected partition table in $CROSSPOINT/partitions.csv" >&2; exit 1; }

for f in "$CP_BIN" "$MS_BIN"; do
  size=$(stat -f %z "$f" 2>/dev/null || stat -c %s "$f")
  if [ "$size" -gt "$SLOT_SIZE" ]; then
    echo "$f is $size bytes, larger than the 0x6A0000 slot" >&2
    exit 1
  fi
  printf '%-60s %8d bytes (%d%% of slot)\n' "$(basename "$(dirname "$f")")/$(basename "$f")" "$size" $((size * 100 / SLOT_SIZE))
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
"$PY" "$GEN_PART" --flash-size 16MB "$CROSSPOINT/partitions.csv" "$WORK/partitions.bin" >/dev/null

PORT_ARGS=()
[ -n "$PORT" ] && PORT_ARGS=(--port "$PORT")
ERASE=("$PY" "$ESPTOOL" --chip esp32c3 ${PORT_ARGS[@]+"${PORT_ARGS[@]}"} --baud 921600 erase_region 0xe000 0x2000)
WRITE=("$PY" "$ESPTOOL" --chip esp32c3 ${PORT_ARGS[@]+"${PORT_ARGS[@]}"} --baud 921600 write_flash
  --flash_mode dio --flash_size 16MB
  0x8000 "$WORK/partitions.bin"
  0x10000 "$CP_BIN"
  0x6B0000 "$MS_BIN")

if [ "${DRY_RUN:-0}" = 1 ]; then
  echo "${WRITE[*]}"
  echo "${ERASE[*]}"
  exit 0
fi
"${WRITE[@]}"
"${ERASE[@]}"
