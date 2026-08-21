# MTV Installer Updates

Public update host for the MTV branded installer (`MTVInstaller.exe`).

The installer checks `update.json` on launch. The manifest is signed with the
seller's Ed25519 private key; the installer verifies the signature with the
embedded public key and refuses anything that does not verify, so this repo
being public is safe — nobody can push a fake update.

## Files

- `update.json` — signed manifest (`version`, `exe`, `exe_sha256`, `min_version`, `notes`, `sig`)
- `MTVInstaller-<version>.exe` — the current installer binary

## Publishing a new version

1. Bump `INSTALLER_VERSION` in `build_tools/installer_ui/mtv_installer.py`.
2. Rebuild both product packages (closes Helios first).
3. Run the seller publisher to sign + stage:
   `python private_source\shared\seller\publish_update.py --exe <new exe> --notes "..." --out protected_release\updates`
4. Upload the two files from `protected_release\updates\` to this repo (root).
5. Existing installers update themselves on next launch; new customers get the
   new exe from the product packages directly.

Never upload the private key or any `private_source` files here.
