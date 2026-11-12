"""Anexa resultado de laboratorio validado, preservando o relatorio original.
Nao mede umidade; somente registra um resultado externo autentico.

Exemplo:
python aplicar_umidade_validada.py relatorios/ID_relatorio.json \
 --umidade 16.2 --limite 14.0 --metodo "Laboratorio - protocolo X" \
 --referencia "Laudo 001"
"""
import argparse
import json
import math
from pathlib import Path


def aplicar(caminho, umidade, limite, metodo, referencia):
    arq = Path(caminho)
    dados = json.loads(arq.read_text(encoding='utf-8'))
    if dados.get('status') != 'PENDENTE_VALIDACAO_UMIDADE' or dados.get('status_cnn') != 'APROVADA':
        raise ValueError('Relatorio nao esta pendente de umidade apos aprovacao CNN')
    if not metodo.strip() or not referencia.strip():
        raise ValueError('Metodo e identificacao do laudo/referencia sao obrigatorios')
    u, l = float(umidade), float(limite)
    if not math.isfinite(u) or not math.isfinite(l) or not (0 <= u <= 100 and 0 <= l <= 100):
        raise ValueError('Percentuais fora do intervalo valido [0,100]')
    dados['umidade_graos_final_percentual'] = u
    dados['umidade_limite_empresa_percentual'] = l
    dados['metodo_umidade'] = metodo.strip()
    dados['referencia_laudo_umidade'] = referencia.strip()
    dados['status'] = 'REPROVADA_UMIDADE' if u > l else 'APROVADA'
    dados['motivo'] = (f'Umidade {u:.2f}% superior ao limite {l:.2f}% (metodo validado: {metodo}).'
                       if u > l else
                       f'CNN aprovada e umidade {u:.2f}% dentro do limite {l:.2f}% (metodo validado: {metodo}).')
    destino = arq.with_name(arq.stem + '_UMIDADE_VALIDADA.json')
    if destino.exists():
        raise FileExistsError(f'Relatorio complementar ja existe: {destino}')
    destino.write_text(json.dumps(dados, ensure_ascii=False, indent=2), encoding='utf-8')
    return destino


def main():
    parser = argparse.ArgumentParser(description='Registrar umidade externa validada; NAO e leitura do NTC')
    parser.add_argument('relatorio')
    parser.add_argument('--umidade', required=True, type=float)
    parser.add_argument('--limite', required=True, type=float)
    parser.add_argument('--metodo', required=True)
    parser.add_argument('--referencia', required=True, help='Numero do laudo/identificador da medicao validada')
    args = parser.parse_args()
    print('Relatorio complementar:', aplicar(args.relatorio, args.umidade, args.limite, args.metodo, args.referencia))


if __name__ == '__main__':
    main()
