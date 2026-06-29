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
link_failures=0
frameworks_dir="${app}/Contents/Frameworks"
executable_dir="${app}/Contents/MacOS"

rpath_exists_for_dependency() {
  local macho="$1"
  local dep="$2"
  local dep_tail="${dep#@rpath/}"
  local loader_dir
  loader_dir="$(dirname "${macho}")"

  while IFS= read -r rpath; do
    local resolved="${rpath}"
    resolved="${resolved//@executable_path/${executable_dir}}"
    resolved="${resolved//@loader_path/${loader_dir}}"
    if [[ -e "${resolved}/${dep_tail}" ]]; then
      return 0
    fi
  done < <(otool -l "${macho}" 2>/dev/null | awk '
    $1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
    in_rpath && $1 == "path" { print $2; in_rpath = 0 }
  ')

  return 1
}

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

  while IFS= read -r dep; do
    dep_base="$(basename "${dep}")"
    case "${dep}" in
      /opt/homebrew/*|/usr/local/*)
        if [[ -f "${frameworks_dir}/${dep_base}" ]]; then
          echo "check-macos-compatibility: bundled dependency still uses absolute local path: ${f} -> ${dep}" >&2
          link_failures=$((link_failures + 1))
        fi
        ;;
      @rpath/*)
        if [[ -f "${frameworks_dir}/${dep_base}" ]] && ! rpath_exists_for_dependency "${f}" "${dep}"; then
          echo "check-macos-compatibility: @rpath dependency cannot resolve inside bundle: ${f} -> ${dep}" >&2
          link_failures=$((link_failures + 1))
        fi
        ;;
    esac
  done < <(otool -L "$f" 2>/dev/null | awk 'NR > 1 { print $1 }')
done < <(find "${app}/Contents" -type f -print0)

if [[ "${failures}" -ne 0 ]]; then
  echo "check-macos-compatibility: FAIL (${failures} Mach-O files require newer macOS than ${max_minos})" >&2
  exit 1
fi

if [[ "${link_failures}" -ne 0 ]]; then
  echo "check-macos-compatibility: FAIL (${link_failures} bundled Mach-O dependencies do not resolve portably)" >&2
  exit 1
fi

echo "check-macos-compatibility: PASS ${app} (all Mach-O minos <= ${max_minos})"
