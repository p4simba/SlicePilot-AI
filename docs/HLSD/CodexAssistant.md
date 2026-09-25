# SlicePilot AI — Codex assistant

The editor exposes a SlicePilot AI menu and a preferences entry. Both open
one modeless assistant window with conversation and connection tabs.
`CodexAssistant.cpp` is an independent wxWidgets UI; the slicer adapter in
`AssistantProjectContext.cpp` supplies live context and saves only the
executable path in AppConfig. `tools/codex_assistant/ui_smoke.cpp` hosts the
same UI with a synthetic subprocess for offline verification.

## Connection

The selected official Codex executable runs `app-server --listen stdio://`.
The native client implements initialization, request IDs, deadlines, account
status, browser login, cancellation, logout, ephemeral threads and streamed
turns. A wx timer drains stdout/stderr without waiting for network responses.
Unexpected server tool and approval requests are rejected. stderr and raw
error payloads are not persisted by the client. Closing the window terminates
its child process group and discards the in-memory conversation.

A dedicated `slicepilot-codex` directory under the application's per-user local
data directory isolates Codex login/logout from the user's ordinary Codex
session. Codex manages tokens and its credential lifecycle. Credentials never
enter AppConfig, projects, or print profiles. Browser login accepts only HTTPS
URLs on auth.openai.com or chatgpt.com. Login uses the user's Codex entitlements
and workspace policies; it is not a generic OAuth grant for arbitrary API use.

## Project context and conversation

Context is captured on the GUI thread on every send, including unsaved edits.
A bounded allowlist selects global settings from PresetBundle::full_config,
object and volume overrides, printable flags, and transformed instance bounding
box dimensions. Object names, source paths, printer host credentials, custom
G-code and mesh data are excluded. Context has a 256 KiB bound and limits on
object/instance/volume counts. The user may preview the context before enabling
project sharing. Sending a question refreshes the displayed context.

Overrides are presented separately, not as merged effective settings. Plate
settings and height-range overrides are explicitly unresolved. The snapshot
contains no sliced estimates, detailed geometry analysis or physical validation.
The agent treats project text as untrusted data and explains uncertainty.

Threads disable shell, unified execution, hooks, apps, remote plugins, web search
and delegation through Codex configuration. Turns use a read-only sandbox with
restricted read roots at an empty dedicated workspace and platform defaults.
The client does not grant execution approvals or perform tools. Advice cannot
modify slicer settings or send a print; the UI clearly reports this boundary.

## Compatibility

No existing profile or 3MF schema changes. The panel currently uses Portuguese
labels. Model requests require a compatible official Codex runtime and a signed-in
account. The protocol was developed against 0.155.0-alpha.2.6; app-server is
experimental, and runtime compatibility must be checked before distribution.

Reference: https://learn.chatgpt.com/docs/app-server . The Python development
probe remains available for headless authentication and saved-3MF inspection;
its global-only snapshots are distinct from the native live-project adapter.
