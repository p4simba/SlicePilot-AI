#!/usr/bin/env python3
"""Development probe for Codex authentication and Orca project context.

Uses only Python's standard library. Does not edit projects or run model turns.
Credentials belong to Codex and never pass through this client's output.
"""
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
import zipfile


class ProtocolError(RuntimeError):
    pass


class CodexClient:
    def __init__(self, executable, state_dir, timeout=15):
        self.timeout = timeout
        self.sequence = 0
        self.notifications = []
        self.messages = queue.Queue()
        state_dir = Path(state_dir).resolve()
        state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        env = os.environ.copy()
        # Isolate slicer login/logout from the user's normal Codex session.
        env["CODEX_HOME"] = str(state_dir)
        for key in ("OPENAI_API_KEY", "CODEX_API_KEY", "CODEX_ACCESS_TOKEN"):
            env.pop(key, None)
        self.process = subprocess.Popen(
            [executable, "app-server", "--listen", "stdio://"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, encoding="utf-8", env=env,
        )
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        try:
            self.request("initialize", {"clientInfo": {
                "name": "orca_assistant_probe", "title": "Slicer Assistant Probe",
                "version": "0.1.0",
            }})
            self._send({"method": "initialized", "params": {}})
        except Exception:
            self.close()
            raise

    def _read(self):
        try:
            for line in self.process.stdout:
                try:
                    self.messages.put(json.loads(line))
                except json.JSONDecodeError:
                    self.messages.put(ProtocolError("Invalid JSON from Codex"))
        finally:
            self.messages.put(ProtocolError("Codex connection closed"))

    def _send(self, message):
        self.process.stdin.write(json.dumps(message) + "\n")
        self.process.stdin.flush()

    def _next(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Codex did not respond in time")
        try:
            message = self.messages.get(timeout=remaining)
        except queue.Empty:
            raise TimeoutError("Codex did not respond in time") from None
        if isinstance(message, Exception):
            raise message
        if not isinstance(message, dict):
            raise ProtocolError("Invalid Codex message")
        if "method" in message and "id" in message:
            # This probe has no execution or tool capabilities.
            self._send({"id": message["id"], "error": {
                "code": -32601, "message": "Client does not support this request",
            }})
            return {}
        return message

    def request(self, method, params=None):
        self.sequence += 1
        request_id = self.sequence
        self._send({"id": request_id, "method": method, "params": params or {}})
        deadline = time.monotonic() + self.timeout
        while True:
            message = self._next(deadline)
            if message.get("id") == request_id:
                if "error" in message:
                    # Avoid echoing raw authentication payloads into logs.
                    raise ProtocolError("Codex rejected " + method)
                if "result" not in message:
                    raise ProtocolError("Codex response has no result")
                return message["result"]
            if "method" in message:
                self.notifications.append(message)

    def wait_for_login(self, login_id, timeout=180):
        deadline = time.monotonic() + timeout
        while True:
            message = (self.notifications.pop(0) if self.notifications
                       else self._next(deadline))
            params = message.get("params", {})
            if (message.get("method") == "account/login/completed"
                    and params.get("loginId") == login_id):
                if not params.get("success"):
                    raise ProtocolError("Codex login did not complete successfully")
                return

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.reader.join(timeout=2)
        self.process.stdin.close()
        self.process.stdout.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# Explicit allowlist: do not include printer credentials, paths, scripts or
# arbitrary project metadata in assistant context.
CONTEXT_KEYS = frozenset({
    "printer_model", "printer_technology", "nozzle_diameter", "filament_type",
    "layer_height", "initial_layer_print_height", "wall_loops",
    "top_shell_layers", "bottom_shell_layers", "sparse_infill_density",
    "sparse_infill_pattern", "enable_support", "support_type",
    "support_threshold_angle", "brim_type", "brim_width", "nozzle_temperature",
    "nozzle_temperature_initial_layer", "hot_plate_temp",
    "hot_plate_temp_initial_layer", "filament_flow_ratio",
    "filament_max_volumetric_speed", "outer_wall_speed", "inner_wall_speed",
    "retraction_length", "retraction_speed",
})


def project_context(path):
    """Read saved global settings only; not effective per-object settings."""
    member = "Metadata/project_settings.config"
    with zipfile.ZipFile(path) as archive:
        if sum(info.filename == member for info in archive.infolist()) != 1:
            raise ValueError("Expected exactly one Orca project settings file")
        if archive.getinfo(member).file_size > 4 * 1024 * 1024:
            raise ValueError("Project settings exceed the 4 MiB limit")
        settings = json.loads(archive.read(member))
    if not isinstance(settings, dict):
        raise ValueError("Project settings must be a JSON object")
    selected = {key: value for key, value in settings.items() if key in CONTEXT_KEYS}
    return {
        "schema_version": 1,
        "scope": "saved_global_settings_only",
        "limitations": ["No geometry", "No object or plate overrides",
                        "No unsaved changes", "No slicing estimates"],
        "settings": selected,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codex", default="codex", help="Codex executable path")
    parser.add_argument("--state-dir", type=Path, required=True,
                        help="Dedicated private directory for this integration")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("status")
    commands.add_parser("login")
    commands.add_parser("logout")
    context = commands.add_parser("context")
    context.add_argument("project", type=Path)
    args = parser.parse_args()
    if args.command == "context":
        print(json.dumps(project_context(args.project), indent=2, ensure_ascii=False))
        return
    with CodexClient(args.codex, args.state_dir) as client:
        if args.command == "login":
            result = client.request("account/login/start", {"type": "chatgpt"})
            print("Open this Codex login URL in your browser (do not share it):", flush=True)
            print(result["authUrl"], flush=True)
            try:
                client.wait_for_login(result["loginId"])
            except (TimeoutError, KeyboardInterrupt):
                client.request("account/login/cancel", {"loginId": result["loginId"]})
                raise
            print("Codex connected.")
        elif args.command == "logout":
            client.request("account/logout")
            print("Disconnected from this integration.")
        else:
            result = client.request("account/read", {"refreshToken": False})
            account = result.get("account")
            print(json.dumps({"connected": account is not None,
                              "type": account.get("type") if account else None}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, ProtocolError, TimeoutError, zipfile.BadZipFile) as error:
        raise SystemExit(str(error))
