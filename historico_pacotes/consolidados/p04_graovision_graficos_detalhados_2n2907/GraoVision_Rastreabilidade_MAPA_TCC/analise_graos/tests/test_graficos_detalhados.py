from interface.graficos_detalhados import DadosGraficos, salvar_png
import csv


def test_captura_todas_72_leituras_e_medias(tmp_path):
    d=DadosGraficos()
    for i,stage in enumerate(('P1','P2','PF','P3','P4','P5')):
        for j in range(1,13):
            assert d.receber(f'VETOR:{stage}:{j}:{i*25000+j*80}:{0 if i in (0,3,5) else 14000}:16.8000')
        assert d.receber(f'{stage}:16.8000')
    assert len(d.vetores)==72
    assert len(d.medias)==6
    assert d.receber('MEDICAO:0:16.8:25.1')
    assert d.receber('MEDICAO:30:16.75:25.4')
    assert len(d.amostras_massa())==74
    assert len(d.amostras_temperatura())==2
    assert not d.receber('AQUECEDOR:ON_COMANDADO:0')
    csv_file=d.salvar_csv(tmp_path)
    with csv_file.open(encoding='utf-8-sig') as f:
        lines=list(csv.DictReader(f,delimiter=';'))
    assert len([r for r in lines if r['tipo']=='vetor'])==72
    assert salvar_png(d,tmp_path).stat().st_size>10000


def test_pontos_extras_ntc_nao_sao_inventados():
    d=DadosGraficos()
    assert d.amostras_temperatura()==[]
    d.receber('TEMP_MONITOR:5:25.20')
    d.receber('MEDICAO:30:16.7:25.30')
    d.receber('TEMP_MONITOR:10:25.25')
    assert [p[0] for p in d.amostras_temperatura()]==[5,10,30]
    assert not d.receber('TEMP_MONITOR:15:ERRO')
    assert len(d.amostras_temperatura())==3
    d.limpar()
    assert not d.temperaturas
    assert not d.vetores
