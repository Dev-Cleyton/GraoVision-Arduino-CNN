# Testes sem rede nem chave real.
from pathlib import Path
import importlib.util
import json

MODULE = Path(__file__).resolve().parent / 'diagnostico_integral_roboflow_v54.py'
spec = importlib.util.spec_from_file_location('diag', MODULE)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

cases = []
def check(label, value):
    assert value, label
    cases.append(label)

def fake(tag, path, opts, method):
    assert opts.get('api_key') == 'CHAVE_FAKE_PARA_TESTES'
    if tag == 'Autenticacao / workspace':
        check('POST somente para autenticar', method == 'POST')
        return 200, {'workspace': 'espaco-teste', 'api_key': 'SEGREDO_QUE_NAO_PODE_SAIR'}
    check('demais endpoints usam GET', method == 'GET')
    if tag == 'Projeto':
        return 200, {'project': {'type':'object-detection', 'images':3000,'classes': ['bad', 'good'], 'splits':{'train': 2400,'valid':300,'test':300}, 'owner':'nomeprivado','api_key': 'SEGREDO_QUE_NAO_PODE_SAIR'}}
    if tag == 'Versao 54':
        return 200, {'version': {'images':2900, 'modelType':'yolov8n', 'train':{'modelType':'yolov8n', 'epochs': 100, 'results': {'mAP':0.80,'precision':0.82,'recall':0.75}}, 'dataset':{'splits':{'train':2300,'valid':300,'test':300}},'weightsUrl':'https://segredo.example/abc?api_key=SEGREDO_QUE_NAO_PODE_SAIR', 'models':{'roboflow-train':{'epochs':[{'epoch': i,'mAP':i/100} for i in range(100)]}}}}
    if tag == 'Treinamentos da versao 54':
        return 200, {'trainings':[{'id':'legacy/v54','modelType':'yolov8n','status':'finished'}]}
    if tag.startswith('Treinamento especifico'):
        check('treinamento selecionado por query', opts.get('trainingId') == 'legacy/v54')
        return 200, {'models':[{'metrics':{'mAP50':0.91,'mAP50_95':0.55}}], 'apiKey':'SEGREDO_QUE_NAO_PODE_SAIR'}
    if tag == 'Avaliacoes existentes':
        return 200, {'evals':[{'id':'eval-123', 'status':'done'}]}
    if tag == 'Avaliacao 1 — mAP por divisao':
        return 200, {'metrics': {'test':{'map50':0.72,'map50_95':0.45}}}
    if tag == 'Avaliacao 1 — matriz de confusao':
        return 403, {'error':'denied'}
    return 200, {'status': 'done', 'reason': 'no matching metrics'}

report, summary = m.execute(simulate=fake, quiet=True)
check('a arquitetura foi encontrada', 'yolov8n' in report)
check('divisao de imagens presente', '2400' in report)
check('classes aparecem', 'bad' in report and 'good' in report)
check('metrica mAP do treino aparece', '0.8' in report)
check('metrica de avaliacao separada aparece', '0.72' in report)
check('resultado negado nao impede as demais', 'acesso negado' not in report or 'Avaliacao 1' in report)
check('segredo nao aparece no TXT', 'SEGREDO_QUE_NAO_PODE_SAIR' not in report)
check('segredo nao aparece no JSON', 'SEGREDO_QUE_NAO_PODE_SAIR' not in json.dumps(summary))
check('nem a chave fake aparece', 'CHAVE_FAKE_PARA_TESTES' not in report)
print('TESTES APROVADOS:', len(cases))
for c in cases: print('-', c)
