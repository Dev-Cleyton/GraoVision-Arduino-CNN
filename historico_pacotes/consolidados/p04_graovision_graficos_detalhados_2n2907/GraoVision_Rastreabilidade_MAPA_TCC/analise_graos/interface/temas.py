"""Tres aparencias da GrãoVision: apresentacao, sem efeito sobre o firmware.

As imagens sao mockups conceituais. Os paineis reais exibem somente
telemetria recebida ou resultados do ultimo ciclo.
"""
from __future__ import annotations

THEMES = {
    'esmeralda': {
        'title': 'Esmeralda',
        'subtitle': 'HMI industrial • Verde-petróleo e ciano • Painel equilibrado',
        'description': 'Pesagens e curva à esquerda; câmera, balança e operação à direita.',
        'image': 'tema_esmeralda.jpg',
        'palette': {
            'bg':'#08151D','sidebar':'#0D2530','panel':'#132B35',
            'panel2':'#1A3944','line':'#31515D','text':'#F4FAFA',
            'muted':'#ACC2C8','accent':'#2DE1CE','blue':'#87C5FA',
            'amber':'#F5BB70','red':'#E98189','white':'#FFFFFF',
        }
    },
    'fluent_azul': {
        'title': 'Fluent Azul',
        'subtitle': 'Microsoft Fluent inspirado • Índigo e azul • Cartões e linha do tempo',
        'description': 'Visão em três colunas com destaque para imagem e histórico do processo.',
        'image': 'tema_fluent_azul.jpg',
        'palette': {
            'bg':'#0D1830','sidebar':'#15264B','panel':'#1A2B50',
            'panel2':'#243A65','line':'#3B5484','text':'#F8F9FF',
            'muted':'#B7C9E9','accent':'#80ACFF','blue':'#7DBCF6',
            'amber':'#E9BE76','red':'#DF828D','white':'#FFFFFF',
        }
    },
    'operador': {
        'title': 'Operador',
        'subtitle': 'Industrial • Grafite e verde • Instruções de operação',
        'description': 'Realça a balança, as orientações e os controles supervisionados.',
        'image': 'tema_operador.jpg',
        'palette': {
            'bg':'#0F151C','sidebar':'#151E26','panel':'#1C2831',
            'panel2':'#263944','line':'#38515A','text':'#F3F8F6',
            'muted':'#B4C8C6','accent':'#42D9A7','blue':'#8CB8CF',
            'amber':'#F0B86A','red':'#E18C8A','white':'#FFFFFF',
        }
    }
}
DEFAULT_THEME = 'esmeralda'

def get_theme(key):
    return THEMES.get(key, THEMES[DEFAULT_THEME])
