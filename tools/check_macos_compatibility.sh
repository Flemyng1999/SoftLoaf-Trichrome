#!/usr/bin/env bash
set -euo pipefail

app="${1:-build/softloaf_trichrome_app.app}"
max_minos="${SOFTLOAF_MACOS_MINOS_MAX:-15.0}"

if [[ ! -d "${app}/Contents" ]]; then
  echo "check-macos-compatibility: missing app bundle: ${app}" >&2
  exit 2
fi

version_gt() {
  local a="$1" b="$2"
  awk -v a="${a}" -v b="${b}" '
    BEGIN {
      split(a, av, "."); split(b, bv, ".");
      for (i = 1; i <= 3; ++i) {
        ai = av[i] == "" ? 0 : av[i] + 0;
        bi = bv[i] == "" ? 0 : bv[i] + 0;
        if (ai > bi) exit 0;
        if (ai < bi) exit 1;
      }
      exit 1;
    }'
}

failures=0
while IFS= read -r -d '' f; do
  if ! file "$f" | grep -q 'Mach-O'; then
    continue
  fi
  minos="$(vtool -show-build "$f" 2>/dev/null | awk '/minos /{print $2; exit}')"
  if [[ -z "${minos}" ]]; then
    continue
  fi
  if version_gt "${minos}" "${max_minos}"; then
    echo "check-macos-compatibility: ${minos} > ${max_minos}: ${f}" >&2
    failures=$((failures + 1))
  fi
done < <(find "${app}/Contents" -type f -print0)

if [[ "${failures}" -ne 0 ]]; then
  echo "check-macos-compatibility: FAIL (${failures} Mach-O files require newer macOS than ${max_minos})" >&2
  exit 1
fi

echo "check-macos-compatibility: PASS ${app} (all Mach-O minos <= ${max_minos})"
