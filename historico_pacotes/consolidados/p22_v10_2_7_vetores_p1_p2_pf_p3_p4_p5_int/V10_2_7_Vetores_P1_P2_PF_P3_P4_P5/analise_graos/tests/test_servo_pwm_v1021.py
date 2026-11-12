"""V10.2.1: integridade da suspensao do PWM e do balanco apos falha.
Testes de estrutura/relatorio, NAO teste eletrico do servo real.
"""
import pathlib
import tempfile
import unittest
from unittest.mock import Mock

RAIZ = pathlib.Path(__file__).resolve().parents[2]
UNO = RAIZ / 'arduino' / 'V10_2_5_Controle_Termico_Real' / 'V10_2_5_Controle_Termico_Real.ino'
SIM = RAIZ / 'arduino' / 'V10_2_5_Controle_Termico_Simulacao' / 'V10_2_5_Controle_Termico_Simulacao.ino'

class TestePWMServo(unittest.TestCase):
    def test_variantes_reais_e_simuladas_distintas(self):
        self.assertIn('NESTA_COMPILACAO = true;', UNO.read_text(encoding='utf8'))
        self.assertIn('NESTA_COMPILACAO = false;', SIM.read_text(encoding='utf8'))

    def test_pwm_desliga_antes_p2_e_so_religa_no_retorno(self):
        s = UNO.read_text(encoding='utf8')
        bloco_secagem = s.split('case VAI_SECAGEM:',1)[1].split('case ESTABILIZA:',1)[0]
        self.assertLess(bloco_secagem.index('moverServo(ANG_SECAGEM)'), bloco_secagem.index('suspenderPWMServoSecagem()'))
        self.assertIn('servoPrato.detach()', s.split('void suspenderPWMServoSecagem() {',1)[1].split('unsigned int periodoPassoUs',1)[0])
        self.assertIn('servoPrato.attach(PIN_SERVO)', s.split('bool moverServo(byte destino) {',1)[1].split('void suspenderPWMServoSecagem() {',1)[0])
        self.assertIn('PWM_SERVO_ATIVO_DURANTE_PESAGEM', s)
        self.assertIn('PWM_SERVO_REATIVADO_DURANTE_MONITOR', s)
        self.assertIn('AQUECEDOR:BLOQUEADO_V10_2_5',s)
        self.assertIn('SERVO:PWM_SUSPENSO:SECAGEM',s)
        self.assertIn('SERVO:PWM_REATIVADO:RETORNO',s)

    def test_eeprom_nao_interrompe_pulsos(self):
        s = UNO.read_text(encoding='utf8')
        trecho=s.split('while (posicaoPassos != destino) {',1)[1].split('  // Grava a posicao final',1)[0]
        self.assertNotIn('persistir(',trecho)
        self.assertIn('PASSOS_ENTRE_LOGS_SERIAL',trecho)

    def test_balanco_parcial_apos_erro(self):
        from models.sample import Sample
        x=Sample();x.peso_inicial=16.6949;x.peso_referencia=16.7721
        x.calcular_variacao_massa()
        self.assertAlmostEqual(x.delta_posicionamento_g,0.0772,places=4)
        self.assertIsNone(x.peso_final)
        self.assertIsNone(x.perda_massa_g)

    def test_eventos_servo_json(self):
        from models.sample import Sample
        from models.analysis_session import AnalysisSession
        from infrastructure.csv_service import salvar_relatorio
        import json
        sess=AnalysisSession()
        sess.sample.eventos_servo_pwm=['SERVO:PWM_SUSPENSO:SECAGEM']
        with tempfile.TemporaryDirectory() as td:
            salvar_relatorio(sess,td)
            j=json.loads((pathlib.Path(td)/f'{sess.sample.id}_relatorio.json').read_text(encoding='utf8'))
            self.assertEqual(j['eventos_servo_pwm'], ['SERVO:PWM_SUSPENSO:SECAGEM'])

    def test_handler_registra_eventos_sem_alterar_tabela_pesagem(self):
        from hardware.arduino_handler import ArduinoHandler
        from models.sample import Sample
        respostas = [
          'SERVO:PWM_SUSPENSO:SECAGEM','P2:16.7721','MEDICAO:0:16.7721:25.07',
          'MEDICAO:30:16.7436:25.11','PF:16.7436',
          'SERVO:PWM_REATIVADO:RETORNO','FIM:1'
        ]
        # substitui _ler em objeto nao conectado
        h=ArduinoHandler.__new__(ArduinoHandler)
        h._ler=Mock(side_effect=respostas)
        a=Sample();a.peso_inicial=16.6949
        h.obter_serie_pesagens(a, timeout=2)
        self.assertEqual(len(a.eventos_servo_pwm),2)
        self.assertEqual(len(a.medicoes),2)
        self.assertAlmostEqual(a.peso_final,16.7436)

if __name__ == '__main__': unittest.main()
