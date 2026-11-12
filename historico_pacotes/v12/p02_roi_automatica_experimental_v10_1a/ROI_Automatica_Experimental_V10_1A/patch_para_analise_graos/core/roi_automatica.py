# -*- coding: utf-8 -*-
"""Localiza a bandeja circular pela borda, sem usar classificacoes CNN.

Uso seguro: se a borda nao puder ser confirmada, interrompa a analise
sem substituir automaticamente por uma ROI fixa ou falsa aprovacao.
"""
from dataclasses import dataclass, asdict
from typing import Optional
import cv2
import numpy as np


@dataclass(frozen=True)
class ResultadoROI:
    cx: int
    cy: int
    raio: int                # raio INTERNO seguro para classificacao
    raio_bandeja: int       # raio detectado da borda exterior
    cobertura: float        # fracao angular que fica visivel na imagem
    suporte_borda: float    # fracao aproximada de arcos com bordas detectadas
    modo: str = 'HOUGH_CIRCULO_INTERNO'

    def to_dict(self):
        return asdict(self)


class ROIFalhou(RuntimeError):
    pass


def detectar_roi(imagem_bgr, proporcao_interna=0.81, limiar_borda=0.40,
                 cobertura_minima=0.95, largura_trabalho=800):
    """Retorna ResultadoROI ou levanta ROIFalhou.

    O circulo interno e calculado a partir do aro real da bandeja.
    O metodo foi testado offline em fotos de bandejas aproximadamente circulares;
    inclinacao acentuada, sombras fortes e bandeja cortada podem falhar.
    """
    if imagem_bgr is None or imagem_bgr.ndim != 3 or imagem_bgr.shape[2] != 3:
        raise ROIFalhou('Imagem invalida para detectar a bandeja.')
    altura, largura = imagem_bgr.shape[:2]
    if min(altura, largura) < 240:
        raise ROIFalhou('Resolucao insuficiente para verificar o aro.')
    escala = min(1.0, float(largura_trabalho) / float(max(largura, altura)))
    if escala < 1.0:
        imagem = cv2.resize(imagem_bgr, None, fx=escala, fy=escala,
                            interpolation=cv2.INTER_AREA)
    else:
        imagem = imagem_bgr.copy()
    h, w = imagem.shape[:2]
    cinza = cv2.cvtColor(imagem, cv2.COLOR_BGR2GRAY)
    cinza = cv2.GaussianBlur(cinza, (9, 9), 1.4)
    bordas = cv2.Canny(cinza, 60, 140)
    bordas_tolerantes = cv2.dilate(
        bordas, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))
    # Multiplas sensibilidades: primeiro deteccoes fortes, depois mais fracas.
    candidatos = []
    for sensibilidade in (75, 60, 50):
        circulos = cv2.HoughCircles(
            cinza, cv2.HOUGH_GRADIENT,
            dp=1.2, minDist=max(35, int(h * 0.28)),
            param1=120, param2=sensibilidade,
            minRadius=int(h * 0.35), maxRadius=int(h * 0.53))
        if circulos is None:
            continue
        for x, y, r in np.rint(circulos[0]).astype(int):
            if r <= 0:
                continue
            angulos = np.linspace(0.0, 2*np.pi, 240, endpoint=False)
            xx = np.rint(x + r*np.cos(angulos)).astype(int)
            yy = np.rint(y + r*np.sin(angulos)).astype(int)
            valido = (xx >= 1) & (xx < w - 1) & (yy >= 1) & (yy < h - 1)
            cobertura = float(np.mean(valido))
            if cobertura < cobertura_minima:
                continue
            suporte = float(np.mean(bordas_tolerantes[yy[valido], xx[valido]] > 0))
            if suporte < limiar_borda:
                continue
            # Priorizar arcos identificados e bandeja inteira visivel.
            nota = suporte * cobertura
            candidatos.append((nota, int(x), int(y), int(r), cobertura, suporte))
        # Um candidato confiavel com alta sensibilidade costuma ser melhor.
        if candidatos:
            break
    if not candidatos:
        raise ROIFalhou(
            'Bandeja nao identificada com seguranca. Verifique enquadramento, '
            'visibilidade da borda, iluminacao e inclinacao. Nao classificar.')
    _, x, y, r, cobertura, suporte = max(candidatos, key=lambda item: item[0])
    cx = int(round(x/escala)); cy = int(round(y/escala))
    r_exterior = int(round(r/escala))
    r_interno = max(1, int(round(proporcao_interna * r_exterior)))
    # Garante que a AREA DE CLASSIFICACAO esteja integralmente na imagem.
    if cx-r_interno < 0 or cy-r_interno < 0 or cx+r_interno >= largura or cy+r_interno >= altura:
        raise ROIFalhou('Area interna da bandeja esta cortada na fotografia.')
    return ResultadoROI(cx, cy, r_interno, r_exterior,
                        round(cobertura, 4), round(suporte, 4))


def visualizar_roi(imagem_bgr, roi: ResultadoROI):
    visual = imagem_bgr.copy()
    cv2.circle(visual, (roi.cx, roi.cy), roi.raio_bandeja, (0, 165, 255), 3)
    cv2.circle(visual, (roi.cx, roi.cy), roi.raio, (255, 255, 0), 3)
    cv2.drawMarker(visual, (roi.cx, roi.cy), (0, 0, 255),
                   cv2.MARKER_CROSS, 24, 3)
    return visual
