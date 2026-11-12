"""Relatorio persistente JSON+CSV, sem credenciais e sem inventar umidade."""
import csv
import json
from datetime import datetime
from pathlib import Path


def salvar_relatorio(session, pasta='relatorios'):
    pasta = Path(pasta)
    pasta.mkdir(parents=True, exist_ok=True)
    sample = session.sample
    nome = sample.id
    dado = {
        'id': sample.id,
        'inicio': session.data_hora_inicio.isoformat(timespec='seconds'),
        'fim': session.data_hora_fim.isoformat(timespec='seconds') if session.data_hora_fim else None,
        'status': sample.status_final,
        'motivo': sample.motivo_final,
        'status_cnn': sample.status_cnn,
        'roi_modo': sample.roi_modo,
        'roi_status': sample.roi_status,
        'roi_registros': sample.roi_registros,
        'motivo_cnn': sample.motivo_cnn,
        'total_deteccoes_validas': sample.total_graos,
        'deteccoes_bad': sample.graos_ruins,
        'percentual_bad': sample.percentual_ruins,
        'classes_detectadas_na_roi': sample.classes_detectadas,
        'imagens': sample.imagens,
        'P1_g': sample.peso_inicial,
        'P2_g': sample.peso_referencia,
        'PF_g': sample.peso_final,
        'delta_posicionamento_P2_menos_P1_g': sample.delta_posicionamento_g,
        'perda_massa_g': sample.perda_massa_g,
        'perda_massa_percentual': sample.perda_massa_percentual,
        'temperatura_ntc_local_C': sample.temperatura_local,
        'umidade_graos_final_percentual': sample.umidade_final,
        'umidade_limite_empresa_percentual': sample.limite_umidade_empresa,
        'metodo_umidade': sample.metodo_umidade,
        'aquecimento_realizado': sample.aquecimento_realizado,
        'movimento_simulado': sample.movimento_simulado,
        'historico': sample.medicoes,
        'movimentos_motor': sample.movimentos,
        'erro': session.mensagem_erro,
        'observacoes': (
            'Perda de massa nao equivale ao teor de umidade dos graos. '
            'NTC mede temperatura proxima da celula. '
            'V10.1B validacao: aquecedor bloqueado. ROI automatica ainda experimental.'
        )
    }
    caminho_json = pasta / f'{nome}_relatorio.json'
    with caminho_json.open('w', encoding='utf-8') as f:
        json.dump(dado, f, indent=2, ensure_ascii=False)
    caminho_csv = pasta / f'{nome}_pesagens.csv'
    with caminho_csv.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['tempo_s', 'peso_g', 'temp_ntc_c'], delimiter=';')
        writer.writeheader()
        writer.writerows(sample.medicoes)
    caminho_mov = pasta / f'{nome}_movimentos.csv'
    with caminho_mov.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['sentido', 'passos_executados', 'passos_totais', 'posicao_comandada'], delimiter=';')
        writer.writeheader()
        writer.writerows(sample.movimentos)
    print(f'[RELATORIO] {caminho_mov}')
    print(f'[RELATORIO] {caminho_json}')
    print(f'[RELATORIO] {caminho_csv}')
    return caminho_json, caminho_csv
