# -*- coding: utf-8 -*-
"""Compara V10.1 (imagem completa) com V9 (recorte+mascara) NA MESMA FOTO.

Modo offline, sem Roboflow:
  python comparar_cnn_roi_v101_v9.py --imagem imagem.jpg --sem-inferencia

Modo online (requer ROBOFLOW_API_KEY em ambiente ou .env):
  python comparar_cnn_roi_v101_v9.py --imagem imagem.jpg --manual-total 20 --env .env

A V9 recorta um quadrado e neutraliza o exterior da ROI com cinza 125.
Uma terceira condicao (recorte sem mascara) isola parte do efeito do recorte.
Nao altera Arduino, nao escreve em imagens originais e nao expoe a API key.
"""
import argparse
import csv
import json
import math
import os
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np

MODELO = "kernel-corn"
VERSAO = 54
CONFIANCA = 28
OVERLAP = 24
LIMITE_BAD = 25.0


def ler_imagem(arquivo):
    """Le imagens em caminhos Unicode no Windows, sem alterar o arquivo."""
    arr = np.fromfile(str(arquivo), dtype=np.uint8)
    imagem = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if imagem is None:
        raise ValueError(f"Nao foi possivel abrir a imagem: {arquivo}")
    return imagem


def salvar_imagem(caminho, imagem):
    ok, buff = cv2.imencode('.jpg', imagem, [cv2.IMWRITE_JPEG_QUALITY, 95])
    if not ok:
        raise RuntimeError(f"Erro ao gerar imagem: {caminho}")
    Path(caminho).parent.mkdir(parents=True, exist_ok=True)
    buff.tofile(str(caminho))


def geometria(imagem, cx=None, cy=None, raio=None):
    h, w = imagem.shape[:2]
    # Geometria relativa original da V10.1.
    cx = int(w * 0.49) if cx is None else int(cx)
    cy = int(h * 0.50) if cy is None else int(cy)
    raio = int(h * 0.35) if raio is None else int(raio)
    if not (0 <= cx < w and 0 <= cy < h and raio > 0):
        raise ValueError("Centro ou raio da ROI invalidos")
    x1, y1 = max(0, cx - raio), max(0, cy - raio)
    x2, y2 = min(w, cx + raio), min(h, cy + raio)
    if x1 >= x2 or y1 >= y2:
        raise ValueError("ROI vazia")
    return (cx, cy, raio), (x1, y1, x2, y2)


def preparar_variantes(imagem, geom, caixa):
    cx, cy, r = geom
    x1, y1, x2, y2 = caixa
    foto = imagem.copy()
    recorte = imagem[y1:y2, x1:x2].copy()
    mascara = np.zeros(recorte.shape[:2], dtype=np.uint8)
    cv2.circle(mascara, (cx-x1, cy-y1), r, 255, -1)
    recorte_mascarado = np.full_like(recorte, 125)
    recorte_mascarado[mascara > 0] = recorte[mascara > 0]
    visual = imagem.copy()
    cv2.circle(visual, (cx, cy), r, (255, 255, 0), 3)
    return {"V10_1_COMPLETA": foto,
            "RECORTE_SEM_MASCARA": recorte,
            "V9_RECORTE_MASCARA": recorte_mascarado}, visual


def filtrar_deteccoes(predicoes, geom, deslocamento):
    """Normaliza predicoes de imagem inteira ou recorte na coordenada original."""
    cx, cy, raio = geom
    dx, dy = deslocamento
    itens = []
    ignoradas = 0
    for numero, p in enumerate(predicoes, 1):
        try:
            x = float(p['x']) + dx
            y = float(p['y']) + dy
            w = float(p['width'])
            h = float(p['height'])
            conf = float(p.get('confidence', 0.0))
            if not all(map(math.isfinite, [x,y,w,h,conf])) or w <= 0 or h <= 0:
                raise ValueError('valor invalido')
        except (TypeError, KeyError, ValueError):
            ignoradas += 1
            continue
        dentro = (x-cx)**2 + (y-cy)**2 <= raio**2
        itens.append({"id":numero, "classe":str(p.get('class','DESCONHECIDA')).lower(),
            "confianca":conf,"x":x,"y":y,"largura":w,"altura":h,
            "dentro_roi":"SIM" if dentro else "NAO"})
    return itens, ignoradas


def estatisticas(itens, total_manual=None, ruins_manual=None):
    aceitas = [p for p in itens if p['dentro_roi']=='SIM']
    good = sum(p['classe']=='good' for p in aceitas)
    bad = sum(p['classe']=='bad' for p in aceitas)
    outras = len(aceitas)-good-bad
    pct = 100.0*bad/len(aceitas) if aceitas else None
    if not aceitas:
        decisao = 'INCONCLUSIVA_SEM_DETECCOES'
    elif outras:
        decisao = 'REVISAR_CLASSES_NAO_PREVISTAS'
    elif pct > LIMITE_BAD:
        decisao = 'REPROVADA_CNN'
    else:
        decisao = 'APROVADA_CNN'
    resultado = {"brutas_validas":len(itens),"dentro_roi":len(aceitas),
        "fora_roi":len(itens)-len(aceitas),"good":good,"bad":bad,"outras":outras,
        "percentual_bad":round(pct,4) if pct is not None else None,"decisao":decisao}
    if total_manual is not None:
        resultado['manual_total'] = total_manual
        resultado['diferenca_contagem'] = len(aceitas)-total_manual
        resultado['erro_numerico_percentual'] = round(abs(len(aceitas)-total_manual)/total_manual*100, 2) if total_manual else None
    if ruins_manual is not None:
        resultado['manual_bad'] = ruins_manual
        resultado['diferenca_bad'] = bad-ruins_manual
    return resultado


def anotar(imagem, itens, geom, nome):
    final = imagem.copy()
    cx,cy,raio = geom
    cv2.circle(final, (cx,cy),raio,(255,255,0),3)
    for item in itens:
        x,y,w,h = (item[z] for z in ('x','y','largura','altura'))
        a=(int(x-w/2),int(y-h/2)); b=(int(x+w/2),int(y+h/2))
        cor = (140,140,140) if item['dentro_roi']=='NAO' else ((0,0,255) if item['classe']=='bad' else (0,185,0))
        cv2.rectangle(final,a,b,cor,2)
        cv2.putText(final,str(item['id']),(max(0,a[0]),max(24,a[1]-5)),cv2.FONT_HERSHEY_SIMPLEX,.55,cor,2)
    cv2.putText(final,nome,(15,35),cv2.FONT_HERSHEY_SIMPLEX,.9,(255,255,255),2)
    return final


def csv_detecoes(caminho, itens):
    with open(caminho, 'w', encoding='utf-8-sig', newline='') as f:
        campos=['id','classe','confianca','x','y','largura','altura','dentro_roi']
        wr=csv.DictWriter(f,fieldnames=campos,delimiter=';');wr.writeheader()
        for row in itens: wr.writerow({k:(round(v,4) if isinstance(v,float) else v) for k,v in row.items()})


def configurar_modelo(env_arg=None):
    try:
        from dotenv import load_dotenv
        from roboflow import Roboflow
    except ImportError as e:
        raise RuntimeError('Instale dependencias: pip install opencv-python numpy python-dotenv roboflow') from e
    if env_arg:
        load_dotenv(dotenv_path=Path(env_arg).resolve())
    else:
        # Procura primeiro .env na pasta atual e nos pais do script.
        load_dotenv()
    chave = os.environ.get('ROBOFLOW_API_KEY', '').strip()
    if not chave:
        raise RuntimeError('ROBOFLOW_API_KEY ausente. Utilize --env CAMINHO\\.env ou variavel de ambiente; nao divulgue a chave.')
    return Roboflow(api_key=chave).workspace().project(MODELO).version(VERSAO).model


def executar(arquivo, saida, geom_arg, manual_total, manual_bad, sem_inferencia, env_arg):
    imagem = ler_imagem(arquivo)
    geom,caixa=geometria(imagem,*geom_arg)
    variantes,visual=preparar_variantes(imagem,geom,caixa)
    pasta=Path(saida);pasta.mkdir(parents=True,exist_ok=True)
    salvar_imagem(pasta/'ROI_Visualizacao.jpg',visual)
    x1,y1,x2,y2=caixa
    caminhos={}
    for nome,imagem_var in variantes.items():
        nome_arquivo=pasta/f'{nome}_entrada.jpg'
        if nome=='V10_1_COMPLETA':
            # Uso do arquivo ORIGINAL para nao recompactar a imagem no baseline.
            caminhos[nome]=str(Path(arquivo).resolve())
        else:
            salvar_imagem(nome_arquivo,imagem_var)
            caminhos[nome]=str(nome_arquivo.resolve())
    print(f'Imagem: {arquivo} | {imagem.shape[1]}x{imagem.shape[0]}')
    print(f'ROI: centro=({geom[0]},{geom[1]}), raio={geom[2]}, caixa=({x1},{y1},{x2},{y2})')
    print(f'Arquivos de visualizacao: {pasta.resolve()}')
    if sem_inferencia:
        print('PREPARACAO OFFLINE CONCLUIDA: sem chamadas ao Roboflow e sem classificacoes novas.')
        return None
    modelo=configurar_modelo(env_arg)
    resumo={"imagem":str(Path(arquivo).resolve()),"momento":datetime.now().isoformat(timespec='seconds'),
        "configuracao":{"modelo":MODELO,"versao":VERSAO,"confianca":CONFIANCA,"overlap":OVERLAP,
          "roi_cx":geom[0],"roi_cy":geom[1],"roi_raio":geom[2],"limite_bad_percentual":LIMITE_BAD},
        "observacao":"Contagem numerica nao equivale a acuracia, precisao, recall ou validacao das classes.",
        "variantes":{}}
    for nome,caminho in caminhos.items():
        print(f'\n[INFERENCIA] {nome}: {caminho}')
        resposta=modelo.predict(caminho,confidence=CONFIANCA,overlap=OVERLAP).json()
        predicoes=resposta.get('predictions',[])
        # No recorte, as predicoes estao em coordenadas locais.
        deslocamento=(0,0) if nome=='V10_1_COMPLETA' else (x1,y1)
        itens,ignoradas=filtrar_deteccoes(predicoes,geom,deslocamento)
        estat=estatisticas(itens,manual_total,manual_bad)
        estat['brutas_recebidas']=len(predicoes)
        estat['ignoradas_invalidas']=ignoradas
        resumo['variantes'][nome]=estat
        csv_detecoes(pasta/f'{nome}_deteccoes.csv',itens)
        salvar_imagem(pasta/f'{nome}_detectada.jpg',anotar(imagem,itens,geom,nome))
        # Resposta bruta para auditoria/reprodutibilidade. Nao inclui chave de API.
        with open(pasta/f'{nome}_predicoes_brutas.json','w',encoding='utf-8') as arq:
            json.dump({"predictions":predicoes},arq,ensure_ascii=False,indent=2)
        print(f"  Aceitas={estat['dentro_roi']}, fora={estat['fora_roi']}, good={estat['good']}, bad={estat['bad']}, "
              f"bad%={estat['percentual_bad']}, decisao={estat['decisao']}")
        if 'erro_numerico_percentual' in estat:
            print(f"  Erro numerico de contagem em relacao ao manual: {estat['erro_numerico_percentual']}%")
    with open(pasta/'COMPARACAO_FINAL.json','w',encoding='utf-8') as arq:
        json.dump(resumo,arq,ensure_ascii=False,indent=2)
    with open(pasta/'COMPARACAO_FINAL.csv','w',encoding='utf-8-sig',newline='') as arq:
        campos=['estrategia','brutas_recebidas','dentro_roi','fora_roi','good','bad','outras',
            'percentual_bad','decisao','manual_total','diferenca_contagem','erro_numerico_percentual',
            'manual_bad','diferenca_bad']
        wr=csv.DictWriter(arq,fieldnames=campos,delimiter=';');wr.writeheader()
        for nome,resultado in resumo['variantes'].items():
            wr.writerow({chave: (nome if chave=='estrategia' else resultado.get(chave,'')) for chave in campos})
    print('\nCOMPARACAO CONCLUIDA. Veja COMPARACAO_FINAL.csv, JSON e imagens numeradas.')
    return resumo


def argumentos():
    p=argparse.ArgumentParser(description='Teste comparativo da CNN V10.1 vs ROI V9 sem Arduino')
    p.add_argument('--imagem',required=True,help='Foto ORIGINAL, sem caixas sobrepostas')
    p.add_argument('--saida',default='resultado_comparativo_cnn')
    p.add_argument('--manual-total',type=int,default=None)
    p.add_argument('--manual-bad',type=int,default=None)
    p.add_argument('--roi-cx',type=int,default=None)
    p.add_argument('--roi-cy',type=int,default=None)
    p.add_argument('--roi-raio',type=int,default=None)
    p.add_argument('--env',default=None,help='Caminho de arquivo .env (NAO enviar sua chave)')
    p.add_argument('--sem-inferencia',action='store_true',help='So gera recorte e mascara; nao usa API Roboflow')
    a=p.parse_args()
    if a.manual_total is not None and a.manual_total < 0:
        p.error('--manual-total nao pode ser negativo')
    if a.manual_bad is not None and a.manual_bad < 0:
        p.error('--manual-bad nao pode ser negativo')
    if a.manual_total is not None and a.manual_bad is not None and a.manual_bad > a.manual_total:
        p.error('--manual-bad nao pode superar --manual-total')
    return a


if __name__ == '__main__':
    a=argumentos()
    try:
        executar(a.imagem,a.saida,(a.roi_cx,a.roi_cy,a.roi_raio),
                 a.manual_total,a.manual_bad,a.sem_inferencia,a.env)
    except (ValueError,RuntimeError,FileNotFoundError) as err:
        raise SystemExit(f'ERRO: {err}')
