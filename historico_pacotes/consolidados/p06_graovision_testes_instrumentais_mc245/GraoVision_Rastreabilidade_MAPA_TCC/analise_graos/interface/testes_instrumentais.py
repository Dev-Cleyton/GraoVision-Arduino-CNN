"""Tela GrãoVision: ensaio ambiental sem calor e anotacoes manuais do MC-245.

So inicia o ciclo usual (com CNN) via start_cycle(instrumental=True). Nao existe
comando de subida isolada na V10.2.7; NAO enviar movimento nao homologado.
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk, messagebox

from interface.ensaios_instrumentais import EnsaioInstrumental


class TestesInstrumentaisUI:
    def __init__(self, app, cores, caption, styled_button):
        self.app=app
        self.COL=cores
        self.caption=caption
        self.styled_button=styled_button
        self.ensaio=EnsaioInstrumental()
        self.tabela=None
        self.feedback=None
        self._ids_por_marco={}

    def _rotulo(self, host, text, cor='muted', size=10):
        return self.caption(host,text,color=cor,size=size)

    def _valor(self, host, text='—', cor='accent'):
        return tk.Label(host,text=text,fg=self.COL[cor],bg=self.COL['panel'],
                        font=('Segoe UI',16,'bold'),anchor='w')

    def construir(self, host):
        c=self.COL
        container=tk.Frame(host,bg=c['bg'])
        container.pack(fill='both',expand=True)
        topo=tk.Frame(container,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
        topo.pack(fill='x',pady=(0,12))
        tk.Label(topo,text='TESTES INSTRUMENTAIS  •  SEM AQUECIMENTO',
                 bg=c['panel'],fg=c['accent'],font=('Segoe UI',15,'bold')).pack(anchor='w',padx=18,pady=(14,5))
        self._rotulo(topo,'A subida usa P1 → CNN aprovada → P2. Não há comando isolado de motor nem acionamento da H4.',size=10).pack(anchor='w',padx=18,pady=(0,5))
        self._rotulo(topo,'MC-245: termômetro corporal (32–42 °C). A 24–27 °C, registre “L”/fora da faixa; não use 37,0 °C de inicialização como leitura.',cor='amber',size=9).pack(anchor='w',padx=18,pady=(0,12))

        painel=tk.Frame(container,bg=c['bg']);painel.pack(fill='x',pady=(0,12))
        painel.grid_columnconfigure(0,weight=1)
        painel.grid_columnconfigure(1,weight=1)
        for index,(titulo,chave) in enumerate([('NTC A2, próximo à célula','ntc'),('HX711, última leitura','massa'),('Atuador, posição comandada','pos'),('Situação do ensaio','situacao')]):
            f=tk.Frame(painel,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
            f.grid(row=index//2,column=index%2,sticky='nsew',padx=(0,8) if index%2==0 else (8,0),pady=(0,8))
            self._rotulo(f,titulo,size=9).pack(anchor='w',padx=14,pady=(10,4))
            attr='indicador_'+chave
            rotulo=self._valor(f)
            rotulo.pack(anchor='w',padx=14,pady=(0,12))
            setattr(self,attr,rotulo)

        medias=tk.Frame(container,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
        medias.pack(fill='x',pady=(0,12))
        self.medias_labels={}
        for coluna,chave in enumerate(('P1','P2','PF','P3','P4','P5')):
            medias.grid_columnconfigure(coluna,weight=1,uniform='medias')
            espaco=tk.Frame(medias,bg=c['panel'])
            espaco.grid(row=0,column=coluna,sticky='nsew',padx=10,pady=(8,8))
            self._rotulo(espaco,chave+' · média de 12 leituras',size=8).pack(anchor='w')
            valor=self._valor(espaco,'— g')
            valor.configure(font=('Segoe UI',12,'bold'))
            valor.pack(anchor='w',pady=(2,0))
            self.medias_labels[chave]=valor

        botoes=tk.Frame(container,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
        botoes.pack(fill='x',pady=(0,12))
        self.styled_button(botoes,'▶  INICIAR ENSAIO SEM H4 (CICLO EXISTENTE)',
            lambda:self.app.start_cycle(instrumental=True),bg='accent',fg='#07131F',font_size=10).pack(side='left',padx=12,pady=12)
        self.styled_button(botoes,'■  SOLICITAR STOP',self.app.request_stop,
            bg='panel2',fg='red',font_size=10).pack(side='left',padx=4,pady=12)
        self.styled_button(botoes,'EXPORTAR CSV + JSON',self.exportar,
            bg='panel2',font_size=10).pack(side='right',padx=12,pady=12)

        form=tk.Frame(container,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
        form.pack(fill='x',pady=(0,12))
        tk.Label(form,text='ANOTAÇÃO SINCRONIZADA  •  OPERADOR',
                 fg=c['text'],bg=c['panel'],font=('Segoe UI',11,'bold')).grid(
                 row=0,column=0,columnspan=5,sticky='w',padx=16,pady=(13,7))
        self._rotulo(form,'Ponto',size=9).grid(row=1,column=0,sticky='w',padx=(16,6))
        self._rotulo(form,'MC-245 (L, H, 32,0–42,0 °C)',size=9).grid(row=1,column=1,sticky='w',padx=6)
        self._rotulo(form,'Observação',size=9).grid(row=1,column=2,sticky='w',padx=6)
        self.selecao=tk.StringVar(value='')
        self.pontos_combo=ttk.Combobox(form,textvariable=self.selecao,values=(),width=13,state='readonly')
        self.pontos_combo.grid(row=2,column=0,sticky='ew',padx=(16,6),pady=(4,14))
        self.entrada_mc245=tk.Entry(form,fg=c['text'],bg=c['panel2'],insertbackground=c['accent'],
                                  font=('Segoe UI',11),width=22)
        self.entrada_mc245.insert(0,'L')
        self.entrada_mc245.grid(row=2,column=1,sticky='ew',padx=6,pady=(4,14),ipady=6)
        self.observacao=tk.Entry(form,fg=c['text'],bg=c['panel2'],insertbackground=c['accent'],
                                  font=('Segoe UI',11),width=25)
        self.observacao.grid(row=2,column=2,sticky='ew',padx=6,pady=(4,14),ipady=6)
        self.styled_button(form,'REGISTRAR',self.registrar,bg='accent',fg='#07131F',font_size=10,
                           padx=12).grid(row=2,column=3,sticky='ew',padx=10,pady=(4,14))
        form.grid_columnconfigure(2,weight=1)
        self.feedback=self._rotulo(form,'Aguarde a primeira MEDICAO do Arduino (tempo 0 s).',size=9)
        self.feedback.grid(row=3,column=0,columnspan=4,sticky='w',padx=16,pady=(0,12))

        tabela_f=tk.Frame(container,bg=c['panel'],highlightthickness=1,highlightbackground=c['line'])
        tabela_f.pack(fill='both',expand=True)
        self._rotulo(tabela_f,'PONTOS MEDIDOS PELO ARDUINO E REGISTRADOS PELO OPERADOR',size=10).pack(
            anchor='w',padx=12,pady=(12,5))
        estilo=ttk.Style()
        estilo.configure('Instrumental.Treeview',background=c['panel2'],foreground=c['text'],
                         fieldbackground=c['panel2'],rowheight=30,font=('Segoe UI',10))
        estilo.configure('Instrumental.Treeview.Heading',font=('Segoe UI',9,'bold'))
        estilo.map('Instrumental.Treeview',background=[('selected',c['line'])])
        cols=('t','ntc','massa','omron','atraso','situacao')
        self.tabela=ttk.Treeview(tabela_f,columns=cols,show='headings',height=7,
                                style='Instrumental.Treeview',selectmode='browse')
        nomes={'t':'Tempo (s)','ntc':'NTC A2 (°C)','massa':'HX711 (g)',
               'omron':'Omron MC-245','atraso':'Atraso anotação','situacao':'Validade'}
        for coluna in cols:
            self.tabela.heading(coluna,text=nomes[coluna])
            self.tabela.column(coluna,anchor='center',width=120 if coluna!='situacao' else 210,
                               stretch=coluna=='situacao')
        # Barra de rolagem permite acessar os pontos 90 e 120 s em telas menores.
        self.tabela.pack(fill='both',expand=True,padx=12,pady=(0,12))
        barra=ttk.Scrollbar(tabela_f,orient='vertical',command=self.tabela.yview)
        barra.place(relx=1.0,rely=0.10,relheight=0.85,anchor='ne')
        self.tabela.configure(yscrollcommand=barra.set)
        self.tabela.bind('<<TreeviewSelect>>',self._selecionar_linha)
        self._atualizar()

    def _selecionar_linha(self,event=None):
        if self.tabela is None:return
        selection=self.tabela.selection()
        if selection:
            self.selecao.set(selection[0])

    def iniciar(self):
        self.ensaio.iniciar()
        self._atualizar()
        self._feedback('Ciclo iniciado sem calor. Subida ocorre somente após P1 e CNN, pelo protocolo existente.')

    def receber(self,linha):
        resposta=self.ensaio.receber(linha)
        if not resposta:return
        tipo,dados=resposta
        if tipo=='ponto':
            self._atualizar()
            if self.pontos_combo is not None and not self.selecao.get():
                self.selecao.set(str(dados['marco_s']))
            self._feedback(f"PONTO {dados['marco_s']} s RECEBIDO: NTC {dados['ntc_c']:.2f} °C; "
                           f"massa {dados['massa_g']:.4f} g. Anote a indicação externa agora.")
            try:self.app.root.bell()
            except tk.TclError:pass
        elif tipo=='peso':
            self._atualizar()
        elif tipo=='alerta':
            self._feedback('ALERTA: firmware informou aquecedor ligado! Desenergize fisicamente a H4.',erro=True)
            # A GUI nunca enviou comando de aquecimento; solicita STOP ao firmware.
            if self.app.active_handler is not None:
                try:self.app.active_handler.parar()
                except Exception as exc:self.app._log('[INSTRUMENTAL] STOP não enviado: '+str(exc))

    def finalizar(self,com_erro=False):
        if not self.ensaio.ativo:return
        completo = all(chave in self.ensaio.pesagens for chave in ('P1','P2','PF','P3','P4','P5'))
        estado = ('INTERROMPIDO_OU_ERRO' if com_erro else
                  'CONCLUIDO_SEM_CALOR' if completo else 'INCOMPLETO_CNN_OU_ENSAIO')
        self.ensaio.finalizar(estado)
        self._atualizar()
        try:
            paths=self.ensaio.salvar()
            self._feedback('Ensaio registrado: '+str(paths[1])+' (pode complementar leituras manuais)')
        except OSError as exc:
            self._feedback('Falha ao salvar: '+str(exc),erro=True)

    def registrar(self):
        try:
            marco=int(self.selecao.get())
            p=self.ensaio.registrar_mc245(marco,self.entrada_mc245.get(),self.observacao.get())
            self.ensaio.salvar()
        except (ValueError,OSError) as exc:
            self._feedback(str(exc),erro=True)
            return
        self.entrada_mc245.delete(0,'end')
        self.entrada_mc245.insert(0,'L')
        self.observacao.delete(0,'end')
        self._atualizar()
        pendentes=self.ensaio.pendentes()
        if pendentes:self.selecao.set(str(pendentes[0]['marco_s']))
        leitura=p['mc245']['indicacao']
        self._feedback(f"Anotado no ponto {marco} s: MC-245 = {leitura}; "
                       f"atraso real de {p['atraso_registro_s']:.1f} s.")

    def exportar(self):
        try:paths=self.ensaio.salvar()
        except (ValueError,OSError) as exc:
            self._feedback(str(exc),erro=True)
            return
        self._feedback('Arquivos atualizados: '+str(paths[0].parent))
        messagebox.showinfo('Exportação instrumental',
                            'Arquivos JSON e CSV salvos em:\n'+str(paths[0].parent),parent=self.app.root)

    def _feedback(self,texto,erro=False):
        if self.feedback and self.feedback.winfo_exists():
            self.feedback.configure(text=texto,fg=self.COL['red'] if erro else self.COL['muted'])
        self.app._log('[INSTRUMENTAL] '+texto)

    def _atualizar(self):
        e=self.ensaio
        if not (self.tabela and self.tabela.winfo_exists()):return
        if e.pontos:
            ult=e.pontos[-1]
            self.indicador_ntc.configure(text=f"{ult['ntc_c']:.2f} °C")
            self.indicador_massa.configure(text=f"{ult['massa_g']:.4f} g")
        else:
            self.indicador_ntc.configure(text='— °C')
            self.indicador_massa.configure(text='— g')
        self.indicador_pos.configure(text=f'{e.posicao_passos} passos' if e.posicao_passos is not None else '— passos')
        for chave,valor in self.medias_labels.items():
            peso=e.pesagens.get(chave)
            valor.configure(text=(f'{peso:.4f} g' if peso is not None else '— g'))
        label = 'EM EXECUÇÃO' if e.ativo else e.status.replace('_',' ')
        self.indicador_situacao.configure(text=label)
        self.pontos_combo['values']=[str(p['marco_s']) for p in e.pontos]
        presentes=set()
        for p in e.pontos:
            marco=str(p['marco_s']); presentes.add(marco)
            mc=p.get('mc245') or {}
            descricao=mc.get('situacao','PENDENTE')
            tempo = ('%.1f s'%p['atraso_registro_s'] if p['atraso_registro_s'] is not None else '—')
            vals=(p['tempo_arduino_s'], f"{p['ntc_c']:.2f}", f"{p['massa_g']:.4f}",
                  mc.get('indicacao','—'),tempo,descricao)
            if self.tabela.exists(marco):self.tabela.item(marco,values=vals)
            else:self.tabela.insert('','end',iid=marco,values=vals)
        for iid in self.tabela.get_children():
            if iid not in presentes:self.tabela.delete(iid)
