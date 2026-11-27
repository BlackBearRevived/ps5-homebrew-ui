# The app folder and the release ZIP

Every build creates and validates the complete `dist/<TITLE_ID>/` title folder
and its `dist/<TITLE_ID>.zip`:

```bash
make app
```

On Windows, `./build.ps1` runs the same build through WSL.

The [Build workflow](https://github.com/BlackBearRevived/ps5-homebrew-ui/actions/workflows/tooling.yml)
runs on pushes to `main`, pull requests, version tags, manual requests, and nightly.
It stores the ZIP and `SHA256SUMS`. GitHub Releases are published by version
tags derived from `contentVersion` in `sce_sys/param.json`, such as
`1.000.020` for `01.000.020`. The public release
ZIP carries a GitHub Actions attestation. To verify a download, use:

```bash
sha256sum --check SHA256SUMS
gh attestation verify PPSA99050.zip -R BlackBearRevived/ps5-homebrew-ui
```

To install a release, extract the ZIP and upload its `<TITLE_ID>/` folder to
`/data/homebrew`; see
[Deployment](DEPLOYMENT.md).

The nightly build keeps GitHub's dependency cache and `ccache` warm. The
workflow verifies the pinned OpenGL SDK and reports compiler cache hits and
misses. The manual **snapshot** option on `main` saves a fallback compiler cache
in the `build-cache` prerelease.

No filesystem image is built. The `.ffpkg` (UFS2) and `.ffpfsc` (compressed)
images of earlier versions are gone, with `make ffpkg`, `make ffpfsc`,
`make packages` and the `DEPLOY_FORMAT` variable. If an old
`<TITLE_ID>.ffpkg` or `<TITLE_ID>.ffpfsc` is still in `/data/homebrew`, delete
it before installing the folder (`make undeploy` removes it together with
the folder): a folder and an image with the same title ID must not both be in
the loader's scan paths.

This project does not create a signed retail PKG/FPKG container.
