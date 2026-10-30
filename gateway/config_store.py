"""Local configuration persistence for the speech gateway."""
import json
import os
import tempfile
import threading
from pathlib import Path
from urllib.parse import urlsplit

DEFAULTS = {
    'ASR_URL': 'https://api.siliconflow.cn/v1/audio/transcriptions', 'ASR_MODEL': 'TeleAI/TeleSpeechASR', 'ASR_KEY': '',
    'DEEPSEEK_URL': 'https://api.deepseek.com/chat/completions',
    'DEEPSEEK_MODEL': 'deepseek-flash', 'DEEPSEEK_KEY': '',
    'TTS_URL': 'https://api.siliconflow.cn/v1/audio/speech', 'TTS_MODEL': 'FunAudioLLM/CosyVoice2-0.5B', 'TTS_VOICE': 'FunAudioLLM/CosyVoice2-0.5B:diana', 'TTS_KEY': '',
    'DEVICE_TOKEN': '', 'LISTEN_HOST': '127.0.0.1', 'PORT': '8080',
    'SYSTEM_PROMPT': '你是一只可爱的大肥鱼，回答简短自然。',
}
SECRET_FIELDS = {'ASR_KEY', 'DEEPSEEK_KEY', 'TTS_KEY', 'DEVICE_TOKEN'}
URL_SUFFIXES = {'ASR_URL': '/audio/transcriptions', 'DEEPSEEK_URL': '/chat/completions', 'TTS_URL': '/audio/speech'}
_LOCK = threading.RLock()


def config_path():
    return Path(os.environ.get('DAFEIYU_CONFIG', str(Path(__file__).resolve().parent / 'config.local.json')))


def stored():
    path = config_path()
    if not path.exists():
        return {}
    with path.open('r', encoding='utf-8') as source:
        value = json.load(source)
    if not isinstance(value, dict):
        raise ValueError('配置文件格式错误')
    return {key: val for key, val in value.items() if key in DEFAULTS and isinstance(val, str)}


def effective():
    with _LOCK:
        result = dict(DEFAULTS)
        result.update(stored())
        result.update({key: os.environ[key] for key in DEFAULTS if key in os.environ})
        return result


def url_for(name, value):
    if not value:
        return value
    parsed = urlsplit(value)
    if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password or parsed.fragment:
        raise ValueError('云服务接口地址必须使用有效的 HTTPS 地址，且不能包含账号密码或锚点')
    path = parsed.path.rstrip('/')
    # Only add the standard operation path for a root or versioned base URL.
    if path == '' or path.endswith('/v1'):
        if parsed.query:
            raise ValueError('基础地址不支持查询参数，请填写完整接口地址')
        value = value.rstrip('/') + URL_SUFFIXES[name]
    return value


def save(updates, clear_secrets=()):
    if not isinstance(updates, dict) or not isinstance(clear_secrets, (list, tuple)):
        raise ValueError('配置数据格式错误')
    if set(updates) - set(DEFAULTS) or set(clear_secrets) - SECRET_FIELDS:
        raise ValueError('包含未知配置项')
    with _LOCK:
        previous = stored()
        for key, value in updates.items():
            if not isinstance(value, str) or len(value) > 4096:
                raise ValueError('配置项必须为长度不超过 4096 的文字')
            value = value.strip()
            if key in SECRET_FIELDS and not value:
                continue  # Empty password field keeps the stored secret.
            if key in URL_SUFFIXES:
                value = url_for(key, value)
            if key == 'LISTEN_HOST' and value not in ('127.0.0.1', '0.0.0.0'):
                raise ValueError('监听地址只能选择本机或局域网')
            if key == 'PORT' and (not value.isdecimal() or (not 1024 <= int(value) <= 65535 or int(value) == 8081)):
                raise ValueError('网关端口须为 1024–65535，且不能与配置页面的 8081 相同')
            previous[key] = value
        for key in clear_secrets:
            previous.pop(key, None)
        path = config_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp = tempfile.mkstemp(prefix='.dafeiyu-', suffix='.tmp', dir=str(path.parent))
        try:
            with os.fdopen(fd, 'w', encoding='utf-8', newline='\n') as out:
                json.dump(previous, out, ensure_ascii=False, indent=2)
                out.write('\n')
            os.replace(tmp, path)
        finally:
            if os.path.exists(tmp):
                os.unlink(tmp)
        return effective()


def public_config():
    result = effective()
    return {
        'values': {k: v for k, v in result.items() if k not in SECRET_FIELDS},
        'configured': {k: bool(result[k]) for k in SECRET_FIELDS},
        'environment_overrides': [k for k in DEFAULTS if k in os.environ],
    }
