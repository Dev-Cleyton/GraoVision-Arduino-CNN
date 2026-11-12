from pathlib import Path
from types import SimpleNamespace
from datetime import datetime

import pytest

from rastreabilidade.banco import CadastroStore
from rastreabilidade.relatorio_pdf import gerar_pdf


def _start(tmp_path):
    store=CadastroStore(tmp_path / 'rastreabilidade.sqlite')
    f=store.salvar_pessoa('FORNECEDOR',{'razao_social':'Produtora Alfa', 'cpf_cnpj':'12.345.678/0001-90',
                                      'cidade':'Itauna','uf':'MG'})
    c=store.salvar_pessoa('CLIENTE',{'razao_social':'Cliente Exemplo','cpf_cnpj':'123.456.789-09',
                                   'cidade':'Divinopolis','uf':'MG'})
    a=store.salvar_amostra({'fornecedor_id':f,'cliente_id':c, 'codigo_amostra':'AM-TEST-001',
                             'produto':'Milho','safra':'2026/2027','lote':'LOTE-A1',
                             'data_recebimento':'10/11/2026','hora_recebimento':'13:40',
                             'data_coleta':'10/11/2026','hora_coleta':'14:10',
                             'responsavel_coleta':'Laboratorio', 'peso_amostra_g':'23,60',
                             'norma_referencia':'Nao declarada'})
    return store,f,c,a


def fake_session():
    sample=SimpleNamespace(id='20261110_ABC123', status_final='PENDENTE_VALIDACAO_UMIDADE',
       motivo_final='Monitoramento sem aquecimento; umidade não determinada.',
       status_cnn='APROVADA',motivo_cnn='Dentro do limite experimental.',
       total_graos=65,graos_ruins=5,percentual_ruins=7.6923,
       peso_inicial=23.20,peso_referencia=23.24,peso_final=23.23,
       peso_retorno=23.21,peso_segunda_subida=23.24,peso_segundo_retorno=23.22,
       temperatura_camara=25.4,umidade_final=None,metodo_umidade=None,
       aquecimento_comandado=False,eventos_aquecedor=[],codigo_erro_arduino=None,
       repetibilidade_centro_g=0.01,repetibilidade_topo_g=0.0,roi_registros=[])
    return SimpleNamespace(sample=sample, data_hora_inicio=datetime(2026,11,10,14,11),
                           data_hora_fim=datetime(2026,11,10,14,16))


def test_cadastro_relatorio_reimpressao(tmp_path):
    store, f,c,a = _start(tmp_path)
    snap=store.snapshot(a)
    idx=store.registrar_analise(a,fake_session(),snapshot_override=snap)
    assert store.analise(idx)['cadastro_snapshot']['cliente']['razao_social']=='Cliente Exemplo'
    # Edicao posterior nao altera o documento historico.
    store.salvar_pessoa('CLIENTE',{'razao_social':'Nome Alterado'},id=c)
    saved=store.analise(idx)
    assert saved['cadastro_snapshot']['cliente']['razao_social']=='Cliente Exemplo'
    pdf=gerar_pdf(saved,tmp_path/'relatorio.pdf')
    assert pdf.exists() and pdf.stat().st_size>3000
    assert len(store.analises())==1
    assert store.analise(idx)['resumo']['umidade'] is None
    from pypdf import PdfReader
    txt=' '.join(page.extract_text() for page in PdfReader(str(pdf)).pages)
    assert 'LOTE-A1' in txt
    assert 'Cliente Exemplo' in txt
    assert 'NÃO É CERTIFICADO' in txt
    assert '10/11/2026' in txt
    assert '13:40' in txt
    assert '14:10' in txt


def test_validacao_campos_obrigatorios(tmp_path):
    store, f,c,a = _start(tmp_path)
    with pytest.raises(ValueError,match='hora coleta'):
        store.salvar_amostra({'fornecedor_id':f,'cliente_id':c,'codigo_amostra':'ERR',
             'produto':'Milho','lote':'L','data_recebimento':'2026-11-10',
             'data_coleta':'2026-11-10','hora_coleta':'33:99','responsavel_coleta':'Pessoa'})
    with pytest.raises(ValueError,match='duplicado'):
        store.salvar_amostra({'fornecedor_id':f,'cliente_id':c,'codigo_amostra':'AM-TEST-001',
             'produto':'Milho','lote':'L','data_recebimento':'2026-11-10',
             'data_coleta':'2026-11-10','responsavel_coleta':'Pessoa'})
    with pytest.raises(ValueError,match='Fornecedor ou cliente'):
        store.salvar_amostra({'fornecedor_id':c,'cliente_id':f,'codigo_amostra':'ERR2',
             'produto':'Milho','lote':'L','data_recebimento':'2026-11-10',
             'data_coleta':'2026-11-10','responsavel_coleta':'Pessoa'})


def test_registro_sem_cadastro_nao_inventa_identidade(tmp_path):
    store=CadastroStore(tmp_path/'r.sqlite')
    id=store.registrar_analise(None,fake_session())
    assert store.analise(id)['cadastro_snapshot']=={}
    assert store.analises()[0]['codigo_amostra']=='SEM CADASTRO'
