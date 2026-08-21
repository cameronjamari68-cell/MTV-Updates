# MTV Installer Updates

Public update host for the MTV branded installer (`MTVInstaller.exe`).

The installer checks `update.json` on launch. Both manifests are signed with
the seller's Ed25519 private key; the installer verifies the signature with the
embedded public key and refuses anything that does not verify, so this repo
being public is safe — nobody can push a fake update.

## Files

- `update.json` — signed installer self-update manifest
  (`version`, `exe`, `exe_sha256`, `min_version`, `notes`, `sig`)
- `MTVInstaller-<version>.exe` — the current installer binary
- `product-Titan-update.json` / `product-RemotePlay-update.json` — signed
  incremental PRODUCT updates (only when one has been published)
- `product/<Product>/...` — changed product files, mirrored paths, one per delta

## Publishing a new installer version

1. Bump `INSTALLER_VERSION` in `build_tools/installer_ui/mtv_installer.py`.
2. Rebuild both product packages (close Helios first).
3. Sign + stage:
   `python private_source\shared\seller\publish_update.py --exe <new exe> --notes "..." --out protected_release\updates`
4. Upload `update.json` + `MTVInstaller-<version>.exe` to this repo root.
5. Existing installers update themselves on next launch.

## Publishing a product update (code / config / meters changed)

1. Bump `$ProductVersion` in `build_tools/build_products.ps1` and rebuild the
   packages, keeping the previous build for the diff.
2. Sign + stage the delta:
   `python private_source\shared\seller\publish_product_update.py --product Titan --old <old pkg> --new <new pkg> --base-version <old> --version <new> --notes "..." --out protected_release\updates`
3. Upload EVERYTHING under `--out` — the `product-<Product>-update.json` plus
   the mirrored `product/<Product>/...` files — to this repo root.
4. Customers' installers download only the changed files, verify each SHA-256,
   patch package integrity, and re-install.

Never upload the private key or any `private_source` files here.
