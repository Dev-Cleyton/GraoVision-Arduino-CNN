"""Telas Tk de cadastro local e emissao de relatorios; sem acesso ao Arduino."""
from __future__ import annotations

import os
import webbrowser
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

from .banco import CadastroStore, PESSOA_FIELDS, AMOSTRA_FIELDS, novo_codigo, criar_backup

TITULOS_PESSOA={
    'razao_social':'Razão social / nome *','nome_fantasia':'Nome fantasia',
    'cpf_cnpj':'CNPJ / CPF','inscricao_estadual':'Inscrição estadual',
    'telefone':'Telefone','email':'E-mail','responsavel':'Responsável / contato',
    'endereco':'Logradouro','numero':'Número','bairro':'Bairro',
    'cidade':'Cidade','uf':'UF','cep':'CEP','observacoes':'Observações',
}
TITULOS_AMOSTRA={
    'codigo_amostra':'Código da amostra *','fornecedor_id':'Fornecedor *',
    'cliente_id':'Cliente / interessado *','produto':'Grão / produto *',
    'variedade':'Variedade / cultivar','safra':'Safra',
    'lote':'Número do lote *','propriedade_origem':'Propriedade / origem',
    'municipio_origem':'Município de origem','uf_origem':'UF origem',
    'nota_fiscal':'Nota fiscal','placa_veiculo':'Placa / transporte',
    'peso_lote_kg':'Peso do lote (kg)','peso_amostra_g':'Massa amostra declarada (g)',
    'data_recebimento':'Chegada / recebimento (DD/MM/AAAA) *',
    'hora_recebimento':'Hora chegada (HH:MM)',
    'data_coleta':'Amostragem (DD/MM/AAAA) *',
    'hora_coleta':'Hora amostragem (HH:MM)',
    'local_coleta':'Local da coleta',
    'responsavel_coleta':'Responsável pela coleta *',
    'metodo_amostragem':'Método / procedimento de coleta',
    'numero_lacre':'Número do lacre','tipo_embalagem':'Embalagem',
    'finalidade':'Finalidade da análise','norma_referencia':'Norma técnica de referência (se aplicável)',
    'observacoes':'Observações / condições de recebimento',
}


class CadastroUI:
    def __init__(self, app, palette, caption, styled_button):
        self.app=app
        self.col=palette
        self.caption=caption
        self.styled_button=styled_button
        self.store=CadastroStore()
        self.fields={}
        self.ids={'FORNECEDOR':None,'CLIENTE':None,'AMOSTRA':None}
        self.trees={}
        self.person_lookup={}
        self.selected_amostra_id=None
        self.last_analise_id=None

    def _input(self,parent,key,kind):
        if key in ('fornecedor_id','cliente_id'):
            field=ttk.Combobox(parent,state='readonly',font=('Segoe UI',10))
        else:
            field=tk.Entry(parent,bd=0,bg=self.col['panel2'],fg=self.col['text'],
                           insertbackground=self.col['text'],font=('Segoe UI',10),
                           highlightthickness=1,highlightbackground=self.col['line'])
        self.fields[kind][key]=field
        return field

    def _make_scrolled(self,parent):
        wrap=tk.Frame(parent,bg=self.col['panel'])
        can=tk.Canvas(wrap,bg=self.col['panel'],highlightthickness=0)
        sc=tk.Scrollbar(wrap,orient='vertical',command=can.yview)
        can.configure(yscrollcommand=sc.set)
        sc.pack(side='right',fill='y')
        can.pack(side='left',fill='both',expand=True)
        inn=tk.Frame(can,bg=self.col['panel'])
        id=can.create_window((0,0),window=inn,anchor='nw')
        inn.bind('<Configure>',lambda e:can.configure(scrollregion=can.bbox('all')))
        can.bind('<Configure>',lambda e:can.itemconfigure(id,width=e.width))
        return wrap,inn

    def _tree(self,parent,columns,widths=None):
        style=ttk.Style(parent)
        style.configure('Grain.Treeview',background=self.col['panel2'],fieldbackground=self.col['panel2'],
                        foreground=self.col['text'],rowheight=29,borderwidth=0,font=('Segoe UI',9))
        style.configure('Grain.Treeview.Heading',background=self.col['panel'],foreground=self.col['text'],
                        font=('Segoe UI',9,'bold'))
        style.map('Grain.Treeview',background=[('selected',self.col['blue'])],
                  foreground=[('selected',self.col['bg'])])
        tree=ttk.Treeview(parent,columns=columns,show='headings',style='Grain.Treeview',selectmode='browse')
        for i,key in enumerate(columns):
            tree.heading(key,text=key)
            tree.column(key,width=(widths or [130]*len(columns))[i],anchor='w',stretch=(i==len(columns)-1))
        vs=ttk.Scrollbar(parent,orient='vertical',command=tree.yview)
        tree.configure(yscrollcommand=vs.set)
        vs.pack(side='right',fill='y')
        tree.pack(fill='both',expand=True)
        return tree

    def build_cadastros(self,parent):
        tabs=ttk.Notebook(parent)
        style=ttk.Style(parent)
        style.configure('TNotebook',background=self.col['bg'],borderwidth=0)
        style.configure('TNotebook.Tab',font=('Segoe UI',10,'bold'),padding=(18,10))
        tabs.pack(fill='both',expand=True)
        for kind,title in (('FORNECEDOR','Fornecedores'),('CLIENTE','Clientes'),('AMOSTRA','Amostras e lotes')):
            panel=tk.Frame(tabs,bg=self.col['bg'])
            tabs.add(panel,text=title)
            self._build_kind(panel,kind)
        self.registry_info=tk.Label(parent,text='Cadastre fornecedor e cliente antes de criar a amostra.',
                                    bg=self.col['bg'],fg=self.col['accent'],font=('Segoe UI',9),anchor='w')
        self.registry_info.pack(fill='x',pady=(8,0))
        self.refresh_all()

    def _build_kind(self,parent,kind):
        parent.grid_columnconfigure(0,weight=3)
        parent.grid_columnconfigure(1,weight=4)
        parent.grid_rowconfigure(0,weight=1)
        listing=tk.Frame(parent,bg=self.col['panel'],highlightthickness=1,
                         highlightbackground=self.col['line'])
        listing.grid(row=0,column=0,sticky='nsew',padx=(0,7),pady=(4,0))
        title={'FORNECEDOR':'Fornecedores cadastrados','CLIENTE':'Clientes cadastrados',
               'AMOSTRA':'Amostras registradas'}[kind]
        self.caption(listing,title,size=12,weight='bold').pack(anchor='w',padx=12,pady=14)
        self.caption(listing,'Selecione um registro para editar. O banco de dados fica no seu PC.',
                     size=8).pack(anchor='w',padx=12,pady=(0,10))
        treetop=tk.Frame(listing,bg=self.col['panel'])
        treetop.pack(fill='both',expand=True,padx=9,pady=(0,9))
        columns=('Código','Nome / Produto','Documento / Lote')
        tree=self._tree(treetop,columns,widths=[85,160,130])
        self.trees[kind]=tree
        tree.bind('<<TreeviewSelect>>',lambda e,k=kind:self._select(k))
        form=tk.Frame(parent,bg=self.col['panel'],highlightthickness=1,
                      highlightbackground=self.col['line'])
        form.grid(row=0,column=1,sticky='nsew',padx=(7,0),pady=(4,0))
        line=tk.Frame(form,bg=self.col['panel'])
        line.pack(fill='x',padx=16,pady=(13,3))
        self.caption(line,'Cadastro '+kind.lower(),size=12,weight='bold').pack(side='left')
        self.styled_button(line,'+ NOVO',lambda k=kind:self.new(k),bg='panel2',padx=10,pady=6).pack(side='right')
        scroll,inn=self._make_scrolled(form)
        scroll.pack(fill='both',expand=True,padx=12,pady=(8,4))
        self.fields[kind]={}
        keys=PESSOA_FIELDS if kind!='AMOSTRA' else AMOSTRA_FIELDS
        for index,key in enumerate(keys):
            col=index%2;row=index//2
            cell=tk.Frame(inn,bg=self.col['panel'])
            cell.grid(row=row,column=col,sticky='ew',padx=7,pady=(0,12))
            inn.grid_columnconfigure(col,weight=1)
            self.caption(cell,(TITULOS_PESSOA if kind!='AMOSTRA' else TITULOS_AMOSTRA)[key],
                         size=9).pack(fill='x',pady=(0,4))
            inp=self._input(cell,key,kind)
            inp.pack(fill='x',ipady=7)
        actions=tk.Frame(form,bg=self.col['panel'])
        actions.pack(fill='x',padx=14,pady=(8,13))
        self.styled_button(actions,'SALVAR CADASTRO',lambda k=kind:self.save(k),
                           bg='accent',fg=self.col['bg'],padx=13,pady=9).pack(side='left')
        if kind=='AMOSTRA':
            self.styled_button(actions,'USAR NA ANÁLISE',self.activate_amostra,
                               bg='panel2',padx=12,pady=9).pack(side='left',padx=(9,0))
        self.new(kind)

    def _get(self,kind):
        data={}
        for key,inp in self.fields[kind].items():
            if isinstance(inp,ttk.Combobox):
                selected=inp.get()
                match=self.person_lookup.get(kind,{}).get(key,{})
                data[key]=match.get(selected,'')
            else:data[key]=inp.get()
        return data

    def _set(self,kind,data):
        for key,inp in self.fields[kind].items():
            if isinstance(inp,ttk.Combobox):
                rev={v:k for k,v in self.person_lookup.get(kind,{}).get(key,{}).items()}
                inp.set(rev.get(data.get(key),'') or rev.get(int(data.get(key) or 0),'') or '')
            else:
                inp.delete(0,'end')
                inp.insert(0,str(data.get(key) or ''))

    def new(self,kind):
        self.ids[kind]=None
        now=datetime.now()
        if kind=='AMOSTRA':
            self._refresh_combos()
            self._set(kind,{
                'codigo_amostra':novo_codigo(),
                'data_recebimento':now.strftime('%d/%m/%Y'),
                'hora_recebimento':now.strftime('%H:%M'),
                'data_coleta':now.strftime('%d/%m/%Y'),
                'hora_coleta':now.strftime('%H:%M'),
                'finalidade':'Análise experimental de grãos (TCC)',
            })
        else:self._set(kind,{})

    def _refresh_combos(self):
        if 'AMOSTRA' not in self.fields:return
        self.person_lookup['AMOSTRA']={}
        for key,typ in (('fornecedor_id','FORNECEDOR'),('cliente_id','CLIENTE')):
            field=self.fields['AMOSTRA'].get(key)
            if field is None:continue
            selected=field.get()
            mapping={f"{person['id']:04d} | {person['razao_social']}":person['id']
                     for person in self.store.pessoas(typ)}
            self.person_lookup['AMOSTRA'][key]=mapping
            field['values']=list(mapping)
            if selected in mapping:field.set(selected)
            else:field.set('')

    def refresh_all(self):
        self._refresh_combos()
        for kind,tree in self.trees.items():
            for row in tree.get_children():tree.delete(row)
            if kind=='AMOSTRA':
                items=self.store.amostras()
                for a in items:
                    tree.insert('','end',iid=str(a['id']),values=(
                        a['codigo_amostra'],a['produto'],a['lote']))
            else:
                items=self.store.pessoas(kind)
                for p in items:
                    tree.insert('','end',iid=str(p['id']),values=(
                        p['id'],p['razao_social'],p['cpf_cnpj']))
        if hasattr(self,'reports_tree'):self.refresh_reports()

    def _select(self,kind):
        selected=self.trees[kind].selection()
        if not selected:return
        id=int(selected[0])
        rec=self.store.amostra(id) if kind=='AMOSTRA' else self.store.pessoa(id)
        if rec:
            self.ids[kind]=id
            if kind=='AMOSTRA':self._refresh_combos()
            self._set(kind,rec)

    def save(self,kind):
        if self.app.working:
            messagebox.showwarning('Cadastro bloqueado','Aguarde a finalização do ciclo.',parent=self.app.root)
            return
        try:
            data=self._get(kind)
            if kind=='AMOSTRA':id=self.store.salvar_amostra(data,self.ids[kind])
            else:id=self.store.salvar_pessoa(kind,data,self.ids[kind])
            self.ids[kind]=id
            self.refresh_all()
            tree=self.trees[kind]
            tree.selection_set(str(id))
            tree.see(str(id))
            if kind=='AMOSTRA':self.selected_amostra_id=id
            if hasattr(self,'registry_info'):
                self.registry_info.configure(text=f'{kind.title()} salvo localmente (registro {id}).')
        except ValueError as exc:
            messagebox.showerror('Verifique os campos',str(exc),parent=self.app.root)
        except Exception as exc:
            messagebox.showerror('Erro ao salvar',str(exc),parent=self.app.root)

    def activate_amostra(self):
        id=self.ids.get('AMOSTRA')
        if id is None:
            messagebox.showwarning('Amostra não salva','Salve o cadastro da amostra antes de vinculá-la.',parent=self.app.root)
            return
        self.selected_amostra_id=id
        item=self.store.amostra(id)
        text=f"Amostra ativa para o próximo ciclo: {item['codigo_amostra']} • {item['produto']} • lote {item['lote']}"
        self.registry_info.configure(text=text)
        if hasattr(self.app,'guide_body'):
            self.app.guide_body.configure(text=text+'\n\nConfirme os dados antes de iniciar a análise.')

    def build_relatorios(self,parent):
        p=tk.Frame(parent,bg=self.col['panel'],highlightthickness=1,
                   highlightbackground=self.col['line'])
        p.pack(fill='both',expand=True)
        self.caption(p,'Histórico de análises vinculadas a fornecedores, clientes e amostras',
                     size=12,weight='bold').pack(anchor='w',padx=18,pady=(16,6))
        self.caption(p,'Relatório em PDF para impressão • dados locais • documento experimental, não certificado MAPA',
                     size=9,color='amber').pack(anchor='w',padx=18,pady=(0,12))
        table_holder=tk.Frame(p,bg=self.col['panel']);table_holder.pack(fill='both',expand=True,padx=14)
        self.reports_tree=self._tree(table_holder,('Análise','Data / Hora','Amostra','Fornecedor','Cliente','Status'),
                                     widths=[70,140,155,180,180,160])
        self.reports_tree.bind('<<TreeviewSelect>>',self._report_selection)
        actions=tk.Frame(p,bg=self.col['panel'])
        actions.pack(fill='x',padx=17,pady=(16,18))
        self.styled_button(actions,'ATUALIZAR',self.refresh_reports,bg='panel2').pack(side='left',padx=(0,9))
        self.styled_button(actions,'GERAR / ABRIR PDF',self.open_report,bg='accent',
                           fg=self.col['bg']).pack(side='left',padx=(0,9))
        self.styled_button(actions,'IMPRIMIR',self.print_report,bg='panel2').pack(side='left',padx=(0,9))
        self.styled_button(actions,'BACKUP CADASTROS',self.backup_cadastros,bg='panel2').pack(side='left')
        self.report_info=self.caption(p,'Selecione uma análise. A impressão usa a impressora padrão do Windows.',size=9)
        self.report_info.pack(anchor='w',padx=18,pady=(0,18))
        self.refresh_reports()

    def refresh_reports(self):
        if not hasattr(self,'reports_tree'):return
        sel=self.reports_tree.selection()
        for item in self.reports_tree.get_children():self.reports_tree.delete(item)
        for r in self.store.analises():
            self.reports_tree.insert('','end',iid=str(r['id']),values=(
                r['id'],r['inicio'].replace('T',' '),r['codigo_amostra'],
                r['fornecedor'],r['cliente'],r['status']))
        if sel and self.reports_tree.exists(sel[0]):
            self.reports_tree.selection_set(sel[0])

    def _report_selection(self,event=None):
        selected=self.reports_tree.selection()
        if selected:
            self.last_analise_id=int(selected[0])
            self.report_info.configure(text=f'Análise selecionada: {self.last_analise_id} • use Abrir PDF ou Imprimir.')

    def create_pdf(self):
        id=self.last_analise_id
        if id is None:
            messagebox.showwarning('Selecione análise','Escolha uma linha da lista de relatórios.',parent=self.app.root)
            return None
        record=self.store.analise(id)
        if record is None:
            messagebox.showerror('Não encontrado','Registro não foi localizado.',parent=self.app.root)
            return None
        out=Path.home()/'.graovision_tcc'/'relatorios_pdf'/f"{record['sessao_id']}_relatorio_experimental.pdf"
        try:
            from .relatorio_pdf import gerar_pdf
            return gerar_pdf(record,out)
        except Exception as exc:
            messagebox.showerror('Falha ao gerar PDF',str(exc),parent=self.app.root)
            return None

    def open_report(self):
        path=self.create_pdf()
        if path:
            webbrowser.open(path.resolve().as_uri())
            self.report_info.configure(text=f'PDF disponível em: {path}')

    def print_report(self):
        path=self.create_pdf()
        if not path:return
        if os.name == 'nt':
            if messagebox.askyesno('Impressão do relatório',
               'Enviar relatório para a impressora padrão do Windows?\n\n'
               'Para escolher impressora, utilize GERAR / ABRIR PDF e pressione Ctrl+P.',
               parent=self.app.root):
                try:os.startfile(str(path),'print')
                except OSError:
                    webbrowser.open(path.resolve().as_uri())
                    messagebox.showinfo('Imprimir','PDF aberto: use Ctrl+P.',parent=self.app.root)
        else:
            webbrowser.open(path.resolve().as_uri())
            messagebox.showinfo('Impressão','PDF aberto: use a opção de impressão do visualizador.',parent=self.app.root)

    def backup_cadastros(self):
        suggested=f'graovision_backup_{datetime.now():%Y%m%d_%H%M%S}.sqlite'
        dest=filedialog.asksaveasfilename(parent=self.app.root,title='Backup local de cadastros e relatórios',
                                          defaultextension='.sqlite',initialfile=suggested,
                                          filetypes=[('Banco SQLite','*.sqlite'),('Todos os arquivos','*.*')])
        if not dest:return
        try:
            backup=criar_backup(self.store,dest)
            self.report_info.configure(text=f'Backup salvo em: {backup}')
        except Exception as exc:
            messagebox.showerror('Erro ao salvar backup',str(exc),parent=self.app.root)
