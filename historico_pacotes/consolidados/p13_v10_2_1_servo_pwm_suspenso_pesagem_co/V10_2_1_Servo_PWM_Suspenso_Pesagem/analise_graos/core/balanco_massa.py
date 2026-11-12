"""Balanco gravimetrico com compensacao de offset de posicao (NAO correcao termica).

P1: massa na base; P2: massa na posicao de secagem ANTES de qualquer calor;
PF: massa FINAL na MESMA posicao de P2.

A igualdade (P1 - (PF - (P2-P1))) == (P2 - PF) pressupoe que o offset
entre posicoes nao mudou. Nao prova perda de agua ou teor de umidade.
"""
import math


def calcular_balanco(p1, p2, pf):
    massas = (p1, p2, pf)
    if any(v is None for v in massas):
        raise ValueError('P1, P2 e PF sao obrigatorios')
    p1, p2, pf = map(float, massas)
    if any(not math.isfinite(v) for v in (p1, p2, pf)) or p1 <= 0 or p2 <= 0 or pf < 0:
        raise ValueError('P1/P2/PF invalidos: P1 e P2 >0; PF >=0; todos finitos')
    offset = p2-p1
    final_corrigido = pf-offset
    delta = p1-final_corrigido
    # Evitar cancelamento numerico em numeros proximos.
    delta = p2-pf
    return {
        'offset_posicao_P2_menos_P1_g': offset,
        'diferenca_inicial_P1_menos_P2_g': -offset,
        'PF_corrigido_referencia_P1_g': final_corrigido,
        'perda_massa_aparente_g': delta,
        'perda_massa_aparente_percentual_ref_P1': 100.0*delta/p1,
        'perda_massa_aparente_percentual_ref_P2': 100.0*delta/p2,
        'tendencia_leitura': ('DIMINUIU' if delta > 0 else 'AUMENTOU' if delta < 0 else 'SEM_VARIACAO'),
        'hipotese': 'offset de posicao aditivo constante; nao corrige deriva termica ou temporal',
        'validade_termica': 'NAO_VALIDADA',
    }
