#!/usr/bin/env bash
set -euo pipefail

dmg="${1:-build/SoftLoaf-Trichrome-0.2.0-beta.2-macOS.dmg}"

if [[ ! -f "${dmg}" ]]; then
  echo "notarize-macos: missing DMG: ${dmg}" >&2
  exit 2
fi

submit_args=(xcrun notarytool submit "${dmg}" --wait)
if [[ -n "${SOFTLOAF_NOTARY_KEYCHAIN_PROFILE:-}" ]]; then
  submit_args+=(--keychain-profile "${SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}")
elif [[ -n "${APPLE_API_KEY_ID:-}" || -n "${APPLE_API_KEY:-}" || -n "${APPLE_API_ISSUER_ID:-}" ]]; then
  : "${APPLE_API_KEY_ID:?set APPLE_API_KEY_ID for App Store Connect API key notarization}"
  : "${APPLE_API_KEY:?set APPLE_API_KEY for App Store Connect API key notarization}"
  api_key_file="$(mktemp)"
  trap 'rm -f "${api_key_file}"' EXIT
  printf '%s' "${APPLE_API_KEY}" > "${api_key_file}"
  submit_args+=(--key "${api_key_file}" --key-id "${APPLE_API_KEY_ID}")
  if [[ -n "${APPLE_API_ISSUER_ID:-}" ]]; then
    submit_args+=(--issuer "${APPLE_API_ISSUER_ID}")
  fi
else
  : "${APPLE_ID:?set Apple ID credentials, App Store Connect API key credentials, or SOFTLOAF_NOTARY_KEYCHAIN_PROFILE}"
  : "${APPLE_TEAM_ID:?set APPLE_TEAM_ID for Apple ID notarization}"
  : "${APPLE_APP_SPECIFIC_PASSWORD:?set APPLE_APP_SPECIFIC_PASSWORD for Apple ID notarization}"
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
