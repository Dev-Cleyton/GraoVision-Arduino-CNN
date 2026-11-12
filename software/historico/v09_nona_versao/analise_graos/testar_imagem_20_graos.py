"""Teste offline do hardware: inferencia Roboflow em fotografia existente.
Executar na pasta analise_graos, com .env contendo ROBOFLOW_API_KEY.
"""
import argparse
import csv
import os
from pathlib import Path

import cv2
from dotenv import load_dotenv
from core.grain_classifier import GrainClassifier


def main():
    parser = argparse.ArgumentParser(description='Diagnostico CNN em imagem existente (sem Arduino/camera)')
    parser.add_argument('--imagem', required=True)
    parser.add_argument('--manual', type=int, default=20)
    parser.add_argument('--saida', default='diagnostico_20_graos')
    args = parser.parse_args()
    caminho = Path(args.imagem).resolve()
    imagem = cv2.imread(str(caminho))
    if imagem is None:
        raise SystemExit(f'Imagem nao encontrada ou invalida: {caminho}')
    load_dotenv(Path(__file__).resolve().parent / '.env')
    chave = os.getenv('ROBOFLOW_API_KEY')
    if not chave:
        raise SystemExit('ROBOFLOW_API_KEY ausente no .env da pasta analise_graos')
    classificador = GrainClassifier(api_key=chave)
    resultado = classificador.model.predict(str(caminho), confidence=classificador.CONFIANCA,
                                             overlap=classificador.OVERLAP).json()
    preds = resultado.get('predictions', [])
    saida = Path(args.saida)
    saida.mkdir(parents=True, exist_ok=True)
    linhas = []
    classes = {}
    for idx, p in enumerate(preds, 1):
        try:
            x, y, w, h = [float(p[k]) for k in ('x','y','width','height')]
        except (KeyError, ValueError, TypeError):
            print(f'Predicao {idx} sem coordenadas; ignorada')
            continue
        classe = str(p.get('class','desconhecida'))
        conf = float(p.get('confidence', 0))
        classes[classe] = classes.get(classe, 0) + 1
        cor = (0,0,255) if classe.lower() == 'bad' else (0,170,0)
        x1,y1,x2,y2 = round(x-w/2),round(y-h/2),round(x+w/2),round(y+h/2)
        cv2.rectangle(imagem,(x1,y1),(x2,y2),cor,2)
        cv2.putText(imagem,f'{idx}: {classe} {conf:.2f}',(max(0,x1),max(20,y1-7)),
                    cv2.FONT_HERSHEY_SIMPLEX,0.55,cor,2)
        linhas.append((idx,classe,round(conf,4),round(x,1),round(y,1),round(w,1),round(h,1)))
    img_saida = saida / 'imagem_20_graos_detectada.jpg'
    csv_saida = saida / 'deteccoes_20_graos.csv'
    if not cv2.imwrite(str(img_saida), imagem):
        raise SystemExit('Falha ao salvar imagem anotada')
    with csv_saida.open('w',encoding='utf-8-sig',newline='') as f:
        writer = csv.writer(f,delimiter=';')
        writer.writerow(('id','classe','confianca','x','y','largura','altura'))
        writer.writerows(linhas)
    print('\n=== VALIDACAO CNN EM FOTO EXISTENTE ===')
    print(f'Imagem: {caminho.name}')
    print(f'Contagem manual informada: {args.manual}')
    print(f'Deteccoes: {len(linhas)} (imagem completa, SEM ROI)')
    print(f'Classes: {classes}')
    print(f'Diferenca numerica: {len(linhas)-args.manual:+d}')
    if args.manual > 0:
        print(f'Erro absoluto relativo de contagem: {100*abs(len(linhas)-args.manual)/args.manual:.2f}%')
    print('ATENCAO: diferenca numerica nao mede acerto por grao; confira caixas, duplicatas e falsos positivos.')
    print(f'Imagem anotada: {img_saida}')
    print(f'CSV: {csv_saida}')


if __name__ == '__main__':
    main()
