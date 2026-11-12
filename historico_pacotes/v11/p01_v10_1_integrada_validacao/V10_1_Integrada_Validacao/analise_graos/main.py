"""Ponto de entrada V10.1. Relatorios automaticos em ./relatorios/."""
from core.analysis_service import AnalysisService


def main():
    print('\n' + '=' * 58)
    print('ANALISE DE GRAOS - V10.1 - VALIDACAO SEM AQUECIMENTO')
    print('P1 -> CNN -> P2 -> SERIE SEM CALOR -> RELATORIO')
    print('=' * 58)
    print('AVISO: todos os contatos de potencia desconectados; '
          'movimentos fisicos desabilitados por padrao.')
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
        print('Perda de massa (%):', sample.perda_massa_percentual)
        print('Umidade de graos: NAO DETERMINADA' if sample.umidade_final is None
              else f'Umidade validada: {sample.umidade_final:.2f}%')
        if input('\nIniciar outro ciclo? (S/N): ').strip().upper() != 'S':
            break


if __name__ == '__main__':
    main()
