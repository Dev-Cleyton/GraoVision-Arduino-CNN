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
    modo: str = 'HOUGH_CIRCULO_INTERNO_V2'

    def to_dict(self):
        return asdict(self)


class ROIFalhou(RuntimeError):
    pass


def detectar_roi(imagem_bgr, proporcao_interna=0.81, limiar_borda=0.40,
                 cobertura_minima=0.80, largura_trabalho=800):
    """Retorna ResultadoROI ou levanta ROIFalhou.

    O circulo interno e calculado a partir do aro real da bandeja.
    O metodo foi testado offline em fotos de bandejas aproximadamente circulares;
    inclinacao acentuada, reflexos ou pouco contorno visivel podem falhar.
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
    # A versao V1 exigia 95% do ARO EXTERNO na foto. Isso rejeitava
    # bandejas na borda inferior mesmo quando a AREA INTERNA estava inteira.
    # V2 admite aro exterior parcialmente visivel, exigindo a ROI interna
    # totalmente na imagem e bordas suficientes para confirmar o circulo.
    candidatos = []
    modos_busca = [
        (75, 0.35, 0.53), (60, 0.35, 0.53), (50, 0.35, 0.53),
        (65, 0.30, 0.68), (50, 0.30, 0.68),
    ]
    for sensibilidade, frac_min, frac_max in modos_busca:
        circulos = cv2.HoughCircles(
            cinza, cv2.HOUGH_GRADIENT,
            dp=1.2, minDist=max(35, int(h * 0.28)),
            param1=120, param2=sensibilidade,
            minRadius=int(h * frac_min), maxRadius=int(h * frac_max))
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
            # A circunferencia interior deve estar 100% visivel.
            raio_interno_testado = proporcao_interna * r
            if (x - raio_interno_testado < 1 or
                y - raio_interno_testado < 1 or
                x + raio_interno_testado >= w - 1 or
                y + raio_interno_testado >= h - 1):
                continue
            # Exige bordas em pelo menos dois setores do aro.
            # A iluminacao pode ocultar metade do aro em fotos reais.
            borda_por_angulo = np.zeros(len(valido), dtype=bool)
            borda_por_angulo[valido] = bordas_tolerantes[yy[valido], xx[valido]] > 0
            setores_validos = 0
            for setor in range(4):
                trecho = slice(setor * 60, (setor + 1) * 60)
                qtd_visivel = int(np.sum(valido[trecho]))
                if qtd_visivel >= 10 and np.mean(borda_por_angulo[trecho][valido[trecho]]) >= 0.16:
                    setores_validos += 1
            if setores_validos < 2:
                continue
            # Priorizar bordas suficientes e circulo mais integral na fotografia.
            nota = suporte * cobertura
            candidatos.append((nota, int(x), int(y), int(r), cobertura, suporte))
        # Um candidato confiavel com alta sensibilidade costuma ser melhor.
        if candidatos:
            break
    if not candidatos:
        raise ROIFalhou(
            'Bandeja nao identificada com seguranca (V2). Verifique enquadramento, '
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
