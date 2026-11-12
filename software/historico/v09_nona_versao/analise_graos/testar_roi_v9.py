"""Versao 9 - teste OFFLINE/ONLINE isolado. Nao controla Arduino.
Uso: python testar_roi_v9.py --imagem imagem_20_graos.jpeg --manual 20
     python testar_roi_v9.py --imagem imagem_20_graos.jpeg --manual 20 --inferir
"""
import argparse
import csv
import os
from pathlib import Path
import cv2
import numpy as np


def preparar_roi(imagem, cx, cy, raio, saida):
    h, w = imagem.shape[:2]
    if not (0 <= cx < w and 0 <= cy < h and raio > 0):
        raise ValueError('Centro ou raio da ROI invalido')
    x1, y1 = max(0, cx-raio), max(0, cy-raio)
    x2, y2 = min(w, cx+raio), min(h, cy+raio)
    # Fundo neutro: reduz objetos externos, sem modificar os pixels dos graos.
    mascara = np.zeros((h,w), dtype=np.uint8)
    cv2.circle(mascara,(cx,cy),raio,255,-1)
    neutro = np.full_like(imagem, 125)
    neutro[mascara>0] = imagem[mascara>0]
    recorte = neutro[y1:y2,x1:x2]
    visual = imagem.copy()
    cv2.circle(visual,(cx,cy),raio,(255,255,0),4)
    cv2.imwrite(str(saida/'roi_visualizacao.jpg'),visual)
    cv2.imwrite(str(saida/'roi_recorte.jpg'),recorte)
    return recorte, (x1,y1), (cx-x1,cy-y1)


def inferir(arquivo, pasta, centro, raio, manual, confianca, overlap):
    from dotenv import load_dotenv
    from roboflow import Roboflow
    load_dotenv()
    chave = os.getenv('ROBOFLOW_API_KEY')
    if not chave:
        raise RuntimeError('ROBOFLOW_API_KEY ausente no .env ou no ambiente')
    modelo = Roboflow(api_key=chave).workspace().project('kernel-corn').version(54).model
    resposta = modelo.predict(str(arquivo), confidence=confianca, overlap=overlap).json()
    predicoes = resposta.get('predictions',[])
    imagem = cv2.imread(str(arquivo))
    aceitas=[]
    descartadas=[]
    for idx,p in enumerate(predicoes,1):
        try:
            x,y = float(p['x']),float(p['y'])
            largura,altura = float(p['width']),float(p['height'])
            conf=float(p.get('confidence',0))
        except (KeyError,ValueError,TypeError):
            continue
        classe=str(p.get('class','desconhecida'))
        dentro=(x-centro[0])**2+(y-centro[1])**2 <= raio**2
        registro=(idx,classe,round(conf,4),round(x,1),round(y,1),round(largura,1),round(altura,1),'SIM' if dentro else 'NAO')
        (aceitas if dentro else descartadas).append(registro)
        cor=(0,180,0) if classe.lower()=='good' else (0,0,255)
        if not dentro: cor=(150,150,150)
        a=(int(x-largura/2),int(y-altura/2));b=(int(x+largura/2),int(y+altura/2))
        cv2.rectangle(imagem,a,b,cor,2)
        cv2.putText(imagem,f'{idx} {classe} {conf:.2f}',(max(0,a[0]),max(20,a[1]-5)),cv2.FONT_HERSHEY_SIMPLEX,.55,cor,2)
    cv2.circle(imagem,centro,raio,(255,255,0),3)
    cv2.imwrite(str(pasta/'roi_detectada.jpg'),imagem)
    with (pasta/'roi_deteccoes.csv').open('w',encoding='utf-8-sig',newline='') as f:
        wr=csv.writer(f,delimiter=';');wr.writerow(['id','classe','confianca','x','y','largura','altura','dentro_roi']);wr.writerows(aceitas+descartadas)
    good=sum(r[1].lower()=='good' for r in aceitas)
    bad=sum(r[1].lower()=='bad' for r in aceitas)
    print(f'Predicoes brutas no recorte: {len(predicoes)}')
    print(f'Dentro da ROI: {len(aceitas)} (good={good}, bad={bad}); fora: {len(descartadas)}')
    if manual is not None:
        print(f'Contagem manual: {manual}; diferenca numerica: {len(aceitas)-manual:+d}; erro relativo absoluto: {abs(len(aceitas)-manual)/manual*100:.2f}%')
        print('ATENCAO: diferenca numerica nao equivale a precisao, recall ou classificacao correta.')
    print(f'Imagem: {pasta/"roi_detectada.jpg"}')
    print(f'CSV: {pasta/"roi_deteccoes.csv"}')


def main():
    a=argparse.ArgumentParser(description='Teste V9 de ROI circular, sem Arduino')
    a.add_argument('--imagem',required=True)
    a.add_argument('--manual',type=int)
    a.add_argument('--inferir',action='store_true',help='Envia recorte ao Roboflow (requer internet e chave)')
    a.add_argument('--cx',type=int,default=560)
    a.add_argument('--cy',type=int,default=980)
    a.add_argument('--raio',type=int,default=445)
    a.add_argument('--confianca',type=int,default=28)
    a.add_argument('--overlap',type=int,default=24)
    a.add_argument('--saida',default='diagnostico_v9')
    args=a.parse_args()
    if args.manual is not None and args.manual <= 0: a.error('--manual deve ser maior que zero')
    origem=Path(args.imagem)
    imagem=cv2.imread(str(origem))
    if imagem is None: a.error(f'Imagem nao encontrada ou ilegivel: {origem}')
    pasta=Path(args.saida);pasta.mkdir(parents=True,exist_ok=True)
    recorte,deslocamento,centro=preparar_roi(imagem,args.cx,args.cy,args.raio,pasta)
    arquivo=pasta/'roi_recorte.jpg'
    print(f'Imagem original: {imagem.shape[1]}x{imagem.shape[0]}')
    print(f'ROI centro=({args.cx},{args.cy}), raio={args.raio}; recorte={recorte.shape[1]}x{recorte.shape[0]}')
    print(f'Visualizacao: {pasta/"roi_visualizacao.jpg"}')
    print(f'Recorte para IA: {arquivo}')
    if args.inferir:
        inferir(arquivo,pasta,centro,args.raio,args.manual,args.confianca,args.overlap)
    else:
        print('Modo offline: nenhum acesso ao Roboflow. Confira a ROI e execute novamente com --inferir.')

if __name__=='__main__':main()
