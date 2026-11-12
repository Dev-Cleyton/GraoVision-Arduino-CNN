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
        'diferenca_inicial_P1_menos_P2_g': sample.diferenca_inicial_p1_menos_p2_g,
        'PF_corrigido_referencia_P1_g': sample.peso_final_corrigido_g,
        'perda_massa_percentual_referencia_P1': sample.perda_massa_percentual_ref_p1,
        'tendencia_leitura_massa': sample.tendencia_leitura_massa,
        'balanco_massa_detalhado': sample.balanco_massa_detalhado,
        'perda_massa_g': sample.perda_massa_g,
        'perda_massa_percentual': sample.perda_massa_percentual,
        'temperatura_ntc_local_C': sample.temperatura_local,  # campo historico
        'temperatura_ntc_camara_C': sample.temperatura_camara,
        'ntc_localizacao_atual': 'CAMARA_A2',
        'seguranca_termica_independente': sample.estado_protecao_termica,
        'umidade_graos_final_percentual': sample.umidade_final,
        'umidade_limite_empresa_percentual': sample.limite_umidade_empresa,
        'metodo_umidade': sample.metodo_umidade,
        'aquecimento_realizado': sample.aquecimento_realizado,
        'aquecedor_habilitado_por_firmware': False,
        'movimento_simulado': sample.movimento_simulado,
        'historico': sample.medicoes,
        'movimentos_motor': sample.movimentos,
        'eventos_servo_pwm': sample.eventos_servo_pwm,
        'erro': session.mensagem_erro,
        'codigo_erro_arduino': sample.codigo_erro_arduino,
        'recuperacao_status': sample.recuperacao_status,
        'eventos_recuperacao': sample.eventos_recuperacao,
        'observacoes': (
            'Perda de massa nao equivale ao teor de umidade dos graos. '
            'NTC A2 foi realocado para a camara conforme relatado; sem '
            'validacao metrologica da temperatura da camara. '
            'V10.2.4: aquecedor bloqueado ate corte termico independente. '
            'Diferenca P1-P2 so compensa offset ADITIVO CONSTANTE e nao corrige '
            'deriva termica, tempo, forca nos cabos, evaporacao nao-aquosa ou ruido. '
            'ROI automatica e CNN exigem validacao metrologica.'
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
    caminho_balanco = pasta / f'{nome}_balanco_massa.csv'
    colunas_balanco = [
        'P1_g', 'P2_g', 'PF_g', 'delta_posicionamento_P2_menos_P1_g',
        'diferenca_inicial_P1_menos_P2_g', 'PF_corrigido_referencia_P1_g',
        'perda_massa_g', 'perda_massa_percentual',
        'perda_massa_percentual_referencia_P1', 'tendencia_leitura_massa',
        'aquecimento_realizado', 'umidade_graos_final_percentual'
    ]
    with caminho_balanco.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=colunas_balanco, delimiter=';')
        writer.writeheader()
        writer.writerow({k: dado.get(k) for k in colunas_balanco})
    caminho_mov = pasta / f'{nome}_movimentos.csv' 
    with caminho_mov.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['sentido', 'passos_executados', 'passos_totais', 'posicao_comandada'], delimiter=';')
        writer.writeheader()
        writer.writerows(sample.movimentos)
    caminho_resumo = pasta / f'{nome}_resultado_tcc.md'
    caminho_resumo.write_text(_montar_resumo_tcc(dado), encoding='utf-8')
    print(f'[RELATORIO] {caminho_resumo}')
    print(f'[RELATORIO] {caminho_mov}')
    print(f'[RELATORIO] {caminho_json}')
    print(f'[RELATORIO] {caminho_balanco}')
    print(f'[RELATORIO] {caminho_csv}')
    return caminho_json, caminho_csv


def _montar_resumo_tcc(dado):
    """Texto fiel aos dados, sem interpretar deriva como desidratacao."""
    def valor(v, unidade=' g'):
        return 'NAO OBTIDO' if v is None else f'{float(v):.4f}'.replace('.', ',') + unidade
    def campo(nome):
        return dado.get(nome)
    aquec = bool(campo('aquecimento_realizado'))
    eventos = campo('historico') or []
    tvalidas = [x.get('temp_ntc_c') for x in eventos if isinstance(x.get('temp_ntc_c'), (int, float))]
    faixa = f"{min(tvalidas):.2f} a {max(tvalidas):.2f} C" if tvalidas else 'NAO OBTIDA'
    linhas = [
        '# Registro de ensaio integrado - TCC', '',
        f"**ID:** {campo('id')}",
        f"**Resultado:** {campo('status')}",
        f"**Motivo:** {campo('motivo')}", '',
        '## Dados experimentais', '',
        '| Variavel | Valor |', '|---|---:|',
        f"| P1 - centro | {valor(campo('P1_g'))} |",
        f"| P2 - secagem antes do calor | {valor(campo('P2_g'))} |",
        f"| PF - final | {valor(campo('PF_g'))} |",
        f"| Offset P2-P1 | {valor(campo('delta_posicionamento_P2_menos_P1_g'))} |",
        f"| PF corrigido = PF-(P2-P1) | {valor(campo('PF_corrigido_referencia_P1_g'))} |",
        f"| Variacao P2-PF | {valor(campo('perda_massa_g'))} |",
        f"| Temperatura NTC na camara - faixa | {faixa} |",
        f"| Deteccoes CNN validas na ROI | {campo('total_deteccoes_validas')} |",
        f"| Deteccoes classificadas bad | {campo('deteccoes_bad')} |",
        f"| Percentual bad | {campo('percentual_bad'):.2f}% |",
        f"| Aquecimento real | {'SIM' if aquec else 'NAO'} |", '',
        '## Interpretacao e limites', '',
        'O offset inicial e P2-P1. O peso final corrigido e PF-(P2-P1).',
        'A variacao de massa e P1-PF_corrigido, equivalente a P2-PF.',
        'Esse calculo pressupoe offset aditivo constante e nao corrige deriva termica.',
        'O NTC foi movido para a camara e a leitura foi registrada no A2.',
        'A protecao termica independente nao foi confirmada; o aquecedor permaneceu bloqueado.',
        'Portanto, este ensaio NAO valida desidratacao ou teor de umidade.',
        'A classificacao CNN e baseada em deteccoes automatizadas ainda sem validacao manual de desempenho.',
        'A posicao do elevador decorre de pulsos comandados, nao de encoder ou fim de curso.', '',
        '## Rastreabilidade', '',
        f"Falha Arduino: {campo('codigo_erro_arduino') or 'NENHUMA'}.",
        f"Recuperacao: {campo('recuperacao_status') or 'NAO_APLICAVEL'}.",
        f"Total de pontos na serie: {len(eventos)}.",
    ]
    return '\n'.join(linhas) + '\n'
