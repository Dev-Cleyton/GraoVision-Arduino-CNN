"""Inspecoes de regressao das alteracoes R3 (nao simula movimento real)."""
from pathlib import Path
import unittest
from unittest.mock import patch

RAIZ = Path(__file__).resolve().parents[2]
ARQUIVO = RAIZ / 'arduino' / 'V10_1C_R3_Movimento_Real_Supervisionado' / 'V10_1C_R3_Movimento_Real_Supervisionado.ino'

class TesteR3(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.firmware = ARQUIVO.read_text(encoding='utf-8')

    def test_suavizacao_eeprom_fora_da_subida(self):
        trecho = self.firmware.split('bool moverVertical(bool subir) {', 1)[1].split('// ======================= ARDUINO', 1)[0]
        inicio = trecho.index('while (posicaoPassos != destino)')
        fim = trecho.index('Serial.print(F("POS_ATUADOR:"))', inicio)
        loop = trecho[inicio:trecho.index('  // Grava a posicao final',inicio)]
        self.assertNotIn('persistir(', loop)
        self.assertIn('periodoPassoUs(contadorDesdeCheckpoint)', loop)
        self.assertIn('PASSOS_ENTRE_LOGS_SERIAL', loop)
        self.assertIn('persistir(P_EXECUTANDO)', trecho[fim-200:fim])

    def test_eeprom_e_estados_preservados(self):
        self.assertIn('RECUPERACAO:PENDENTE', self.firmware)
        self.assertIn('EEPROM_INICIO = 64', self.firmware)
        self.assertIn('RECUPERAR:X', self.firmware)
        self.assertIn('AQUECEDOR:BLOQUEADO_V10_1', self.firmware)
        self.assertIn('CURSO_VERTICAL_PASSOS = 14000L', self.firmware)

    def test_servo_tara_espera(self):
        self.assertIn('ASSENTAMENTO_CENTRO_TARA_MS = 2500UL', self.firmware)
        self.assertIn('TARA:VERIFICANDO_ESTABILIDADE', self.firmware)
        self.assertIn('TARA:INSTAVEL', self.firmware)
        self.assertIn('const byte INTERVALO_SERVO_MS = 24', self.firmware)

    def test_entrada_python_antes_da_tara(self):
        texto=(RAIZ/'analise_graos/core/analysis_service.py').read_text(encoding='utf-8')
        self.assertLess(texto.index('centralizar_antes_tara()'), texto.index('Pressione ENTER para autorizar a TARA'))
        self.assertLess(texto.index('Pressione ENTER para autorizar a TARA'), texto.index('realizar_tara()'))

if __name__ == '__main__':
    unittest.main()
