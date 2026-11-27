# Showcase ZIP and PS5 package

`make app` builds the staged title folder and its ZIP. `make pkg` also builds
and verifies the PS5 package from that same folder using the boilerplate's
SHA-256-pinned ps5-pkgkit v0.1.0. The packaging step uses a Python virtual
environment under `.deps/`.

| File in `dist/` | Use |
| --- | --- |
| `PPSA99050.zip` | Extract the title folder to `/data/homebrew/` for ShadowMount. |
| `SHA256SUMS` | Verify the ZIP with `sha256sum -c SHA256SUMS`. |
| `PPSA99050.pkg` | Open with a compatible homebrew package installer. |
| `PPSA99050.pkg.sha256` | Verify the PKG with `sha256sum -c PPSA99050.pkg.sha256`. |

The [Build workflow](https://github.com/BlackBearRevived/ps5-homebrew-ui/actions/workflows/tooling.yml)
uploads all four files together. Pull requests get a build labelled with the PR
number and commit. Public builds outside pull requests also get GitHub build
attestations for both downloads. Verify either file with:

```bash
gh attestation verify PPSA99050.zip -R BlackBearRevived/ps5-homebrew-ui
gh attestation verify PPSA99050.pkg -R BlackBearRevived/ps5-homebrew-ui
```

A push of a version tag matching `sce_sys/param.json` publishes the verified
files as a GitHub Release. For example, `01.000.020` corresponds to tag
`1.000.020`. An existing release with a ZIP keeps its published files.
