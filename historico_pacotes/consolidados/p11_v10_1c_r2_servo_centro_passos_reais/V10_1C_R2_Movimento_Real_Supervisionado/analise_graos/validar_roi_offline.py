# -*- coding: utf-8 -*-
"""Exibe a ROI da V10.1B em foto existente, sem Arduino, CNN ou internet."""
import argparse
import json
from pathlib import Path
import cv2
from core.roi_automatica import detectar_roi, ROIFalhou, visualizar_roi


def main():
    ap = argparse.ArgumentParser(description='ROI automatica V10.1B: teste offline')
    ap.add_argument('--imagem', required=True, help='Caminho da fotografia original JPG')
    ap.add_argument('--saida', default='resultado_roi_v101b')
    a = ap.parse_args()
    out = Path(a.saida); out.mkdir(parents=True, exist_ok=True)
    img = cv2.imread(a.imagem)
    if img is None:
        raise SystemExit('ERRO: fotografia nao localizada: ' + a.imagem)
    try:
        roi = detectar_roi(img)
    except ROIFalhou as exc:
        print('[REVISAR] ' + str(exc))
        (out/'roi_resultado.json').write_text(json.dumps({'status':'REVISAR','motivo':str(exc)},indent=2,ensure_ascii=False), encoding='utf8')
        return 2
    destino = out/'roi_resultado.jpg'
    if not cv2.imwrite(str(destino), visualizar_roi(img,roi)):
        raise SystemExit('Falha ao salvar visualizacao')
    (out/'roi_resultado.json').write_text(json.dumps({'status':'OK','roi':roi.to_dict()},indent=2,ensure_ascii=False), encoding='utf8')
    print(f'[OK] centro=({roi.cx},{roi.cy}), raio_interno={roi.raio}, '
          f'raio_externo={roi.raio_bandeja}, cobertura={roi.cobertura:.2f}, '
          f'suporte={roi.suporte_borda:.2f}')
    print('[IMAGEM] ' + str(destino))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
