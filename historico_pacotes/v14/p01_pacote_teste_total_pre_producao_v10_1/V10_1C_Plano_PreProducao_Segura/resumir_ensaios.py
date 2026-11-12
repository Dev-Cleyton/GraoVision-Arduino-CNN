#!/usr/bin/env python3
"""Resumo OFFLINE de relatórios JSON V10.1B, sem Arduino, API Roboflow ou câmera.

Uso (na pasta analise_graos, após extrair este arquivo):
 python resumir_ensaios.py --pasta relatorios --saida resumo_ensaios.csv
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

CAMPOS = [
    'id', 'inicio', 'status', 'status_cnn', 'roi_status', 'roi_modo',
    'roi_centro_x', 'roi_centro_y', 'roi_raio_px',
    'roi_cobertura', 'roi_suporte', 'deteccoes_validas', 'deteccoes_bad',
    'percentual_bad', 'roi_fixa_validas', 'roi_fixa_bad', 'roi_fixa_pct_bad',
    'P1_g', 'P2_g', 'PF_g', 'delta_posicionamento_g', 'variacao_PF_menos_P2_g',
    'perda_massa_pct', 'movimento_simulado', 'aquecimento_realizado', 'motivo',
]


def simples(x):
    return '' if x is None else x


def linha(d):
    registros = d.get('roi_registros') or []
    reg = next((r for r in registros if r.get('status') == 'OK'), {})
    roi = reg.get('roi') or {}
    fixo = reg.get('comparacao_roi_fixa') or {}
    p2, pf = d.get('P2_g'), d.get('PF_g')
    diferenca = (round(float(pf)-float(p2), 6)
                if isinstance(p2, (int, float)) and isinstance(pf, (int, float)) else None)
    return {
        'id': d.get('id'), 'inicio': d.get('inicio'), 'status': d.get('status'),
        'status_cnn': d.get('status_cnn'), 'roi_status': d.get('roi_status'),
        'roi_modo': d.get('roi_modo'), 'roi_centro_x': roi.get('cx'),
        'roi_centro_y': roi.get('cy'), 'roi_raio_px': roi.get('raio'),
        'roi_cobertura': roi.get('cobertura'), 'roi_suporte': roi.get('suporte_borda'),
        'deteccoes_validas': d.get('total_deteccoes_validas'),
        'deteccoes_bad': d.get('deteccoes_bad'), 'percentual_bad': d.get('percentual_bad'),
        'roi_fixa_validas': fixo.get('deteccoes_aceitas'),
        'roi_fixa_bad': fixo.get('deteccoes_bad'),
        'roi_fixa_pct_bad': fixo.get('percentual_bad'),
        'P1_g': d.get('P1_g'), 'P2_g': p2, 'PF_g': pf,
        'delta_posicionamento_g': d.get('delta_posicionamento_P2_menos_P1_g'),
        'variacao_PF_menos_P2_g': diferenca,
        'perda_massa_pct': d.get('perda_massa_percentual'),
        'movimento_simulado': d.get('movimento_simulado'),
        'aquecimento_realizado': d.get('aquecimento_realizado'),
        'motivo': d.get('motivo'),
    }


def processar(pasta, saida):
    relatorios = sorted(pasta.glob('*_relatorio.json'))
    if not relatorios:
        raise FileNotFoundError(f'Nenhum *_relatorio.json encontrado em {pasta}')
    linhas, invalidos = [], []
    for arquivo in relatorios:
        try:
            d = json.loads(arquivo.read_text(encoding='utf-8-sig'))
            if not isinstance(d, dict):
                raise ValueError('JSON nao e objeto')
            linhas.append({k: simples(v) for k, v in linha(d).items()})
        except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
            invalidos.append(f'{arquivo.name}: {exc}')
    saida.parent.mkdir(parents=True, exist_ok=True)
    with saida.open('w', encoding='utf-8-sig', newline='') as f:
        wr = csv.DictWriter(f, fieldnames=CAMPOS, delimiter=';')
        wr.writeheader()
        wr.writerows(linhas)
    status = Counter(x['status'] for x in linhas)
    print(f'Relatorios lidos: {len(linhas)} | invalidos: {len(invalidos)}')
    for k, v in status.most_common():
        print(f'  {k}: {v}')
    print(f'Arquivo de resumo: {saida.resolve()}')
    if invalidos:
        print('Arquivos ignorados:')
        for problema in invalidos:
            print(' -', problema)
    print('Nota: erro de camera pode ser desconexao proposital; confirme a causa no registro do ensaio.')
    print('Nota: deteccoes da CNN nao sao contagem manual; massa perdida nao prova teor de umidade.')
    return len(linhas)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pasta', type=Path, default=Path('relatorios'))
    parser.add_argument('--saida', type=Path, default=Path('resumo_ensaios.csv'))
    args = parser.parse_args()
    processar(args.pasta, args.saida)
