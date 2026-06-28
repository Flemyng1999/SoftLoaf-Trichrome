# Repository Notes

## macOS Notarization Secret Pointer

The local Apple app-specific password for notarization is stored outside this
repository at:

```text
/Users/flemyng/.config/softloaf-trichrome/notary.env
```

Do not copy this secret into tracked files. To sync it into GitHub Actions:

```bash
sed -n 's/^APPLE_APP_SPECIFIC_PASSWORD=//p' \
  /Users/flemyng/.config/softloaf-trichrome/notary.env \
  | gh secret set APPLE_APP_SPECIFIC_PASSWORD --app actions
```
