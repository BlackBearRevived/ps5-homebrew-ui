# Platform reference

How the app is built, packaged, deployed and tested as a native PS5 title.
These pages come from
[PS5 Native App Boilerplate](https://github.com/BlackBearRevived/ps5-native-app-boilerplate),
the foundation for this app. The native dependency setup, runtime build script,
and ZIP packager come from [template revision 7d3b975](https://github.com/BlackBearRevived/ps5-native-app-boilerplate/tree/7d3b97577f7d2e05c150e01300f3d399f5a647c7).
The app keeps its OpenGL linker settings and audio asset indexing.

| Page | Covers |
| --- | --- |
| [GETTING_STARTED.md](GETTING_STARTED.md) | Host setup, first build, first deploy |
| [CONFIGURATION.md](CONFIGURATION.md) | `sce_sys/param.json`: title identity and versions |
| [NATIVE_TOOLING.md](NATIVE_TOOLING.md) | The compiler wrapper, the ELF converter, signing |
| [RUNTIME_SHIM.md](RUNTIME_SHIM.md) | The clean-room `libc.prx` runtime module |
| [RELEASE_ZIP.md](RELEASE_ZIP.md) | The app folder and its ZIP, the only outputs |
| [DEPLOYMENT.md](DEPLOYMENT.md) | `make deploy` and `make undeploy` over FTP |
| [PRESENTATION_ASSETS.md](PRESENTATION_ASSETS.md) | Icon, backgrounds and selection music in `sce_sys/` |
| [PACBREW.md](PACBREW.md) | Optional prebuilt third-party libraries |
| [PLATFORM_NOTES.md](PLATFORM_NOTES.md) | Platform constraints worth knowing |
| [RECIPES.md](RECIPES.md) | Capability recipes (notifications, storage, input) |
| [TESTING.md](TESTING.md) | The host test suites |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Build, packaging and launch problems |

The OpenGL side (EGL bring-up, display modes, what the driver likes) is in
[`src/platform/ps5/display_egl.cpp`](../../src/platform/ps5/display_egl.cpp)
and [PERFORMANCE.md](../PERFORMANCE.md).
