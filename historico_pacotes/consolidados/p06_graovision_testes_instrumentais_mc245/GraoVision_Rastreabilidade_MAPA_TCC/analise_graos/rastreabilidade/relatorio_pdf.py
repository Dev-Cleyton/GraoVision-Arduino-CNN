"""Gerador de PDF de rastreabilidade para demonstracao/TCC (nao oficial MAPA)."""
from __future__ import annotations

from datetime import datetime
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
                               KeepTogether, Image as RLImage, HRFlowable)

DARK = colors.HexColor('#15263E')
TEAL = colors.HexColor('#008E89')
GREY = colors.HexColor('#58677B')
PALE = colors.HexColor('#EDF3F6')


def ddate(s):
    if not s:return 'Não informado'
    try:return datetime.fromisoformat(s).strftime('%d/%m/%Y %H:%M' if 'T' in s else '%d/%m/%Y')
    except ValueError:return str(s)


def val(v):
    return 'Não informado' if v is None or str(v).strip()=='' else str(v)


def _n(v, precision=4):
    try:return f'{float(v):.{precision}f}'
    except (ValueError,TypeError):return '—'


def gerar_pdf(analise, destino, diretorio_imagens=None):
    """Analise e snapshot vieram do banco local, sem acessar a API nem sensores."""
    out = Path(destino)
    out.parent.mkdir(parents=True,exist_ok=True)
    doc=SimpleDocTemplate(str(out),pagesize=A4,rightMargin=18*mm,leftMargin=18*mm,
                          topMargin=18*mm,bottomMargin=16*mm,title='GrãoVision | Relatório técnico experimental')
    styles=getSampleStyleSheet()
    styles.add(ParagraphStyle(name='GVTitle',fontName='Helvetica-Bold',fontSize=18,leading=22,
                              textColor=DARK,spaceAfter=6))
    styles.add(ParagraphStyle(name='GVHead',fontName='Helvetica-Bold',fontSize=10,leading=14,
                              textColor=DARK,spaceBefore=11,spaceAfter=6))
    styles.add(ParagraphStyle(name='GVSmall',fontName='Helvetica',fontSize=8,leading=11,
                              textColor=DARK,spaceAfter=4))
    styles.add(ParagraphStyle(name='GVMini',fontName='Helvetica',fontSize=7,leading=10,
                              textColor=GREY,spaceAfter=3))
    styles.add(ParagraphStyle(name='GVWarn',fontName='Helvetica-Bold',fontSize=8,leading=12,
                              textColor=colors.HexColor('#8C4D0A'),spaceAfter=6))
    story=[]
    story.append(Paragraph('GrãoVision <font color="#008E89">|</font> Relatório técnico',styles['GVTitle']))
    story.append(Paragraph('RASTREABILIDADE DE AMOSTRA E RESULTADOS EXPERIMENTAIS • TCC',styles['GVSmall']))
    story.append(Paragraph('DOCUMENTO EXPERIMENTAL — NÃO É CERTIFICADO DE CLASSIFICAÇÃO VEGETAL DO MAPA, NEM LAUDO LABORATORIAL ACREDITADO.',styles['GVWarn']))
    story.append(HRFlowable(width='100%',thickness=1,color=TEAL,spaceBefore=3,spaceAfter=8))

    snap=analise.get('cadastro_snapshot') or {}
    a=snap.get('amostra') or {}
    f=snap.get('fornecedor') or {}
    c=snap.get('cliente') or {}
    r=analise.get('resumo') or {}

    def heading(s):story.append(Paragraph(escape(s),styles['GVHead']))
    def table(rows):
        data=[]
        for label,value in rows:
            data.append([Paragraph(escape(str(label)),styles['GVMini']),
                         Paragraph(escape(val(value)),styles['GVSmall'])])
        t=Table(data,colWidths=[51*mm,123*mm],hAlign='LEFT',repeatRows=0)
        t.setStyle(TableStyle([
            ('VALIGN',(0,0),(-1,-1),'TOP'),('ROWBACKGROUNDS',(0,0),(-1,-1),[colors.white,PALE]),
            ('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),6),
            ('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5),
            ('LINEBELOW',(0,-1),(-1,-1),0.25,colors.HexColor('#CFD9E2'))
        ]))
        story.append(t)

    heading('Identificação do relatório')
    table([
        ('Sessão / análise',r.get('sessao_id') or analise.get('sessao_id')),
        ('Código da amostra',a.get('codigo_amostra')),
        ('Início da análise',ddate(r.get('inicio'))),
        ('Fim da análise',ddate(r.get('fim'))),
        ('Emissão do documento',datetime.now().strftime('%d/%m/%Y %H:%M')),
    ])
    heading('Fornecedor dos grãos / origem comercial')
    table([
        ('Razão social / nome',f.get('razao_social')),
        ('Nome fantasia',f.get('nome_fantasia')),
        ('CNPJ / CPF',f.get('cpf_cnpj')),
        ('Inscrição estadual',f.get('inscricao_estadual')),
        ('Município / UF',f"{val(f.get('cidade'))} / {val(f.get('uf'))}"),
        ('Endereço',', '.join(filter(None,[str(f.get('endereco') or ''),str(f.get('numero') or ''),str(f.get('bairro') or '')]))),
    ])
    heading('Cliente / interessado na análise')
    table([
        ('Razão social / nome',c.get('razao_social')),
        ('CNPJ / CPF',c.get('cpf_cnpj')),
        ('Contato',c.get('responsavel')),
        ('Telefone / e-mail',' / '.join(filter(None,[c.get('telefone') or '',c.get('email') or '']))),
        ('Município / UF',f"{val(c.get('cidade'))} / {val(c.get('uf'))}"),
    ])

    heading('Identificação do produto, lote e amostra')
    table([
        ('Produto / variedade',' / '.join(filter(None,[a.get('produto') or '',a.get('variedade') or '']))),
        ('Safra / lote',f"{val(a.get('safra'))} / {val(a.get('lote'))}"),
        ('Origem / propriedade',a.get('propriedade_origem')),
        ('Localidade de origem',f"{val(a.get('municipio_origem'))} / {val(a.get('uf_origem'))}"),
        ('Nota fiscal / placa',f"{val(a.get('nota_fiscal'))} / {val(a.get('placa_veiculo'))}"),
        ('Peso do lote / amostra',f"{val(a.get('peso_lote_kg'))} kg / {val(a.get('peso_amostra_g'))} g"),
        ('Recebimento / chegada',f"{ddate(a.get('data_recebimento'))} {a.get('hora_recebimento') or ''}"),
        ('Amostragem / coleta',f"{ddate(a.get('data_coleta'))} {a.get('hora_coleta') or ''}"),
        ('Local / responsável coleta',f"{val(a.get('local_coleta'))} / {val(a.get('responsavel_coleta'))}"),
        ('Método de amostragem',a.get('metodo_amostragem')),
        ('Lacre / embalagem',f"{val(a.get('numero_lacre'))} / {val(a.get('tipo_embalagem'))}"),
        ('Finalidade da análise',a.get('finalidade')),
        ('Norma declarada / referência',a.get('norma_referencia')),
    ])

    heading('Resultados da análise experimental — CNN e pesagem')
    table([
        ('Status da análise',r.get('status')),
        ('Resultado da CNN',r.get('cnn_status')),
        ('Detecções válidas',r.get('deteccoes_validas')),
        ('Detecções ruins (bad)',r.get('deteccoes_bad')),
        ('Percentual visual (CNN)',_n(r.get('percentual_bad'),2)+' %'),
        ('P1 / P2 / PF (g)',f"{_n(r.get('P1'))} / {_n(r.get('P2'))} / {_n(r.get('PF'))}"),
        ('P3 / P4 / P5 (g)',f"{_n(r.get('P3'))} / {_n(r.get('P4'))} / {_n(r.get('P5'))}"),
        ('Temperatura NTC (°C)',_n(r.get('temp_camara'),2)),
        ('Aquecimento comandado','SIM' if r.get('aquecimento_comandado') else 'NÃO'),
        ('Umidade final (%)',_n(r.get('umidade'),2) if r.get('umidade') is not None else 'NÃO DETERMINADA'),
        ('Método de umidade',r.get('metodo_umidade') or 'Não validado'),
    ])
    # Imagem anotada e opcional: so incluir quando salva localmente pela CNN.
    image_name = r.get('imagem_anotada')
    if image_name:
        image_file=Path(str(image_name))
        if not image_file.is_absolute():
            base=Path(diretorio_imagens) if diretorio_imagens else Path(__file__).resolve().parents[1]
            image_file=base / image_file
        try:
            if image_file.is_file() and image_file.stat().st_size<20_000_000:
                from PIL import Image as PilImage
                with PilImage.open(image_file) as pil:
                    iw,ih=pil.size
                scale=min(155*mm/iw,80*mm/ih)
                heading('Imagem da classificação CNN (registro experimental)')
                story.append(RLImage(str(image_file),width=iw*scale,height=ih*scale))
                story.append(Paragraph('Imagem anotada automaticamente; detecções não validadas por contagem manual.',
                                       styles['GVMini']))
        except (OSError, ValueError, ImportError):
            pass
    heading('Observações e limitações')
    notes=(r.get('motivo') or '') + ' ' + (a.get('observacoes') or '')
    story.append(Paragraph(escape(notes.strip() or 'Sem observações adicionais.'),styles['GVSmall']))
    story.append(Paragraph('As contagens da CNN são detecções automatizadas e não substituem a contagem manual nem a classificação oficial. '
                           'Variação aparente de massa não prova umidade sem método de referência validado. '
                           'Dados de recebimento e amostragem são declaratórios e devem ser conferidos pelo responsável.',styles['GVMini']))
    story.append(Paragraph('Referências regulatórias para conferência conforme produto e finalidade: Lei nº 9.972/2000; '
                           'Decreto nº 12.709/2025; norma técnica específica vigente para o produto. '
                           'A existência destes campos NÃO comprova conformidade regulatória.',styles['GVMini']))
    story.append(Spacer(1,15))
    story.append(Paragraph('Responsável técnico / operador: ___________________________________  Data: ____/____/________',styles['GVSmall']))
    story.append(Paragraph('Assinatura e registro profissional, quando aplicáveis, devem ser conferidos externamente.',styles['GVMini']))

    def footer(canvas, doc):
        canvas.saveState()
        canvas.setStrokeColor(TEAL)
        canvas.line(18*mm,13*mm,192*mm,13*mm)
        canvas.setFont('Helvetica',7)
        canvas.setFillColor(GREY)
        canvas.drawString(18*mm,9*mm,'GrãoVision — TCC • documento experimental • uso interno')
        canvas.drawRightString(192*mm,9*mm,f'Página {doc.page}')
        canvas.restoreState()
    doc.build(story,onFirstPage=footer,onLaterPages=footer)
    return out
