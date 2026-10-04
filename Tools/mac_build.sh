#!/bin/bash
# Build the Gem Roller editor target from Terminal and show the compiler errors, if any.
# Handy on macOS when Unreal's "could not be compiled" dialog does not say why.
#
#   Tools/mac_build.sh                       # newest Unreal install in /Users/Shared/Epic Games
#   UE_ROOT="/Users/Shared/Epic Games/UE_5.5" Tools/mac_build.sh
set -u

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/GemRoller.uproject"
LOG="${TMPDIR:-/tmp}/gemroller_build.log"

if [ -z "${UE_ROOT:-}" ]; then
  # newest UE_x.y folder (sort -V handles 5.10 > 5.9)
  UE_ROOT="$(ls -d "/Users/Shared/Epic Games"/UE_* 2>/dev/null | sort -V | tail -n 1)"
fi
BUILD="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"

if [ -z "$UE_ROOT" ] || [ ! -x "$BUILD" ]; then
  echo "Could not find Unreal Engine. Install it from the Epic Games Launcher, or set UE_ROOT, e.g."
  echo "  UE_ROOT=\"/Users/Shared/Epic Games/UE_5.4\" Tools/mac_build.sh"
  exit 2
fi

echo "Engine : $UE_ROOT"
echo "Project: $UPROJECT"
echo "Log    : $LOG"
echo "Building GemRollerEditor (Mac, Development)... the first build takes a few minutes."
"$BUILD" GemRollerEditor Mac Development -project="$UPROJECT" -waitmutex 2>&1 | tee "$LOG"
STATUS=${PIPESTATUS[0]}

echo
if [ "$STATUS" -eq 0 ]; then
  echo "BUILD SUCCEEDED. Open GemRoller.uproject."
else
  echo "BUILD FAILED (exit $STATUS). Errors:"
  grep -E "error:|error C[0-9]+|fatal error" "$LOG" | sort -u | head -40
  echo
  echo "Send the lines above (or the whole log at $LOG) back and I will fix them."
fi
exit "$STATUS"
