"""Ponto de entrada V10.1. Relatorios automaticos em ./relatorios/."""
from core.analysis_service import AnalysisService


def main():
    print('\n' + '=' * 58)
    print('ANALISE DE GRAOS - V10.2.4 - INTEGRADO P1/P2/PF + NTC NA CAMARA - SEM AQUECIMENTO')
    print('P1 -> CNN -> P2 -> MONITORAMENTO NTC CAMARA -> PF -> RELATORIO')
    print('=' * 58)
    print('AVISO: AQUECEDOR FISICAMENTE DESCONECTADO; SEM SEGURANCA TERMICA.')
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
        print('Offset P2-P1 (g):', sample.delta_posicionamento_g)
        print('Falha Arduino:', sample.codigo_erro_arduino)
        print('NTC na camara (C), ultima leitura:', sample.temperatura_camara)
        print('Recuperacao:', sample.recuperacao_status)
        if sample.recuperacao_status == 'RETORNO_COMANDADO_CONFERIR_X':
            print('AVISO: confirme fisicamente o X inferior antes de novo ciclo.')
        print('Eventos PWM do servo:', sample.eventos_servo_pwm)
        print('PF corrigido para referencia P1 (g):', sample.peso_final_corrigido_g)
        print('Variacao de massa aparente P2-PF (g):', sample.perda_massa_g)
        print('Variacao aparente (% ref. P1):', sample.perda_massa_percentual_ref_p1)
        print('Variacao aparente (% ref. P2):', sample.perda_massa_percentual)
        print('ATENCAO: sem secagem e sem metodologia validada, nao interpretar como umidade.')
        print('Umidade de graos: NAO DETERMINADA' if sample.umidade_final is None
              else f'Umidade validada: {sample.umidade_final:.2f}%')
        if input('\nIniciar outro ciclo? (S/N): ').strip().upper() != 'S':
            break


if __name__ == '__main__':
    main()
