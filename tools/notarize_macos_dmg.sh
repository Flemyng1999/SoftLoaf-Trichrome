#!/usr/bin/env bash
set -euo pipefail

dmg="${1:-build/SoftLoaf-Trichrome-0.1.0-alpha.1-macOS.dmg}"

if [[ ! -f "${dmg}" ]]; then
  echo "notarize-macos: missing DMG: ${dmg}" >&2
  exit 2
fi

submit_args=(xcrun notarytool submit "${dmg}" --wait)
if [[ -n "${SOFTLOAF_NOTARY_KEYCHAIN_PROFILE:-}" ]]; then
  submit_args+=(--keychain-profile "${SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}")
else
  : "${APPLE_ID:?set APPLE_ID or SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}"
  : "${APPLE_TEAM_ID:?set APPLE_TEAM_ID or SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}"
  : "${APPLE_APP_SPECIFIC_PASSWORD:?set APPLE_APP_SPECIFIC_PASSWORD or SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}"
  submit_args+=(--apple-id "${APPLE_ID}" --team-id "${APPLE_TEAM_ID}" --password "${APPLE_APP_SPECIFIC_PASSWORD}")
fi

echo "notarize-macos: submitting ${dmg}"
"${submit_args[@]}"

echo "notarize-macos: stapling ${dmg}"
xcrun stapler staple "${dmg}"
xcrun stapler validate "${dmg}"

echo "notarize-macos: assessing ${dmg}"
spctl -a -t open --context context:primary-signature -v "${dmg}"

shasum -a 256 "${dmg}" | tee "${dmg}.sha256"
echo "notarize-macos: PASS ${dmg}"
