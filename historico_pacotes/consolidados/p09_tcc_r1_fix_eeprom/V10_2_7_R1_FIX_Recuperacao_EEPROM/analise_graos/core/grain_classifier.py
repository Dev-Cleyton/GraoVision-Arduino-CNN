# -*- coding: utf-8 -*-
"""V10.1B: classificacao da imagem ORIGINAL com ROI AUTO pela borda da bandeja.

A ROI fixa so serve para comparacao opcional e NUNCA e utilizada como
fallback silencioso quando ROI_AUTOMATICA=1. A decisao continua >25% bad.
"""
import csv
import json
import math
import os
from collections import Counter
from pathlib import Path

import cv2
from roboflow import Roboflow
from core.roi_automatica import detectar_roi, ROIFalhou, visualizar_roi


class GrainClassifier:
    ROI_CX = 0.49
    ROI_CY = 0.50
    ROI_RAIO_ALTURA = 0.35
    CONFIANCA = 28
    OVERLAP = 24

    def __init__(self, api_key=None, project='kernel-corn', version=54,
                 model=None, usar_roi_automatica=None):
        # model injetado e usado SOMENTE nos testes unitarios sem rede.
        if model is None:
            if not api_key:
                raise ValueError('ROBOFLOW_API_KEY nao configurada no .env')
            rf = Roboflow(api_key=api_key)
            model = rf.workspace().project(project).version(version).model
        self.model = model
        if usar_roi_automatica is None:
            usar_roi_automatica = os.getenv('ROI_AUTOMATICA', '1').strip() != '0'
        self.usar_roi_automatica = bool(usar_roi_automatica)

    @staticmethod
    def _dentro(x, y, cx, cy, raio):
        return (x - cx) ** 2 + (y - cy) ** 2 <= raio ** 2

    @staticmethod
    def _escrever_json(caminho, dados):
        with open(caminho, 'w', encoding='utf-8') as arquivo:
            json.dump(dados, arquivo, indent=2, ensure_ascii=False)

    def classificar(self, sample):
        amostras = list(sample.imagens)
        if len(amostras) != 1:
            # No protocolo V10.1, imagens adicionais do mesmo lote poderiam
            # duplicar os graos. Nao aceitar soma silenciosa.
            raise RuntimeError(f'V10.1B requer 1 fotografia; recebidas {len(amostras)}')

        total = ruins = 0
        classes_dentro_roi = Counter()
        sample.roi_modo = 'AUTOMATICA_BANDEJA_V2' if self.usar_roi_automatica else 'FIXA_LEGADA_COMPARACAO'
        sample.roi_status = 'NAO_EXECUTADA'

        for indice, caminho in enumerate(amostras, 1):
            original = cv2.imread(caminho)
            if original is None:
                raise RuntimeError(f'Imagem nao foi encontrada ou esta ilegivel: {caminho}')
            h, w = original.shape[:2]
            cx_f = int(w * self.ROI_CX)
            cy_f = int(h * self.ROI_CY)
            r_f = int(h * self.ROI_RAIO_ALTURA)
            base = str(Path(caminho).with_suffix(''))
            caminho_roi_json = base + '_roi_automatica.json'

            if self.usar_roi_automatica:
                try:
                    roi = detectar_roi(original)
                except ROIFalhou as exc:
                    sample.roi_status = 'REVISAR'
                    registro = {
                        'status': 'REVISAR', 'modo': 'AUTOMATICA_BANDEJA_V2',
                        'imagem': caminho, 'motivo': str(exc),
                        'fallback_fixo_usado': False,
                    }
                    sample.roi_registros.append(registro)
                    self._escrever_json(caminho_roi_json, registro)
                    raise ROIFalhou(f'Falha na localizacao automatica da bandeja: {exc}') from exc
                cx, cy, raio = roi.cx, roi.cy, roi.raio
                info_roi = roi.to_dict()
                overlay = visualizar_roi(original, roi)
            else:
                cx, cy, raio = cx_f, cy_f, r_f
                info_roi = {'cx': cx, 'cy': cy, 'raio': raio, 'modo': 'FIXA_LEGADA_COMPARACAO'}
                overlay = original.copy()
                cv2.circle(overlay, (cx, cy), raio, (255, 255, 0), 3)

            caminho_so_roi = base + '_roi_visualizada.jpg'
            if not cv2.imwrite(caminho_so_roi, overlay):
                raise RuntimeError(f'Falha ao gravar imagem de ROI: {caminho_so_roi}')
            print(f'[ROI] {sample.roi_modo}: centro=({cx},{cy}) raio={raio} px')
            if self.usar_roi_automatica:
                print(f'[ROI] borda externa={roi.raio_bandeja}px; '
                      f'cobertura={roi.cobertura:.2f}; suporte={roi.suporte_borda:.2f}')
            print(f'[ROI] Visualizacao: {caminho_so_roi}')
            # Registrar geometria ANTES de chamar a API; se Roboflow falhar,
            # o relatorio ainda preserva a ROI detectada na fotografia.
            registro = {
                'status': 'OK', 'modo': sample.roi_modo, 'imagem': caminho,
                'roi': info_roi, 'inferencia_na_imagem_original': True,
                'imagem_roi': caminho_so_roi, 'fallback_fixo_usado': False,
            }
            sample.roi_registros.append(registro)
            sample.roi_status = 'OK'
            self._escrever_json(caminho_roi_json, registro)

            # IMPORTANTE: inferencia na foto ORIGINAL. Nao misturar efeitos
            # de recorte/máscara com o efeito da nova geometria da ROI.
            resultado = self.model.predict(
                caminho, confidence=self.CONFIANCA, overlap=self.OVERLAP
            ).json()
            predicoes = resultado.get('predictions', [])
            if not isinstance(predicoes, list):
                raise RuntimeError('Resposta do Roboflow sem lista predictions valida')

            aceitas = descartadas = bad = 0
            cnt_fixo = bad_fixo = 0
            contagem_classes = Counter()
            linhas = []
            anotada = overlay.copy()

            for numero, pred in enumerate(predicoes, 1):
                try:
                    x = float(pred['x']); y = float(pred['y'])
                    largura_caixa = float(pred['width']); altura_caixa = float(pred['height'])
                    confianca = float(pred.get('confidence', 0))
                    if not all(map(math.isfinite, (x,y,largura_caixa,altura_caixa,confianca))):
                        raise ValueError('valores nao finitos')
                except (KeyError, ValueError, TypeError, OverflowError) as exc:
                    print(f'[IA][AVISO] Predicao {numero} com geometria invalida: {exc}')
                    continue
                classe = str(pred.get('class', 'DESCONHECIDA'))
                contagem_classes[classe] += 1
                dentro = self._dentro(x, y, cx, cy, raio)
                dentro_fixo = self._dentro(x, y, cx_f, cy_f, r_f)
                if dentro_fixo:
                    cnt_fixo += 1
                    if classe.lower() == 'bad': bad_fixo += 1
                if dentro:
                    aceitas += 1
                    classes_dentro_roi[classe] += 1
                    if classe.lower() == 'bad': bad += 1
                else:
                    descartadas += 1

                x1 = int(x - largura_caixa / 2); y1 = int(y - altura_caixa / 2)
                x2 = int(x + largura_caixa / 2); y2 = int(y + altura_caixa / 2)
                cor = ((0,0,255) if classe.lower() == 'bad' else (0,180,0)) if dentro else (130,130,130)
                cv2.rectangle(anotada, (x1,y1), (x2,y2), cor, 2)
                label = f'{classe} {confianca:.2f}' + (' FORA' if not dentro else '')
                cv2.putText(anotada, label, (max(0, x1), max(20, y1-6)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.48, cor, 2)
                linhas.append([numero, classe, round(confianca,4), round(x,1),round(y,1),
                               round(largura_caixa,1),round(altura_caixa,1),
                               'SIM' if dentro else 'NAO', 'SIM' if dentro_fixo else 'NAO'])

            total += aceitas
            ruins += bad
            nome = str(Path(caminho).with_suffix(''))
            caminho_anotada = nome + '_detectada.jpg'
            caminho_csv = nome + '_deteccoes.csv'
            if not cv2.imwrite(caminho_anotada, anotada):
                raise RuntimeError(f'Falha ao gravar imagem anotada: {caminho_anotada}')
            with open(caminho_csv, 'w', newline='', encoding='utf-8-sig') as arquivo:
                writer = csv.writer(arquivo, delimiter=';')
                writer.writerow(['id','classe','confianca','x','y','largura','altura',
                                 'dentro_roi', 'dentro_roi_fixa_referencia'])
                writer.writerows(linhas)

            comparacao_fixa = {
                'centro_x': cx_f, 'centro_y': cy_f, 'raio': r_f,
                'deteccoes_aceitas': cnt_fixo, 'deteccoes_bad': bad_fixo,
                'percentual_bad': round(100.0*bad_fixo/cnt_fixo,4) if cnt_fixo else None,
            }
            registro.update({
                'comparacao_roi_fixa': comparacao_fixa,
                'deteccoes_brutas': len(predicoes),
                'deteccoes_aceitas': aceitas, 'deteccoes_fora': descartadas,
                'deteccoes_bad': bad,
                'percentual_bad': round(100.0*bad/aceitas,4) if aceitas else None,
                'classes_brutas': dict(contagem_classes),
                'imagem_anotada': caminho_anotada,
                'arquivo_deteccoes': caminho_csv,
            })
            self._escrever_json(caminho_roi_json, registro)
            print(f'[IA] {len(predicoes)} brutas; {aceitas} aceitas; {descartadas} fora; '
                  f'{bad} bad; percentual={registro["percentual_bad"]}')
            print(f'[IA] ROI fixa (comparacao, sem influenciar a decisao): '
                  f'{cnt_fixo} aceitas; {bad_fixo} bad; '
                  f'percentual={comparacao_fixa["percentual_bad"]}')
            print(f'[IA] Resultado: {caminho_anotada} | {caminho_csv}')

        sample.classes_detectadas = dict(classes_dentro_roi)
        sample.total_graos = total
        sample.graos_ruins = ruins
        sample.percentual_ruins = 100.0 * ruins / total if total else 0.0
        print('[IA][ATENCAO] As contagens representam DETECCOES, nao contagem manual validada.')
        return sample
