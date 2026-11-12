"""Simulador matematico OFFLINE: sem Arduino, camera, sensor ou relé.

Usa P1/P2 medidos e um PF HIPOTETICO, sem alterar o relatorio original.
Exemplo: python simular_secagem_offline.py --p1 10.8414 --p2 10.8994 --pf 10.4994
"""
import argparse
import json
from core.balanco_massa import calcular_balanco


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--p1', type=float, required=True)
    ap.add_argument('--p2', type=float, required=True)
    ap.add_argument('--pf', type=float, required=True)
    args = ap.parse_args()
    resultado = calcular_balanco(args.p1, args.p2, args.pf)
    print('SIMULACAO MATEMATICA - SEM AQUECIMENTO REAL')
    print(json.dumps(resultado, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
