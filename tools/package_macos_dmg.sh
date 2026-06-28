#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
version="${SOFTLOAF_TRICHROME_VERSION:-0.1.0-alpha.1}"
app_target="softloaf_trichrome_app"
app="${build_dir}/${app_target}.app"
dist_app_name="SoftLoaf Trichrome.app"
dmg="${build_dir}/SoftLoaf-Trichrome-${version}-macOS.dmg"
staging="${build_dir}/dmg_staging"
identity="${SOFTLOAF_CODESIGN_IDENTITY:--}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
  echo "package-macos: missing ${build_dir}/CMakeCache.txt" >&2
  exit 2
fi

deployment_target="$(awk -F= '/^CMAKE_OSX_DEPLOYMENT_TARGET:/{print $2}' "${build_dir}/CMakeCache.txt" | tail -1 || true)"
if [[ "${deployment_target}" != "15.0" ]]; then
  echo "package-macos: build must target macOS 15.0; got '${deployment_target:-unset}'" >&2
  echo "package-macos: reconfigure with cmake -S . -B ${build_dir} -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 ..." >&2
  exit 2
fi

cmake --build "${build_dir}" --target "${app_target}"

if [[ ! -d "${app}/Contents" ]]; then
  echo "package-macos: missing app bundle: ${app}" >&2
  exit 2
fi

if [[ -n "${SOFTLOAF_MACDEPLOYQT:-}" ]]; then
  macdeployqt="${SOFTLOAF_MACDEPLOYQT}"
else
  qt6_dir="$(awk -F= '/^Qt6_DIR:/{print $2}' "${build_dir}/CMakeCache.txt" | tail -1 || true)"
  macdeployqt=""
  if [[ -n "${qt6_dir}" ]]; then
    qt_prefix="$(cd "${qt6_dir}/../../.." && pwd)"
    for cand in \
      "${qt_prefix}/bin/macdeployqt" \
      "${qt_prefix}/tools/Qt6/bin/macdeployqt" \
      "${qt_prefix}"/*/tools/Qt6/bin/macdeployqt; do
      [[ -x "${cand}" ]] && { macdeployqt="${cand}"; break; }
    done
  fi
  if [[ -z "${macdeployqt}" ]]; then
    macdeployqt="$(brew --prefix qt)/bin/macdeployqt"
  fi
fi

if [[ ! -x "${macdeployqt}" ]]; then
  echo "package-macos: macdeployqt not found: ${macdeployqt}" >&2
  exit 2
fi

echo "package-macos: using macdeployqt: ${macdeployqt}"
"${macdeployqt}" "${app}" -verbose=1 -qmldir="${repo_root}/qml"

sign_args=(--force --sign "${identity}")
if [[ "${identity}" != "-" ]]; then
  sign_args+=(--options runtime --timestamp)
fi

codesign_retry() {
  local attempt
  for attempt in 1 2 3; do
    if codesign "$@"; then
      return 0
    fi
    sleep $((attempt * 5))
  done
  return 1
}

for root in "${app}/Contents/Frameworks" "${app}/Contents/PlugIns" "${app}/Contents/Resources/qml"; do
  [[ -d "${root}" ]] || continue
  find "${root}" -type f -print0 | while IFS= read -r -d '' f; do
    if file "$f" | grep -q 'Mach-O'; then
      codesign_retry "${sign_args[@]}" "$f"
    fi
  done
done

codesign_retry "${sign_args[@]}" "${app}"
codesign --verify --deep --strict --verbose=2 "${app}"
"${repo_root}/tools/check_macos_compatibility.sh" "${app}"

rm -rf "${staging}"
mkdir -p "${staging}"
cp -R "${app}" "${staging}/${dist_app_name}"
ln -s /Applications "${staging}/Applications"

rm -f "${dmg}"
hdiutil create \
  -volname "SoftLoaf Trichrome ${version}" \
  -srcfolder "${staging}" \
  -ov -fs HFS+ -format UDZO \
  "${dmg}"
hdiutil verify "${dmg}"
rm -rf "${staging}"

if [[ "${identity}" != "-" ]]; then
  codesign_retry --force --sign "${identity}" --timestamp "${dmg}"
  codesign --verify --verbose=2 "${dmg}"
fi

shasum -a 256 "${dmg}" | tee "${dmg}.sha256"
echo "package-macos: ${dmg}"
