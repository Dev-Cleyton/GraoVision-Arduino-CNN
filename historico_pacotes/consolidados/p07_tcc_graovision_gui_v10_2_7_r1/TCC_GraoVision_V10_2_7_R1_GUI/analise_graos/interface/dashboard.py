"""GraoVision: HMI desktop para o TCC Arduino + CNN (Tkinter/Pillow).

Separacao obrigatoria:
- A GUI somente envia comandos existentes e aguarda autorizacoes do firmware.
- Botao de retorno nunca inventa homing, nem envia RECUPERAR:X no alto.
- STOP por software nao substitui chave de emergencia fisica.
- Aquecimento real nao e habilitado por esta GUI experimental.
"""
from __future__ import annotations

import io
import os
from pathlib import Path
import queue
import re
import sys
import threading
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk

try:
    from PIL import Image, ImageTk
except ImportError:
    Image = ImageTk = None

from interface.operacao import (
    LocalAdmin, PromptBroker, identify_error, load_settings,
    save_settings, prompt_kind,
)

COL = {
    'bg':'#08111F', 'sidebar':'#0B1729', 'panel':'#101E32',
    'panel2':'#15273E', 'line':'#263951', 'text':'#F4F8FC',
    'muted':'#94A8C1', 'accent':'#38D6C0', 'blue':'#70AAFF',
    'amber':'#FFBD69', 'red':'#FF6C80', 'white':'#FFFFFF',
}
FONT = 'Segoe UI'


def styled_button(parent, text, action, bg='panel2', fg='text', font_size=10,
                  padx=16, pady=11, width=None, state=tk.NORMAL):
    b = tk.Button(parent, text=text, command=action, bg=COL.get(bg,bg),
                  fg=COL.get(fg,fg), activebackground=COL['line'],
                  activeforeground=COL['white'], bd=0, relief=tk.FLAT,
                  font=(FONT,font_size,'bold'), padx=padx, pady=pady,
                  highlightthickness=0, takefocus=False,
                  disabledforeground='#5A6E86', cursor='hand2', state=state)
    if width is not None:
        b.configure(width=width)
    return b


def caption(parent, text, color='muted', size=10, weight='normal', bg='panel'):
    return tk.Label(parent, text=text, fg=COL.get(color,color),
                    bg=COL.get(bg,bg), font=(FONT,size,weight), anchor='w')


class GuiApp:
    def __init__(self, root):
        self.root = root
        self.root.title('GrãoVision | Central de análise e classificação - TCC')
        self.root.configure(bg=COL['bg'])
        self.root.geometry('1460x910')
        self.root.minsize(1120, 730)
        self.root.protocol('WM_DELETE_WINDOW', self._close)
        self.events = queue.Queue()
        self.broker = PromptBroker(self.events)
        self.settings = load_settings()
        self.admin = LocalAdmin()
        self.admin_active = False
        self.active_handler = None
        self.working = False
        self._thread = None
        self._photo = None
        self._pending_dialog = None
        self._current_prompt = None
        self._latest_image = None
        self._return_permitted = False
        self._reference_pending = False
        self._cooling_deadline = None
        self._last_error = None
        self._cycle_measurements = []
        self._cooling_measurements = []
        self._weights = {key: None for key in ('P1','P2','PF','P3','P4','P5')}
        self._current_stage = 'AGUARDANDO'
        self._build()
        self.root.after(100, self._pump)
        self._log('GrãoVision iniciado. Modo térmico da GUI: DESABILITADO.\n')

    def _build(self):
        self.sidebar = tk.Frame(self.root, bg=COL['sidebar'], width=224)
        self.sidebar.pack(side='left', fill='y')
        self.sidebar.pack_propagate(False)
        tk.Label(self.sidebar, text='◈  GRÃOVISION', bg=COL['sidebar'],
                 fg=COL['accent'], font=(FONT,18,'bold'), anchor='w').pack(
                 padx=23, pady=(30,5), fill='x')
        caption(self.sidebar, 'TCC  •  VISÃO COMPUTACIONAL', size=8,
                bg='sidebar').pack(padx=24, pady=(0,36), anchor='w')
        self._nav_button('▦  Visão geral', 'dashboard')
        self._nav_button('◉  Eventos & alarmes', 'eventos')
        self._nav_button('⚙  Configurações', 'config')
        tk.Frame(self.sidebar, bg=COL['line'], height=1).pack(fill='x', padx=20,pady=25)
        self.connection_badge = caption(self.sidebar, '●  Aguardando Arduino',
                                        color='amber', bg='sidebar',size=10)
        self.connection_badge.pack(anchor='w', padx=24,pady=(0,10))
        self.role_badge = caption(self.sidebar, 'Perfil: Operador', size=10,bg='sidebar')
        self.role_badge.pack(anchor='w',padx=24)
        tk.Frame(self.sidebar,bg=COL['sidebar']).pack(fill='both',expand=True)
        caption(self.sidebar, 'VERSÃO 10.2.7 R1  ·  HMI 1.0', size=9,bg='sidebar').pack(
                anchor='w',padx=23,pady=(0,8))
        caption(self.sidebar, 'Arduino Uno  •  HX711  •  CNN',size=9,bg='sidebar').pack(
                anchor='w',padx=23,pady=(0,28))

        self.body = tk.Frame(self.root,bg=COL['bg'])
        self.body.pack(side='left',fill='both',expand=True)
        top = tk.Frame(self.body,bg=COL['bg'])
        top.pack(fill='x',padx=27,pady=(22,17))
        textbox = tk.Frame(top,bg=COL['bg'])
        tk.Label(textbox,text='Central de operação',bg=COL['bg'],fg=COL['text'],
                 font=(FONT,22,'bold')).pack(anchor='w')
        self.subtitle = caption(textbox,'Controle supervisionado • Pesagem • CNN • Secagem',
                                size=10,bg='bg')
        self.subtitle.pack(anchor='w',pady=(4,0))
        textbox.pack(side='left')
        self.status = tk.Label(top,text='PRONTO PARA CONECTAR',bg=COL['panel2'],
                               fg=COL['accent'],padx=15,pady=10,
                               font=(FONT,9,'bold'))
        self.status.pack(side='right',padx=10)

        self.view_host=tk.Frame(self.body,bg=COL['bg'])
        self.view_host.pack(fill='both',expand=True,padx=26,pady=(0,22))
        self.views={}
        for name in ('dashboard','eventos','config'):
            self.views[name]=tk.Frame(self.view_host,bg=COL['bg'])
        self._build_dashboard(self.views['dashboard'])
        self._build_events(self.views['eventos'])
        self._build_settings(self.views['config'])
        self._show('dashboard')

    def _nav_button(self, text, view):
        styled_button(self.sidebar,text,lambda: self._show(view),
                      bg='sidebar',fg='text',font_size=11,padx=12,pady=16).pack(
                      fill='x',padx=12,pady=2)

    def _show(self, name):
        if name=='config' and not self.admin_active:
            if not self._admin_gate():
                return
        for frame in self.views.values():
            frame.pack_forget()
        self.views[name].pack(fill='both',expand=True)

    def _panel(self,parent, title, extra=None):
        f=tk.Frame(parent,bg=COL['panel'],highlightbackground=COL['line'],highlightthickness=1)
        head=tk.Frame(f,bg=COL['panel'])
        head.pack(fill='x',padx=18,pady=(16,10))
        caption(head,title,size=11,weight='bold').pack(side='left')
        if extra:
            caption(head,extra,size=9).pack(side='right')
        return f

    def _build_dashboard(self, host):
        host.grid_rowconfigure(0,weight=1)
        host.grid_columnconfigure(0,weight=3)
        host.grid_columnconfigure(1,weight=2)
        left=tk.Frame(host,bg=COL['bg']); left.grid(row=0,column=0,sticky='nsew',padx=(0,10))
        right=tk.Frame(host,bg=COL['bg']); right.grid(row=0,column=1,sticky='nsew',padx=(10,0))
        left.grid_rowconfigure(2,weight=1)
        left.grid_rowconfigure(3,weight=1)
        right.grid_rowconfigure(0,weight=1)
        right.grid_rowconfigure(2,weight=1)
        metrics=tk.Frame(left,bg=COL['bg']); metrics.grid(row=0,column=0,sticky='ew')
        for i in range(3): metrics.columnconfigure(i,weight=1)
        self.metric_labels={}
        metrics_spec=[('P1','PESO INICIAL'),('P2','REFERÊNCIA SUPERIOR'),
                      ('PF','PESO FINAL'),('P3','1º RETORNO'),
                      ('P4','2ª SUBIDA'),('P5','2º RETORNO')]
        for idx,(key,title) in enumerate(metrics_spec):
            box=tk.Frame(metrics,bg=COL['panel'],highlightthickness=1,
                         highlightbackground=COL['line'])
            box.grid(row=idx//3,column=idx%3,sticky='nsew',padx=(0 if idx%3==0 else 5,5),
                     pady=(0,10))
            caption(box,title,size=8).pack(anchor='w',padx=14,pady=(12,3))
            value=tk.Label(box,text='—',bg=COL['panel'],fg=COL['text'],
                           font=(FONT,20,'bold'),anchor='w')
            value.pack(fill='x',padx=14,pady=(0,12))
            self.metric_labels[key]=value

        stage_panel=self._panel(left,'Etapas do processo','Fluxo supervisionado')
        stage_panel.grid(row=1,column=0,sticky='ew',pady=(4,12))
        stage_bar=tk.Frame(stage_panel,bg=COL['panel'])
        stage_bar.pack(fill='x',padx=12,pady=(1,17))
        self.stage_labels={}
        for key in ('P1','CNN','P2','PF','P3','P4','P5'):
            lbl=tk.Label(stage_bar,text=key,bg=COL['panel2'],fg=COL['muted'],
                         font=(FONT,9,'bold'),padx=10,pady=9)
            lbl.pack(side='left',expand=True,fill='x',padx=3)
            self.stage_labels[key]=lbl

        chart_panel=self._panel(left,'Temperatura da câmara','NTC A2 • °C')
        chart_panel.grid(row=2,column=0,sticky='nsew',pady=(0,12))
        self.temp_canvas=tk.Canvas(chart_panel,bg=COL['panel'],highlightthickness=0,
                                   height=198)
        self.temp_canvas.pack(fill='both',expand=True,padx=14,pady=(0,11))
        self.temp_canvas.bind('<Configure>',lambda e:self._plot())

        log_panel=self._panel(left,'Telemetria e comunicação','Protocolo serial')
        log_panel.grid(row=3,column=0,sticky='nsew')
        self.console=self._text_area(log_panel,height=8)
        self.console.pack(fill='both',expand=True,padx=13,pady=(0,13))

        preview=self._panel(right,'Classificação visual','Webcam / Roboflow')
        preview.grid(row=0,column=0,sticky='nsew',pady=(0,12))
        self.image_frame=tk.Frame(preview,bg='#080F18')
        self.image_frame.pack(fill='both',expand=True,padx=16,pady=(0,8))
        self.image_label=tk.Label(self.image_frame,text='◉\n\nAguardando captura da webcam',
                                  bg='#080F18',fg=COL['muted'],font=(FONT,14))
        self.image_label.pack(fill='both',expand=True)
        self.image_frame.bind('<Configure>',lambda e:self._refresh_image())
        self.img_caption=caption(preview,'Prévia em tempo real durante a captura; inferência após salvar imagem.',size=9)
        self.img_caption.pack(fill='x',padx=17,pady=(0,15))

        state_panel=self._panel(right,'Status do equipamento','Movimentos comandados')
        state_panel.grid(row=1,column=0,sticky='ew',pady=(0,12))
        grid=tk.Frame(state_panel,bg=COL['panel']);grid.pack(fill='x',padx=18,pady=(0,17))
        for i in range(2):grid.grid_columnconfigure(i,weight=1)
        self.temp_value=tk.Label(grid,text='— °C',font=(FONT,18,'bold'),fg=COL['accent'],bg=COL['panel'])
        self.pos_value=tk.Label(grid,text='— passos',font=(FONT,18,'bold'),fg=COL['blue'],bg=COL['panel'])
        caption(grid,'TEMPERATURA',size=8).grid(row=0,column=0,sticky='w')
        caption(grid,'ATUADOR (COMANDADO)',size=8).grid(row=0,column=1,sticky='w')
        self.temp_value.grid(row=1,column=0,sticky='w',pady=(4,0))
        self.pos_value.grid(row=1,column=1,sticky='w',pady=(4,0))
        self.heater_label=caption(state_panel,'Aquecedor: bloqueado pela GUI',color='amber',size=9)
        self.heater_label.pack(fill='x',padx=18,pady=(0,14))

        commands=self._panel(right,'Controles supervisionados','Somente com autorização')
        commands.grid(row=2,column=0,sticky='nsew')
        btn_area=tk.Frame(commands,bg=COL['panel'])
        btn_area.pack(fill='x',padx=16,pady=(4,9))
        self.start_btn=styled_button(btn_area,'▶  INICIAR ANÁLISE',self.start_cycle,
                                      bg='accent',fg='#07131F',font_size=11)
        self.start_btn.pack(fill='x',pady=(0,8))
        self.stop_btn=styled_button(btn_area,'■  SOLICITAR STOP',self.request_stop,
                                     bg='red',fg='#271017',font_size=11,state=tk.DISABLED)
        self.stop_btn.pack(fill='x',pady=(0,8))
        self.return_btn=styled_button(btn_area,'↶  RETORNO SUPERVISIONADO',self.request_return,
                                       bg='blue',fg='#07131F',state=tk.DISABLED)
        self.return_btn.pack(fill='x',pady=(0,8))
        self.x_btn=styled_button(btn_area,'✓  CONFIRMAR REFERÊNCIA X',self.request_x,
                                  bg='panel2',fg='text',state=tk.DISABLED)
        self.x_btn.pack(fill='x',pady=(0,8))
        caption(commands,'O STOP por software não substitui a parada física.',size=9).pack(
                anchor='w',padx=17,pady=(1,15))

    @staticmethod
    def _text_area(parent,height=10):
        t=tk.Text(parent,bg=COL['bg'],fg=COL['muted'],insertbackground=COL['text'],
                  font=('Consolas',9),bd=0,highlightthickness=0,wrap='word',height=height,
                  padx=12,pady=8,state='disabled')
        return t

    def _build_events(self,host):
        p=self._panel(host,'Eventos, diagnósticos e alarmes','Histórico desta sessão')
        p.pack(fill='both',expand=True)
        self.event_console=self._text_area(p,height=24)
        self.event_console.pack(fill='both',expand=True,padx=14,pady=(0,16))

    def _build_settings(self,host):
        panel=self._panel(host,'Configurações de operação','Acesso administrativo local')
        panel.pack(fill='x')
        self.settings_inputs={}
        entries=[('porta_serial','Porta serial Arduino (ex.: COM4)'),
                 ('camera_indice','Índice da webcam (0 a 12)')]
        for key,label in entries:
            line=tk.Frame(panel,bg=COL['panel']);line.pack(fill='x',padx=24,pady=10)
            caption(line,label).pack(side='left',fill='x',expand=True)
            entry=tk.Entry(line,bg=COL['panel2'],fg=COL['text'],insertbackground=COL['accent'],
                           font=(FONT,12),bd=0,width=15,justify='center')
            entry.insert(0,str(self.settings[key]))
            entry.pack(side='right',ipady=10)
            self.settings_inputs[key]=entry
        self.check_reals=tk.BooleanVar(value=self.settings['movimentos_reais'])
        self.check_roi=tk.BooleanVar(value=self.settings['roi_automatica'])
        for text,var in [('Permitir solicitar movimentos reais (exige ARMAR presencial)',self.check_reals),
                         ('ROI automática para a CNN',self.check_roi)]:
            chk=tk.Checkbutton(panel,text=text,variable=var,bg=COL['panel'],fg=COL['text'],
                               selectcolor=COL['panel2'],activebackground=COL['panel'],
                               activeforeground=COL['text'],font=(FONT,10))
            chk.pack(anchor='w',padx=22,pady=7)
        caption(panel,'Aquecimento real: DESABILITADO na interface. Proteções não podem ser anuladas por configuração.',
                color='amber',size=10).pack(anchor='w',padx=24,pady=(10,12))
        styled_button(panel,'SALVAR CONFIGURAÇÕES',self.save_config,bg='accent',fg='#07131F').pack(
                      anchor='w',padx=22,pady=(5,24))
        panel2=self._panel(host,'Acesso administrativo','Proteção da interface, não do firmware')
        panel2.pack(fill='x',pady=(18,0))
        caption(panel2,'Permissões de administrador controlam apenas a interface local. Não substituem E-stop, homing e intertravamentos.',
                size=10).pack(anchor='w',padx=22,pady=(4,12))
        styled_button(panel2,'SAIR DO MODO ADMINISTRADOR',self.logout_admin).pack(
                      anchor='w',padx=22,pady=(0,22))

    def _admin_gate(self):
        if not self.admin.configured:
            if not messagebox.askyesno('Primeiro acesso',
                         'Defina um PIN administrativo local (mínimo 6 dígitos).\n\n'
                         'O PIN não desbloqueia proteções físicas do Arduino.'):
                return False
            pin=simpledialog.askstring('Criar PIN administrador','Escolha PIN numérico (mín. 6 dígitos):',
                                     show='*',parent=self.root)
            if pin is None:return False
            again=simpledialog.askstring('Confirmar PIN','Repita o PIN:',show='*',parent=self.root)
            if again!=pin:
                messagebox.showerror('PIN','Confirmação não coincide.')
                return False
            try:self.admin.configure(pin)
            except ValueError as exc:
                messagebox.showerror('PIN inválido',str(exc));return False
        else:
            pin=simpledialog.askstring('Acesso administrador','Digite o PIN administrativo:',
                                     show='*',parent=self.root)
            if pin is None or not self.admin.verify(pin):
                messagebox.showwarning('Acesso negado','PIN incorreto ou acesso cancelado.')
                return False
        self.admin_active=True
        self.role_badge.configure(text='Perfil: Administrador',fg=COL['accent'])
        return True

    def logout_admin(self):
        self.admin_active=False
        self.role_badge.configure(text='Perfil: Operador',fg=COL['muted'])
        self._show('dashboard')

    def save_config(self):
        if self.working:
            messagebox.showwarning('Configuração bloqueada','Pare o ciclo antes de alterar parâmetros.')
            return
        try:
            self.settings=save_settings({
                'porta_serial':self.settings_inputs['porta_serial'].get(),
                'camera_indice':self.settings_inputs['camera_indice'].get(),
                'movimentos_reais':self.check_reals.get(),
                'roi_automatica':self.check_roi.get(),
            })
        except (ValueError,TypeError) as exc:
            messagebox.showerror('Configuração inválida',str(exc));return
        messagebox.showinfo('Configuração salva','Parâmetros salvos para o próximo ciclo.\nAquecimento continua desabilitado.')
        self._log(f"Configuração: porta={self.settings['porta_serial']} movimentos_reais={int(self.settings['movimentos_reais'])}")

    def _append_text(self,widget,message):
        widget.configure(state='normal')
        widget.insert('end',message)
        num=int(widget.index('end-1c').split('.')[0])
        if num>450:
            widget.delete('1.0',f'{num-420}.0')
        widget.see('end')
        widget.configure(state='disabled')

    def _log(self,message):
        message=message.rstrip('\n')+'\n'
        self._append_text(self.console,message)
        self._append_text(self.event_console,message)

    def _emit(self,kind,payload):
        # Thread segura: widgets Tk nunca sao tocados no worker.
        self.events.put((kind,payload))

    def start_cycle(self):
        if self.working: return
        if not messagebox.askyesno('Iniciar novo ensaio',
            'Confirme que a bandeja está em X inferior, servo centrado, prato vazio, percurso livre e parada física acessível.\n\n'
            'Aquecimento real permanecerá desabilitado pela interface.\n\nIniciar?'):
            return
        self.working=True
        self._last_error=None
        self._return_permitted=False
        self._reference_pending=False
        self._weights={key:None for key in self._weights}
        self._cycle_measurements=[]
        self._cooling_measurements=[]
        self._refresh_metrics()
        self._plot()
        self.start_btn.configure(state=tk.DISABLED)
        self.stop_btn.configure(state=tk.NORMAL)
        self.return_btn.configure(state=tk.DISABLED)
        self.x_btn.configure(state=tk.DISABLED)
        self._status('CICLO EM EXECUÇÃO','accent')
        self._thread=threading.Thread(target=self._run_cycle,daemon=True)
        self._thread.start()

    def _run_cycle(self):
        self._emit('log','[INTERFACE] Novo ciclo iniciado; monitoramento em thread de trabalho.')
        try:
            from core.analysis_service import AnalysisService
            from core.sample_service import SampleService
            from hardware.arduino_handler import ArduinoHandler
            # Configuracao local: nao altera .env do usuario nem chaves Roboflow.
            os.environ['MOVIMENTOS_REAIS']='1' if self.settings['movimentos_reais'] else '0'
            os.environ['AQUECIMENTO_REAL']='0'  # HMI experimental SEM calor ate auditoria
            os.environ['ROI_AUTOMATICA']='1' if self.settings['roi_automatica'] else '0'
            os.chdir(str(Path(__file__).resolve().parents[1]))
            handler=ArduinoHandler(port=self.settings['porta_serial'],on_event=self._emit,
                                   prompt=self.broker.ask)
            self.active_handler=handler
            sample_service=SampleService(on_frame=lambda jpeg:self._emit('preview',jpeg),
                                         on_image=lambda path:self._emit('image',path),
                                         camera_index=self.settings['camera_indice'])
            service=AnalysisService(arduino=handler,sample_service=sample_service,
                                     prompt=self.broker.ask)
            session=service.executar(interativo=True,salvar=True)
            self._emit('session',session)
        except Exception as exc:
            self._emit('external_error',str(exc))
        finally:
            self.active_handler=None
            self._emit('finished',None)

    def request_stop(self):
        if not self.working or self.active_handler is None:
            messagebox.showinfo('STOP','Não existe conexão ativa no momento.')
            return
        if not messagebox.askyesno('Solicitar STOP',
            'Enviar STOP ao Arduino?\n\nUse a parada de emergência física se houver perigo imediato.\n'
            'STOP por software não garante o corte físico do aquecedor.'):
            return
        self.broker.cancel()
        try:self.active_handler.parar()
        except Exception as exc:
            messagebox.showerror('Falha ao transmitir STOP',str(exc))
        self._log('[OPERADOR] Comando STOP solicitado; aguardar confirmação Arduino.')

    def request_return(self):
        if not self._return_permitted or self.broker.pending is None or \
           prompt_kind(self.broker.pending['label'])!='supervised_return':
            messagebox.showwarning('Retorno bloqueado',
                'O Arduino não autorizou retorno supervisionado nesta sessão.\n\n'
                'Não é seguro mover usando apenas a EEPROM.')
            return
        self._prompt_window(self.broker.pending)

    def request_x(self):
        if not self._reference_pending or self.broker.pending is None or \
           prompt_kind(self.broker.pending['label'])!='confirm_x':
            messagebox.showwarning('Referência X bloqueada',
                'O firmware não solicitou validação de X. A confirmação lógica só é permitida\n'
                'depois de conferir o atuador realmente no ponto inferior.')
            return
        self._prompt_window(self.broker.pending)

    def _status(self,message,color='accent'):
        self.status.configure(text=message,fg=COL.get(color,color))

    def _refresh_metrics(self):
        for key,v in self._weights.items():
            self.metric_labels[key].configure(text=f'{v:.4f} g' if v is not None else '—')

    def _set_stage(self,key):
        if key not in self.stage_labels: return
        self._current_stage=key
        for stage,lbl in self.stage_labels.items():
            lbl.configure(bg=COL['accent'] if stage==key else COL['panel2'],
                          fg=COL['bg'] if stage==key else COL['muted'])

    def _event_serial(self,direction,line):
        if line.startswith('PC:ALIVE'):
            return
        self._log(('[UNO] ' if direction=='rx' else '[PC]  ')+line)
        if direction!='rx':return
        if line=='READY:V10.1':
            self.connection_badge.configure(text='●  Arduino conectado',fg=COL['accent'])
        if line.startswith('RECUPERACAO:PENDENTE:'):
            self._reference_pending=True
            self._status('REFERÊNCIA X PENDENTE','amber')
        if line.startswith('RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL'):
            self._return_permitted=True
            self.return_btn.configure(state=tk.NORMAL)
        if line.startswith('ERRO:'):
            code=line.partition(':')[2]
            self._last_error=code
            self._status('FALHA: '+code[:35],'red')
            self.root.after(10,lambda c=code:self._error_popup(c))
        if line.startswith('P1:') and not line.startswith('P1:ERRO'):
            self._register_weight('P1',line)
        for key in ('P2','PF','P3','P4','P5'):
            if line.startswith(key+':') and 'ERRO' not in line:
                self._register_weight(key,line)
        if line=='CMD:ACTIVATE_CAMERA':self._set_stage('CNN')
        if line.startswith('VETOR:'):
            key=line.split(':')[1]
            self._set_stage(key)
        if line.startswith('PASSOS:'):
            try:self.pos_value.configure(text=f'{int(line.rsplit(":",1)[1]):,} passos'.replace(',','.'))
            except ValueError:pass
        if line.startswith('EEPROM:POS_COMANDADA:'):
            try:self.pos_value.configure(text=f'{line.split(":")[2]} passos')
            except IndexError:pass
        if line.startswith('MEDICAO:'):
            parts=line.split(':')
            if len(parts)==4:
                try:
                    t=float(parts[3]);seg=int(parts[1]);self._cycle_measurements.append((seg,t))
                    self.temp_value.configure(text=f'{t:.1f} °C');self._plot()
                except ValueError: pass
        if line.startswith('RESFRIAMENTO:'):
            parts=line.split(':')
            if len(parts)==3:
                try:
                    seg=int(parts[1]);t=float(parts[2]);self._cooling_measurements.append((seg,t))
                    self.temp_value.configure(text=f'{t:.1f} °C');self._plot()
                except ValueError: pass
        if line.startswith('AQUECEDOR:ON_COMANDADO:'):
            self.heater_label.configure(text='Aquecedor: ON comandado pelo firmware',fg=COL['red'])
        if line.startswith('AQUECEDOR:OFF:'):
            self.heater_label.configure(text='Aquecedor: OFF comandado ('+line.split(':')[-1]+')',fg=COL['amber'])
        if line=='FIM:1':self._status('CICLO CONCLUÍDO','accent')

    def _register_weight(self,key,line):
        try:
            value=float(line.split(':',1)[1]);self._weights[key]=value
            self._refresh_metrics();self._set_stage(key)
        except ValueError:pass

    def _plot(self):
        c=self.temp_canvas
        if not c.winfo_exists():return
        c.delete('all')
        w=max(300,c.winfo_width());h=max(145,c.winfo_height())
        left,right,top,bottom=48,w-18,18,h-34
        for i in range(5):
            y=top+(bottom-top)*i/4
            c.create_line(left,y,right,y,fill=COL['line'],width=1)
        data=self._cycle_measurements
        cooling=self._cooling_measurements
        points=[(t,v) for t,v in data]
        if cooling:
            offset=(max([a for a,_ in data],default=0)+1)
            points.extend((offset+a,v) for a,v in cooling)
        if not points:
            c.create_text(w/2,h/2,text='Aguardando medições do NTC',
                          font=(FONT,12),fill=COL['muted'])
            return
        mint=min(v for _,v in points)-3;maxt=max(v for _,v in points)+3
        if maxt-mint<5:maxt=mint+5
        minx=0;maxx=max(120,max(t for t,_ in points))
        def coords(t,v):
            return left+(t-minx)/maxx*(right-left),bottom-(v-mint)/(maxt-mint)*(bottom-top)
        for i in range(5):
            val=maxt-i*(maxt-mint)/4
            c.create_text(left-7,top+i*(bottom-top)/4,text=f'{val:.0f}°',
                          anchor='e',font=(FONT,8),fill=COL['muted'])
        prev=None
        for t,v in points:
            x,y=coords(t,v)
            if prev is not None:
                c.create_line(prev[0],prev[1],x,y,fill=COL['accent'],width=3)
            c.create_oval(x-3,y-3,x+3,y+3,fill=COL['accent'],outline='')
            prev=(x,y)
        c.create_text(left,bottom+16,text='0 s',anchor='w',font=(FONT,9),fill=COL['muted'])
        c.create_text(right,bottom+16,text=f'{maxx:.0f} s',anchor='e',font=(FONT,9),fill=COL['muted'])

    def _image_update(self,source):
        if Image is None:
            self.image_label.configure(text='Instale Pillow para exibir imagens: pip install Pillow')
            return
        try:
            if isinstance(source,bytes):
                im=Image.open(io.BytesIO(source)).convert('RGB')
            else:
                path=Path(source)
                if not path.exists():return
                im=Image.open(path).convert('RGB')
            self._latest_image=im
            self._refresh_image()
        except (OSError,ValueError): pass

    def _refresh_image(self):
        if Image is None or self._latest_image is None:return
        w=max(300,self.image_frame.winfo_width()-12)
        h=max(170,self.image_frame.winfo_height()-12)
        im=self._latest_image.copy()
        im.thumbnail((w,h),Image.Resampling.LANCZOS)
        self._photo=ImageTk.PhotoImage(im)
        self.image_label.configure(image=self._photo,text='')

    def _error_popup(self,code):
        title,action=identify_error(code)
        dialog=tk.Toplevel(self.root)
        dialog.configure(bg=COL['panel']);dialog.title('Alarme do sistema')
        dialog.geometry('590x320');dialog.resizable(False,False)
        tk.Label(dialog,text='⚠  FALHA IDENTIFICADA',bg=COL['panel'],fg=COL['red'],
                 font=(FONT,17,'bold')).pack(anchor='w',padx=28,pady=(24,12))
        caption(dialog,code,color='amber',size=11).pack(anchor='w',padx=28)
        tk.Label(dialog,text=title,bg=COL['panel'],fg=COL['text'],
                 wraplength=515,justify='left',font=(FONT,11)).pack(anchor='w',padx=28,pady=(14,10))
        tk.Label(dialog,text='AÇÃO: '+action,bg=COL['panel'],fg=COL['muted'],
                 wraplength=520,justify='left',font=(FONT,10)).pack(anchor='w',padx=28,pady=3)
        styled_button(dialog,'ENTENDI • VERIFICAR EQUIPAMENTO',dialog.destroy,bg='panel2').pack(
                side='bottom',anchor='e',padx=26,pady=22)

    def _prompt_window(self,obj):
        if obj is None or obj['done'].is_set(): return
        if self._pending_dialog and self._pending_dialog.winfo_exists():
            self._pending_dialog.lift();return
        kind=prompt_kind(obj['label'])
        is_critical=kind in ('confirm_x','thermal_return','supervised_return',
                             'arm_move','heating','diagnostic')
        title={
            'confirm_x':'Conferência física • Referência X',
            'thermal_return':'Autorização de retorno após resfriamento',
            'supervised_return':'Retorno supervisionado',
            'diagnostic':'Segunda subida e descida • P4/P5',
            'arm_move':'Armamento de movimentos reais',
            'heating':'Inspeção térmica (não disponível pela GUI)',
            'tare':'Confirmar prato vazio e estabilidade',
            'load':'Preparar amostra',
        }.get(kind,'Confirmação presencial')
        details={
            'confirm_x':'O comando RECUPERAR:X APENAS grava a referência lógica. NÃO move o motor.\n'
                        'É proibido confirmar se o eixo estiver no alto.',
            'thermal_return':'Verifique temperatura real dos componentes, suporte da bandeja, cabos, percurso e parada física.\n'
                             'O firmware exige confirmação dentro do prazo de 180 s após a liberação.',
            'supervised_return':'Somente o Arduino pode autorizar o retorno nesta mesma sessão.\n'
                                'Confirme apoio da bandeja e aquecedor fisicamente desconectado.',
            'diagnostic':'Ensaio opcional de mais 14.000 passos de subida e descida, sem calor.',
            'arm_move':'Movimentos reais exigem inspeção presencial do percurso e parada de emergência.',
            'tare':'Certifique-se de que o prato esteja vazio e completamente parado.',
            'load':'Coloque a amostra no prato e confirme sua estabilidade.',
        }.get(kind,'Leia a instrução abaixo antes de confirmar.')
        dialog=tk.Toplevel(self.root)
        self._pending_dialog=dialog
        self._current_prompt=obj
        dialog.title(title);dialog.geometry('640x470' if is_critical else '600x345')
        dialog.configure(bg=COL['panel']);dialog.resizable(False,False)
        dialog.transient(self.root);dialog.grab_set()
        def resolve(answer):
            if self.broker.answer(answer,obj):
                self._return_permitted=False
                self._reference_pending=False
                self.return_btn.configure(state=tk.DISABLED)
                self.x_btn.configure(state=tk.DISABLED)
            try:dialog.grab_release();dialog.destroy()
            except tk.TclError:pass
            self._pending_dialog=None
        dialog.protocol('WM_DELETE_WINDOW',lambda:resolve(None))
        tk.Label(dialog,text=title,bg=COL['panel'],fg=COL['text'],
                 font=(FONT,17,'bold'),wraplength=575).pack(anchor='w',padx=28,pady=(26,14))
        tk.Label(dialog,text=details,bg=COL['panel'],fg=COL['muted'],
                 font=(FONT,10),justify='left',wraplength=580).pack(anchor='w',padx=28,pady=(0,15))
        tk.Label(dialog,text=obj['label'],bg=COL['panel2'],fg=COL['text'],
                 font=(FONT,10),padx=12,pady=9,wraplength=560).pack(fill='x',padx=26)
        checks=[]
        if is_critical:
            txts={
                'confirm_x':('Atuador REALMENTE na posição X inferior e servo em 90°',
                             'Prato vazio, percurso livre e aquecedor desenergizado'),
                'thermal_return':('Aquecedor desenergizado e temperatura física segura',
                                  'Cabos, sustentação, percurso e parada física inspecionados'),
                'supervised_return':('Motor parado, eixo sustentado, percurso livre',
                                     'Aquecedor fisicamente desligado e parada acessível'),
                'diagnostic':('Inspeção do percurso e cabos realizada',
                              'Presença do operador e parada física disponível'),
                'arm_move':('Percurso e cabos inspecionados',
                            'Parada de emergência física disponível'),
                'heating':('Aquecedor independente auditado',),
            }.get(kind,())
            for txt in txts:
                v=tk.BooleanVar(value=False)
                tk.Checkbutton(dialog,text=txt,variable=v,bg=COL['panel'],fg=COL['text'],
                               selectcolor=COL['panel2'],activebackground=COL['panel'],
                               activeforeground=COL['text'],font=(FONT,9),
                               wraplength=540,justify='left').pack(anchor='w',padx=26,pady=5)
                checks.append(v)
        buttons=tk.Frame(dialog,bg=COL['panel']);buttons.pack(side='bottom',fill='x',padx=26,pady=26)
        styled_button(buttons,'CANCELAR',lambda:resolve(None),bg='panel2').pack(side='left')
        if self.working:
            def stop_from_modal():
                if messagebox.askyesno('STOP do equipamento',
                        'Solicitar STOP ao Arduino? Use a parada física se houver risco imediato.',
                        parent=dialog):
                    try:
                        if self.active_handler is not None: self.active_handler.parar()
                    except Exception as exc:
                        messagebox.showerror('STOP não enviado',str(exc),parent=dialog)
                    resolve(None)
            styled_button(buttons,'■ STOP',stop_from_modal,bg='red',fg='#271017').pack(
                          side='left',padx=10)
        affirmative={
            'confirm_x':('CONFIRMAR X', 'CONFIRMO-X'),
            'thermal_return':('AUTORIZAR RETORNO APÓS RESFRIAMENTO','RETORNO-RESFRIADO'),
            'supervised_return':('SOLICITAR RETORNO SUPERVISIONADO','RETORNAR'),
            'diagnostic':('AUTORIZAR P4 / P5','AUTORIZAR-P4-P5'),
            'arm_move':('ARMAR MOVIMENTOS','ARMAR'),
            'tare':('CONFIRMAR TARA',''),
            'load':('INICIAR PESAGEM',''),
            'confirm':('CONFIRMAR',''),
        }
        label,value=affirmative.get(kind,('CONFIRMAR',''))
        def accept():
            if checks and not all(v.get() for v in checks):
                messagebox.showwarning('Inspeção incompleta',
                                       'Confirme todos os itens de segurança antes de continuar.',parent=dialog)
                return
            resolve(value)
        if kind=='heating':
            message=caption(dialog,'A HMI não habilita aquecimento nesta revisão.',color='red')
            message.pack(anchor='w',padx=28,pady=5)
        else:
            styled_button(buttons,label,accept,bg='accent',fg='#07131F').pack(side='right')
        if kind=='diagnostic':
            styled_button(buttons,'PULAR P4/P5',lambda:resolve(''),bg='panel2').pack(
                          side='right',padx=10)
        self.root.after(30,dialog.focus_force)

    def _pump(self):
        if not self.root.winfo_exists():return
        # Descarte previsoes antigas se houver excesso de frames em buffer.
        processed=0
        last_preview=None
        annotated_received=False
        while processed<90:
            try:kind,payload=self.events.get_nowait()
            except queue.Empty:break
            processed+=1
            if kind=='preview':last_preview=payload
            elif kind=='image':
                annotated_received=True
                self._image_update(payload)
                self.img_caption.configure(text='Resultado da CNN • caixas e ROI salvas em arquivo')
            elif kind=='log':self._log(payload)
            elif kind=='rx' or kind=='tx':self._event_serial(kind,payload)
            elif kind=='prompt':
                self._prompt_window(payload)
                if prompt_kind(payload['label'])=='confirm_x':
                    self._reference_pending=True
                    self.x_btn.configure(state=tk.NORMAL)
                if prompt_kind(payload['label'])=='supervised_return' and self._return_permitted:
                    self.return_btn.configure(state=tk.NORMAL)
            elif kind=='session':
                sample=payload.sample
                self._log('[SESSÃO] RESULTADO: '+str(sample.status_final))
                self._log('[SESSÃO] '+str(sample.motivo_final))
                if sample.status_final=='ERRO_PROCESSO' and not self._last_error:
                    self._error_popup(sample.codigo_erro_arduino or sample.motivo_final or 'ERRO_PROCESSO')
            elif kind=='external_error':
                self._status('FALHA AO INICIAR','red')
                self._log('[ERRO] '+payload)
                messagebox.showerror('Falha ao iniciar o ciclo',payload)
            elif kind=='finished':
                self.working=False
                self.start_btn.configure(state=tk.NORMAL)
                self.stop_btn.configure(state=tk.DISABLED)
                self.return_btn.configure(state=tk.DISABLED)
                self.x_btn.configure(state=tk.DISABLED)
                self.connection_badge.configure(text='●  Sessão encerrada',fg=COL['muted'])
                self._return_permitted=False;self._reference_pending=False
                if self._last_error:
                    self._status('ERRO • VERIFIQUE O EQUIPAMENTO','red')
                else:
                    self._status('PRONTO PARA NOVO CICLO','accent')
        if last_preview is not None and not annotated_received:self._image_update(last_preview)
        self.root.after(90,self._pump)

    def _close(self):
        if self.working:
            messagebox.showwarning('Ciclo em andamento',
                'Não feche a interface durante movimentos ou aquecimento.\n'
                'Solicite STOP, verifique o estado físico e aguarde a finalização.')
            return
        self.root.destroy()


def run():
    root=tk.Tk()
    app=GuiApp(root)
    root.mainloop()
    return app

if __name__=='__main__':
    run()
