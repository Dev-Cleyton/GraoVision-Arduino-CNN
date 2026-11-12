"""Regressao: modo interativo deve oferecer CONFIRMO-X mesmo com MOVIMENTOS_REAIS=0."""
import io
from unittest.mock import patch
import pytest
from hardware.arduino_handler import ArduinoHandler

class SerialFalsa:
    is_open = True
    def __init__(self):
        self.filas = [b'EEPROM:POS_COMANDADA:14000:ANGULO:180:SEQ:965\n',
                      b'RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR\n']
        self.escritas = []
    def readline(self):
        return self.filas.pop(0) if self.filas else b''
    def write(self, valor):
        self.escritas.append(valor)
        if valor == b'RECUPERAR:X\n':
            self.filas += [b'RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL\n', b'READY:V10.1\n']
    def flush(self): pass

def test_confirma_x_exigida_e_recuperacao_enviada():
    serial = SerialFalsa()
    handler = ArduinoHandler(serial_obj=serial)
    with patch('builtins.input', return_value='CONFIRMO-X'):
        handler.aguardar_pronto(timeout=2, recuperar_interativamente=True)
    assert serial.escritas == [b'RECUPERAR:X\n']

def test_rejeita_confirma_incorreta_sem_gravar_x():
    serial = SerialFalsa()
    handler = ArduinoHandler(serial_obj=serial)
    with patch('builtins.input', return_value='S'):
        with pytest.raises(RuntimeError, match='nao autorizada'):
            handler.aguardar_pronto(timeout=2, recuperar_interativamente=True)
    assert serial.escritas == []

def test_nao_interativo_continua_bloqueado():
    serial = SerialFalsa()
    handler = ArduinoHandler(serial_obj=serial)
    with pytest.raises(RuntimeError, match='EEPROM requer verificacao'):
        handler.aguardar_pronto(timeout=2, recuperar_interativamente=False)
    assert serial.escritas == []
