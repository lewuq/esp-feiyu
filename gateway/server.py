"""Configurable speech gateway for the XIAO Dafeiyu device.

The device POSTs 16 kHz, mono, signed 16-bit little-endian PCM to /voice.
It receives the same PCM format. Providers use OpenAI-compatible ASR/TTS APIs.
"""
import audioop
import collections
import re
import threading
import io
import json
import logging
import os
import secrets
import urllib.error
import urllib.request
import wave
import sys
import webbrowser
import config_store
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SAMPLE_RATE = 16000
MAX_AUDIO = SAMPLE_RATE * 2 * 6
MAX_RESPONSE = 8 * 1024 * 1024
# Bounded, process-local conversation history; erased when the gateway restarts.
HISTORY = collections.OrderedDict()
HISTORY_LOCK = threading.Lock()


def required(name):
    value = config_store.effective().get(name, '').strip()
    if not value:
        raise RuntimeError(f'missing configuration: {name}')
    return value


def wav_from_pcm(pcm):
    with io.BytesIO() as target:
        with wave.open(target, 'wb') as out:
            out.setnchannels(1)
            out.setsampwidth(2)
            out.setframerate(SAMPLE_RATE)
            out.writeframes(pcm)
        return target.getvalue()


def pcm_from_wav(data):
    with wave.open(io.BytesIO(data), 'rb') as source:
        channels, width, rate = source.getnchannels(), source.getsampwidth(), source.getframerate()
        if source.getcomptype() != 'NONE' or width != 2 or channels not in (1, 2):
            raise ValueError('TTS must return 16-bit PCM WAV, mono or stereo')
        audio = source.readframes(source.getnframes())
    if channels == 2:
        audio = audioop.tomono(audio, 2, 0.5, 0.5)
    if rate != SAMPLE_RATE:
        audio, _ = audioop.ratecv(audio, 2, 1, rate, SAMPLE_RATE, None)
    return audio


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def request(url, key, body, content_type, timeout=60):
    if not url.startswith('https://'):
        raise ValueError('provider URLs must use HTTPS')
    req = urllib.request.Request(url, data=body, headers={
        'Authorization': 'Bearer ' + key, 'Content-Type': content_type,
    }, method='POST')
    # Do not forward provider credentials to a different URL via redirect.
    opener = urllib.request.build_opener(NoRedirect())
    with opener.open(req, timeout=timeout) as response:
        result = response.read(MAX_RESPONSE + 1)
        if len(result) > MAX_RESPONSE:
            raise ValueError('provider response too large')
        return result


def transcribe(pcm):
    boundary = '----dafeiyu-' + secrets.token_hex(12)
    parts = []
    def field(name, data, filename=None, kind=None):
        disposition = f'Content-Disposition: form-data; name="{name}"'
        if filename:
            disposition += f'; filename="{filename}"'
        head = f'--{boundary}\r\n{disposition}\r\n'
        if kind:
            head += f'Content-Type: {kind}\r\n'
        parts.extend([head.encode(), b'\r\n', data, b'\r\n'])
    field('model', required('ASR_MODEL').encode())
    field('file', wav_from_pcm(pcm), 'speech.wav', 'audio/wav')
    parts.append(f'--{boundary}--\r\n'.encode())
    data = json.loads(request(required('ASR_URL'), required('ASR_KEY'), b''.join(parts),
                              f'multipart/form-data; boundary={boundary}'))
    text = data.get('text', '').strip()
    if not text:
        raise ValueError('ASR returned empty text')
    return text


def chat(text, session='default'):
    system = {'role': 'system', 'content': config_store.effective()['SYSTEM_PROMPT']}
    with HISTORY_LOCK:
        history = HISTORY.get(session, [])
        messages = [system, *history, {'role': 'user', 'content': text}]
        data = json.dumps({'model': config_store.effective()['DEEPSEEK_MODEL'],
                           'messages': messages, 'stream': False}, ensure_ascii=False).encode()
        result = json.loads(request(config_store.effective()['DEEPSEEK_URL'],
                                    required('DEEPSEEK_KEY'), data, 'application/json'))
        reply = result['choices'][0]['message']['content'].strip()
        if not reply:
            raise ValueError('DeepSeek returned empty reply')
        HISTORY[session] = (history + [{'role': 'user', 'content': text},
                                       {'role': 'assistant', 'content': reply}])[-12:]
        HISTORY.move_to_end(session)
        while len(HISTORY) > 16:
            HISTORY.popitem(last=False)
        return reply


def synthesize(text):
    data = json.dumps({'model': required('TTS_MODEL'), 'voice': required('TTS_VOICE'),
                       'input': text, 'response_format': 'wav'}, ensure_ascii=False).encode()
    return pcm_from_wav(request(required('TTS_URL'), required('TTS_KEY'), data, 'application/json'))


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/voice':
            self.send_error(404)
            return
        token = config_store.effective()['DEVICE_TOKEN']
        supplied = self.headers.get('Authorization', '')
        if token and not secrets.compare_digest(supplied, 'Bearer ' + token):
            self.send_error(401)
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length <= 0 or length > MAX_AUDIO or length % 2:
                self.send_error(413)
                return
            pcm = self.rfile.read(length)
            if len(pcm) != length:
                raise ValueError('truncated upload')
            session = self.headers.get('X-Device-ID', 'default')
            if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', session):
                self.send_error(400)
                return
            response = synthesize(chat(transcribe(pcm), session))
            if not response:
                raise ValueError('empty audio response')
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Length', str(len(response)))
            self.end_headers()
            self.wfile.write(response)
        except Exception as exc:
            logging.error('voice request failed: %s', type(exc).__name__)
            self.send_error(502, 'Speech service failed')

    def log_message(self, format, *args):
        logging.info('%s %s', self.address_string(), args[0] if args else '')


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    config = config_store.effective()
    address, port = config['LISTEN_HOST'], int(config['PORT'])
    voice_server = ThreadingHTTPServer((address, port), Handler)
    # The configuration interface listens on loopback only, even if audio uses LAN.
    import admin
    admin_server = admin.create_server(port=8081)
    thread = threading.Thread(target=admin_server.serve_forever, daemon=True)
    thread.start()
    logging.info('设备语音网关: %s:%d/voice', address, port)
    logging.info('本机配置页面: http://127.0.0.1:8081/')
    if '--open' in sys.argv:
        webbrowser.open('http://127.0.0.1:8081/')
    try:
        voice_server.serve_forever()
    finally:
        admin_server.shutdown()
        admin_server.server_close()
        voice_server.server_close()
