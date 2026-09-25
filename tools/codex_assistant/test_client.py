import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile

from client import CodexClient, ProtocolError, project_context


# A subprocess exercises real pipe framing and interleaved messages without
# network requests, credentials, or an installed Codex runtime.
FAKE_SERVER = r'''
import json, sys
for line in sys.stdin:
    request = json.loads(line)
    method = request.get("method")
    if "id" not in request:
        continue
    if method == "initialize":
        result = {}
    elif method == "account/login/start":
        print(json.dumps({"method": "account/login/completed", "params": {
            "loginId": "test-login", "success": True}}), flush=True)
        result = {"loginId": "test-login", "authUrl": "https://example.invalid"}
    elif method == "account/read":
        result = {"account": None}
    elif method == "test/silence":
        continue
    else:
        print(json.dumps({"id": request["id"], "error": {
            "message": "sensitive error detail"}}), flush=True)
        continue
    print(json.dumps({"id": request["id"], "result": result}), flush=True)
'''


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        real_popen = subprocess.Popen

        def launch(_command, **kwargs):
            return real_popen([sys.executable, "-u", "-c", FAKE_SERVER], **kwargs)

        self.launcher = patch("client.subprocess.Popen", side_effect=launch)
        self.launcher.start()
        self.addCleanup(self.launcher.stop)

    def test_login_handles_notification_before_response(self):
        with CodexClient("unused", self.directory.name) as client:
            result = client.request("account/login/start", {"type": "chatgpt"})
            client.wait_for_login(result["loginId"])
        self.assertIsNotNone(client.process.poll())

    def test_status_handles_signed_out_account(self):
        with CodexClient("unused", self.directory.name) as client:
            self.assertIsNone(client.request("account/read")["account"])

    def test_errors_do_not_echo_payload(self):
        with CodexClient("unused", self.directory.name) as client:
            with self.assertRaisesRegex(ProtocolError, "^Codex rejected unknown$"):
                client.request("unknown")

    def test_unresponsive_request_has_deadline(self):
        with CodexClient("unused", self.directory.name) as client:
            client.timeout = 0.1
            with self.assertRaises(TimeoutError):
                client.request("test/silence")


class ContextTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "part.3mf"

    def write_settings(self, content):
        with zipfile.ZipFile(self.path, "w") as archive:
            archive.writestr("Metadata/project_settings.config", content)

    def test_context_excludes_credentials_scripts_and_unknown_fields(self):
        self.write_settings(json.dumps({"layer_height": "0.2",
                                       "print_host": "private-printer",
                                       "printhost_apikey": "secret",
                                       "machine_start_gcode": "G28",
                                       "unknown": "value"}))
        context = project_context(self.path)
        self.assertEqual(context["settings"], {"layer_height": "0.2"})
        self.assertIn("No object or plate overrides", context["limitations"])

    def test_non_object_settings_are_rejected(self):
        self.write_settings("[]")
        with self.assertRaises(ValueError):
            project_context(self.path)

    def test_oversized_settings_are_rejected(self):
        self.write_settings(" " * (4 * 1024 * 1024 + 1))
        with self.assertRaisesRegex(ValueError, "4 MiB"):
            project_context(self.path)

    def test_duplicate_settings_are_rejected(self):
        self.write_settings("{}")
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.path, "a") as archive:
                archive.writestr("Metadata/project_settings.config", "{}")
        with self.assertRaises(ValueError):
            project_context(self.path)


if __name__ == "__main__":
    unittest.main()
