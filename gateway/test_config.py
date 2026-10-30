import json
import os
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import patch
import admin
import config_store
import server


class ConfigurationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.env = patch.dict(os.environ, {'DAFEIYU_CONFIG': str(Path(self.temp.name) / 'settings.json')})
        self.env.start()

    def tearDown(self):
        self.env.stop()
        self.temp.cleanup()

    def test_standard_urls_and_secret_retention(self):
        config_store.save({'ASR_URL':'https://provider.test/v1/', 'ASR_KEY':'secret',
                           'TTS_URL':'https://provider.test/custom/speech'})
        self.assertEqual(config_store.effective()['ASR_URL'], 'https://provider.test/v1/audio/transcriptions')
        self.assertEqual(config_store.effective()['TTS_URL'], 'https://provider.test/custom/speech')
        config_store.save({'ASR_KEY':''})
        self.assertEqual(config_store.effective()['ASR_KEY'], 'secret')
        public = json.dumps(config_store.public_config())
        self.assertNotIn('secret', public)
        self.assertTrue(config_store.public_config()['configured']['ASR_KEY'])
        config_store.save({}, ['ASR_KEY'])
        self.assertFalse(config_store.public_config()['configured']['ASR_KEY'])

    def test_reject_invalid_and_preserve_previous(self):
        config_store.save({'ASR_URL':'https://provider.test/v1'})
        with self.assertRaises(ValueError):
            config_store.save({'ASR_URL':'http://bad.test'})
        self.assertEqual(config_store.effective()['ASR_URL'], 'https://provider.test/v1/audio/transcriptions')
        with self.assertRaises(ValueError):
            config_store.save({'PORT':'22'})
        with self.assertRaises(ValueError):
            config_store.save({'PORT':'8081'})

    def test_environment_override(self):
        config_store.save({'ASR_MODEL':'saved'})
        with patch.dict(os.environ, {'ASR_MODEL':'environment'}):
            self.assertEqual(config_store.effective()['ASR_MODEL'], 'environment')
            self.assertIn('ASR_MODEL', config_store.public_config()['environment_overrides'])

    def test_gateway_uses_saved_configuration_immediately(self):
        config_store.save({'ASR_URL':'https://example.test/v1', 'ASR_MODEL':'first', 'ASR_KEY':'key'})
        with patch.object(server, 'request', return_value=b'{"text":"hello"}') as send:
            self.assertEqual(server.transcribe(b'\x00\x00'), 'hello')
            self.assertEqual(send.call_args.args[0], 'https://example.test/v1/audio/transcriptions')
            self.assertIn(b'first', send.call_args.args[2])
            config_store.save({'ASR_MODEL':'second'})
            server.transcribe(b'\x00\x00')
            self.assertIn(b'second', send.call_args.args[2])

    def test_provider_redirects_are_disabled(self):
        self.assertIsNone(server.NoRedirect().redirect_request(None, None, 302, 'redirect', {}, 'https://other.example'))

    def test_admin_auth_and_no_secret_disclosure(self):
        httpd = admin.create_server(0)
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        origin = f'http://127.0.0.1:{httpd.server_port}'
        try:
            payload = json.dumps({'values':{'DEEPSEEK_KEY':'private'}}).encode()
            def post(origin_header, token):
                req = urllib.request.Request(origin+'/api/config', data=payload, headers={
                    'Origin':origin_header, 'Content-Type':'application/json', 'X-Admin-Token':token})
                return urllib.request.urlopen(req)
            with self.assertRaises(urllib.error.HTTPError) as denied:
                post('http://evil.test', admin.ADMIN_TOKEN)
            self.assertEqual(denied.exception.code, 403)
            with self.assertRaises(urllib.error.HTTPError):
                post(origin, 'wrong')
            with post(origin, admin.ADMIN_TOKEN) as response:
                self.assertNotIn(b'private', response.read())
            with urllib.request.urlopen(origin+'/api/config') as response:
                self.assertNotIn(b'private', response.read())
            req = urllib.request.Request(origin+'/', headers={'Host':'attacker.test'})
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(req)
        finally:
            httpd.shutdown(); httpd.server_close(); thread.join()

if __name__ == '__main__':
    unittest.main()
