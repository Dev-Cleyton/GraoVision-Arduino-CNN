"""Gráficos detalhados de telemetria REAL para o GrãoVision.

Apenas interpreta mensagens RX. Nenhum comando de atuador/aquecimento é enviado.
A temperatura é representada nos instantes realmente transmitidos (sem interpolar
pontos como se fossem medições). As 12 leituras por vetor são preservadas.
"""
from __future__ import annotations

import csv
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import messagebox

RESULTADOS_DIR = Path.home() / '.graovision_tcc' / 'graficos'
ETAPAS = ('P1', 'P2', 'PF', 'P3', 'P4', 'P5')


class DadosGraficos:
    def __init__(self):
        self.limpar()

    def limpar(self):
        self.vetores = []  # (millis, etapa, indice, pos, massa)
        self.temperaturas = []  # (segundo relativo monitor, valor, origem)
        self.massas_monitor = []  # (segundo relativo monitor, peso)
        self.medias = {}
        self.p2_timestamp_ms = None

    def receber(self, linha):
        p = str(linha).strip().split(':')
        try:
            if len(p) == 6 and p[0] == 'VETOR' and p[1] in ETAPAS:
                _, etapa, indice, millis, pos, peso = p
                indice, millis, pos, peso = int(indice), int(millis), int(pos), float(peso)
                if not (1 <= indice <= 12):return False
                self.vetores.append((millis,etapa,indice,pos,peso))
                if etapa == 'P2' and indice == 12:self.p2_timestamp_ms = millis
                return True
            if len(p) == 4 and p[0] == 'MEDICAO':
                segundo, peso, temp = int(p[1]), float(p[2]), float(p[3])
                self.temperaturas.append((segundo,temp,'NTC A2'))
                self.massas_monitor.append((segundo,peso))
                return True
            if len(p) == 3 and p[0] == 'TEMP_MONITOR':
                segundo,temp = int(p[1]),float(p[2])
                self.temperaturas.append((segundo,temp,'NTC A2 (amostra extra)'))
                return True
            if len(p) == 3 and p[0] == 'RESFRIAMENTO':
                segundo,temp = int(p[1]),float(p[2])
                # Separado: origem de tempo reiniciada no resfriamento.
                self.temperaturas.append((segundo,temp,'RESFRIAMENTO'))
                return True
            if len(p) == 2 and p[0] in ETAPAS:
                self.medias[p[0]] = float(p[1]);return True
        except (ValueError,OverflowError):
            return False
        return False

    def amostras_temperatura(self):
        # A série de resfriamento é independente para não construir um eixo
        # de tempo fictício no mesmo gráfico do monitoramento.
        atual = [(s,v,origem) for s,v,origem in self.temperaturas if origem != 'RESFRIAMENTO']
        return sorted(atual,key=lambda x:x[0])

    def amostras_massa(self):
        # Segundos desde a primeira leitura P1; sincroniza MEDICAO pelo último P2.
        origem = min((v[0] for v in self.vetores),default=None)
        if origem is None:return []
        pontos=[((millis-origem)/1000.,peso,f'{etapa} · leitura {idx}/12','VETOR')
                for millis,etapa,idx,pos,peso in self.vetores]
        if self.p2_timestamp_ms is not None:
            for s,peso in self.massas_monitor:
                t=(self.p2_timestamp_ms-origem)/1000.+s
                pontos.append((t,peso,f'Monitoramento {s} s','MEDICAO'))
        return sorted(pontos,key=lambda item:item[0])

    def linhas_csv(self):
        for millis,etapa,idx,pos,peso in self.vetores:
            yield ('vetor',millis,etapa,idx,pos,f'{peso:.4f}','','')
        for seg,temp,origem in self.temperaturas:
            yield ('temperatura',seg,origem,'','','',f'{temp:.2f}','')
        for seg,peso in self.massas_monitor:
            yield ('monitoramento',seg,'MEDICAO','','',f'{peso:.4f}','','')
        for etapa,peso in self.medias.items():
            yield ('media','',etapa,'','',f'{peso:.4f}','','')

    def salvar_csv(self,pasta=RESULTADOS_DIR):
        pasta=Path(pasta);pasta.mkdir(parents=True,exist_ok=True)
        caminho=pasta / ('graovision_medicoes_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.csv')
        with caminho.open('w',encoding='utf-8-sig',newline='') as arquivo:
            writer=csv.writer(arquivo,delimiter=';')
            writer.writerow(('tipo','tempo_millis_ou_s','etapa_ou_origem','indice','posicao_passos','massa_g','temperatura_c','observacao'))
            writer.writerows(self.linhas_csv())
        return caminho


def _desenhar(canvas, pontos, cor, unidade, tipo, mouse=None):
    canvas.delete('all')
    w=max(360,canvas.winfo_width()); h=max(175,canvas.winfo_height())
    l,r,t,b=65,w-22,19,h-40
    cores=cor
    for i in range(5):
        y=t+(b-t)*i/4
        canvas.create_line(l,y,r,y,fill=cores['line'])
    if not pontos:
        canvas.create_text(w/2,h/2,text='Aguardando leituras REAIS do Arduino',font=('Segoe UI',11),fill=cores['muted'])
        return
    xs=[float(x[0]) for x in pontos]; ys=[float(x[1]) for x in pontos]
    minx=min(0,min(xs));maxx=max(xs);maxx=max(maxx,120 if tipo=='temp' else 1)
    miny,maxy=min(ys),max(ys)
    margin=max((maxy-miny)*0.22,0.6 if tipo=='temp' else .025)
    miny-=margin;maxy+=margin
    def xy(x,y):return l+(x-minx)/(maxx-minx)*(r-l),b-(y-miny)/(maxy-miny)*(b-t)
    for i in range(5):
        y=t+(b-t)*i/4
        canvas.create_text(l-9,y,text=f'{maxy-(maxy-miny)*i/4:.2f}',anchor='e',font=('Segoe UI',9),fill=cores['muted'])
    canvas.create_text(l,b+20,text=f'{minx:.0f} s',anchor='w',font=('Segoe UI',9),fill=cores['muted'])
    canvas.create_text(r,b+20,text=f'{maxx:.0f} s',anchor='e',font=('Segoe UI',9),fill=cores['muted'])
    previous=None
    render=[]
    for x,y,label,origem in pontos:
        px,py=xy(x,y)
        # Não ligar o fim de um vetor ao início de outro, pois existem intervalos sem pesagens.
        if previous is not None and (tipo=='temp' or (origem=='VETOR' and previous[3]==origem and previous[2].split(' ·')[0]==label.split(' ·')[0])):
            canvas.create_line(previous[0],previous[1],px,py,fill=cores['accent'] if tipo=='temp' else cores['blue'],width=2)
        c=cores['accent'] if tipo=='temp' else (cores['blue'] if origem=='VETOR' else cores['amber'])
        canvas.create_oval(px-2.7,py-2.7,px+2.7,py+2.7,fill=c,outline='')
        previous=(px,py,label,origem);render.append((px,py,x,y,label,origem))
    if tipo=='massa':
        # Identificação da posição, com o centro temporal das 12 amostras.
        for etapa in ETAPAS:
            grupo=[(xx,yy) for xx,yy,et,orig in pontos if orig=='VETOR' and et.split(' ·')[0]==etapa]
            if grupo:
                momento=sum(x for x,_ in grupo)/len(grupo)
                xp,_=xy(momento,grupo[-1][1])
                canvas.create_text(xp,t-8,text=etapa,fill=cores['muted'],font=('Segoe UI',9,'bold'))
    if mouse is not None and render:
        nearest=min(render,key=lambda p:(p[0]-mouse[0])**2+(p[1]-mouse[1])**2)
        px,py,x,y,label,_=nearest
        if abs(px-mouse[0])<=26:
            canvas.create_line(px,t,px,b,fill=cores['muted'],dash=(3,3))
            tip=f'{x:.1f} s  •  {y:.4f} {unidade}' if tipo!='temp' else f'{x:.0f} s  •  {y:.2f} °C'
            bx=min(w-285,max(5,px-85));by=max(5,py-58)
            canvas.create_rectangle(bx,by,bx+275,by+45,fill=cores['panel2'],outline=cores['accent'])
            canvas.create_text(bx+8,by+12,text=tip,anchor='w',fill=cores['text'],font=('Segoe UI',10,'bold'))
            canvas.create_text(bx+8,by+31,text=label,anchor='w',fill=cores['muted'],font=('Segoe UI',9))


class GraficosDetalhadosUI:
    def __init__(self, app, cores, caption, styled_button):
        self.app=app;self.cores=cores;self.caption=caption;self.button=styled_button
        self.dados=DadosGraficos();self.canvas_temp=None;self.canvas_massa=None

    def construir(self,host):
        c=self.cores
        cab=tk.Frame(host,bg=c['panel'],highlightbackground=c['line'],highlightthickness=1)
        cab.pack(fill='x',pady=(0,9))
        tk.Label(cab,text='GRÁFICOS DETALHADOS · DADOS REAIS',font=('Segoe UI',14,'bold'),bg=c['panel'],fg=c['accent']).pack(side='left',padx=15,pady=13)
        self.button(cab,'RELATÓRIOS / PDF',lambda:self.app._show('relatorios'),bg='panel2',font_size=9).pack(side='right',padx=5,pady=8)
        self.button(cab,'EXPORTAR CSV',self.exportar,bg='panel2',font_size=9).pack(side='right',padx=5,pady=8)
        self.button(cab,'EXPORTAR PNG',self.exportar_png,bg='panel2',font_size=9).pack(side='right',padx=4,pady=8)
        self.subtitle=self.caption(host,'NTC A2: somente pontos recebidos  •  Massa: 12 leituras por vetor + monitoramento  •  H4 bloqueada',size=9,bg='bg')
        self.subtitle.pack(fill='x',pady=(0,9))
        for title,tipo in [('Temperatura do NTC A2 × tempo do monitoramento','temp'),('Massa HX711 × tempo total do ciclo','massa')]:
            frame=tk.Frame(host,bg=c['panel'],highlightbackground=c['line'],highlightthickness=1)
            frame.pack(fill='both',expand=True,pady=(0,9))
            self.caption(frame,title,size=11,weight='bold').pack(anchor='w',padx=14,pady=(10,3))
            canvas=tk.Canvas(frame,bg=c['panel'],height=210,highlightthickness=0,cursor='crosshair')
            canvas.pack(fill='both',expand=True,padx=7,pady=(0,9))
            if tipo=='temp':self.canvas_temp=canvas
            else:self.canvas_massa=canvas
            canvas.bind('<Configure>',lambda event:self.desenhar())
            canvas.bind('<Motion>',lambda event,tip=tipo:self.desenhar(tip,(event.x,event.y)))
            canvas.bind('<Leave>',lambda event:self.desenhar())
        self.desenhar()

    def iniciar(self):self.dados.limpar();self.desenhar()

    def receber(self,linha):
        if self.dados.receber(linha):self.desenhar()

    def desenhar(self,hover=None,mouse=None):
        if self.canvas_temp is not None and self.canvas_temp.winfo_exists():
            pontos=[(seg,val,'NTC A2','NTC') for seg,val,origem in self.dados.amostras_temperatura()]
            _desenhar(self.canvas_temp,pontos,self.cores,'°C','temp',mouse if hover=='temp' else None)
        if self.canvas_massa is not None and self.canvas_massa.winfo_exists():
            pontos=self.dados.amostras_massa()
            _desenhar(self.canvas_massa,pontos,self.cores,'g','massa',mouse if hover=='massa' else None)
        if hasattr(self,'subtitle') and self.subtitle.winfo_exists():
            ntemp=len(self.dados.amostras_temperatura())
            nv=len(self.dados.vetores)
            self.subtitle.configure(text=f'{ntemp} leituras reais de NTC  •  {nv} leituras dos vetores HX711  •  '
                                         f'{len(self.dados.massas_monitor)} pontos de massa no monitoramento  •  H4 desabilitada')

    def exportar_png(self):
        if not(self.dados.vetores or self.dados.temperaturas):
            messagebox.showinfo('Sem medições','Execute um ciclo antes de exportar.',parent=self.app.root);return
        try:path=salvar_png(self.dados)
        except (RuntimeError,OSError) as exc:
            messagebox.showerror('Erro ao exportar PNG',str(exc),parent=self.app.root);return
        self.app._log('[GRAFICOS] PNG exportado: '+str(path))
        messagebox.showinfo('PNG salvo',str(path),parent=self.app.root)

    def exportar(self):
        if not(self.dados.vetores or self.dados.temperaturas):
            messagebox.showinfo('Sem medições','Execute um ciclo para gerar dados exportáveis.',parent=self.app.root);return
        try:path=self.dados.salvar_csv()
        except OSError as exc:
            messagebox.showerror('Erro ao exportar',str(exc),parent=self.app.root);return
        self.app._log('[GRAFICOS] CSV real exportado: '+str(path))
        messagebox.showinfo('CSV salvo',str(path),parent=self.app.root)


def salvar_png(dados, pasta=RESULTADOS_DIR):
    """Exporta figura estática com dados reais (sem criar pontos intermediários)."""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as exc:
        raise RuntimeError('Pillow é necessário para exportar PNG.') from exc
    pasta=Path(pasta);pasta.mkdir(parents=True,exist_ok=True)
    caminho=pasta / ('graovision_graficos_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.png')
    im=Image.new('RGB',(1330,820),'#0C1826');d=ImageDraw.Draw(im)
    try: f=ImageFont.truetype('DejaVuSans.ttf',19)
    except OSError:f=ImageFont.load_default()
    def desenhar_area(top, titulo, pontos, cor, unidade, linha=True):
        d.text((32,top),titulo,fill='#F4F8FC',font=f)
        l,r,t,b=100,1280,top+50,top+310
        if not pontos:
            d.text((l+50,t+125),'Sem medições',fill='#94A8C1',font=f);return
        xs=[float(p[0]) for p in pontos];ys=[float(p[1]) for p in pontos]
        xx0=0.;xx1=max(max(xs),1.);mn=min(ys);mx=max(ys)
        margem=max((mx-mn)*.2,.6 if unidade=='°C' else .03)
        mn-=margem;mx+=margem
        def xy(x,y):return (int(l+(x-xx0)/(xx1-xx0)*(r-l)),int(b-(y-mn)/(mx-mn)*(b-t)))
        for k in range(5):
            y=int(t+(b-t)*k/4);d.line([(l,y),(r,y)],fill='#263951')
            d.text((15,y-10),f'{mx-(mx-mn)*k/4:.2f} {unidade}',fill='#94A8C1',font=f)
        d.text((l,b+10),'0 s',fill='#94A8C1',font=f)
        d.text((r-110,b+10),f'{xx1:.0f} s',fill='#94A8C1',font=f)
        anterior=None
        for x,y,label,origem in pontos:
            q=xy(x,y)
            if anterior and (linha or (origem=='VETOR' and anterior[2]=='VETOR' and label.split(' ·')[0]==anterior[3].split(' ·')[0])):
                d.line([(anterior[0],anterior[1]),q],fill=cor,width=3)
            d.ellipse((q[0]-3,q[1]-3,q[0]+3,q[1]+3),fill=cor)
            anterior=(q[0],q[1],origem,label)
    temp=[(seg,v,'NTC A2',orig) for seg,v,orig in dados.amostras_temperatura()]
    mass=dados.amostras_massa()
    desenhar_area(22,'Temperatura NTC A2 × tempo do monitoramento',temp,'#38D6C0','°C',True)
    desenhar_area(420,'Massa HX711 × tempo total do ciclo',mass,'#70AAFF','g',False)
    d.text((35,787),'GrãoVision • Apenas telemetria recebida • Não é calibração de temperatura/umidade',fill='#94A8C1',font=f)
    im.save(caminho)
    return caminho
