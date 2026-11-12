"""Sem hardware: confirma protocolos de UI e controles de seguranca."""
import json
import queue
import threading
import pytest
from interface.operacao import (
    LocalAdmin, PromptBroker, identify_error, load_settings, save_settings,
    prompt_kind,
)


def test_tipos_de_confirmacao():
    assert prompt_kind('Apos CONFERIR, digite CONFIRMO-X:') == 'confirm_x'
    assert prompt_kind('Apos verificar, digite RETORNO-RESFRIADO:') == 'thermal_return'
    assert prompt_kind('Digite AUTORIZAR-P4-P5') == 'diagnostic'
    assert prompt_kind('Digite RETORNAR para tentar regressar') == 'supervised_return'


def test_broker_confirmaacao_nao_e_automatica():
    events=queue.Queue()
    broker=PromptBroker(events)
    values=[]
    t=threading.Thread(target=lambda:values.append(broker.ask('CONFIRMO-X')))
    t.start()
    tag,obj=events.get(timeout=1)
    assert tag=='prompt'
    assert not obj['done'].is_set()
    assert broker.answer('CONFIRMO-X',obj)
    t.join(timeout=1)
    assert values==['CONFIRMO-X']
    assert not t.is_alive()


def test_broker_bloqueia_confirmacao_repetida():
    events=queue.Queue();broker=PromptBroker(events)
    t=threading.Thread(target=lambda:broker.ask('confirmar'))
    t.start();_,obj=events.get(timeout=1)
    assert broker.answer('')
    assert not broker.answer('')
    t.join(timeout=1)


def test_admin_pin_sem_padrao(tmp_path):
    access=LocalAdmin(tmp_path/'admin.json')
    assert not access.configured
    with pytest.raises(ValueError):access.configure('123')
    access.configure('593817')
    assert access.configured
    assert access.verify('593817')
    assert not access.verify('123456')
    content=(tmp_path/'admin.json').read_text(encoding='utf-8')
    assert '593817' not in content


def test_configuracao_gui_validada(tmp_path):
    path=tmp_path/'config.json'
    cfg=save_settings({'porta_serial':'com5','camera_indice':'1',
                       'movimentos_reais':True,'roi_automatica':True},path)
    assert cfg['porta_serial']=='COM5'
    assert load_settings(path)==cfg
    with pytest.raises(ValueError):save_settings({'porta_serial':'/dev/sda'},path)


def test_ajuda_dos_erros_mais_criticos():
    for error in ('TIMEOUT_CONFIRMACAO_RETORNO','LIMITE_TEMPERATURA_SOFTWARE',
                  'CNN_TIMEOUT','EEPROM_FALHA'):
        text=identify_error(error)
        assert len(text)==2 and all(text)
