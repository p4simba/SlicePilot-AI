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
