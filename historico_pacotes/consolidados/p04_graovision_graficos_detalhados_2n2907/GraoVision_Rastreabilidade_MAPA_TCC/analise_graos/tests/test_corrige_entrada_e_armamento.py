import unittest
from unittest.mock import MagicMock, patch

class SmokeEntradaEArmamento(unittest.TestCase):
    def test_main_abre_gui_em_vez_do_terminal(self):
        import main
        with patch('sys.argv',['main.py']), patch('iniciar_interface.run') as run:
            main.main()
            run.assert_called_once_with()

    def test_main_terminal_so_por_flag(self):
        import main
        with patch('sys.argv',['main.py','--terminal']), patch('main_terminal.main') as terminal:
            main.main()
            terminal.assert_called_once_with()

    def test_arduino_reconhece_armamento_necessario(self):
        from hardware.arduino_handler import ArduinoHandler
        serial=MagicMock()
        serial.is_open=True
        serial.readline.return_value=b'MOVIMENTOS:REAIS_EXIGEM_ARMAR\n'
        arduino=ArduinoHandler(serial_obj=serial)
        self.assertEqual(arduino._ler(), 'MOVIMENTOS:REAIS_EXIGEM_ARMAR')
        self.assertTrue(arduino.requer_armamento_real)
        self.assertFalse(serial.write.called)

if __name__=='__main__': unittest.main()
