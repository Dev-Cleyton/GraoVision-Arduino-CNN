# -*- coding: utf-8 -*-
"""Teste de geometria ROI sem Roboflow/Arduino, numa ou mais fotografias."""
import argparse
import json
import sys
from pathlib import Path
import cv2
import numpy as np
from roi_automatica import detectar_roi, visualizar_roi, ROIFalhou


def ler_unicode(caminho):
    dados=np.fromfile(str(caminho),dtype=np.uint8)
    return cv2.imdecode(dados,cv2.IMREAD_COLOR)


def salvar_unicode(caminho, imagem):
    ok, buf=cv2.imencode('.jpg',imagem,[cv2.IMWRITE_JPEG_QUALITY,95])
    if not ok: raise RuntimeError(f'Falha ao salvar: {caminho}')
    buf.tofile(str(caminho))


def executar(arquivos, saida):
    saida.mkdir(parents=True,exist_ok=True)
    erros=0
    for numero, arquivo in enumerate(arquivos,1):
        origem=Path(arquivo)
        imagem=ler_unicode(origem)
        if imagem is None:
            print(f'[ERRO] imagem indisponivel: {origem}')
            erros+=1
            continue
        nome=f'{numero:02d}_{origem.stem}'
        try:
            roi=detectar_roi(imagem)
            salvar_unicode(saida/f'{nome}_ROI_AUTO.jpg', visualizar_roi(imagem,roi))
            with (saida/f'{nome}_ROI_AUTO.json').open('w',encoding='utf-8') as f:
                json.dump({'imagem':str(origem),'roi':roi.to_dict(),'status':'OK'}, f,ensure_ascii=False,indent=2)
            print(f'[OK] {origem.name} | centro=({roi.cx},{roi.cy}), raio_interno={roi.raio}, '
                  f'raio_bandeja={roi.raio_bandeja}, cobertura={roi.cobertura:.2f}, suporte={roi.suporte_borda:.2f}')
        except ROIFalhou as exc:
            print(f'[REVISAR] {origem.name}: {exc}')
            with (saida/f'{nome}_ROI_AUTO.json').open('w',encoding='utf-8') as f:
                json.dump({'imagem':str(origem),'status':'REVISAR','motivo':str(exc)},f,ensure_ascii=False,indent=2)
            erros+=1
    print(f'Processamento terminado: {len(arquivos)-erros} sucesso(s), {erros} revisar.')
    return 0 if not erros else 2


def main():
    ap=argparse.ArgumentParser(description='Encontra bandeja automaticamente, sem Roboflow ou Arduino.')
    ap.add_argument('--imagem',action='append',required=True,help='Imagem original; repita para varias fotos.')
    ap.add_argument('--saida',default='resultado_roi_automatica')
    a=ap.parse_args()
    return executar(a.imagem,Path(a.saida))


if __name__ == '__main__':
    sys.exit(main())
