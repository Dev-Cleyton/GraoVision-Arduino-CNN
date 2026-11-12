"""Testa handshake ARMAR:REAL sem Arduino fisico."""
import unittest
import sys
import types
try:
    import serial
except ImportError:
    serial_stub = types.ModuleType('serial')
    serial_stub.Serial = lambda *a, **k: None
    sys.modules['serial'] = serial_stub
from hardware.arduino_handler import ArduinoHandler

class SerialFalsa:
    is_open = True
    def __init__(self, aceitar=True):
        self.aceitar = aceitar
        self.enviados = []
        self.fila = []
    def write(self, payload):
        comando = payload.decode('ascii').strip()
        self.enviados.append(comando)
        if comando == 'ARMAR:REAL':
            self.fila.append(b'ARMADO:REAL\n' if self.aceitar else b'ERRO:MOVIMENTO_REAL_NAO_COMPILADO\n')
        return len(payload)
    def readline(self):
        return self.fila.pop(0) if self.fila else b''
    def flush(self): pass
    def close(self): self.is_open=False

class Armamento(unittest.TestCase):
    def test_armar_com_ack(self):
        falso=SerialFalsa()
        ArduinoHandler(serial_obj=falso).armar_movimentos_reais()
        self.assertEqual(falso.enviados,['ARMAR:REAL'])
    def test_firmware_simulacao_bloqueia_real(self):
        falso=SerialFalsa(aceitar=False)
        with self.assertRaisesRegex(RuntimeError, 'REAL_NAO_COMPILADO'):
            ArduinoHandler(serial_obj=falso).armar_movimentos_reais()

if __name__ == '__main__': unittest.main()
