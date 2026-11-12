"""Camada de eventos, autorizacoes humanas e estado da interface, sem conexao de hardware."""
from __future__ import annotations

import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import secrets
import threading
import time

CONFIG_DIR = Path.home() / '.graovision_tcc'
CONFIG_PATH = CONFIG_DIR / 'config.json'
ACCESS_PATH = CONFIG_DIR / 'admin.json'
DEFAULT_SETTINGS = {
    'porta_serial': 'COM4', 'camera_indice': 0,
    'movimentos_reais': False, 'roi_automatica': True,
    'tema_visual': 'esmeralda',
}

ERROR_HELP = {
    'TIMEOUT_CONFIRMACAO_RETORNO': (
        'A autorizacao para retornar nao foi recebida no prazo definido pelo Arduino.',
        'Mantenha aquecedor desenergizado e o motor parado. Nao tente recuperar X'
        ' no alto. Verifique resfriamento, sustentacao e procedimento presencial.'
    ),
    'TIMEOUT_RESFRIAMENTO_SEM_ATINGIR_LIMIAR': (
        'A camara nao atingiu o limiar de retorno no tempo previsto.',
        'Nao force retorno. Desenergize a fonte do aquecedor e verifique'
        ' aquecimento residual, NTC e temperatura real dos componentes.'
    ),
    'CNN_TIMEOUT': (
        'O Arduino esgotou o prazo para a resposta da classificacao.',
        'Verifique comunicacao, Roboflow, internet e sequencia do protocolo.'
        ' O retorno so e permitido se o Arduino emitir o aviso de disponibilidade.'
    ),
    'LIMITE_TEMPERATURA_SOFTWARE': (
        'A protecao termica por software foi acionada.',
        'Desenergize o aquecedor, confirme o corte independente e aguarde'
        ' resfriamento. Nao aumente o limite para contornar o alarme.'
    ),
    'TIMEOUT_SUPERVISAO_PC': (
        'Arduino perdeu a comunicacao de supervisao durante aquecimento.',
        'Mantenha o aquecedor desligado, verifique porta COM e alimentacao.'
        ' Referencia mecanica exige inspecao fisica.'
    ),
    'PESO_MONITOR_INSTAVEL': (
        'A medicao da balanca ficou instavel.',
        'Verifique cabos da celula de carga, apoio do prato e vibracoes.'
        ' Nao considere uma leitura invalida como perda de umidade.'
    ),
    'TIMEOUT_CNN': (
        'A resposta da CNN nao chegou no prazo.',
        'Verifique webcam, API e comunicacao serial antes de novo ensaio.'
    ),
}


def identify_error(code: str):
    code = (code or '').strip()
    if code in ERROR_HELP:
        return ERROR_HELP[code]
    if 'EEPROM' in code or 'RECUPERACAO' in code or 'POSICAO' in code:
        return (
            'A referencia ou o journal da EEPROM requer verificacao.',
            'Nunca ajuste a referencia X apenas pelo numero de passos gravados.'
            ' Confira fisicamente o atuador inferior, servo em 90 graus e prato vazio.'
        )
    if 'NTC' in code or 'TEMPERATURA' in code:
        return (
            'Falha relacionada ao controle termico ou ao sensor NTC.',
            'Desenergize o aquecedor e verifique o sensor e a temperatura real.'
            ' Nao reinicie ate esclarecer a causa.'
        )
    return (
        'O Arduino ou o processo informou uma falha que precisa de revisao.',
        'Interrompa o ensaio; confira os logs, a posicao real do mecanismo e'
        ' o estado fisico do aquecedor. Nao tente um desbloqueio forcado.'
    )


def load_settings(path=CONFIG_PATH):
    out = DEFAULT_SETTINGS.copy()
    try:
        stored = json.loads(Path(path).read_text(encoding='utf-8'))
        if isinstance(stored, dict):
            for key in out:
                if key in stored and type(stored[key]) is type(out[key]):
                    out[key] = stored[key]
    except (OSError, ValueError, TypeError):
        pass
    if not out['porta_serial'].strip() or len(out['porta_serial']) > 32:
        out['porta_serial'] = 'COM4'
    if not 0 <= out['camera_indice'] <= 12:
        out['camera_indice'] = 0
    if out['tema_visual'] not in ('esmeralda', 'fluent_azul', 'operador'):
        out['tema_visual'] = 'esmeralda'
    return out


def save_settings(settings, path=CONFIG_PATH):
    validated = DEFAULT_SETTINGS.copy()
    port = str(settings.get('porta_serial', 'COM4')).strip().upper()
    if not port.startswith('COM') or not port[3:].isdigit() or len(port) > 8:
        raise ValueError('Porta serial invalida; use COM4, COM5, etc.')
    validated['porta_serial'] = port
    camera = int(settings.get('camera_indice', 0))
    if camera not in range(13):
        raise ValueError('Indice da camera deve estar entre 0 e 12')
    validated['camera_indice'] = camera
    validated['movimentos_reais'] = bool(settings.get('movimentos_reais', False))
    validated['roi_automatica'] = bool(settings.get('roi_automatica', True))
    tema = str(settings.get('tema_visual', 'esmeralda'))
    if tema not in ('esmeralda', 'fluent_azul', 'operador'):
        raise ValueError('Tema visual desconhecido')
    validated['tema_visual'] = tema
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(validated, indent=2, ensure_ascii=False), encoding='utf-8')
    return validated


def hash_pin(pin: str, salt: bytes, iterations=220_000) -> str:
    return hashlib.pbkdf2_hmac('sha256', pin.encode('utf-8'), salt, iterations).hex()


class LocalAdmin:
    """Controle local de interface, NAO autenticacao do firmware ou seguranca fisica."""
    def __init__(self, file=ACCESS_PATH):
        self.file = Path(file)

    @property
    def configured(self):
        return self.file.exists()

    def configure(self, pin):
        if not (isinstance(pin, str) and pin.isdigit() and len(pin) >= 6):
            raise ValueError('PIN deve ter pelo menos 6 digitos numericos')
        salt = secrets.token_bytes(16)
        entry = {'version': 1, 'iterations': 220000, 'salt': salt.hex(),
                 'hash': hash_pin(pin, salt)}
        self.file.parent.mkdir(parents=True, exist_ok=True)
        self.file.write_text(json.dumps(entry), encoding='utf-8')
        try:
            os.chmod(self.file, 0o600)
        except OSError:
            pass

    def verify(self, pin):
        try:
            entry = json.loads(self.file.read_text(encoding='utf-8'))
            computed = hash_pin(pin, bytes.fromhex(entry['salt']), int(entry['iterations']))
            return hmac.compare_digest(computed, entry['hash'])
        except (OSError, KeyError, ValueError, TypeError):
            return False


class PromptBroker:
    """Operador confirma na thread Tk. Thread da analise espera sem tocar na GUI."""
    def __init__(self, events):
        self.events = events
        self._lock = threading.Lock()
        self.pending = None

    def ask(self, label: str):
        with self._lock:
            if self.pending is not None:
                raise RuntimeError('Ja existe uma confirmacao aguardando operador')
            obj = {'label': label, 'answer': None, 'done': threading.Event()}
            self.pending = obj
        self.events.put(('prompt', obj))
        obj['done'].wait()
        with self._lock:
            if self.pending is obj:
                self.pending = None
        if obj['answer'] is None:
            raise RuntimeError('Operador cancelou a confirmacao presencial')
        return obj['answer']

    def answer(self, value, obj=None):
        with self._lock:
            pending = self.pending
            if pending is None or (obj is not None and obj is not pending):
                return False
            if pending['done'].is_set():
                return False
            pending['answer'] = value
            pending['done'].set()
            return True

    def cancel(self):
        return self.answer(None)


def prompt_kind(prompt):
    text = prompt.upper()
    if 'CONFIRMO-X' in text:
        return 'confirm_x'
    if 'RETORNO-RESFRIADO' in text:
        return 'thermal_return'
    if 'RETORNAR' in text:
        return 'supervised_return'
    if 'AUTORIZAR-P4-P5' in text:
        return 'diagnostic'
    if 'INSPECAO-TERMICA' in text:
        return 'heating'
    if 'DIGITE ARMAR' in text:
        return 'arm_move'
    if 'TARA' in text and 'VAZIO' in text.upper():
        return 'tare'
    if 'GRAOS' in text and 'INICIAR' in text:
        return 'load'
    return 'confirm'
