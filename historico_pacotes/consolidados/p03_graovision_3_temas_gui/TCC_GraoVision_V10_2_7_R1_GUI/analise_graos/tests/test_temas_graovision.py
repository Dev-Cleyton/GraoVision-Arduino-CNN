"""Escolha de tema altera APENAS apresentacao e configurações locais."""
from interface.temas import THEMES, DEFAULT_THEME
from interface.operacao import load_settings, save_settings
import json
import pytest


def test_tres_temas_validos_e_diferentes():
    assert set(THEMES)=={'esmeralda','fluent_azul','operador'}
    assert len({tuple(item['palette'].values()) for item in THEMES.values()})==3
    assert all(item['title'] and item['image'] for item in THEMES.values())


def test_tema_persistido_e_recarregado(tmp_path):
    path=tmp_path/'user_theme.json'
    for nome in THEMES:
        saved=save_settings({'porta_serial':'COM4','camera_indice':0,
                             'movimentos_reais':False,'roi_automatica':True,
                             'tema_visual':nome},path)
        assert saved['tema_visual']==nome
        assert load_settings(path)['tema_visual']==nome
    assert json.loads(path.read_text(encoding='utf8'))['tema_visual']=='operador'


def test_config_antiga_sem_tema_ainda_abre(tmp_path):
    path=tmp_path/'antiga.json'
    path.write_text(json.dumps({'porta_serial':'COM5','movimentos_reais':True}))
    assert load_settings(path)['tema_visual']==DEFAULT_THEME
    assert load_settings(path)['movimentos_reais'] is True


def test_tema_invalido_bloqueado(tmp_path):
    with pytest.raises(ValueError):
        save_settings({'tema_visual':'liberar_reles'},tmp_path/'x.json')
