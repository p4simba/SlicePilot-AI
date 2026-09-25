# SlicePilot AI — Codex assistant integration boundary

The development probe in `tools/codex_assistant` communicates with an official
local Codex app-server over newline-delimited JSON on standard input/output.
It uses a dedicated Codex state directory and supports account status, browser
login, login cancellation and logout. No OAuth client secret is embedded in
the slicer: Codex owns the browser callback and credential lifecycle.

This is a protocol feasibility layer. It is not yet connected to the wxWidgets
GUI, the open document, or the slicing engine. It never starts model turns.
The existing slicing pipeline, project formats and profiles are unaffected.

The companion context reader accepts a saved Orca 3MF archive and selects a
bounded set of global print settings. It does not extract files onto disk or
include connection credentials and custom G-code. The snapshot explicitly
declares its limitations: no geometry, object/plate overrides, unsaved state,
or slicing estimates. Incoming archive text remains untrusted project data.

The client uses request IDs, deadlines, explicit initialization and login
completion notifications. Unexpected server requests are rejected because
the probe has no tool execution capability. Authentication errors are not
dumped verbatim into logs. Closing the client terminates its child process.

The protocol reference is https://learn.chatgpt.com/docs/app-server . Runtime
compatibility must be checked against the actual bundled/selected Codex
version. ChatGPT login uses the user's own entitlements and workspace policy;
it is not a generic OAuth grant for arbitrary OpenAI API usage.
