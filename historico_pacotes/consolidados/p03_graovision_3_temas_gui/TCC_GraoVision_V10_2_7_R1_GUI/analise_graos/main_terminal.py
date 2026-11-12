"""Ponto de entrada V10.1. Relatorios automaticos em ./relatorios/."""
from core.analysis_service import AnalysisService


def main():
    print('\n' + '=' * 58)
    print('ANALISE DE GRAOS - V10.2.7 - VETORES P1 P2 PF P3 P4 P5 - D7 BLOQUEADO')
    print('P1 -> CNN -> P2 -> 120s -> PF -> P3 -> AUTORIZACAO -> P4 -> P5 -> RELATORIO')
    print('=' * 58)
    print('AVISO: D7 BLOQUEADO NESTA COMPILACAO; nao energize a lampada sem verificacao independente.')
    print('Movimentos reais supervisionados exigem MOVIMENTOS_REAIS=1 no .env, '
          'habilitacao no Arduino e autorizacao presencial por ciclo.')
    while True:
        try:
            service = AnalysisService()
            session = service.executar(interativo=True, salvar=True)
        except Exception as erro:
            print(f'Falha ao conectar/iniciar: {erro}')
            break
        sample = session.sample
        print('\nRESULTADO:', sample.status_final)
        print('MOTIVO:', sample.motivo_final)
        print('P1:', sample.peso_inicial, 'g; P2:', sample.peso_referencia,
              'g; PF:', sample.peso_final, 'g')
        print('P3:', sample.peso_retorno, 'g; P4:', sample.peso_segunda_subida, 'g; P5:', sample.peso_segundo_retorno, 'g')
        print('P4-P2 (g):', sample.repetibilidade_topo_g, 'P5-P3 (g):', sample.repetibilidade_centro_g)
        print('Offset P2-P1 (g):', sample.delta_posicionamento_g)
        print('Falha Arduino:', sample.codigo_erro_arduino)
        print('NTC na camara (C), ultima leitura:', sample.temperatura_camara)
        print('Recuperacao:', sample.recuperacao_status)
        if sample.recuperacao_status == 'RETORNO_COMANDADO_CONFERIR_X':
            print('AVISO: confirme fisicamente o X inferior antes de novo ciclo.')
        print('Eventos PWM do servo:', sample.eventos_servo_pwm)
        print('Comando do aquecedor:', sample.eventos_aquecedor)
        print('Duracao comando ON (ms):', sample.tempo_comando_on_ms)
        print('Motivo OFF:', sample.motivo_desligamento_aquecedor)
        print('PF corrigido para referencia P1 (g):', sample.peso_final_corrigido_g)
        print('Variacao de massa aparente P2-PF (g):', sample.perda_massa_g)
        print('Variacao aparente (% ref. P1):', sample.perda_massa_percentual_ref_p1)
        print('Variacao aparente (% ref. P2):', sample.perda_massa_percentual)
        print('ATENCAO: variacao de massa aparente nao comprova umidade dos graos.')
        print('Umidade de graos: NAO DETERMINADA' if sample.umidade_final is None
              else f'Umidade validada: {sample.umidade_final:.2f}%')
        if input('\nIniciar outro ciclo? (S/N): ').strip().upper() != 'S':
            break


if __name__ == '__main__':
    main()
