# MTV-Updates

Public home for MTV downloads and updates.

- **Download page (GitHub Pages):** https://cameronjamari68-cell.github.io/MTV-Updates/
- **Installer self-update manifest:** `update.json` + `MTVInstaller-<version>.exe`
- **Full packages (GitHub Release assets):** `titan.2.3.remastered.zip` and `remoteplay.2.3.Remastered.zip` under the `v2.3.0` release

## How updates work

The branded installer (`MTVInstaller.exe`) checks `update.json` on launch. The
manifest is Ed25519-signed with the seller's private key; the installer verifies
the signature with the embedded public key and refuses anything that does not
verify, so this repo being public is safe — nobody can push a fake update.

Incremental PRODUCT updates (only when published):
- `product-Titan-update.json` / `product-RemotePlay-update.json` — signed deltas
- `product/<Product>/...` — changed files, mirrored paths

## Publishing

### New installer version
1. Bump `INSTALLER_VERSION` in `build_tools/installer_ui/mtv_installer.py`.
2. Rebuild both product packages (close Helios first).
3. Sign + stage: `python private_source\shared\seller\publish_update.py --exe <new exe> --notes "..." --out protected_release\updates`
4. Upload `update.json` + `MTVInstaller-<version>.exe` to this repo root.

### New full release (new users)
1. Rebuild packages, upload the two zips as release assets to a new release tag
   (e.g. `v2.4.0`).
2. Update the two download links in `index.html` to the new tag's asset URLs.

### Product code update (existing users)
1. Bump `$ProductVersion` in `build_tools/build_products.ps1`, rebuild, keep the old package for the diff.
2. `python private_source\shared\seller\publish_product_update.py --product Titan --old <old pkg> --new <new pkg> --base-version <old> --version <new> --notes "..." --out protected_release\updates`
3. Upload the manifest + mirrored `product/<Product>/...` files here.

Never upload the private key or any `private_source` files here.
