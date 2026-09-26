# SlicePilot AI — Codex integration development probe

This standard-library Python tool exercises the official Codex app-server
authentication protocol and reads an allowlisted subset of saved Orca 3MF
settings. It is a development tool, not an integrated slicer assistant.
It does not send model requests, change profiles, or print anything.

Requires Python 3.9+ and the official Codex CLI. Tested protocol runtime:
`codex-cli 0.155.0-alpha.2.6`. The app-server surface is experimental;
pin and validate a runtime before distributing a desktop integration.

Use a dedicated private state directory outside the repository. Never select
your usual Codex home: disconnect should affect only the slicer integration.

```sh
python3 tools/codex_assistant/client.py --state-dir /private/path/slicer-codex status
python3 tools/codex_assistant/client.py --state-dir /private/path/slicer-codex login
python3 tools/codex_assistant/client.py --state-dir /private/path/slicer-codex logout
python3 tools/codex_assistant/client.py --state-dir /private/path/slicer-codex context model.3mf
python3 -m unittest discover -s tools/codex_assistant -p 'test_*.py' -v
```

`--codex` accepts the full path to the CLI if it is not on PATH. Login uses
the URL supplied by Codex; complete that flow in a browser. The process waits
for login completion for three minutes. Credentials are stored by Codex in
the dedicated state directory, never in the project or slicer profiles.
Access is subject to the user's plan and workspace policies.

Context extraction is entirely local. Output omits G-code scripts, host
addresses, credentials, filenames, meshes and unknown settings. It includes
only global values saved in the archive, not effective per-object or plate
settings. Do not present this output as a complete live project inspection.

Reference: https://learn.chatgpt.com/docs/app-server

## Native UI verification

The editor exposes **SlicePilot AI > Assistente / Conectar ao Codex** and a
SlicePilot AI preferences tab. The modeless native window supports connection,
project preview, advice and explicit approval of supported global process changes.
The user reviews before/after values, clicks **Aplicar alterações**, and can use
**Desfazer** if the project has not changed. See the [design](../../docs/HLSD/CodexAssistant.md) for scope and validation limits.

`ui_smoke.cpp` hosts the production window with a synthetic Codex subprocess.
It does not connect to OpenAI. After building the upstream wxWidgets dependency:

```sh
cmake -S tools/codex_assistant -B build/assistant-ui
cmake --build build/assistant-ui
ctest --test-dir build/assistant-ui --output-on-failure
```

The test requires a graphical desktop session (on Linux, a display such as
Xvfb). Set `wxWidgets_CONFIG_EXECUTABLE` if wx-config is elsewhere. Windows
requires the usual CMake wxWidgets library/include paths. The test exercises
connection, live-context preview, context transmission, streaming response,
completion and logout through real subprocess pipes. Running the executable
without `--self-test` opens the same window for visual inspection.

Real OAuth completion and an actual model answer require the user's interactive
login and account entitlement; the synthetic test does not establish those.

## Local macOS application build

The macOS bundle uses the SlicePilot AI display name and a separate application
identifier and settings directory. Translation catalogs retain their upstream
OrcaSlicer names. Embedded Python disables bytecode writes so imports cannot
modify sealed resources inside a signed application.

For Apple Silicon with Xcode 26.5, build dependencies with the SDK path exported,
then configure the application. Xcode enables a legacy integer-conversion warning
that needs demotion for this upstream revision:

```sh
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
./build_release_macos.sh -d -a arm64 -j4
cmake -S . -B build/arm64 -G Xcode -DORCA_TOOLS=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3 \
  -DCMAKE_IGNORE_PREFIX_PATH='/opt/local:/usr/local:/opt/homebrew' \
  -DCMAKE_CXX_FLAGS=-Wno-error=shorten-64-to-32
./build_release_macos.sh -s -b -a arm64 -j4
mkdir -p build/release
ditto build/arm64/OrcaSlicer/OrcaSlicer.app 'build/release/SlicePilot AI.app'
codesign --force --deep --sign - 'build/release/SlicePilot AI.app'
codesign --verify --deep --strict 'build/release/SlicePilot AI.app'
```

Dependency builds also require the documented upstream prerequisites, including
Autotools and `makeinfo` from Texinfo on PATH. The local signature is ad hoc;
this is a development build, not an Apple-notarized release.
