import os
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import patch
from http.server import ThreadingHTTPServer
import server


class GatewayTest(unittest.TestCase):
    def test_wav_roundtrip(self):
        pcm = b'\x01\x00\xff\xff' * 100
        self.assertEqual(server.pcm_from_wav(server.wav_from_pcm(pcm)), pcm)

    def test_providers_are_chained_in_order(self):
        fake = [b'{"text":"hello"}', b'{"choices":[{"message":{"content":"hi"}}]}',
                server.wav_from_pcm(b'\x00\x00' * 100)]
        config = {'ASR_URL':'https://asr.example/api', 'ASR_KEY':'a', 'ASR_MODEL':'s',
                  'DEEPSEEK_KEY':'b', 'TTS_URL':'https://tts.example/api',
                  'TTS_KEY':'c', 'TTS_MODEL':'t', 'TTS_VOICE':'v'}
        with patch.dict(os.environ, config), patch.object(server, 'request', side_effect=fake) as req:
            result = server.synthesize(server.chat(server.transcribe(b'\0\0' * 10)))
        self.assertEqual(len(result), 200)
        self.assertEqual(req.call_count, 3)
        self.assertIn(b'RIFF', req.call_args_list[0].args[2])
        self.assertIn(b'hello', req.call_args_list[1].args[2])
        self.assertIn(b'hi', req.call_args_list[2].args[2])

    def test_history_is_separate_and_bounded(self):
        server.HISTORY.clear()
        result = b'{"choices":[{"message":{"content":"ok"}}]}'
        with patch.dict(os.environ, {'DEEPSEEK_KEY': 'k'}), patch.object(server, 'request', return_value=result) as req:
            server.chat('first', 'device1')
            server.chat('second', 'device1')
            self.assertIn(b'first', req.call_args.args[2])
            server.chat('other', 'device2')
            self.assertNotIn(b'first', req.call_args.args[2])
            for i in range(10):
                server.chat(str(i), 'device1')
        self.assertEqual(len(server.HISTORY['device1']), 12)

    def test_endpoint_auth_and_pcm(self):
        httpd = ThreadingHTTPServer(('127.0.0.1', 0), server.Handler)
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        url = f'http://127.0.0.1:{httpd.server_port}/voice'
        try:
            with patch.dict(os.environ, {'DEVICE_TOKEN':'test-token'}), \
                 patch.object(server, 'transcribe', return_value='q'), \
                 patch.object(server, 'chat', return_value='a'), \
                 patch.object(server, 'synthesize', return_value=b'\x01\x00'):
                with self.assertRaises(urllib.error.HTTPError) as denied:
                    urllib.request.urlopen(urllib.request.Request(url, data=b'\x00\x00'))
                self.assertEqual(denied.exception.code, 401)
                req = urllib.request.Request(url, data=b'\x00\x00', headers={'Authorization':'Bearer test-token'})
                with urllib.request.urlopen(req) as response:
                    self.assertEqual(response.read(), b'\x01\x00')
        finally:
            httpd.shutdown()
            httpd.server_close()
            thread.join()

if __name__ == '__main__':
    unittest.main()
