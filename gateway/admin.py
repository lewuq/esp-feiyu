"""Loopback-only browser configuration for the speech gateway."""
import json
import secrets
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import config_store

ADMIN_TOKEN = secrets.token_urlsafe(32)


class AdminHandler(BaseHTTPRequestHandler):
    def _host_ok(self):
        host = self.headers.get('Host', '')
        return host in (f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}')

    def _respond(self, status, body, kind):
        self.send_response(status)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.end_headers()
        self.wfile.write(body)

    def _json(self, status, value):
        self._respond(status, json.dumps(value, ensure_ascii=False).encode(), 'application/json; charset=utf-8')

    def do_GET(self):
        if not self._host_ok():
            return self.send_error(403)
        if self.path == '/':
            page = Path(__file__).with_name('settings.html').read_text(encoding='utf-8')
            return self._respond(200, page.replace('__ADMIN_TOKEN__', ADMIN_TOKEN).encode(), 'text/html; charset=utf-8')
        if self.path == '/api/config':
            try:
                return self._json(200, config_store.public_config())
            except (OSError, ValueError):
                return self._json(500, {'error': '配置文件无法读取，请检查 config.local.json'})
        return self.send_error(404)

    def do_POST(self):
        if not self._host_ok() or self.path != '/api/config':
            return self.send_error(403)
        host = self.headers.get('Host', '')
        if self.headers.get('Origin') != 'http://' + host:
            return self.send_error(403)
        if not secrets.compare_digest(self.headers.get('X-Admin-Token', ''), ADMIN_TOKEN):
            return self.send_error(403)
        if self.headers.get_content_type() != 'application/json':
            return self.send_error(415)
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length <= 0 or length > 65536:
                return self.send_error(413)
            payload = json.loads(self.rfile.read(length))
            if not isinstance(payload, dict):
                raise ValueError('配置格式错误')
            config_store.save(payload.get('values', {}), payload.get('clear', []))
            return self._json(200, config_store.public_config())
        except (ValueError, TypeError, json.JSONDecodeError) as exc:
            return self._json(400, {'error': str(exc)})
        except OSError:
            return self._json(500, {'error': '保存失败，请检查网关目录写入权限'})

    def log_message(self, format, *args):
        pass


def create_server(port=8081):
    return ThreadingHTTPServer(('127.0.0.1', port), AdminHandler)
